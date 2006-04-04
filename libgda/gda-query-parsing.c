/* gda-query-parsing.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-query.h"
#include "gda-entity-field.h"
#include "gda-query-field.h"
#include "gda-query-field-field.h"
#include "gda-query-field-value.h"
#include "gda-query-field-all.h"
#include "gda-query-field-func.h"
#include "gda-query-field-agg.h"
#include "gda-object-ref.h"
#include "gda-query-target.h"
#include "gda-entity.h"
#include "gda-dict.h"
#include "gda-dict-table.h"
#include "gda-dict-database.h"
#include "gda-dict-constraint.h"
#include "gda-xml-storage.h"
#include "gda-referer.h"
#include "gda-renderer.h"
#include "gda-parameter.h"
#include "gda-query-join.h"
#include "gda-parameter-list.h"
#include "gda-query-condition.h"
#include "gda-connection.h"
#include "gda-dict-function.h"
#include "gda-dict-aggregate.h"
#include "gda-data-handler.h"
#include "gda-server-provider.h"
#include "gda-value.h"

#include "gda-query-private.h"
#include "gda-query-parsing.h"


static void       clean_old_targets (GdaQuery *query, ParseData *pdata);
static void       clean_old_fields (GdaQuery *query, ParseData *pdata);

static GdaQueryTarget    *parsed_create_target_sql_table  (GdaQuery *query, ParseData *pdata, sql_table *table, GError **error);
static GdaQueryTarget    *parsed_create_target_db_table   (GdaQuery *query, ParseData *pdata,
							   const gchar *table, const gchar *as, GError **error);
static GdaQueryTarget    *parsed_create_target_sub_select (GdaQuery *query, ParseData *pdata, 
							   sql_select_statement *select, const gchar *as, GError **error);
static gboolean           parsed_create_join_sql_table    (GdaQuery *query, ParseData *pdata, sql_table *table, GError **error);

static GdaQueryCondition *parsed_create_simple_condition  (GdaQuery *query, ParseData *pdata, sql_condition *sqlcond, 
							   gboolean try_existing_field, GSList **targets_return, 
							   GError **error);
static GdaEntityField    *parsed_create_field_query_field  (GdaQuery *query, gboolean add_to_query, 
							    ParseData *pdata, GList *field_names,
							    gboolean try_existing_field, gboolean *new_field, 
							    GdaQueryTarget **target_return, GError **error);
static GdaEntityField    *parsed_create_value_query_field  (GdaQuery *query, gboolean add_to_query,
							    ParseData *pdata, const gchar *value, GList *param_specs, 
							    gboolean *new_field, GError **error);
static GdaEntityField    *parsed_create_func_query_field   (GdaQuery *query, gboolean add_to_query, 
							    ParseData *pdata, const gchar *funcname, 
							    GList *funcargs, 
							    gboolean try_existing_field, gboolean *new_field, GError **error);

/*
 * Conversion between sql_condition_operator enum and GdaQueryConditionType type.
 * WARNING: the converstion is not straight forward, and some more testing is required on
 * sql_condition_operator.
 */
static GdaQueryConditionType parse_condop_converter[] = {GDA_QUERY_CONDITION_LEAF_EQUAL, 
						   GDA_QUERY_CONDITION_LEAF_EQUAL,
						   GDA_QUERY_CONDITION_LEAF_IN, 
						   GDA_QUERY_CONDITION_LEAF_LIKE,
						   GDA_QUERY_CONDITION_LEAF_BETWEEN,
						   GDA_QUERY_CONDITION_LEAF_SUP, 
						   GDA_QUERY_CONDITION_LEAF_INF,
						   GDA_QUERY_CONDITION_LEAF_SUPEQUAL, 
						   GDA_QUERY_CONDITION_LEAF_INFEQUAL,
						   GDA_QUERY_CONDITION_LEAF_DIFF, 
						   GDA_QUERY_CONDITION_LEAF_REGEX,
						   GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE, 
						   GDA_QUERY_CONDITION_LEAF_NOT_REGEX,
						   GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE, 
						   GDA_QUERY_CONDITION_LEAF_SIMILAR};

/*
 * Conversion between sql_join_type enum and GdaQueryJoinType type.
 */
static GdaQueryJoinType parse_join_converter[] = {GDA_QUERY_JOIN_TYPE_CROSS, GDA_QUERY_JOIN_TYPE_INNER,
					    GDA_QUERY_JOIN_TYPE_LEFT_OUTER, GDA_QUERY_JOIN_TYPE_RIGHT_OUTER,
					    GDA_QUERY_JOIN_TYPE_FULL_OUTER};

/*
 * Creates and initialises a new ParseData structure
 */
ParseData *
parse_data_new (GdaQuery *query)
{
	ParseData *pdata = g_new0 (ParseData, 1);
	pdata->prev_targets = g_slist_copy (query->priv->targets);
	pdata->prev_fields = NULL;
	pdata->parsed_targets = NULL;
	pdata->new_targets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	pdata->sql_table_targets = g_hash_table_new (NULL, NULL);

	return pdata;
}

void
parse_data_destroy (ParseData *pdata)
{
	if (pdata->prev_targets)
		g_slist_free (pdata->prev_targets);
	if (pdata->prev_fields)
		g_slist_free (pdata->prev_fields);
	if (pdata->parsed_targets)
		g_slist_free (pdata->parsed_targets);
	if (pdata->new_targets)
		g_hash_table_destroy (pdata->new_targets);
	if (pdata->sql_table_targets)
		g_hash_table_destroy (pdata->sql_table_targets);
	g_free (pdata);
}

/*
 * Fills @targets_hash with what's already present in @query.
 * For each target, the target's alias is inserted as a key to the target object, and if there
 * is only one target representing an entity, then the entity name is also inserted as a key
 * to the target object.
 */
void
parse_data_compute_targets_hash (GdaQuery *query, ParseData *pdata)
{
	GSList *list = query->priv->targets;
	GHashTable *tmp_hash;
	gchar *str;
	gint i;
	GdaEntity *ent;
	const gchar *name;

	tmp_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	list = query->priv->targets;
	while (list) {
		str = g_utf8_strdown (gda_query_target_get_alias (GDA_QUERY_TARGET (list->data)), -1);
		g_hash_table_insert (pdata->new_targets, str, list->data);

		ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (list->data));
		if (ent) 
			name = gda_object_get_name (GDA_OBJECT (ent));
		else
			name = gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (list->data));

		if (name && *name) {
			str = g_utf8_strdown (name, -1);
			i = GPOINTER_TO_INT (g_hash_table_lookup (tmp_hash, str));
			g_hash_table_insert (tmp_hash, str, GINT_TO_POINTER (i+1));
		}

		list = g_slist_next (list);
	}
	
	list = query->priv->targets;
	while (list) {
		ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (list->data));
		if (ent)
			name = gda_object_get_name (GDA_OBJECT (ent));
		else
			name = gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (list->data));

		if (name && *name) {
			str = g_utf8_strdown (name, -1);
			i = GPOINTER_TO_INT (g_hash_table_lookup (tmp_hash, str));
			if (i == 1) 
				g_hash_table_insert (pdata->new_targets, str, list->data);
			else
				g_free (str);
		}

		list = g_slist_next (list);
	}
	g_hash_table_destroy (tmp_hash);
}

static void
clean_old_targets (GdaQuery *query, ParseData *pdata)
{
	GSList *list;

	/* cleaning old non parsed targets */
	list = pdata->prev_targets;
	while (list) {
		gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (pdata->prev_targets);
	pdata->prev_targets = NULL;

	/* compute targets hash */
	parse_data_compute_targets_hash (query, pdata);

	/* new list of fields */
	pdata->prev_fields = g_slist_copy (query->priv->fields);
}

static void
clean_old_fields (GdaQuery *query, ParseData *pdata)
{
	GSList *list;

	/* cleaning old non parsed targets */
	list = pdata->prev_fields;
	while (list) {
		if (g_slist_find (query->priv->fields, list->data))
			gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (pdata->prev_fields);
	pdata->prev_fields = NULL;
}

/*
 * main SELECT analysis
 */
gboolean 
parsed_create_select_query (GdaQuery *query, sql_select_statement *select, GError **error)
{
	gboolean has_error = FALSE;
	ParseData *pdata = parse_data_new (query);

	gda_query_set_query_type (query, GDA_QUERY_TYPE_SELECT);
	/* FIXME: UNION, EXCEPT, etc forms of query are not yet handled by
	   the libgdasql library, it should be done there before we can do anything here */

	/*
	 * DISTINCT flag
	 */
	query->priv->global_distinct = select->distinct ? TRUE : FALSE;

	/*
	 * Targets: creating #GdaQueryTarget objects
	 */
	if (select->from) {
		GList *list = select->from;
		GdaQueryTarget *target;
		while (list && !has_error) {
			target = parsed_create_target_sql_table (query, pdata, (sql_table *) list->data, error);
			if (target)
				g_hash_table_insert (pdata->sql_table_targets, list->data, target);
			else 
				has_error = TRUE;
			list = g_list_next (list);
		}
	}
	clean_old_targets (query, pdata);

	/*
	 * Joins: creating #GdaQueryJoin objects
	 */
	while (query->priv->joins_flat)
		gda_object_destroy (GDA_OBJECT (query->priv->joins_flat->data));
	if (!has_error && select->from) {
		GList *list = select->from;
		while (list && !has_error) {
			has_error = ! parsed_create_join_sql_table (query, pdata, (sql_table *) list->data, error);
			list = g_list_next (list);
		}
	}

       	/*
	 * Fields: creating #GdaEntityField objects
	 */
	if (!has_error && select->fields) {
		GList *list = select->fields;
		gint i = 0;

		while (list && !has_error) {
			sql_field *field = (sql_field *) (list->data);
			GdaEntityField *qfield;

			qfield = parsed_create_global_query_field (query, TRUE, pdata, field, FALSE, NULL, NULL, error);
			if (!qfield)
				has_error = TRUE;
			else {
				/* place the field at position 'i' */
				if (g_slist_index (query->priv->fields, qfield) != i) {
					query->priv->fields = g_slist_remove (query->priv->fields, qfield);
					query->priv->fields = g_slist_insert (query->priv->fields, qfield, i);
				}

				/* set that field visible in case it was already there but not visible */
				gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), TRUE);
			}
			list = g_list_next (list);
			i++;
		}
	}
	clean_old_fields (query, pdata);

	/* 
	 * Make sure that for each join condition, all the fields participating in the condition are groupped
	 * together where the first visible field is.
	 */
	gda_query_order_fields_using_join_conds (query);

	/*
	 * WHERE clause: creating a #GdaQueryCondition object
	 */
	if (query->priv->cond)
		gda_object_destroy (GDA_OBJECT (query->priv->cond));
	if (!has_error && select->where) {
		GdaQueryCondition *cond = parsed_create_complex_condition (query, pdata, select->where, TRUE, NULL, error);
		if (!cond)
			has_error = TRUE;
		else {
			gda_query_set_condition (query, cond);
			g_object_unref (G_OBJECT (cond));
		}
	}

	/*
	 * ORDER BY clause
	 */
	if (!has_error && select->order) {
		GList *list = select->order;
		gint i = 0;

		while (list && !has_error) {
			sql_order_field *of = (sql_order_field *) list->data;
			GdaEntityField *qfield;
			gboolean created;
			
			/* try to interpret the order as an int >= 1 */
			gint order = 0;
			if (g_list_length (of->name) == 1) {
				order = atoi (of->name->data);
				if (order > 0) {
					gchar *str = g_strdup_printf ("%d", order);
					if (strcmp (str, of->name->data))
						order = 0;
					g_free (str);
				}
			}

			if (order <= 0) {
				qfield = parsed_create_field_query_field (query, TRUE, pdata, of->name, TRUE, &created, 
									  NULL, error);
				if (qfield) {
					if (created)
						gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), FALSE);
					gda_query_set_order_by_field (query, GDA_QUERY_FIELD (qfield), i, of->order_type == SQL_asc);
				}
				else {
					has_error = TRUE;
					if (error && !*error) 
						g_set_error (error,
							     GDA_QUERY_ERROR,
							     GDA_QUERY_SQL_ANALYSE_ERROR,
							     _("Invalid ORDER BY clause"));
				}
			}
			else {
				qfield = (GdaEntityField *) gda_entity_get_field_by_index (GDA_ENTITY (query), order-1);
				if (qfield) 
					gda_query_set_order_by_field (query, GDA_QUERY_FIELD (qfield), i, 
								      of->order_type == SQL_asc);
				else {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Invalid ORDER BY clause: can't find field number %d"), order);
					has_error = TRUE;
				}
			}
			list = g_list_next (list);
			i++;
		}
	}

	parse_data_destroy (pdata);

#ifdef GDA_DEBUG_NO
	if (has_error) {
		if (error && *error)
			g_print ("Analysed SELECT query (ERROR: %s):\n", (*error)->message);
		else
			g_print ("Analysed SELECT query (ERROR: NO_MSG):\n");
	}
	else
		g_print ("Analysed SELECT query:\n");
	/* if (!gda_referer_is_active (GDA_REFERER (query))) */
		gda_object_dump (GDA_OBJECT (query), 10);
#endif
	return !has_error;
}


/*
 * main UPDATE analysis
 */
gboolean 
parsed_create_update_query (GdaQuery *query, sql_update_statement *update, GError **error)
{
	gboolean has_error = FALSE;
	ParseData *pdata = parse_data_new (query);

	gda_query_set_query_type (query, GDA_QUERY_TYPE_UPDATE);

	/*
	 * Target: creating the #GdaQueryTarget object
	 */
	if (update->table) {
		has_error = parsed_create_target_sql_table (query, pdata, update->table, error) ? FALSE : TRUE;
		
		clean_old_targets (query, pdata);
	}
	else {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Missing UPDATE target entity"));
		has_error = TRUE;
	}

       	/*
	 * Fields: creating #GdaEntityField objects
	 */
	if (!has_error) {
		if (update->set) {
			GList *list = update->set;
			
			while (list && !has_error) {
				GdaQueryCondition *cond;

				cond = parsed_create_simple_condition (query, pdata, (sql_condition *) (list->data),
								       FALSE, NULL, error);
				if (!cond)
					has_error = TRUE;
				else {
					GdaQueryField *field_left, *field_right;

					g_assert (gda_query_condition_get_cond_type (cond) == GDA_QUERY_CONDITION_LEAF_EQUAL);

					field_left = gda_query_condition_leaf_get_operator (cond, GDA_QUERY_CONDITION_OP_LEFT);
					field_right = gda_query_condition_leaf_get_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT);

					if (GDA_IS_QUERY_FIELD_FIELD (field_left)) {
						g_object_set (G_OBJECT (field_left), "value_provider", field_right, NULL);
						gda_query_field_set_visible (field_left, TRUE);
						gda_query_field_set_visible (field_right, FALSE);
					}
					else {
						g_set_error (error,
							     GDA_QUERY_ERROR,
							     GDA_QUERY_SQL_ANALYSE_ERROR,
							     _("UPDATE target field is not an entity's field"));
						has_error = TRUE;
					}
					g_object_unref (G_OBJECT (cond));
				}
				list = g_list_next (list);
			}
		}
		else {
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_SQL_ANALYSE_ERROR,
				     _("Missing target fields to update"));
			has_error = TRUE;	
		}
	}
	clean_old_fields (query, pdata);

	/*
	 * WHERE clause: creating a #GdaQueryCondition object
	 */
	if (query->priv->cond)
		gda_object_destroy (GDA_OBJECT (query->priv->cond));
	if (!has_error && update->where) {
		GdaQueryCondition *cond = parsed_create_complex_condition (query, pdata, update->where, TRUE, NULL, error);
		if (!cond)
			has_error = TRUE;
		else {
			gda_query_set_condition (query, cond);
			g_object_unref (G_OBJECT (cond));
		}
	}

	parse_data_destroy (pdata);

#ifdef GDA_DEBUG_NO
	if (has_error) {
		if (error && *error)
			g_print ("Analysed UPDATE query (ERROR: %s):\n", (*error)->message);
		else
			g_print ("Analysed UPDATE query (ERROR: NO_MSG):\n");
	}
	else
	g_print ("Analysed UPDATE query:\n");
	gda_object_dump (GDA_OBJECT (query), 10);
#endif
	return !has_error;
}


/*
 * main INSERT analysis
 */
gboolean
parsed_create_insert_query (GdaQuery *query, sql_insert_statement *insert, GError **error)
{
	gboolean has_error = FALSE;
	GSList *fields = NULL;
	GdaQueryTarget *target = NULL;
	ParseData *pdata = parse_data_new (query);

	gda_query_set_query_type (query, GDA_QUERY_TYPE_INSERT);

	/*
	 * Target: creating the #GdaQueryTarget object
	 */
	if (insert->table) {
		has_error = parsed_create_target_sql_table (query, pdata, insert->table, error) ? FALSE : TRUE;
		if (!has_error)
			target = query->priv->targets->data;

		clean_old_targets (query, pdata);
	}
	else {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Missing INSERT target entity"));
		has_error = TRUE;
	}

       	/*
	 * Fields: creating visible #GdaEntityField objects
	 */
	if (!has_error && insert->fields) {
		GList *list = insert->fields;
		
		while (list && !has_error) {
			sql_field *field = (sql_field *) (list->data);
			GdaEntityField *qfield;
			
			qfield = parsed_create_global_query_field (query, TRUE, pdata, field, FALSE, NULL, NULL, error);
			if (!qfield)
				has_error = TRUE;
			else {
				if (GDA_IS_QUERY_FIELD_FIELD (qfield)) {
					gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), TRUE);
					fields = g_slist_append (fields, qfield);
				}
				else {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("INSERT field does not have a valid syntax"));
					has_error = TRUE;
				}
			}
			
			list = g_list_next (list);
		}
	}
	clean_old_fields (query, pdata);

	/*
	 * Values: creating hidden #GdaEntityField objects
	 */
	if (!has_error && insert->values) {
		GList *list;
		GSList *entity_fields = NULL;
		gint pos;

		if (fields) {
			if (g_slist_length (fields) < g_list_length (insert->values)) {
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_SQL_ANALYSE_ERROR,
					     _("INSERT has more expression values than insert fields"));
				has_error = TRUE;
			}
			if (g_slist_length (fields) > g_list_length (insert->values)) {
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_SQL_ANALYSE_ERROR,
					     _("INSERT has more insert fields than expression values"));
				has_error = TRUE;
			}
		}
		else {
			GdaEntity *tmpent;

			tmpent = gda_query_target_get_represented_entity (target);
			if (tmpent) {
				entity_fields = gda_entity_get_fields (tmpent);
				if (g_slist_length (entity_fields) < g_list_length (insert->values)) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("INSERT has more expression values than insert fields"));
					has_error = TRUE;
				}
			}
		}
		
		list = insert->values;
		pos = 0;
		while (list && !has_error) {
			sql_field *field = (sql_field *) (list->data);
			GdaEntityField *qfield;
			
			qfield = parsed_create_global_query_field (query, TRUE, pdata, field, FALSE, NULL, NULL, error);
			if (!qfield)
				has_error = TRUE;
			else {
				if (!GDA_IS_QUERY_FIELD_FIELD (qfield)) {
					gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), FALSE);
					if (fields)
						g_object_set (G_OBJECT (g_slist_nth_data (fields, pos)), "value_provider",
							      qfield, NULL);
					else {
						GdaEntityField *field;

						field = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
								 "dict", gda_object_get_dict (GDA_OBJECT (query)), 
								 "query", query, NULL);
						if (entity_fields)
							g_object_set (G_OBJECT (field), "target", target, 
								      "field", g_slist_nth_data (entity_fields, pos), NULL);
						else {
							gchar *fname;
							
							fname = g_strdup_printf ("unnamed_field_%d", pos);
							g_object_set (G_OBJECT (field), "target", target, 
								      /*"field_name", fname,*/ NULL);
							g_free (fname);
							g_warning ("Dictionary is recommended for this INSERT query as "
								   "the fields to insert into have not been named");
						}

						gda_query_field_set_visible (GDA_QUERY_FIELD (field), TRUE);
						gda_entity_add_field (GDA_ENTITY (query), field);
						g_object_set (G_OBJECT (field), "value_provider", qfield, NULL);
						g_object_unref (G_OBJECT (field));
					}
				}
				else {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("INSERT expression is a target field"));
					has_error = TRUE;
				}
			}
			
			list = g_list_next (list);
			pos ++;
		}

		if (entity_fields)
			g_slist_free (entity_fields);
	}

	parse_data_destroy (pdata);

#ifdef GDA_DEBUG_NO
	if (has_error) {
		if (error && *error)
			g_print ("Analysed INSERT query (ERROR: %s):\n", (*error)->message);
		else
			g_print ("Analysed INSERT query (ERROR: NO_MSG):\n");
	}
	else
	g_print ("Analysed INSERT query:\n");
	gda_object_dump (GDA_OBJECT (query), 10);
#endif
	return !has_error;	
}

/*
 * main DELETE analysis
 */
gboolean
parsed_create_delete_query (GdaQuery *query, sql_delete_statement *del, GError **error)
{
	gboolean has_error = FALSE;
	ParseData *pdata = parse_data_new (query);

	gda_query_set_query_type (query, GDA_QUERY_TYPE_DELETE);

	/*
	 * Target: creating the #GdaQueryTarget object
	 */
	if (del->table) {
		has_error = parsed_create_target_sql_table (query, pdata, del->table, error) ? FALSE : TRUE;
		clean_old_targets (query, pdata);
	}
	else {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Missing DELETE target entity"));
		has_error = TRUE;
	}
	clean_old_fields (query, pdata);


	/*
	 * WHERE clause: creating a #GdaQueryCondition object
	 */
	if (query->priv->cond)
		gda_object_destroy (GDA_OBJECT (query->priv->cond));
	if (!has_error && del->where) {
		GdaQueryCondition *cond = parsed_create_complex_condition (query, pdata, del->where, TRUE, NULL, error);
		if (!cond)
			has_error = TRUE;
		else {
			gda_query_set_condition (query, cond);
			g_object_unref (G_OBJECT (cond));
		}
	}

	parse_data_destroy (pdata);

#ifdef GDA_DEBUG_NO
	if (has_error) {
		if (error && *error)
			g_print ("Analysed DELETE query (ERROR: %s):\n", (*error)->message);
		else
			g_print ("Analysed DELETE query (ERROR: NO_MSG):\n");
	}
	else
	g_print ("Analysed DELETE query:\n");
	gda_object_dump (GDA_OBJECT (query), 10);
#endif
	return !has_error;

}


/*
 * parsed_create_target_sql_table
 * @query: the query to work on
 * @pdata:
 * @table: the sql_table to analyse
 * @error: place to store the error, or %NULL
 *
 * Analyse a sql_table struct and makes a #GdaQueryTarget for it, or use an already existing one.
 *
 * Returns: a new GdaQueryTarget corresponding to the 'right' part of the join if we have a join.
 */
static GdaQueryTarget *
parsed_create_target_sql_table (GdaQuery *query, ParseData *pdata, sql_table *table, GError **error)
{
	GdaQueryTarget *target = NULL;

	/* GdaQueryTarget creation */
	switch (table->type) {
	case SQL_simple:
		target = parsed_create_target_db_table (query, pdata, table->d.simple, table->as, error);
		break;
	case SQL_nestedselect:
		target = parsed_create_target_sub_select (query, pdata, table->d.select, table->as, error);
		break;
	case SQL_tablefunction:
		TO_IMPLEMENT;
		break;
	default:
		g_assert_not_reached ();
	}
	
	if (target)
		pdata->parsed_targets = g_slist_prepend (pdata->parsed_targets, target);
	
	return target;
}

/* 
 * parsed_create_join_sql_table
 * @query: the query to work on
 * @pdata:
 * @table: the sql_table to analyse
 * @error: place to store the error, or %NULL
 *
 * Analyse a sql_table struct and makes the #GdaQueryJoin object for it
 *
 * Returns: a new GdaQueryTarget corresponding to the 'right' part of the join if we have a join.
 */
static gboolean
parsed_create_join_sql_table (GdaQuery *query, ParseData *pdata,
			      sql_table *table, GError **error)
{
	gboolean has_error = FALSE;
	GdaQueryTarget *target = g_hash_table_lookup (pdata->sql_table_targets, table);
	
	if (!target) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Internal error: can't find target"));
		has_error = TRUE;
	}

	if (!has_error && (table->join_type != SQL_cross_join)) {
		if (table->join_cond) { /* we have a join with a condition */
			GdaQueryCondition *cond = NULL;
			GSList *targets = NULL;
			
			cond = parsed_create_complex_condition (query, pdata,
								table->join_cond, FALSE, &targets, error);
			if (cond) {
				/* test nb of targets */
				if (g_slist_length (targets) != 2) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Join condition must be between two entities"));
					has_error = TRUE;
				}
				else {
					GdaQueryJoin *join;
					GdaQueryTarget *left_target = NULL;
					left_target = (GdaQueryTarget *) targets->data;
					if (left_target == target)
						left_target = (GdaQueryTarget *) g_slist_next (targets)->data;
					
					join = GDA_QUERY_JOIN (gda_query_join_new_with_targets (query, left_target, target));
					has_error = !gda_query_add_join (query, join);
					gda_query_join_set_join_type (join, parse_join_converter [table->join_type]);
					g_object_unref (G_OBJECT (join));
					
					if (!has_error && cond)
						has_error = !gda_query_join_set_condition (join, cond);
					
				}
				
				if (targets)
					g_slist_free (targets);
				g_object_unref (G_OBJECT (cond));
			}
		}
		else { /* we have a join without any condition; try to find the correct other target */
			GSList *targets = g_slist_find (pdata->parsed_targets, target);
			GdaQueryJoin *join;
			GdaQueryTarget *left_target = NULL;
			GdaEntity *ent = gda_query_target_get_represented_entity (target);

			g_assert (targets);
			targets = targets->next;
			if (ent) {
				gboolean stop_search = FALSE;
				while (targets && !stop_search) {
					GdaEntity *left_ent;
					GSList *list = NULL;
					left_ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (targets->data));
					if (left_ent) {
						list = gda_dict_get_entities_fk_constraints (gda_object_get_dict ((GdaObject*) query),
											     ent, left_ent, FALSE);
						if (list) {
							if (g_slist_length (list) != 1) {
								g_set_error (error,
									     GDA_QUERY_ERROR,
									     GDA_QUERY_SQL_ANALYSE_ERROR,
									     _("Ambiguous join: multiple possible foreign key constraints"));
								has_error = TRUE;
							}
							else {
								if (left_target) {
									g_set_error (error,
										     GDA_QUERY_ERROR,
										     GDA_QUERY_SQL_ANALYSE_ERROR,
										     _("Ambiguous join: multiple possible targets to join with"));
									has_error = TRUE;
								}
								else
									left_target = GDA_QUERY_TARGET (targets->data);
							}
							g_slist_free (list);
						}
					}
					else {
						left_target = GDA_QUERY_TARGET (targets->data);
						stop_search = TRUE;
					}
					targets = g_slist_next (targets);
				}
			}
			else {
				if (targets)
					left_target = GDA_QUERY_TARGET (targets->data);
				else {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Ambiguous join: no target to join with"));
					has_error = TRUE;
				}
			}
			
			if (left_target && !has_error) {
				join = GDA_QUERY_JOIN (gda_query_join_new_with_targets (query, left_target, target));
				has_error = !gda_query_add_join (query, join);
				gda_query_join_set_join_type (join, parse_join_converter [table->join_type]);
				g_object_unref (G_OBJECT (join));
			}
		}
	}

	return ! has_error;
}

/*
 * Create a new target within query, from a sub query
 * @query: the query to add the target to
 * @pdata:
 * @select: corresponding SELECT sub query for the target
 * @as: corresponding alias for the target
 * @error: place to store the error, or %NULL
 *
 * Creates a new target from the @select and @as entries. 
 * However, if a target was already present in @prev_targets_hash,
 * then no new target is created and the existing target is returned, 
 * after having been removed from @prev_targets_hash
 */
static GdaQueryTarget *
parsed_create_target_sub_select (GdaQuery *query, ParseData *pdata, sql_select_statement *select, const gchar *as, GError **error)
{
	GdaQueryTarget *target = NULL;
	GdaQuery *subq;
	gboolean err = FALSE;
	gchar *str;

	/* if there is a suitable target in @prev_targets_hash, then don't create a new one */
	if (as && *as) {
		GSList *list = pdata->prev_targets;
		while (list && !target) {
			GdaQueryTarget *tmp = GDA_QUERY_TARGET (list->data);
			const gchar *tmpstr = gda_query_target_get_alias (GDA_QUERY_TARGET (tmp));
			
			if (!strcmp (tmpstr, as))
				target = GDA_QUERY_TARGET (list->data);
			if (!target) {
				str = g_utf8_strdown (as, -1);
				if (!strcmp (tmpstr, str))
					target = GDA_QUERY_TARGET (list->data);
				g_free (str);
			}
		}
		
		list = g_slist_next (list);
	}
	
	if (target) {
		GdaEntity *ent = gda_query_target_get_represented_entity (target);
		
		if (!GDA_IS_QUERY (ent))
			target = NULL; /* the target found MUST reference a sub query, otherwise we don't want it */
	}
	
	if (!target) {
		/* create new sub query and target */
		subq = GDA_QUERY (gda_query_new (gda_object_get_dict (GDA_OBJECT (query))));
		err = !parsed_create_select_query (subq, select, error);
		if (!err) {
			gda_query_add_sub_query (query, subq);
			target = g_object_new (GDA_TYPE_QUERY_TARGET, 
					       "dict", gda_object_get_dict (GDA_OBJECT (query)), 
					       "query", query, "entity", subq, NULL);
			if (as && *as) {
				gda_object_set_name (GDA_OBJECT (subq), as);
				gda_query_target_set_alias (target, as);
			}
			if (! gda_query_add_target (query, target, error)) {
				g_object_unref (target);
				target = NULL;
			}
			g_object_unref (target);
		}
		g_object_unref (subq);
	}
	else {
		/* only parse the existing target's sub query */
		subq = GDA_QUERY (gda_query_target_get_represented_entity (target));
		err = !parsed_create_select_query (subq, select, error);
		if (!err) 
			pdata->prev_targets = g_slist_remove (pdata->prev_targets, target);			
		else
			target = NULL;
	}

	return target;
}
			       


/*
 * Create a new target within query, from a table name
 * @query: the query to add the target to
 * @pdata:
 * @table: corresponding table name for the target
 * @as: corresponding alias for the target
 * @error: place to store the error, or %NULL
 *
 * Creates a new target from the @name and @as entries. 
 * However, if a target was already present in @prev_targets_hash,
 * then no new target is created and the existing target is returned, 
 * after having been removed from @prev_targets_hash
 */
static GdaQueryTarget *
parsed_create_target_db_table (GdaQuery *query, ParseData *pdata,
			       const gchar *table, const gchar *as, GError **error)
{
	GdaQueryTarget *target = NULL;
	GdaDictTable *db_table;
	GdaDictDatabase *db = gda_dict_get_database (gda_object_get_dict (GDA_OBJECT (query)));
	gboolean has_error = FALSE;
	gchar *str;
	GSList *list;

	/* referenced entity: first try to find the table corresponding to @table */
	db_table = gda_dict_database_get_table_by_name (db, table); /* REM: db_table may be set to NULL */

	/* if there is a suitable target in pdata->prev_targets, then don't create a new one */
	list = pdata->prev_targets;
	while (list) {
		GdaQueryTarget *tmp = NULL;
		
		if ((db_table &&
		     gda_query_target_get_represented_entity (GDA_QUERY_TARGET (list->data)) == 
		     (GdaEntity *) db_table) 
		    ||
		    (!db_table && 
		     !strcmp (gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (list->data)),
			      table))) {
			tmp = GDA_QUERY_TARGET (list->data);
			if (!target)
				target = tmp;
		}
		
		if (as && *as && tmp) {
			/* look for target alias */
			const gchar *tmpstr = gda_query_target_get_alias (GDA_QUERY_TARGET (tmp));
			
			if (!strcmp (tmpstr, as))
				target = GDA_QUERY_TARGET (list->data);
			if (!target) {
				str = g_utf8_strdown (as, -1);
				if (!strcmp (tmpstr, str))
					target = GDA_QUERY_TARGET (list->data);
				g_free (str);
			}
		}
		
		list = g_slist_next (list);
	}
	
	if (target) {
		if (as && *as)
			gda_query_target_set_alias (target, as);
		pdata->prev_targets = g_slist_remove (pdata->prev_targets, target);
		return target;
	}
	
	if (db_table) {
		target = g_object_new (GDA_TYPE_QUERY_TARGET, 
				       "dict", gda_object_get_dict (GDA_OBJECT (query)), 
				       "query", query, "entity", db_table, NULL);

		gda_object_set_name (GDA_OBJECT (target), gda_object_get_name (GDA_OBJECT (db_table)));
	}
	else {
		target = g_object_new (GDA_TYPE_QUERY_TARGET, 
				       "dict", gda_object_get_dict (GDA_OBJECT (query)), 
				       "query", query, "entity_name", table, NULL);

		gda_object_set_name (GDA_OBJECT (target), table);
	}

	has_error = ! gda_query_add_target (query, target, error);
	if (as && *as)
		gda_query_target_set_alias (target, as);
	g_object_unref (G_OBJECT (target));

	if (has_error && target)
		target = NULL;

	return target;
}

/*
 * Create a new GdaQueryField object from the sql_field_item structure, and put it into @query.
 * Returns NULL if the creation failed.
 * @query: the query to add the field to
 * @pdata:
 * @field: sql_field_item to be analysed
 * @new_field: a place to store if a new field has been created, or one already existing has
 *             been returned
 * @target_return: place to store the target which has been used, or %NULL (on error, it is unchanged)
 * @error: place to store the error, or %NULL
 *
 * Returns: NULL if value has not been interpreted as a valid field name
 */
GdaEntityField *
parsed_create_global_query_field (GdaQuery *query, gboolean add_to_query, 
				  ParseData *pdata, sql_field *field, 
				  gboolean try_existing_field, gboolean *new_field,
				  GdaQueryTarget **target_return, GError **error)
{
	sql_field_item *field_item = (sql_field_item *) (field->item);
	GdaEntityField *qfield = NULL;
	GError *my_error = NULL;
	GError **error_ptr;

	if (error)
		error_ptr = error;
	else
		error_ptr = &my_error;

	switch (field_item->type) {
	case SQL_name:
		/* first try a value QueryField */
		if (g_list_length (field_item->d.name) == 1)
			qfield = parsed_create_value_query_field (query, add_to_query, pdata,
								  (gchar*) field_item->d.name->data, 
								  field->param_spec, new_field, error_ptr);
		if (!qfield && !(*error_ptr))
			/* then try an entity.field QueryField */
			qfield = parsed_create_field_query_field (query, add_to_query, pdata,
								  field_item->d.name, try_existing_field, new_field, 
								  target_return, error_ptr);
		if (qfield && field->as && *(field->as) && !GDA_IS_QUERY_FIELD_ALL (qfield)) {
			gda_query_field_set_alias (GDA_QUERY_FIELD (qfield), field->as);
			gda_object_set_name (GDA_OBJECT (qfield), field->as);
		}
			
		break;
	case SQL_equation:
		/* Example: SELECT ..., a+b, ... FROM table */
		/* not yet doable, need the creation of a GnomeDbQfEquation object which inherits GdaQueryField */
		TO_IMPLEMENT;
		break;
	case SQL_inlineselect:
		/* Example: SELECT ..., (SELECT a FROM table WHERE...), ... FROM table */
		/* not yet doable, need the creation of a GnomeDbQfSelect object which inherits GdaQueryField */
		TO_IMPLEMENT;
		break;
	case SQL_function:
		qfield = parsed_create_func_query_field (query, add_to_query, pdata,
							 field_item->d.function.funcname,
							 field_item->d.function.funcarglist, 
							 try_existing_field, new_field, error_ptr);
		break;
	}

	if (*error_ptr && !error) 
		g_error_free (*error_ptr);

	return qfield;
}


/*
 * Create a GdaQueryFieldField object for the provided name.
 * @query: the query to add the field to
 * @pdata:
 * @field_names: a list of string for each dot separated field: table.field => 2 strings 
 * @new_field: place to store if a new field has been created, or if the returned value is an existing field
 * @target_return: place to store the target which has been used, or %NULL (on error, it is unchanged)
 * @error: place to store the error, or %NULL
 *
 * Returns: NULL if value has not been interpreted as a valid field name
 */
static GdaEntityField *
parsed_create_field_query_field (GdaQuery *query, gboolean add_to_query, 
				 ParseData *pdata, GList *field_names,
				 gboolean try_existing_field, gboolean *new_field, 
				 GdaQueryTarget **target_return, GError **error)
{
	GdaEntityField *qfield = NULL;
	GList *list;
	gint i;
	gboolean has_error = FALSE;
	GString *sql_name;

	/* build complete SQL name */
	sql_name = g_string_new ("");
	list = field_names;
	while (list) {
		gchar *str = (gchar *) list->data;
		
		if (list != field_names)
			g_string_append (sql_name, ".");
		g_string_append (sql_name, str);

		while (*str && !has_error) {
			if (*str == '\'') {
				has_error = TRUE;
				g_set_error (error,
					     GDA_QUERY_ERROR,
					     GDA_QUERY_SYNTAX_ERROR,
					     _("Invalid field name: %s"), (gchar *) list->data);
			}
			str++;
		}
			
		list = g_list_next (list);
	}

	if (has_error) {
		g_string_free (sql_name, TRUE);
		return NULL;
	}

	/* try to find an existing field 
	 * if try_existing_field is TRUE, try it, beware with the value_provider which need updating
	 * which might be a problem.
	 */
	if (try_existing_field && * (sql_name->str)) {
		if (pdata->prev_fields)
			qfield = (GdaEntityField *) gda_query_get_field_by_sql_naming_fields (query, sql_name->str, 
											      pdata->prev_fields);
		else
			qfield = (GdaEntityField *) gda_query_get_field_by_sql_naming (query, sql_name->str);
		if (qfield) {
			g_string_free (sql_name, TRUE);
			if (new_field)
				*new_field = FALSE;
			pdata->prev_fields = g_slist_remove (pdata->prev_fields, qfield);
			return qfield;
		}
	}

	/* no pre-existing field found */
	list = field_names;
	i = g_list_length (list);
	if ((i>2) || (i==0)) {
		if (i==0) 
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_SYNTAX_ERROR,
				     _("Invalid empty field name"));
		else 
			g_set_error (error,
				     GDA_QUERY_ERROR,
				     GDA_QUERY_SYNTAX_ERROR,
				     _("Invalid field name '%s'"), sql_name->str);
		
		has_error = TRUE;
	}
	else {
		GdaQueryTarget *target = NULL;
		GdaEntityField *field = NULL;
		
		gchar *table_alias = NULL;
		gchar *field_name = NULL;
		
		if (i==2) { /* Like: "table.field" or "table_alias.field" */
			table_alias = g_utf8_strdown ((gchar *) (list->data), -1);
			field_name = (gchar *) (g_list_next (list)->data);
			target = g_hash_table_lookup (pdata->new_targets, table_alias); /* can be NULL */

			if (*field_name == '*') {
				if (!target) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Non-declared target '%s'"), table_alias);
					has_error = TRUE;
				}
				else {
					qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_ALL, 
									    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
									    "query", query, NULL);
					g_object_set (G_OBJECT (qfield), "target", target, NULL);

					if (target_return)
						*target_return = target;
				}
			}
			else {
				if (!strcmp (field_name, "null")) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Invalid field '%s.%s'"), table_alias, field_name);
					has_error = TRUE;
				}
				else {
					if (target) {
						GdaEntity *ent;

						ent = gda_query_target_get_represented_entity (target);
						if (ent)
							field = gda_entity_get_field_by_name (ent, field_name);
						
						if (field) {
							qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
									     "dict", gda_object_get_dict (GDA_OBJECT (query)), 
									      "query", query, NULL);
							g_object_set (G_OBJECT (qfield), "target", target, 
								      "field", field, NULL);
							if (target_return)
								*target_return = target;
						}
						else {
							qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
									   "dict", gda_object_get_dict (GDA_OBJECT (query)), 
									   "query", query, NULL);
							if (target) {
								g_object_set (G_OBJECT (qfield), "target", target, 
									      "field_name", field_name, NULL);
								if (target_return)
									*target_return = target;
							}
							else
								g_object_set (G_OBJECT (qfield), "target_name", table_alias, 
									      "field_name", field_name, NULL);
						}
					}
					else {
						g_set_error (error,
							     GDA_QUERY_ERROR,
							     GDA_QUERY_SQL_ANALYSE_ERROR,
							     _("Non declared target '%s'"), table_alias);
						has_error = TRUE;
					}
				}
			}
			g_free (table_alias);
		}
		else { /* Like: "field" */
			GSList *targets = query->priv->targets;
			field_name = (gchar *) (list->data);
			if (*field_name != '*') {
				if (!strcmp (field_name, "null")) {
					/* we don't know the exact data type of the value! */
					qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_VALUE, 
										  "dict", gda_object_get_dict (GDA_OBJECT (query)), 
										  "query", query,	  
										  "gda_type", GDA_VALUE_TYPE_NULL, NULL);
				}
				else {
					while (targets && !has_error) {
						GdaEntityField *tmp = NULL;
						GdaEntity *ent;
						
						ent = gda_query_target_get_represented_entity 
							(GDA_QUERY_TARGET (targets->data));
						if (ent)
							tmp = gda_entity_get_field_by_name (ent, field_name);
						if (tmp) {
							if (field) {
								g_set_error (error,
									     GDA_QUERY_ERROR,
									     GDA_QUERY_SQL_ANALYSE_ERROR,
									     _("Ambiguous field '%s'"), field_name);
								has_error = TRUE;
							}
							else {
								target = (GdaQueryTarget *) targets->data;
								field = tmp;
							}
						}
						targets = g_slist_next (targets);
					}
					if (!has_error) {
						if (field) {
							qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
								 "dict", gda_object_get_dict (GDA_OBJECT (query)), 
								 "query", query, NULL);
							g_object_set (G_OBJECT (qfield), "target", target, 
								      "field", field, NULL);
							if (target_return)
								*target_return = target;
						}
						else {
							qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, 
								 "dict", gda_object_get_dict (GDA_OBJECT (query)), 
								 "query", query, NULL);
							g_object_set (G_OBJECT (qfield), "field_name", field_name, NULL);
						}
					}
				}
			}
			else {
				/* field_name == '*' */
				if (g_slist_length (query->priv->targets) != 1) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Ambiguous field '*'"));
					has_error = TRUE;	
				}
				else {
					target = GDA_QUERY_TARGET (query->priv->targets->data);
					qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_ALL, 
									    "dict", gda_object_get_dict (GDA_OBJECT (query)), 
									    "query", query, NULL);
					g_object_set (G_OBJECT (qfield), "target", target, NULL);

					if (target_return)
						*target_return = target;
				}
			}
		}
		
		if (qfield) {
			/* actual field association to the query */
			gda_object_set_name (GDA_OBJECT (qfield), field_name);
			
			if (add_to_query) {
				gda_entity_add_field (GDA_ENTITY (query), qfield);
				g_object_unref (G_OBJECT (qfield));
			}
		}
	}

	g_string_free (sql_name, TRUE);
	if (new_field && !has_error)
		*new_field = TRUE;

	if (has_error)
		g_assert (!qfield);

	return qfield;
}

/*
 * Create a GdaQueryFieldValue object for the provided value.
 * @query: the query to add the field to
 * @pdata:
 * @value: string SQL representation of the value
 * @error: place to store the error, or %NULL
 *
 * Returns: NULL if value has not been interpreted as a valid value
 */
static GdaEntityField *
parsed_create_value_query_field (GdaQuery *query, gboolean add_to_query, ParseData *pdata,
				 const gchar *value, GList *param_specs, gboolean *new_field, GError **error)
{
	GdaDict *dict;
	GdaConnection *cnc;
	GdaServerProvider *prov = NULL;

	GdaEntityField *qfield = NULL;
	GList *list;
	GdaDataHandler *dh;
	GdaValue *gdaval = NULL;
	GdaDictType *real_type = NULL;
	GdaValueType gdatype = GDA_VALUE_TYPE_UNKNOWN;
	gboolean unspecvalue = FALSE;
	gboolean isparam;

	dict = gda_object_get_dict (GDA_OBJECT (query));
	cnc = gda_dict_get_connection (dict);
	if (cnc)
		prov = gda_connection_get_provider_obj (cnc);

	if (!value || *value == 0)
		/* the value is not specified, it needs to be a parameter and we need a type specification */
		unspecvalue = TRUE; 

	/*
	 * try to determine a suitable GdaDictType or at least GdaValyeType for the value 
	 */
	list = param_specs;
	while (list && !real_type) {
		if (((param_spec*) list->data)->type == PARAM_type) {
			/* try to find a DBMS data type */
			real_type = gda_dict_get_data_type_by_name (dict, 
								    ((param_spec*) list->data)->content);
			if (!real_type) {
				/* try a generic GDA type then */
				gdatype = gda_type_from_string (((param_spec*) list->data)->content);
				if (gdatype == GDA_VALUE_TYPE_UNKNOWN) {
					g_set_error (error,
						     GDA_QUERY_ERROR,
						     GDA_QUERY_SQL_ANALYSE_ERROR,
						     _("Unknown data type '%s'"), 
						     ((param_spec*) list->data)->content);
					return NULL;
				}
			}
		}
		
		list = g_list_next (list);
	}

	if (!real_type && (gdatype == GDA_VALUE_TYPE_UNKNOWN) && unspecvalue) {
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Data type must be specified when no default value is provided"));
		return NULL;
	}

	if (!real_type && (gdatype == GDA_VALUE_TYPE_UNKNOWN)) {
		if (prov) {
			gchar *type = NULL;
			gdaval = gda_server_provider_string_to_value (prov, cnc, value, 
								      GDA_VALUE_TYPE_UNKNOWN, &type);
			if (gdaval)
				real_type = gda_dict_get_data_type_by_name (dict, type);
			else
				/* Don't set error here because not finding any real type can mean that we 
				   don't have a value in the first place */
				return NULL;
		}
		else {
			/* no provider, try default data handlers to find a suitable type */
			gint i;
			GdaDataHandler *dh;
			GdaValueType test_types[] = {
				GDA_VALUE_TYPE_SMALLINT, 
				GDA_VALUE_TYPE_INTEGER,
				GDA_VALUE_TYPE_BIGINT,
				GDA_VALUE_TYPE_BIGUINT,
				GDA_VALUE_TYPE_SINGLE,
				GDA_VALUE_TYPE_DOUBLE,
				GDA_VALUE_TYPE_BOOLEAN,
				GDA_VALUE_TYPE_TIME,
				GDA_VALUE_TYPE_DATE,
				GDA_VALUE_TYPE_TIMESTAMP,
				GDA_VALUE_TYPE_GEOMETRIC_POINT,
				GDA_VALUE_TYPE_STRING,
				GDA_VALUE_TYPE_NUMERIC,
				GDA_VALUE_TYPE_BINARY
			};

			i = 0;
			while ((gdatype == GDA_VALUE_TYPE_UNKNOWN) && 
			       (i < sizeof (test_types) / sizeof (GdaValueType))) {
				gdaval = NULL;
				dh = gda_dict_get_default_handler (dict, test_types[i]);
				if (dh)
					gdaval = gda_data_handler_get_value_from_sql (dh, value, test_types[i]);
				if (gdaval) {
                                        gchar *sql = gda_data_handler_get_sql_from_value (dh, gdaval);
                                        if (sql) {
                                                if (!strcmp (value, sql))
							gdatype = test_types[i];
                                                /*g_print ("TEST : %s => %s => %s\n", value, sql, !strcmp (value, sql) ? "OK": "NOK");*/
                                                g_free (sql);
                                        }
					if (gdatype == GDA_VALUE_TYPE_UNKNOWN) {
						gda_value_free (gdaval);
						gdaval = NULL;
					}
                                }
				i++;
			}

			if (gdatype == GDA_VALUE_TYPE_UNKNOWN)
				return NULL;
		}
	}

	if (real_type)
		gdatype = gda_dict_type_get_gda_type (real_type);
	g_assert (gdatype != GDA_VALUE_TYPE_UNKNOWN);

	/*
	 * Actual GdaEntityField creation
	 */
	qfield = (GdaEntityField *) gda_query_field_value_new (query, gdatype);
	if (real_type)
		gda_query_field_value_set_dict_type (GDA_QUERY_FIELD_VALUE (qfield), real_type);
	if (unspecvalue) 
		/* value MUST BE a parameter */
		gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfield), TRUE);
	else {
		if (!gdaval) {
			if (prov)
				dh = gda_server_provider_get_data_handler_gda (prov, cnc, gdatype);
			else
				dh = gda_dict_get_default_handler (dict, gdatype);
			gdaval = gda_data_handler_get_value_from_sql (dh, value, gdatype);
			g_assert (gdaval);
		}

		gda_query_field_value_set_value (GDA_QUERY_FIELD_VALUE (qfield), gdaval);
		gda_value_free (gdaval);
		gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfield), FALSE);
	}

	if (add_to_query) {
		gda_entity_add_field (GDA_ENTITY (query), qfield);
		g_object_unref (G_OBJECT (qfield));
	}
	
	/*
	 * Using the param spec to set attributes
	 */
	isparam = g_list_length (param_specs) > 0 ? TRUE : FALSE;
	list = param_specs;
	while (list) {
		param_spec *pspec = (param_spec*) list->data;

		switch (pspec->type) {
		case PARAM_name:
			gda_object_set_name (GDA_OBJECT (qfield), pspec->content);
			break;
		case PARAM_descr:
			gda_object_set_description (GDA_OBJECT (qfield), pspec->content);
			break;
		case PARAM_isparam:
			isparam = ! ((* pspec->content == 'f') || * pspec->content == 'F');
			break;
		case PARAM_nullok:
			gda_query_field_value_set_not_null (GDA_QUERY_FIELD_VALUE (qfield), 
						  ! ((* pspec->content == 't') || (* pspec->content == 'T')));
			break;
		default:
			break;
		}		
		list = g_list_next (list);
	}
	if (! gda_query_field_value_is_parameter (GDA_QUERY_FIELD_VALUE (qfield)))
		gda_query_field_value_set_is_parameter (GDA_QUERY_FIELD_VALUE (qfield), isparam);

	if (new_field)
		*new_field = TRUE;
	return qfield;
}


/*
 * Create a GdaQueryFieldFunc or GdaQueryFieldAgg object correspponding to @funcname
 * @query: the query to add the field to
 * @pdata:
 * @funcname: the name of the function or the aggregate
 * @funcargs: arguments of the function or aggregate
 * @error: place to store the error, or %NULL
 *
 * Returns: NULL if value has not been interpreted as a valid value
 */
static GdaEntityField *
parsed_create_func_query_field (GdaQuery *query, gboolean add_to_query, ParseData *pdata,
				const gchar *funcname, GList *funcargs, 
				gboolean try_existing_field, gboolean *new_field, GError **error)
{
	GdaEntityField *qfield = NULL;
	GSList *args = NULL, *arg_types = NULL;
	GList *dlist;
	GdaDictFunction *function = NULL;
	GdaDictAggregate *agg = NULL;
 	gboolean has_error = FALSE;
	gboolean created = TRUE; /* FIXME: compute that value */
	GdaDict *dict;
	gboolean missing_arg_type = FALSE;

	dict = gda_object_get_dict (GDA_OBJECT (query));

	/* Make alist of all the arguments */
	dlist = funcargs;
	while (dlist) {
		gboolean new_field;

		GdaEntityField *qfield = parsed_create_global_query_field (query, TRUE, pdata, 
									   (sql_field *)dlist->data, 
									   try_existing_field, &new_field, 
									   NULL, error);

		if (!qfield) 
			return NULL;
		else {
			GdaDictType *dtype;
			args = g_slist_append (args, qfield);
			dtype = gda_query_field_get_dict_type (GDA_QUERY_FIELD (qfield));
			arg_types = g_slist_append (arg_types, dtype);
			if (!dtype)
				missing_arg_type = TRUE;

			if (new_field)
				gda_query_field_set_visible (GDA_QUERY_FIELD (qfield), FALSE);
		}
		dlist = g_list_next (dlist);
	}

	/* if all the arguments can be found as GdaDictType objects, 
	 * then try to find the right GdaDictFunction or GdaDictAggregate object */
	if (!missing_arg_type) {
		function = gda_dict_get_function_by_name_arg (dict, funcname, arg_types);
	
		/* try to find the right GdaDictAggregate object */
		if (!function && (g_slist_length (arg_types) == 1)) 
			agg = gda_dict_get_aggregate_by_name_arg (dict, funcname, arg_types->data);
	}
	

	if (agg) {
		GdaQueryField *arg = NULL;
		if (args && (g_slist_length (args) == 1))
			arg = GDA_QUERY_FIELD (args->data);
		
		qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_AGG, 
							  "dict", gda_object_get_dict (GDA_OBJECT (query)), 
							  "query", query, NULL);
		g_object_set (G_OBJECT (qfield), "aggregate", agg, NULL);
		if (arg)
			has_error = ! gda_query_field_agg_set_arg (GDA_QUERY_FIELD_AGG (qfield), arg);
		else
			has_error = TRUE;
	}
	else {
		qfield = (GdaEntityField *) g_object_new (GDA_TYPE_QUERY_FIELD_FUNC, 
							  "dict", gda_object_get_dict (GDA_OBJECT (query)), 
							  "query", query, NULL);
		if (function) 
			g_object_set (G_OBJECT (qfield), "function", function, NULL);
		else 
			g_object_set (G_OBJECT (qfield), "function_name", funcname, NULL);

		has_error = ! gda_query_field_func_set_args (GDA_QUERY_FIELD_FUNC (qfield), args);
	}


	gda_object_set_name (GDA_OBJECT (qfield), funcname);
	
	if (has_error) {
		if (qfield)
			g_object_unref (qfield);
		qfield = NULL;
		g_set_error (error,
			     GDA_QUERY_ERROR,
			     GDA_QUERY_SQL_ANALYSE_ERROR,
			     _("Can't set aggregate or function's arguments"));
	}
	else {
		if (add_to_query) {
			gda_entity_add_field (GDA_ENTITY (query), qfield);
			g_object_unref (G_OBJECT (qfield));
		}
	}

	/* memory de-allocation */
	g_slist_free (args);
	g_slist_free (arg_types);

	if (new_field)
		*new_field = created;
	return qfield;
}


GdaQueryCondition *
parsed_create_complex_condition (GdaQuery *query, ParseData *pdata, sql_where *where,
				 gboolean try_existing_field, GSList **targets_return, GError **error)
{
	GdaQueryCondition *cond = NULL, *tmpcond, *tmpcond2;

	g_return_val_if_fail (where, NULL);

	switch (where->type) {
	case SQL_single:
		cond = parsed_create_simple_condition (query, pdata, where->d.single, try_existing_field, 
						       targets_return, error);
		break;
	case SQL_negated:
		tmpcond = parsed_create_complex_condition (query, pdata, where->d.negated, 
							   try_existing_field, targets_return, error);
		if (tmpcond) {
			cond = (GdaQueryCondition*) gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_NOT);
			if (! gda_query_condition_node_add_child (cond, tmpcond, error)) {
				g_object_unref (G_OBJECT (cond));
				cond = NULL;
			}
			g_object_unref (G_OBJECT (tmpcond));
		}
		break;
	case SQL_pair:
		tmpcond = parsed_create_complex_condition (query, pdata, where->d.pair.left, 
							   try_existing_field, targets_return, error);
		if (tmpcond) {
			tmpcond2 = parsed_create_complex_condition (query, pdata, where->d.pair.right, 
								    try_existing_field, targets_return, error);
			if (tmpcond2) {
				switch (where->d.pair.op) {
				case SQL_and:
					cond = (GdaQueryCondition*) gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_AND);
					break;
				case SQL_or:
					cond = (GdaQueryCondition*) gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_OR);
					break;
				default:
					g_assert_not_reached ();
				}
				if (! gda_query_condition_node_add_child (cond, tmpcond, error)) {
					g_object_unref (G_OBJECT (cond));
					cond = NULL;
				}
				if (cond && ! gda_query_condition_node_add_child (cond, tmpcond2, error)) {
					g_object_unref (G_OBJECT (cond));
					cond = NULL;
				}
				g_object_unref (G_OBJECT (tmpcond));
				g_object_unref (G_OBJECT (tmpcond2));
			}
		}
		break;
	}

	return cond;
}


/*
 *  Always return a LEAF GdaQueryCondition object
 */
static GdaQueryCondition *
parsed_create_simple_condition (GdaQuery *query, ParseData *pdata, sql_condition *sqlcond, 
				gboolean try_existing_field, GSList **targets_return, GError **error)
{
	GdaQueryCondition *cond = NULL;
	GdaQueryConditionType condtype = parse_condop_converter [sqlcond->op];
	gboolean has_error = FALSE;
	GdaEntityField *fields[] = {NULL, NULL, NULL};
	GdaQueryTarget *target;

	/* getting GdaQueryField objects */
	if (condtype == GDA_QUERY_CONDITION_LEAF_BETWEEN) {
		gboolean created;
		target = NULL;
		fields[0] = parsed_create_global_query_field (query, TRUE, pdata, 
							      sqlcond->d.between.field, 
							      try_existing_field , &created, 
							      &target, error);
		if (created && fields[0])
			gda_query_field_set_visible (GDA_QUERY_FIELD (fields[0]), FALSE);
		if (fields[0]) {
			if (targets_return && target && !g_slist_find (*targets_return, target))
				*targets_return = g_slist_append (*targets_return, target);

			target = NULL;
			fields[1] = parsed_create_global_query_field (query, TRUE, pdata, 
								      sqlcond->d.between.lower, 
								      try_existing_field, &created, 
								      &target, error);
		}

		if (created && fields[1])
			gda_query_field_set_visible (GDA_QUERY_FIELD (fields[1]), FALSE);
		
		if (fields[1]) {
			if (targets_return && target && !g_slist_find (*targets_return, target))
				*targets_return = g_slist_append (*targets_return, target);
			
			target = NULL;
			fields[2] = parsed_create_global_query_field (query, TRUE, pdata, 
								      sqlcond->d.between.upper, 
								      try_existing_field, &created, 
								      &target, error);
		}
		if (created && fields[2])
			gda_query_field_set_visible (GDA_QUERY_FIELD (fields[2]), FALSE);

		if (fields[2]) {
			if (targets_return && target && !g_slist_find (*targets_return, target))
				*targets_return = g_slist_append (*targets_return, target);
		}

		if (! fields[0] || ! fields[1] || ! fields[2])
			has_error = TRUE;
	}
	else {
		gboolean created;
		target = NULL;
		fields[0] = parsed_create_global_query_field (query, TRUE, pdata, 
							      sqlcond->d.pair.left, 
							      try_existing_field, &created, 
							      &target, error);
		if (created && fields[0])
			gda_query_field_set_visible (GDA_QUERY_FIELD (fields[0]), FALSE);
		
		if (fields[0]) {
			if (targets_return && target && !g_slist_find (*targets_return, target))
				*targets_return = g_slist_append (*targets_return, target);

			target = NULL;
			fields[1] = parsed_create_global_query_field (query, TRUE, pdata, 
								      sqlcond->d.pair.right, 
								      try_existing_field, &created, 
								      &target, error);
		}
		if (created && fields[1])
			gda_query_field_set_visible (GDA_QUERY_FIELD (fields[1]), FALSE);

		if (fields[1]) {
			if (targets_return && target && !g_slist_find (*targets_return, target))
				*targets_return = g_slist_append (*targets_return, target);
		}

		if (! fields[0] || ! fields[1])
			has_error = TRUE;
	}
	if (has_error) 
		return NULL;

	/* from now on, all the required GdaQueryField objects do exist */
	/* cond creation */
	cond = (GdaQueryCondition*) gda_query_condition_new (query, condtype);
	gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_LEFT, GDA_QUERY_FIELD (fields[0]));
	gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT, GDA_QUERY_FIELD (fields[1]));
	if (condtype == GDA_QUERY_CONDITION_LEAF_BETWEEN)
		gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT2, GDA_QUERY_FIELD (fields[2]));

	if (sqlcond->negated) {
		GdaQueryCondition *cond2 = (GdaQueryCondition*) gda_query_condition_new (query, GDA_QUERY_CONDITION_NODE_NOT);
		gda_query_condition_node_add_child (cond2, cond, NULL);
		g_object_unref (G_OBJECT (cond));
		cond = cond2;
	}

	return cond;
}

