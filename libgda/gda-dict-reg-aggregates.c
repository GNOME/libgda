/* gda-dict-reg-aggregates.c
 *
 * Copyright (C) 2006 Vivien Malerba
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
#include "gda-dict-private.h"
#include "gda-dict-reg-aggregates.h"
#include "gda-dict-aggregate.h"
#include "gda-dict-type.h"
#include "gda-xml-storage.h"

#define XML_GROUP_TAG "gda_dict_aggregates"
#define XML_ELEM_TAG "gda_dict_aggregate"
#define SYNC_KEY "AGGREGATES"

static gboolean aggregates_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error);
static gboolean aggregates_load_xml_tree (GdaDict *dict, xmlNodePtr aggregates, GError **error);
static gboolean aggregates_save_xml_tree (GdaDict *dict, xmlNodePtr aggregates, GError **error);
static GSList  *aggregates_get_objects  (GdaDict *dict);

GdaDictRegisterStruct *
gda_aggregates_get_register ()
{
	GdaDictRegisterStruct *reg = NULL;

	reg = g_new0 (GdaDictRegisterStruct, 1);
	reg->type = GDA_TYPE_DICT_AGGREGATE;
	reg->xml_group_tag = XML_GROUP_TAG;
	reg->dbms_sync_key = SYNC_KEY;
	reg->dbms_sync_descr = _("Aggregates analysis");
	reg->dbms_sync = aggregates_dbms_sync; 
	reg->free = NULL;
	reg->get_objects = aggregates_get_objects;
	reg->get_by_name = NULL; /* aggregates are polymorph */
	reg->load_xml_tree = aggregates_load_xml_tree;
	reg->save_xml_tree = aggregates_save_xml_tree;
	reg->sort = TRUE;

	reg->all_objects = NULL;
	reg->assumed_objects = NULL;

	return reg;
}

static gboolean
aggregates_load_xml_tree (GdaDict *dict, xmlNodePtr aggregates, GError **error)
{
	xmlNodePtr qnode = aggregates->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp ((gchar *) qnode->name, XML_ELEM_TAG)) {
			GdaDictAggregate *agg;

			agg = GDA_DICT_AGGREGATE (gda_dict_aggregate_new (dict));
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (agg), qnode, error);
			if (allok) 
				gda_dict_assume_object (dict, (GdaObject *) agg);

			g_object_unref (G_OBJECT (agg));
		}
		qnode = qnode->next;
	}

	return allok;
}


static gboolean
aggregates_save_xml_tree (GdaDict *dict, xmlNodePtr group, GError **error)
{
	GSList *list, *allaggregates;
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);

	list = reg->assumed_objects;
	allaggregates = reg->all_objects;

	for (; allaggregates; allaggregates = allaggregates->next) {
		xmlNodePtr qnode;
		
		qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (allaggregates->data), error);
		if (qnode) {
			xmlAddChild (group, qnode);
			if (! g_slist_find (list, list->data))
				xmlSetProp (qnode, BAD_CAST "custom", BAD_CAST "t");
		}
		else 
			/* error handling */
			retval = FALSE;
	}

	return retval;
}

static GSList *
aggregates_get_objects (GdaDict *dict)
{
	GSList *retval = NULL;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);

	if (reg->assumed_objects)
		return g_slist_copy (reg->assumed_objects);
	else
		return NULL;

	return retval;
}


static gboolean
aggregates_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error)
{
	GdaDictAggregate *agg;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *list, *updated_aggs = NULL, *todelete_aggs = NULL;
	GSList *original_aggregates;
	gboolean insert;
	GdaDictRegisterStruct *reg;

	if (limit_object_name)
		TO_IMPLEMENT;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);

	/* here we get the complete list of aggregates, and for each aggregate, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_AGGREGATES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_AGGREGATES_ERROR,
			     _("Can't get list of aggregates"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 7, 
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_AGGREGATES_ERROR,
			     _("Schema for list of aggregates is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	original_aggregates = gda_dict_get_objects (dict, GDA_TYPE_DICT_AGGREGATE);
	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		GdaDictType *outdt = NULL; /* return type for the aggregate */
		GdaDictType *indt = NULL;  /* argument for the aggregate */
		const GValue *value;
		
		insert = TRUE;

		/* fetch return type */
		value = gda_data_model_get_value_at (rs, 4, now);
		str = gda_value_stringify ((GValue *) value);
		if (*str && (*str != '-')) {
			outdt = gda_dict_get_dict_type_by_name (dict, str);
			if (!outdt)
				insert = FALSE;
		}
		else 
			insert = FALSE;
		g_free (str);

		/* fetch argument type */
		value = gda_data_model_get_value_at (rs, 5, now);
		str = gda_value_stringify ((GValue *) value);
		if (str) {
			if (*str && (*str != '-')) {
				indt = gda_dict_get_dict_type_by_name (dict, str);
				if (!indt)
					insert = FALSE;
			}
			g_free (str);
		}

		/* fetch a agg if there is already one with the same id */
		value = gda_data_model_get_value_at (rs, 1, now);
		str = gda_value_stringify ((GValue *) value);
		agg = gda_dict_get_aggregate_by_dbms_id (dict, str);
		g_free (str);

		if (!agg) {
			/* try to find the aggregate using its name, return type and argument type, 
			   and not its DBMS id, this is usefull if the DBMS has changed and the
			   DBMS id have changed */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GValue *) value);
			agg = gda_aggregates_get_by_name_arg_in_list (dict, original_aggregates, str, indt);
			g_free (str);

			if (agg && (gda_dict_aggregate_get_ret_type (agg) != outdt))
				agg = NULL;
		}

		if (!insert) 
			agg = NULL;
		else {
			if (agg) {
				/* does the aggregate we found have the same outdt and indt
				   as the one we have now? */
				gboolean isequal = TRUE;
				if (gda_dict_aggregate_get_arg_type (agg) != indt)
					isequal = FALSE;
				if (isequal && (gda_dict_aggregate_get_ret_type (agg) != outdt)) 
					isequal = FALSE;
				
				if (isequal) {
					updated_aggs = g_slist_append (updated_aggs, agg);
					insert = FALSE;
				}
				else 
					agg = NULL;
			}

			if (!agg) {
				/* creating new ServerAggregate object */
				agg = GDA_DICT_AGGREGATE (gda_dict_aggregate_new (dict));
				gda_dict_aggregate_set_ret_type (agg, outdt);
				gda_dict_aggregate_set_arg_type (agg, indt);
				
				/* mark aggregate as updated */
				updated_aggs = g_slist_append (updated_aggs, agg);
			}
		}
	
		/* aggregate's attributes update */
		if (agg) {
			/* unique id */
			value = gda_data_model_get_value_at (rs, 1, now);
			str = gda_value_stringify ((GValue *) value);
			gda_dict_aggregate_set_dbms_id (agg, str);
			g_free (str);

			/* description */
			value = gda_data_model_get_value_at (rs, 3, now);
			if (value && !gda_value_is_null ((GValue *) value) && 
			    (* g_value_get_string((GValue *) value))) {
				str = gda_value_stringify ((GValue *) value);
				gda_object_set_description (GDA_OBJECT (agg), str);
				g_free (str);
			}
			
			/* sqlname */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GValue *) value);
			gda_dict_aggregate_set_sqlname (agg, str);
			g_free (str);
			
			/* owner */
			value = gda_data_model_get_value_at (rs, 2, now);
			if (value && !gda_value_is_null ((GValue *) value) && 
			    (* g_value_get_string((GValue *) value))) {
				str = gda_value_stringify ((GValue *) value);
				gda_object_set_owner (GDA_OBJECT (agg), str);
				g_free (str);
			}
			else
				gda_object_set_owner (GDA_OBJECT (agg), NULL);
		}

		/* Real insertion */
		if (insert) {
			gda_dict_assume_object (dict, (GdaObject *) agg);
			g_object_unref (agg);
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", SYNC_KEY, now, total);
		now ++;
	}

	g_object_unref (G_OBJECT (rs));
	if (original_aggregates)
		g_slist_free (original_aggregates);

	/* cleanup for the aggregates which do not exist anymore */
        list = reg->assumed_objects;
        while (list) {
		if (!g_slist_find (updated_aggs, list->data)) 
			todelete_aggs = g_slist_prepend (todelete_aggs, list->data);
		list = g_slist_next (list);
        }
	g_slist_free (updated_aggs);

	list = todelete_aggs;
	while (list) {
		gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (todelete_aggs);
		
	g_signal_emit_by_name (G_OBJECT (dict), "update_progress", NULL, 0, 0);
	
	return TRUE;
}

static gboolean 
LC_NAMES (GdaDict *dict) 
{
	GdaConnection *cnc;
	GdaServerProviderInfo *sinfo = NULL;
	
	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	return (sinfo && sinfo->is_case_insensitive);
}


/**
 * gda_aggregates_get_by_name_arg
 * @dict: a #GdaDict object
 * @aggname: the name of the aggregate
 * @argtype: the type of argument or %NULL
 *
 * To find a DBMS aggregate which is uniquely identified by its name and the type
 * of its argument.
 *
 * About the aggregates accepting any data type for their argument: if @argtype is not %NULL
 * then such an aggregate will be a candidate, and if @argtype is %NULL
 * then only such an aggregate will be a candidate.
 *
 * If several aggregates are found, then the aggregate completely matching will be returned, or
 * an aggregate where the argument type has the same GDA typa as the @argtype, or lastly an
 * aggregate accepting any data type as argument.
 *
 * Returns: The aggregate or NULL if not found
 */
GdaDictAggregate *
gda_aggregates_get_by_name_arg (GdaDict *dict, const gchar *aggname, GdaDictType *argtype)
{
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);
	return gda_aggregates_get_by_name_arg_in_list (dict, reg->assumed_objects, aggname, argtype);
}


GdaDictAggregate *
gda_aggregates_get_by_name_arg_in_list (GdaDict *dict, GSList *aggregates, const gchar *aggname, GdaDictType *argtype)
{
	GdaDictAggregate *agg = NULL; /* prefectely matching aggregate */
	GdaDictAggregate *anytypeagg = NULL; /* matching because it accepts any data type */
	GdaDictAggregate *gdatypeagg = NULL; /* matching because its arg type is the same gda type as requested */
	GSList *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	GdaServerProviderInfo *sinfo = NULL;
	GdaConnection *cnc;

	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (aggname, -1);
	else
		cmpstr = (gchar *) aggname;

	list = aggregates;
	while (list && !agg) {
		GdaDictType *testdt = gda_dict_aggregate_get_arg_type (GDA_DICT_AGGREGATE (list->data));
		GdaDictAggregate *tmp = NULL;
		gint mode = 0;

		if (argtype == testdt) {
			tmp = GDA_DICT_AGGREGATE (list->data);
			mode = 1;
		}
		else {
			if (!testdt) {
				tmp = GDA_DICT_AGGREGATE (list->data);
				mode = 2;
			}
			else {
				if (argtype && testdt &&
				    sinfo && sinfo->implicit_data_types_casts &&
				    (gda_dict_type_get_gda_type (testdt) == 
				     gda_dict_type_get_gda_type (argtype))) {
					tmp = GDA_DICT_AGGREGATE (list->data);
					mode = 3;
				}
			}
		}

		if (tmp) {
			if (LC_NAMES (dict)) {
				cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
				if (strcmp (cmpstr2, cmpstr))
					tmp = NULL;
				g_free (cmpstr2);
			}
			else
				if (strcmp (cmpstr, gda_object_get_name (GDA_OBJECT (list->data))))
					tmp = NULL;
		}

		if (tmp) {
			switch (mode) {
			case 1:
				agg = tmp;
				break;
			case 2:
				anytypeagg = tmp;
				break;
			case 3:
				gdatypeagg = tmp;
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
		
		list = g_slist_next (list);
	}

	if (!agg && gdatypeagg)
		agg = gdatypeagg;

	if (!agg && anytypeagg)
		agg = anytypeagg;

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return agg;	
}

/**
 * gda_dict_get_aggregate_by_dbms_id
 * @dict: a #GdaDict object
 * @dbms_id: 
 *
 * To find a DBMS functions which is uniquely identified by its name and the type
 * of its argument.
 *
 * Returns: The aggregate or NULL if not found
 */
GdaDictAggregate *
gda_aggregates_get_by_dbms_id (GdaDict *dict, const gchar *dbms_id)
{
	GdaDictAggregate *agg = NULL;
	GSList *list;
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (dbms_id && *dbms_id, NULL);

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);

	list = reg->assumed_objects;
	while (list && !agg) {
		gchar *str;

		str = gda_dict_aggregate_get_dbms_id (GDA_DICT_AGGREGATE (list->data));
		if (!strcmp (dbms_id, str))
			agg = GDA_DICT_AGGREGATE (list->data);
		g_free (str);
		list = g_slist_next (list);
	}

	return agg;
}


GSList *
gda_aggregates_get_by_name (GdaDict *dict, const gchar *aggname)
{
	GSList *retval = NULL, *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (aggname && *aggname, NULL);

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_AGGREGATE);
	g_assert (reg);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (aggname, -1);
	else
		cmpstr = (gchar *) aggname;

	list = reg->assumed_objects;
	while (list) {
		if (LC_NAMES (dict)) {
			cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
			if (!strcmp (cmpstr2, cmpstr))
				retval = g_slist_prepend (retval, list->data);
			g_free (cmpstr2);
		}
		else
			if (!strcmp (gda_object_get_name (GDA_OBJECT (list->data)), cmpstr))
				retval = g_slist_prepend (retval, list->data);
		list = g_slist_next (list);
	}

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return retval;;
}
