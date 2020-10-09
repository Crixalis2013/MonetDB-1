/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

/*
 * Implementation of COPY BINARY INTO
 */

#include "monetdb_config.h"
#include "mapi_prompt.h"
#include "sql.h"
#include "mal_backend.h"
#include "mal_interpreter.h"
#include "copybinary.h"

#define bailout(...) do { \
		msg = createException(MAL, "sql.importColumn", SQLSTATE(42000) __VA_ARGS__); \
		goto end; \
	} while (0)


static str
BATattach_as_bytes(BAT *bat, stream *s, int tt, lng rows_estimate, void (*fixup)(void*,void*), int *eof_seen)
{
	str msg = MAL_SUCCEED;
	size_t asz = (size_t) ATOMsize(tt);
	size_t chunk_size = 1<<20;

	bool eof = false;
	while (!eof) {
		assert(chunk_size % asz == 0);
		size_t n;
		if (rows_estimate > 0) {
			n = rows_estimate;
			rows_estimate = 0;
		} else {
			n = chunk_size / asz;
		}

		// First make some room
		BUN validCount = bat->batCount;
		BUN newCount = validCount + n;
		if (BATextend(bat, newCount) != GDK_SUCCEED)
			bailout("BATattach_as_bytes: %s", GDK_EXCEPTION);

		// Read into the newly allocated space
		char *start = Tloc(bat, validCount);
		char *cur = start;
		char *end = Tloc(bat, newCount);
		while (cur < end) {
			ssize_t nread = mnstr_read(s, cur, 1, end - cur);
			if (nread < 0)
				bailout("BATattach_as_bytes: %s", mnstr_peek_error(s));
			if (nread == 0) {
				eof = true;
				size_t tail = (cur - start) % asz;
				if (tail != 0) {
					bailout("BATattach_as_bytes: final item incomplete: %d bytes instead of %d", (int) tail, (int) asz);
				}
				end = cur;
			}
			cur += (size_t) nread;
		}
		fixup(start, end);
		bat->batCount += (end - start) / asz;
	}

	BATsetcount(bat, bat->batCount);
	bat->tseqbase = oid_nil;
	bat->tnonil = bat->batCount == 0;
	bat->tnil = false;
	if (bat->batCount <= 1) {
		bat->tsorted = true;
		bat->trevsorted = true;
		bat->tkey = true;
	} else {
		bat->tsorted = false;
		bat->trevsorted = false;
		bat->tkey = false;
	}

end:
	*eof_seen = (int)eof;
	return msg;
}

static
void convert_bte(void *start, void *end)
{
	(void)start;
	(void)end;
}

static
void convert_sht(void *start, void *end)
{
	for (sht *p = start; p < (sht*)end; p++)
		COPY_BINARY_CONVERT16(*p);
}

static
void convert_int(void *start, void *end)
{
	for (int *p = start; p < (int*)end; p++)
		COPY_BINARY_CONVERT32(*p);
}

static
void convert_lng(void *start, void *end)
{
	for (lng *p = start; p < (lng*)end; p++)
		COPY_BINARY_CONVERT64(*p);
}

#ifdef HAVE_HGE
static
void convert_hge(void *start, void *end)
{
	for (hge *p = start; p < (hge*)end; p++)
		COPY_BINARY_CONVERT128(*p);
}
#endif

static void
convert_line_endings(char *text)
{
	// read- and write pointers
	const char *r = text;
	char *w = text;
	while (*r) {
		if (r[0] == '\r' && r[1] == '\n')
			r++;
		*w++ = *r++;
	}
	*w = '\0';
}

static str
append_text(BAT *bat, char *start, char *end)
{
	(void)bat;

	char *cr = memchr(start, '\r', end - start);
	if (cr)
		convert_line_endings(cr);

	if (BUNappend(bat, start, false) != GDK_SUCCEED)
		return createException(SQL, "sql.importColumn", GDK_EXCEPTION);

	return MAL_SUCCEED;
}

// Load items from the stream and put them in the BAT.
// Because it's text read from a binary stream, we replace \r\n with \n.
// We don't have to validate the utf-8 structure because BUNappend does that for us.
static str
load_zero_terminated_text(BAT *bat, stream *s, int *eof_reached)
{
	str msg = MAL_SUCCEED;
	bstream *bs = NULL;

	bs = bstream_create(s, 1 << 20);
	if (bs == NULL) {
		msg = createException(SQL, "sql", SQLSTATE(HY013) MAL_MALLOC_FAIL);
		goto end;
	}

	// In the outer loop we refill the buffer until the stream ends.
	// In the inner loop we look for complete \0-terminated strings.
	while (1) {
		ssize_t nread = bstream_next(bs);
		if (nread < 0)
			bailout("%s", mnstr_peek_error(s));
		if (nread == 0)
			break;

		char *buf_start = &bs->buf[bs->pos];
		char *buf_end = &bs->buf[bs->len];
		char *start, *end;
		for (start = buf_start; (end = memchr(start, '\0', buf_end - start)) != NULL; start = end + 1) {
			msg = append_text(bat, start, end);
			if (msg != NULL)
				goto end;
		}
		bs->pos = start - buf_start;
	}

	// It's an error to have date left after falling out of the outer loop
	if (bs->pos < bs->len)
		bailout("unterminated string at end");

end:
	*eof_reached = 0;
	if (bs != NULL) {
		*eof_reached = (int)bs->eof;
		bs->s = NULL;
		bstream_destroy(bs);
	}
	return msg;
}



static struct type_rec {
	char *method;
	int gdk_type;
	str (*loader)(BAT *bat, stream *s, int *eof_reached);
	void (*convert_in_place)(void *start, void *end);
} type_recs[] = {
	{ "bte", TYPE_bte, .convert_in_place=convert_bte, },
	{ "sht", TYPE_sht, .convert_in_place=convert_sht, },
	{ "int", TYPE_int, .convert_in_place=convert_int, },
	{ "lng", TYPE_lng, .convert_in_place=convert_lng, },
#ifdef HAVE_HGE
	{ "hge", TYPE_hge, .convert_in_place=convert_hge, },
#endif
	{ "str", TYPE_str, .loader=load_zero_terminated_text },
};


static struct type_rec*
find_type_rec(str name)
{
	struct type_rec *end = (struct type_rec*)((char *)type_recs + sizeof(type_recs));
	for (struct type_rec *t = &type_recs[0]; t < end; t++)
		if (strcmp(t->method, name) == 0)
			return t;
	return NULL;
}


static str
load_column(struct type_rec *rec, BAT *bat, stream *s, int *eof_reached)
{
	str msg = MAL_SUCCEED;
	(void)rec;
	(void)bat;
	(void)s;
	(void)eof_reached;

	if (rec->loader != NULL) {
		msg = rec->loader(bat, s, eof_reached);
	} else if (rec->convert_in_place != NULL) {
		// These types can be read directly into the BAT
		msg = BATattach_as_bytes(bat, s, rec->gdk_type, 0, rec->convert_in_place, eof_reached);
	} else {
		*eof_reached = 0;
		bailout("invalid loader configuration for '%s'", rec->method);
	}

	end:
		return msg;
}


static str
start_mapi_file_upload(backend *be, str path, stream **s)
{
	str msg = MAL_SUCCEED;
	*s = NULL;

	stream *ws = be->mvc->scanner.ws;
	bstream *bs = be->mvc->scanner.rs;
	stream *rs = bs->s;
	assert(isa_block_stream(ws));
	assert(isa_block_stream(rs));

	mnstr_write(ws, PROMPT3, sizeof(PROMPT3)-1, 1);
	mnstr_printf(ws, "rb %s\n", path);
	mnstr_flush(ws, MNSTR_FLUSH_DATA);
	while (!bs->eof)
		bstream_next(bs);
	char buf[80];
	if (mnstr_readline(rs, buf, sizeof(buf)) > 1) {
		msg = createException(IO, "sql.importColumn", "Error %s", buf);
		goto end;
	}
	set_prompting(rs, PROMPT2, ws);

	*s = rs;
end:
	return msg;
}


static str
finish_mapi_file_upload(backend *be, bool eof_reached)
{
	str msg = MAL_SUCCEED;
	stream *ws = be->mvc->scanner.ws;
	bstream *bs = be->mvc->scanner.rs;
	stream *rs = bs->s;
	assert(isa_block_stream(ws));
	assert(isa_block_stream(rs));

	set_prompting(rs, NULL, NULL);
	if (!eof_reached) {
		// Probably due to an error. Read until message boundary.
		char buf[8190];
		while (1) {
			ssize_t nread = mnstr_read(rs, buf, 1, sizeof(buf));
			if (nread > 0)
				continue;
			if (nread < 0)
				msg = createException(
					IO, "sql.importColumn",
					"while syncing read stream: %s", mnstr_peek_error(rs));
			break;
		}
	}
	mnstr_write(ws, PROMPT3, sizeof(PROMPT3)-1, 1);
	mnstr_flush(ws, MNSTR_FLUSH_DATA);

	return msg;
}



/* Import a single file into a new BAT.
 */
static str
importColumn(backend *be, bat *ret, lng *retcnt, str method, str path, int onclient,  lng nrows)
{
	// In this function we create the BAT and open the file, and tidy
	// up when things go wrong. The actual work happens in load_column().

	// These are managed by the end: block.
	str msg = MAL_SUCCEED;
	BAT *bat = NULL;
	stream *stream_to_close = NULL;
	bool do_finish_mapi = false;
	int eof_reached = -1; // 1 = read to the end; 0 = stopped reading early; -1 = unset, a bug.

	// This one is not managed by the end: block
	stream *s;

	// Set safe values
	*ret = 0;
	*retcnt = 0;

	// Figure out what kind of data we have
	struct type_rec *rec = find_type_rec(method);
	if (rec == NULL)
		bailout("COPY BINARY FROM not implemented for '%s'", method);

	// Create the BAT
	bat = COLnew(0, rec->gdk_type, nrows, TRANSIENT);
	if (bat == NULL)
		bailout("%s", GDK_EXCEPTION);

	// Open the input stream
	if (onclient) {
		s = NULL;
		do_finish_mapi = true;
		msg = start_mapi_file_upload(be, path, &s);
		if (msg != MAL_SUCCEED)
			goto end;
	} else {
		s = stream_to_close = open_rstream(path);
		if (s == NULL)
			bailout("Couldn't open '%s' on server: %s", path, mnstr_peek_error(NULL));
	}

	// Do the work
	msg = load_column(rec, bat, s, &eof_reached);
	if (eof_reached != 0 && eof_reached != 1) {
		if (msg)
			bailout("internal error in sql.importColumn: eof_reached not set (%s). Earlier error: %s", method, msg);
		else
			bailout("internal error in sql.importColumn: eof_reached not set (%s)", method);
	}
	// Fall through into the end block which will clean things up
end:
	if (do_finish_mapi) {
		str msg1 = finish_mapi_file_upload(be, eof_reached == 1);
		if (msg == MAL_SUCCEED)
			msg = msg1;
	}

	if (stream_to_close)

		close_stream(stream_to_close);

	// Manage the return values and `bat`.
	if (msg == MAL_SUCCEED) {
		BBPkeepref(bat->batCacheid); // should I call this?
		*ret = bat->batCacheid;
		*retcnt = BATcount(bat);
	} else {
		if (bat != NULL) {
			BBPunfix(bat->batCacheid);
			bat = NULL;
		}
		*ret = 0;
		*retcnt = 0;
	}

	return msg;
}


str
mvc_bin_import_column_wrap(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	(void)mb;

	assert(pci->retc == 2);
	bat *ret = getArgReference_bat(stk, pci, 0);
	lng *retcnt = getArgReference_lng(stk, pci, 1);

	assert(pci->argc == 6);
	str method = *getArgReference_str(stk, pci, 2);
	str path = *getArgReference_str(stk, pci, 3);
	int onclient = *getArgReference_int(stk, pci, 4);
	lng nrows = *getArgReference_lng(stk, pci, 5);

	backend *be = cntxt->sqlcontext;

	return importColumn(be, ret, retcnt, method, path, onclient, nrows);
}
