/* gda-dict-reg-types.c
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
#include "gda-dict-reg-types.h"
#include "gda-dict-type.h"
#include "gda-xml-storage.h"

/* NOTE about referencing data type objects:
 *
 * There are 2 kinds of data types: the ones returned by the GdaConnection schema data types list and
 * the ones added by gda_dict_declare_object().
 * 
 * Data types of the first category are:
 * -> assumed by GdaDict
 *
 * Data types of the 2nd category (custom ones) are:
 * -> just declared to GdaDict
 *
 * When a database sync occurs, if a data type which was a custom data type is found, then
 * it becomes a data type of the 1st category.
 */


#define XML_GROUP_TAG "gda_dict_types"
#define XML_ELEM_TAG "gda_dict_type"
#define SYNC_KEY "DATA_TYPES"

static gboolean types_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error);
static gboolean types_load_xml_tree (GdaDict *dict, xmlNodePtr types, GError **error);
static gboolean types_save_xml_tree (GdaDict *dict, xmlNodePtr types, GError **error);
static GSList  *types_get_objects  (GdaDict *dict);
static GdaObject *types_get_by_name (GdaDict *dict, const gchar *name);

GdaDictRegisterStruct *
gda_types_get_register ()
{
	GdaDictRegisterStruct *reg = NULL;

	reg = g_new0 (GdaDictRegisterStruct, 1);
	reg->type = GDA_TYPE_DICT_TYPE;
	reg->xml_group_tag = XML_GROUP_TAG;
	reg->dbms_sync_key = SYNC_KEY;
	reg->dbms_sync_descr = _("Data types analysis");
	reg->dbms_sync = types_dbms_sync; 
	reg->free = NULL;
	reg->get_objects = types_get_objects;
	reg->get_by_name = types_get_by_name;
	reg->load_xml_tree = types_load_xml_tree;
	reg->save_xml_tree = types_save_xml_tree;
	reg->sort = TRUE;

	reg->all_objects = NULL;
	reg->assumed_objects = NULL;

	return reg;
}

static gboolean
types_load_xml_tree (GdaDict *dict, xmlNodePtr types, GError **error)
{
	xmlNodePtr qnode = types->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp (qnode->name, XML_ELEM_TAG)) {
			gchar *prop;
			gboolean load = TRUE;

			prop = xmlGetProp (qnode, "custom");
			if (prop) {
				if (*prop == 't')
					load = FALSE;
				g_free (prop);
			}
			
			if (load) {
				GdaDictType *type;
			
				type = GDA_DICT_TYPE (gda_dict_type_new (dict));
				allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (type), qnode, error);
				if (allok) 
					gda_dict_assume_object (dict, (GdaObject *) type);
				g_object_unref (G_OBJECT (type));
			}
		}
		qnode = qnode->next;
	}

	return allok;
}


static gboolean
types_save_xml_tree (GdaDict *dict, xmlNodePtr group, GError **error)
{
	GSList *list, *alltypes;
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
	g_assert (reg);

	list = reg->assumed_objects;
	alltypes = reg->all_objects;

	for (; alltypes; alltypes = alltypes->next) {
		xmlNodePtr qnode;
		
		qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (alltypes->data), error);
		if (qnode) {
			xmlAddChild (group, qnode);
			if (! g_slist_find (list, alltypes->data))
				xmlSetProp (qnode, "custom", "t");
		}
		else 
			/* error handling */
			retval = FALSE;
	}

	return retval;
}

static GSList *
types_get_objects (GdaDict *dict)
{
	GSList *retval = NULL;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
	g_assert (reg);

	if (reg->all_objects)
		return g_slist_copy (reg->all_objects);
	else
		return NULL;

	return retval;
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

static GdaObject *
types_get_by_name (GdaDict *dict, const gchar *name)
{
	GSList *list;
	GdaDictType *datatype = NULL;
	GdaDictRegisterStruct *reg;
	gchar *cmpstr1, *cmpstr2;
	gboolean ci;
	
	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
	g_assert (reg);

	/* compare the data types names */
	ci = LC_NAMES (dict);
	if (ci)
		cmpstr1 = g_utf8_strdown (name, -1);
	else
		cmpstr1 = (gchar *) name;

	for (list = reg->all_objects; list && !datatype; list = list->next) {
		if (ci)
			cmpstr2 = g_utf8_strdown (gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data)), -1);
		else
			cmpstr2 = (gchar *) gda_dict_type_get_sqlname (GDA_DICT_TYPE (list->data));
		if (!strcmp (cmpstr1, cmpstr2))
			datatype = GDA_DICT_TYPE (list->data);
		if (ci)
			g_free (cmpstr2);
	}
	
	/* if not found, then compare with the synonyms */
	for (list = reg->all_objects; list && !datatype; list = list->next) {
		const GSList *synlist = gda_dict_type_get_synonyms (GDA_DICT_TYPE (list->data));
		while (synlist && !datatype) {
			if (ci)
				cmpstr2 = g_utf8_strdown ((gchar *) (synlist->data), -1);
			else
				cmpstr2 = (gchar *) (synlist->data);

			if (!strcmp (cmpstr1, cmpstr2))
				datatype = GDA_DICT_TYPE (list->data);
			if (ci)
				g_free (cmpstr2);
			synlist = g_slist_next (synlist);
		}
	}

	if (ci)
		g_free (cmpstr1);

	return (GdaObject *) datatype;
}

static gboolean
types_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error)
{
	GdaDictType *dt;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *updated_dt = NULL;
	gboolean has_synonyms;
	GSList *list, *all, *assumed;
	GdaDictRegisterStruct *reg;

	if (limit_object_name)
		TO_IMPLEMENT;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_TYPE);
	g_assert (reg);

	/* here we get the complete list of types, and for each type, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_TYPES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_DATATYPE_ERROR,
			     _("Can't get list of data types"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 4, 
				       G_TYPE_STRING, 
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_ULONG)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_DATATYPE_ERROR,
			     _("Schema for list of data types is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}
	has_synonyms = utility_check_data_model (rs, 5, 
						 G_TYPE_STRING, 
						 G_TYPE_STRING,
						 G_TYPE_STRING,
						 G_TYPE_ULONG,
						 G_TYPE_STRING);

	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		const GValue *value;
		gboolean newdt = FALSE;

		value = gda_data_model_get_value_at (rs, 0, now);
		str = gda_value_stringify ((GValue *) value);
		dt = (GdaDictType *) gda_dict_get_object_by_name (dict, GDA_TYPE_DICT_TYPE, str);
		if (!dt) {
			/* type name */
			dt = GDA_DICT_TYPE (gda_dict_type_new (dict));
			gda_dict_type_set_sqlname (dt, str);
			newdt = TRUE;
			
			/* REM: we don't add it right now to the GdaDict because we need to set the
			   attributes of @dt first */
		}
		g_free (str);
		
		updated_dt = g_slist_append (updated_dt, dt);

		/* FIXME: number of params */
		/*dt->numparams = 0;*/

		/* description */
		value = gda_data_model_get_value_at (rs, 2, now);
		if (value && !gda_value_is_null ((GValue *) value) && 
		    g_value_get_string((GValue *) value) && 
		    (* g_value_get_string((GValue *) value))) {
			str = gda_value_stringify ((GValue *) value);
			gda_object_set_description (GDA_OBJECT (dt), str);
			g_free (str);
		}
		else 
			gda_object_set_description (GDA_OBJECT (dt), NULL);

		/* owner */
		value = gda_data_model_get_value_at (rs, 1, now);
		if (value && !gda_value_is_null ((GValue *) value) && 
		    g_value_get_string((GValue *) value) && 
		    (* g_value_get_string((GValue *) value))) {
			str = gda_value_stringify ((GValue *) value);
			gda_object_set_owner (GDA_OBJECT (dt), str);
			g_free (str);
		}
		else
			gda_object_set_owner (GDA_OBJECT (dt), NULL);
				
		/* gda_type */
		value = gda_data_model_get_value_at (rs, 3, now);
		if (value && !gda_value_is_null ((GValue *) value)) 
			gda_dict_type_set_gda_type (dt, g_value_get_ulong ((GValue *) value));
		
		/* data type synomyms */
		gda_dict_type_clear_synonyms (dt);
		if (has_synonyms) {
			value = gda_data_model_get_value_at (rs, 4, now);
			if (value && !gda_value_is_null ((GValue *) value) && 
			    g_value_get_string ((GValue *) value) && 
			    (* g_value_get_string((GValue *) value))) {
				gchar *tok, *buf;

				str = gda_value_stringify ((GValue *) value);
				tok = strtok_r (str, ",", &buf);
				if (tok) {
					if (*tok) 
						gda_dict_type_add_synonym (dt, tok);
					tok = strtok_r (NULL, ",", &buf);
					while (tok) {
						if (*tok) 
							gda_dict_type_add_synonym (dt, tok);

						tok = strtok_r (NULL, ",", &buf);				
					}
				}
				g_free (str);
			}
		}

		if (newdt) {
			gda_dict_assume_object (dict, (GdaObject*) dt);
			g_object_unref ((GObject *) dt);
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", SYNC_KEY,
				       now, total);
		now++;
	}
	g_object_unref (G_OBJECT (rs));
	
	/* if a data type has been updated, make sure it's assumed and not just referenced */
	all = reg->all_objects;
	assumed = reg->assumed_objects;
	for (list = all; list ; list = list->next) {
		if (!g_slist_find (assumed, list->data) &&
		    g_slist_find (updated_dt, list->data))
			gda_dict_assume_object (dict, (GdaObject *) list->data);
	}

	/* remove the data types not existing anymore and not in the custom list */
	all = reg->all_objects;
	assumed = reg->assumed_objects;
	list = all;
	while (list) {
		if (!g_slist_find (assumed, list->data) && 
		    !g_slist_find (updated_dt, list->data)) {
			GSList *tmp = list->next;
			gda_object_destroy ((GdaObject *) list->data);
			list = tmp;
		}
		else
			list = list->next;
	}
	
	g_slist_free (updated_dt);
	g_signal_emit_by_name (G_OBJECT (dict), "update_progress", NULL, 0, 0);
	
	return TRUE;
}
