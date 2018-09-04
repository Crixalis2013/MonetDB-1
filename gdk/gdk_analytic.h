/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2018 MonetDB B.V.
 */

/*
* Pedro Ferreira
* This file contains implementations for SQL window analytical functions.
*/

#ifndef _GDK_ANALYTIC_H_
#define _GDK_ANALYTIC_H_

#include "gdk.h"

gdk_export gdk_return GDKanalyticaldiff(BAT *r, BAT *b, BAT *c, int tpe);

gdk_export gdk_return GDKanalyticalntile(BAT *r, BAT *b, BAT *p, BAT *o, int tpe, const void* restrict ntile);
gdk_export gdk_return GDKanalyticallag(BAT *r, BAT *b, BAT *p, BAT *o, BUN lag, const void* restrict default_value, int tpe);
gdk_export gdk_return GDKanalyticallead(BAT *r, BAT *b, BAT *p, BAT *o, BUN lead, const void* restrict default_value, int tpe);
gdk_export gdk_return GDKanalyticalfirst(BAT *r, BAT *b, BAT *p, BAT *o, int tpe);
gdk_export gdk_return GDKanalyticallast(BAT *r, BAT *b, BAT *p, BAT *o, int tpe);
gdk_export gdk_return GDKanalyticalnthvalue(BAT *r, BAT *b, BAT *p, BAT *o, BUN nth, int tpe);

gdk_export gdk_return GDKanalyticalmin(BAT *r, BAT *b, BAT *p, BAT *o, bit force_order, int tpe, BUN start, BUN end);
gdk_export gdk_return GDKanalyticalmax(BAT *r, BAT *b, BAT *p, BAT *o, bit force_order, int tpe, BUN start, BUN end);
gdk_export gdk_return GDKanalyticalcount(BAT *r, BAT *b, BAT *p, BAT *o, const bit* restrict ignore_nils, int tpe, BUN start, BUN end);
gdk_export gdk_return GDKanalyticalsum(BAT *r, BAT *b, BAT *p, BAT *o, bit force_order, int tp1, int tp2, BUN start, BUN end);
gdk_export gdk_return GDKanalyticalprod(BAT *r, BAT *b, BAT *p, BAT *o, bit force_order, int tp1, int tp2, BUN start, BUN end);
gdk_export gdk_return GDKanalyticalavg(BAT *r, BAT *b, BAT *p, BAT *o, bit force_order, int tpe, BUN start, BUN end);

#endif //_GDK_ANALYTIC_H_
