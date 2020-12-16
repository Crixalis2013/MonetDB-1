
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "rel_statistics.h"
#include "rel_optimizer.h"
#include "rel_rewriter.h"

static void exps_have_or_or_semantics(list *exps, bool *have_or, bool *have_semantics);

static void
exp_have_or_or_semantics(sql_exp *e, bool *have_or, bool *have_semantics)
{
	if (e->type == e_cmp) {
		if (e->semantics)
			*have_semantics = true;
		if (e->flag == cmp_in || e->flag == cmp_notin) {
			exp_have_or_or_semantics(e->l, have_or, have_semantics);
			exps_have_or_or_semantics(e->r, have_or, have_semantics);
		} else if (e->flag == cmp_or) {
			*have_or = true;
			exps_have_or_or_semantics(e->l, have_or, have_semantics);
			exps_have_or_or_semantics(e->r, have_or, have_semantics);
		} else if (e->flag == cmp_filter) {
			exps_have_or_or_semantics(e->l, have_or, have_semantics);
			exps_have_or_or_semantics(e->r, have_or, have_semantics);
		} else {
			exp_have_or_or_semantics(e->l, have_or, have_semantics);
			exp_have_or_or_semantics(e->r, have_or, have_semantics);
			if (e->f)
				exp_have_or_or_semantics(e->f, have_or, have_semantics);
		}
	}
}

static void
exps_have_or_or_semantics(list *exps, bool *have_or, bool *have_semantics)
{
	if (exps)
		for (node *n = exps->h ; n ; n = n->next)
			exp_have_or_or_semantics(n->data, have_or, have_semantics);
}

static sql_exp *
comparison_find_column(sql_exp *input, sql_exp *e)
{
	switch (input->type) {
		case e_convert:
			return comparison_find_column(input->l, e) ? input : NULL;
		case e_column:
			return exp_match(e, input) ? input : NULL;
		default:
			return NULL;
	}
}

static sql_exp *
rel_propagate_column_ref_statistics(mvc *sql, sql_rel *rel, sql_exp *e)
{
	atom *lval, *rval;
	sql_exp *ne = NULL;

	assert(e->type == e_column);
	if (rel) {
		switch(rel->op) {
		case op_left:
		case op_right:
		case op_full:
		case op_join:
		case op_select:
		/* case op_anti: later */
		case op_semi: {
			bool have_or = false, have_semantics = false, found_left = false, found_right = false, found_on_exps = false;
			sql_exp *ne = NULL;

			exps_have_or_or_semantics(rel->exps, &have_or, &have_semantics);
			if (!list_empty(rel->exps)) { /* if there's an or, the MIN and MAX get difficult to propagate */
				for (node *n = rel->exps->h ; n ; n = n->next) {
					sql_exp *comp = n->data;

					if (comp->type == e_cmp) {
						switch (comp->flag) {
						case cmp_equal: {
							sql_exp *le = comp->l, *lne = NULL, *re = comp->r, *rne = NULL;

							if ((lne = comparison_find_column(le, e)) || (rne = comparison_find_column(re, e))) {
								if (is_outerjoin(rel->op)) {
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MIN)))
										set_min_property(sql, e, lval);
								} else {
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MAX)) &&
										(rval = find_prop_and_get(re->p, PROP_MAX)))
										set_max_property(sql, e, atom_min(lval, rval)); /* for equality reduce */
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MIN)) &&
										(rval = find_prop_and_get(re->p, PROP_MIN)))
										set_min_property(sql, e, atom_max(lval, rval));
								}
							}
							ne = ne ? ne : lne ? lne : rne;
						} break;
						case cmp_notequal: {
							sql_exp *le = comp->l, *lne = NULL, *re = comp->r, *rne = NULL;

							if ((lne = comparison_find_column(le, e)) || (rne = comparison_find_column(re, e))) {
								if (is_outerjoin(rel->op)) {
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MIN)))
										set_min_property(sql, e, lval);
								} else {
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MAX)) &&
										(rval = find_prop_and_get(re->p, PROP_MAX)))
										set_max_property(sql, e, atom_max(lval, rval));/* for inequality expand */
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MIN)) &&
										(rval = find_prop_and_get(re->p, PROP_MIN)))
										set_min_property(sql, e, atom_min(lval, rval));
								}
							}
							ne = ne ? ne : lne ? lne : rne;
						} break;
						case cmp_gt:
						case cmp_gte: {
							sql_exp *le = comp->l, *lne = NULL, *re = comp->r, *rne = NULL;

							assert(!comp->f);
							if ((lne = comparison_find_column(le, e)) || (rne = comparison_find_column(re, e))) {
								if (is_outerjoin(rel->op)) {
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re->p, PROP_MIN)))
										set_min_property(sql, e, lval);
								} else if (lne) {
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (rval = find_prop_and_get(re->p, PROP_MAX)))
										set_min_property(sql, e, rval);
								} else {
									if (!have_or && (lval = find_prop_and_get(le->p, PROP_MIN)))
										set_max_property(sql, e, lval);
									if (!have_or && (rval = find_prop_and_get(re->p, PROP_MIN)))
										set_min_property(sql, e, rval);
								}
							}
							ne = ne ? ne : lne ? lne : rne;
						} break;
						case cmp_lt:
						case cmp_lte: {
							sql_exp *le = comp->l, *lne = NULL, *re = comp->r, *rne = NULL, *fe = comp->f, *fne = NULL;

							if ((lne = comparison_find_column(le, e)) || (rne = comparison_find_column(re, e)) || (fe && (fne = comparison_find_column(fe, e)))) {
								if (is_outerjoin(rel->op)) {
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re ? re->p : fe->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (lval = find_prop_and_get(le ? le->p : re ? re->p : fe->p, PROP_MIN)))
										set_min_property(sql, e, lval);
								} else if (lne) {
									if (fe) { /* range case */
										if (!have_or && (lval = find_prop_and_get(fe->p, PROP_MIN)))
											set_max_property(sql, e, lval);
										if (!have_or && (rval = find_prop_and_get(re->p, PROP_MAX)))
											set_min_property(sql, e, rval);
									} else {
										if (!have_or && (lval = find_prop_and_get(re->p, PROP_MIN)))
											set_max_property(sql, e, lval);
										if (!have_or && (rval = find_prop_and_get(le->p, PROP_MIN)))
											set_min_property(sql, e, rval);
									}
								} else if (rne) {
									if (fe) { /* range case */
										if (!have_or && (lval = find_prop_and_get(re->p, PROP_MIN)))
											set_max_property(sql, e, lval);
										if (!have_or && (rval = find_prop_and_get(le->p, PROP_MIN)))
											set_min_property(sql, e, rval);
									} else {
										if (!have_or && (lval = find_prop_and_get(re->p, PROP_MAX)))
											set_max_property(sql, e, lval);
										if (!have_or && (rval = find_prop_and_get(le->p, PROP_MAX)))
											set_min_property(sql, e, rval);
									}
								} else { /* range case */
									if (!have_or && (lval = find_prop_and_get(fe->p, PROP_MAX)))
										set_max_property(sql, e, lval);
									if (!have_or && (rval = find_prop_and_get(le->p, PROP_MAX)))
										set_min_property(sql, e, rval);
								}
							}
							ne = ne ? ne : lne ? lne : rne ? rne : fne;
						} break;
						default: /* Maybe later I can do cmp_in and cmp_notin */
							break;
						}
					}
				}
			}
			if (ne)
				found_on_exps = true;
			if (!ne && (ne = rel_propagate_column_ref_statistics(sql, rel->l, e)))
				found_left = true;
			if (!ne && is_join(rel->op) && (ne = rel_propagate_column_ref_statistics(sql, rel->r, e)))
				found_right = true;
			if (ne) {
				/* if semantics flag was found, null values will pass */
				if (is_full(rel->op) || (is_left(rel->op) && found_right) || (is_right(rel->op) && found_left) || have_semantics)
					set_has_nil(e);
				else if (found_on_exps && (!has_nil(ne) || !is_outerjoin(rel->op))) /* at an outer join, null values pass */
					set_has_no_nil(e);
			}
		} break;
		/* case op_table: later */
		case op_basetable: {
			if (e->l && (ne = exps_bind_column2(rel->exps, e->l, e->r, NULL))) {
				if ((lval = find_prop_and_get(ne->p, PROP_MAX)))
					set_max_property(sql, e, lval);
				if ((lval = find_prop_and_get(ne->p, PROP_MIN)))
					set_min_property(sql, e, lval);
				if (!has_nil(ne))
					set_has_no_nil(e);
			}
		} break;
		case op_union:
		case op_except:
		case op_inter:
		case op_project:
		case op_groupby: {
			ne = e->l ? exps_bind_column2(rel->exps, e->l, e->r, NULL) : exps_bind_column(rel->exps, e->r, NULL, NULL, 1);
			if (ne) {
				if ((lval = find_prop_and_get(ne->p, PROP_MAX)))
					set_max_property(sql, e, lval);
				if ((lval = find_prop_and_get(ne->p, PROP_MIN)))
					set_min_property(sql, e, lval);
				if (!has_nil(ne))
					set_has_no_nil(e);
			}
		} break;
		default: /* if there is a topN or sample relation in between, then the MIN and MAX values are lost */
			break;
		}
	}
	return ne;
}

static atom *
atom_from_valptr( sql_allocator *sa, sql_subtype *tpe, ValPtr pt)
{
	atom *a = SA_ZNEW(sa, atom);

	a->tpe = *tpe;
	a->data.vtype = tpe->type->localtype;
	if (ATOMstorage(a->data.vtype) == TYPE_str) {
		if (VALisnil(pt)) {
			VALset(&a->data, a->data.vtype, (ptr) ATOMnilptr(a->data.vtype));
		} else {
			a->data.val.sval = sa_strdup(sa, pt->val.sval);
			a->data.len = strlen(a->data.val.sval);
		}
	} else {
		VALcopy(&a->data, pt);
	}
	a->isnull = VALisnil(&a->data);
	return a;
}

static sql_exp *
rel_basetable_get_statistics(visitor *v, sql_rel *rel, sql_exp *e, int depth)
{
	mvc *sql = v->sql;
	sql_column *c = NULL;

	(void)depth;
	if ((c = name_find_column(rel, e->l, e->r, -2, NULL))) {
		ValPtr min = NULL, max = NULL;

		if (has_nil(e) && mvc_has_no_nil(sql, c))
			set_has_no_nil(e);
 
		if (EC_NUMBER(c->type.type->eclass) || EC_VARCHAR(c->type.type->eclass) || EC_TEMP_NOFRAC(c->type.type->eclass) || c->type.type->eclass == EC_DATE) {
			if ((max = mvc_has_max_value(sql, c))) {
				prop *p = e->p = prop_create(sql->sa, PROP_MAX, e->p);
				p->value = atom_from_valptr(sql->sa, &c->type, max);
			}
			if ((min = mvc_has_min_value(sql, c))) {
				prop *p = e->p = prop_create(sql->sa, PROP_MIN, e->p);
				p->value = atom_from_valptr(sql->sa, &c->type, min);
			}
		}

	}
	return e;
}

static void
rel_set_get_statistics(mvc *sql, sql_rel *rel, sql_exp *e, int i)
{
	sql_exp *le = list_fetch(((sql_rel*)(rel->l))->exps, i);
	sql_exp *re = list_fetch(((sql_rel*)(rel->r))->exps, i);
	atom *lval, *rval;

	assert(le && e);
	if ((lval = find_prop_and_get(le->p, PROP_MAX)) && (rval = find_prop_and_get(re->p, PROP_MAX))) {
		if (rel->op == op_union)
			set_max_property(sql, e, atom_max(lval, rval)); /* for union the new max will be the max of the two */
		else if (rel->op == op_inter)
			set_max_property(sql, e, atom_min(lval, rval)); /* for intersect the new max will be the min of the two */
		else
			set_max_property(sql, e, lval);
	}
	if ((lval = find_prop_and_get(le->p, PROP_MIN)) && (rval = find_prop_and_get(re->p, PROP_MIN))) {
		if (rel->op == op_union)
			set_min_property(sql, e, atom_min(lval, rval)); /* for union the new min will be the min of the two */
		else if (rel->op == op_inter)
			set_min_property(sql, e, atom_max(lval, rval)); /* for intersect the new min will be the max of the two */
		else
			set_min_property(sql, e, lval);
	}

	if (rel->op == op_union) {
		if (!has_nil(le) && !has_nil(re))
			set_has_no_nil(e);
	} else if (rel->op == op_inter) {
		if (!has_nil(le) || !has_nil(re))
			set_has_no_nil(e);
	} else {
		if (!has_nil(le))
			set_has_no_nil(e);
	}
}

static sql_exp *
rel_propagate_statistics(visitor *v, sql_rel *rel, sql_exp *e, int depth)
{
	mvc *sql = v->sql;
	atom *lval;

	(void) depth;
	switch(e->type) {
	case e_column: {
		switch (rel->op) {
		case op_join:
		case op_left:
		case op_right:
		case op_full: {
			sql_exp *found = rel_propagate_column_ref_statistics(sql, rel->l, e);
			if (!found)
				(void) rel_propagate_column_ref_statistics(sql, rel->r, e);
		} break;
		case op_semi:
		case op_select:
		case op_project:
		case op_groupby:
			(void) rel_propagate_column_ref_statistics(sql, rel->l, e);
			break;
		case op_insert:
		case op_update:
		case op_delete:
			(void) rel_propagate_column_ref_statistics(sql, rel->r, e);
			break;
		default:
			break;
		}
	} break;
	case e_convert: {
		sql_subtype *to = exp_totype(e), *from = exp_fromtype(e);
		sql_exp *l = e->l;

		if (EC_NUMBER(from->type->eclass) && EC_NUMBER(to->type->eclass)) {
			if ((lval = find_prop_and_get(l->p, PROP_MAX))) {
				atom *res = atom_dup(sql->sa, lval);
				if (atom_cast(sql->sa, res, to))
					set_max_property(sql, e, res);
			}
			if ((lval = find_prop_and_get(l->p, PROP_MIN))) {
				atom *res = atom_dup(sql->sa, lval);
				if (atom_cast(sql->sa, res, to))
					set_min_property(sql, e, res);
			}
		}
		if (!has_nil(l))
			set_has_no_nil(e);
	} break;
	case e_aggr:
	case e_func: {
		sql_subfunc *f = e->f;

		if (!f->func->s) {
			int key = hash_key(f->func->base.name); /* Using hash lookup */
			sql_hash_e *he = sql_functions_lookup->buckets[key&(sql_functions_lookup->size-1)];
			lookup_function look = NULL;

			for (; he && !look; he = he->chain) {
				struct function_properties* fp = (struct function_properties*) he->value;

				if (!strcmp(f->func->base.name, fp->name))
					look = fp->func;
			}
			if (look)
				look(sql, e);
		}
		if (!e->semantics && e->l && !have_nil(e->l))
			set_has_no_nil(e);
	} break;
	case e_atom: {
		if (e->l) {
			atom *a = (atom*) e->l;
			if (!a->isnull) {
				set_max_property(sql, e, a);
				set_min_property(sql, e, a);
			}
		} else if (e->f) {
			list *vals = (list *) e->f;
			sql_exp *first = vals->h ? vals->h->data : NULL;
			atom *max = NULL, *min = NULL; /* all child values must have a valid min/max */

			if (first) {
				max = ((lval = find_prop_and_get(first->p, PROP_MAX))) ? lval : NULL;
				min = ((lval = find_prop_and_get(first->p, PROP_MIN))) ? lval : NULL;
			}

			for (node *n = vals->h ? vals->h->next : NULL ; n ; n = n->next) {
				sql_exp *ee = n->data;

				if (max) {
					if ((lval = find_prop_and_get(ee->p, PROP_MAX))) {
						max = atom_cmp(lval, max) > 0 ? lval : max;
					} else {
						max = NULL;
					}
				}
				if (min) {
					if ((lval = find_prop_and_get(ee->p, PROP_MIN))) {
						min = atom_cmp(min, lval) > 0 ? lval : min;
					} else {
						min = NULL;
					}
				}
			}

			if (max)
				set_max_property(sql, e, max);
			if (min)
				set_min_property(sql, e, min);
		}
	} break;
	case e_cmp:
		/* propagating min and max of booleans is not very worth it */
		if (e->flag == cmp_or || e->flag == cmp_filter) {
			if (!have_nil(e->l) && !have_nil(e->r))
				set_has_no_nil(e);
		} else if (e->flag == cmp_in || e->flag == cmp_notin) {
			sql_exp *le = e->l;
			if (!has_nil(le) && !have_nil(e->r))
				set_has_no_nil(e);
		} else {
			sql_exp *le = e->l, *re = e->r, *fe = e->f;
			if (!has_nil(le) && !has_nil(re) && (!e->f || !has_nil(fe)))
				set_has_no_nil(e);
		}
		break;
	case e_psm:
		break;
	}
	return e;
}

static sql_rel *
rel_get_statistics(visitor *v, sql_rel *rel)
{
	switch(rel->op){
	case op_basetable:
		rel->exps = exps_exp_visitor_bottomup(v, rel, rel->exps, 0, &rel_basetable_get_statistics, false);
		break;
	case op_union:
	case op_inter:
	case op_except: {
		int i = 0;
		for (node *n = rel->exps->h ; n ; n = n->next) {
			rel_set_get_statistics(v->sql, rel, n->data, i);
			i++;
		}
	} break;
	case op_join:
	case op_left:
	case op_right:
	case op_full:
	case op_semi:
	case op_select:
	case op_project:
	case op_groupby:
	case op_insert:
	case op_update:
	case op_delete:
		rel->exps = exps_exp_visitor_bottomup(v, rel, rel->exps, 0, &rel_propagate_statistics, false);
		if (is_simple_project(rel->op) && !list_empty(rel->r))
			rel->r = exps_exp_visitor_bottomup(v, rel, rel->r, 0, &rel_propagate_statistics, false);
		break;
	/*These relations are less important for now
	case op_anti:
	case op_ddl:
	case op_table:
	case op_truncate:
	case op_topn:
	case op_sample:*/
	default:
		break;
	}

	return rel;
}

static sql_rel *
rel_simplify_count(visitor *v, sql_rel *rel)
{
	mvc *sql = v->sql;

	if (is_groupby(rel->op)) {
		int ncountstar = 0;

		/* Convert count(no null) into count(*) */
		for (node *n = rel->exps->h; n ; n = n->next) {
			sql_exp *e = n->data;

			if (exp_aggr_is_count(e) && !need_distinct(e)) {
				if (list_length(e->l) == 0) {
					ncountstar++;
				} else if (list_length(e->l) == 1 && !has_nil((sql_exp*)((list*)e->l)->h->data)) {
					sql_subfunc *cf = sql_bind_func(sql->sa, sql->session->schema, "count", sql_bind_localtype("void"), NULL, F_AGGR);
					sql_exp *ne = exp_aggr(sql->sa, NULL, cf, 0, 0, e->card, 0);
					if (exp_name(e))
						exp_prop_alias(sql->sa, ne, e);
					n->data = ne;
					ncountstar++;
				}
			}
		}
		/* With multiple count(*), use exp_ref to reduce the number of calls to this aggregate */
		if (ncountstar > 1) {
			sql_exp *count_star = NULL;
			for (node *n = rel->exps->h; n ; n = n->next) {
				sql_exp *e = n->data;

				if (exp_aggr_is_count(e) && !need_distinct(e) && list_length(e->l) == 0) {
					if (!count_star) {
						count_star = e;
					} else {
						sql_exp *ne = exp_ref(sql, count_star);
						if (exp_name(e))
							exp_prop_alias(sql->sa, ne, e);
						n->data = ne;
					}
				}
			}
		}
	}
	return rel;
}

sql_rel *
rel_statistics(mvc *sql, sql_rel *rel)
{
	visitor v = { .sql = sql };
	global_props gp = (global_props) {.cnt = {0},};
	rel_properties(sql, &gp, rel);

	rel = rel_visitor_bottomup(&v, rel, &rel_get_statistics);
	if (gp.cnt[op_groupby])
		rel = rel_visitor_bottomup(&v, rel, &rel_simplify_count);
	return rel;
}
