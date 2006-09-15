/* gda-dict-reg-functions.c
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
#include "gda-dict-reg-functions.h"
#include "gda-dict-function.h"
#include "gda-dict-type.h"
#include "gda-xml-storage.h"

#define XML_GROUP_TAG "gda_dict_procedures"
#define XML_ELEM_TAG "gda_dict_function"
#define SYNC_KEY "FUNCTIONS"

static gboolean functions_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error);
static gboolean functions_load_xml_tree (GdaDict *dict, xmlNodePtr functions, GError **error);
static gboolean functions_save_xml_tree (GdaDict *dict, xmlNodePtr functions, GError **error);
static GSList  *functions_get_objects  (GdaDict *dict);

GdaDictRegisterStruct *
gda_functions_get_register ()
{
	GdaDictRegisterStruct *reg = NULL;

	reg = g_new0 (GdaDictRegisterStruct, 1);
	reg->type = GDA_TYPE_DICT_FUNCTION;
	reg->xml_group_tag = XML_GROUP_TAG;
	reg->dbms_sync_key = SYNC_KEY;
	reg->dbms_sync_descr = _("Functions analysis");
	reg->dbms_sync = functions_dbms_sync; 
	reg->free = NULL;
	reg->get_objects = functions_get_objects;
	reg->get_by_name = NULL; /* functions are polymorph */
	reg->load_xml_tree = functions_load_xml_tree;
	reg->save_xml_tree = functions_save_xml_tree;
	reg->sort = TRUE;

	reg->all_objects = NULL;
	reg->assumed_objects = NULL;

	return reg;
}

static gboolean
functions_load_xml_tree (GdaDict *dict, xmlNodePtr functions, GError **error)
{
	xmlNodePtr qnode = functions->children;
	gboolean allok = TRUE;
	
	while (qnode && allok) {
		if (!strcmp ((gchar *) qnode->name, XML_ELEM_TAG)) {
			GdaDictFunction *function;

			function = GDA_DICT_FUNCTION (gda_dict_function_new (dict));
			allok = gda_xml_storage_load_from_xml (GDA_XML_STORAGE (function), qnode, error);
			if (allok) 
				gda_dict_assume_object (dict, (GdaObject *) function);

			g_object_unref (G_OBJECT (function));
		}
		qnode = qnode->next;
	}

	return allok;
}


static gboolean
functions_save_xml_tree (GdaDict *dict, xmlNodePtr group, GError **error)
{
	GSList *list, *allfunctions;
	gboolean retval = TRUE;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);

	list = reg->assumed_objects;
	allfunctions = g_slist_copy (reg->all_objects);

	for (; list; list = list->next) {
		xmlNodePtr qnode;
		qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
		if (qnode)
			xmlAddChild (group, qnode);
		else 
			/* error handling */
			retval = FALSE;
		allfunctions = g_slist_remove (allfunctions, list->data);
	}
	
	
	for (list = allfunctions; list; list = list->next) {
		xmlNodePtr qnode;
		qnode = gda_xml_storage_save_to_xml (GDA_XML_STORAGE (list->data), error);
		if (qnode) {
			xmlAddChild (group, qnode);
			xmlSetProp (qnode, "custom", "t");
		}
		else 
			/* error handling */
			retval = FALSE;
	}
	g_slist_free (allfunctions);

	return retval;
}

static GSList *
functions_get_objects (GdaDict *dict)
{
	GSList *retval = NULL;
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);

	if (reg->assumed_objects)
		return g_slist_copy (reg->assumed_objects);
	else
		return NULL;

	return retval;
}


static gboolean
functions_dbms_sync (GdaDict *dict, const gchar *limit_object_name, GError **error)
{
	GdaDictFunction *func;
	GdaDataModel *rs;
	gchar *str;
	guint now, total;
	GSList *list, *updated_fn = NULL, *todelete_fn = NULL;
	GSList *original_functions;
	gboolean insert;
	GdaDictRegisterStruct *reg;

	if (limit_object_name)
		TO_IMPLEMENT;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);

	/* here we get the complete list of functions, and for each function, update or
	   create the entry in the list if not yet there. */
	rs = gda_connection_get_schema (GDA_CONNECTION (dict->priv->cnc),
					GDA_CONNECTION_SCHEMA_PROCEDURES, NULL, NULL);

	if (!rs) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FUNCTIONS_ERROR,
			     _("Can't get list of functions"));
		return FALSE;
	}


	if (!utility_check_data_model (rs, 8, 
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_INT,
				       G_TYPE_STRING,
				       G_TYPE_STRING)) {
		g_set_error (error, GDA_DICT_ERROR, GDA_DICT_FUNCTIONS_ERROR,
			     _("Schema for list of functions is wrong"));
		g_object_unref (G_OBJECT (rs));
		return FALSE;
	}

	original_functions = gda_dict_get_functions (dict);
	total = gda_data_model_get_n_rows (rs);
	now = 0;		
	while ((now < total) && !dict->priv->stop_update) {
		GdaDictType *rettype = NULL; /* return type for the function */
		GSList *dtl = NULL;     /* list of params for the function */
		const GValue *value;
		gchar *ptr;
		gchar *tok;
		
		insert = TRUE;

		/* fetch return type */
		value = gda_data_model_get_value_at (rs, 4, now);
		str = gda_value_stringify ((GValue *) value);
		if (*str && (*str != '-')) {
			rettype = gda_dict_get_dict_type_by_name (dict, str);
			if (!rettype)
				insert = FALSE;
		}
		else 
			insert = FALSE;
		g_free (str);

		/* fetch argument types */
		value = gda_data_model_get_value_at (rs, 6, now);
		str = gda_value_stringify ((GValue *) value);
		if (str) {
			ptr = strtok_r (str, ",", &tok);
			while (ptr && *ptr) {
				GdaDictType *indt;

				if (*ptr && (*ptr == '-'))
					dtl = g_slist_append (dtl, NULL); /* any data type will do */
				else {
					indt = gda_dict_get_dict_type_by_name (dict, ptr);
					if (indt)
						dtl = g_slist_append (dtl, indt);
					else 
						insert = FALSE;
				}
				ptr = strtok_r (NULL, ",", &tok);
			}
			g_free (str);
		}

		/* fetch a func if there is already one with the same id */
		value = gda_data_model_get_value_at (rs, 1, now);
		str = gda_value_stringify ((GValue *) value);
		func = gda_dict_get_function_by_dbms_id (dict, str);
		g_free (str);

		if (!func) {
			/* try to find the function using its name, return type and argument type, 
			   and not its DBMS id, this is usefull if the DBMS has changed and the
			   DBMS id have changed */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GValue *) value);
			func = gda_functions_get_by_name_arg_in_list (dict, original_functions, str, dtl);
			g_free (str);

			if (func && (gda_dict_function_get_ret_type (func) != rettype))
				func = NULL;
		}

		if (!insert) {
			if (func)
				/* If no insertion, then func MUST be updated */
				todelete_fn = g_slist_append (todelete_fn, func);
			func = NULL;
		}
		else {
			if (func) {
				/* does the function we found have the same rettype and params 
				   as the one we have now? */
				gboolean isequal = TRUE;
				GSList *hlist;
				
				list = (GSList *) gda_dict_function_get_arg_types (func);
				hlist = dtl;
				while (list && hlist && isequal) {
					if (list->data != hlist->data)
						isequal = FALSE;
					list = g_slist_next (list);
					hlist = g_slist_next (hlist);
				}
				if (isequal && (gda_dict_function_get_ret_type (func) != rettype)) 
					isequal = FALSE;
				
				if (isequal) {
					updated_fn = g_slist_append (updated_fn, func);
					insert = FALSE;
				}
				else {
					todelete_fn = g_slist_append (todelete_fn, func);
					func = NULL;
				}
			}

			if (!func) {
				/* creating new ServerFunction object */
				func = GDA_DICT_FUNCTION (gda_dict_function_new (dict));
				gda_dict_function_set_ret_type (func, rettype);
				gda_dict_function_set_arg_types (func, dtl);

				/* mark function as updated */
				updated_fn = g_slist_append (updated_fn, func);
			}
		}
	
		if (dtl)
			g_slist_free (dtl);
		
		/* function's attributes update */
		if (func) {
			/* unique id */
			value = gda_data_model_get_value_at (rs, 1, now);
			str = gda_value_stringify ((GValue *) value);
			gda_dict_function_set_dbms_id (func, str);
			g_free (str);

			/* description */
			value = gda_data_model_get_value_at (rs, 3, now);
			if (value && !gda_value_is_null ((GValue *) value) && 
			    (* g_value_get_string((GValue *) value))) {
				str = gda_value_stringify ((GValue *) value);
				gda_object_set_description (GDA_OBJECT (func), str);
				g_free (str);
			}
			
			/* sqlname */
			value =  gda_data_model_get_value_at (rs, 0, now);
			str = gda_value_stringify ((GValue *) value);
			gda_dict_function_set_sqlname (func, str);
			g_free (str);
			
			/* owner */
			value = gda_data_model_get_value_at (rs, 2, now);
			if (value && !gda_value_is_null ((GValue *) value) && 
			    (* g_value_get_string((GValue *) value))) {
				str = gda_value_stringify ((GValue *) value);
				gda_object_set_owner (GDA_OBJECT (func), str);
				g_free (str);
			}
			else
				gda_object_set_owner (GDA_OBJECT (func), NULL);
		}

		/* Real insertion */
		if (insert) {
			gda_dict_assume_object (dict, (GdaObject *) func);
			g_object_unref (func);
		}

		g_signal_emit_by_name (G_OBJECT (dict), "update_progress", SYNC_KEY, now, total);
		now ++;
	}

	g_object_unref (G_OBJECT (rs));
	if (original_functions)
		g_slist_free (original_functions);

	/* cleanup for the functions which do not exist anymore */
        list = reg->assumed_objects;
        while (list && !dict->priv->stop_update) {
		if (!g_slist_find (updated_fn, list->data)) 
			todelete_fn = g_slist_append (todelete_fn, list->data);
		list = g_slist_next (list);
        }
	g_slist_free (updated_fn);

	list = todelete_fn;
	while (list) {
		gda_object_destroy (GDA_OBJECT (list->data));
		list = g_slist_next (list);
	}
	g_slist_free (todelete_fn);
	
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
 * gda_functions_get_by_name_arg
 * @dict: a #GdaDict object
 * @funcname: the name of the function
 * @argtypes: a list of types of argument or %NULL
 *
 * To find a DBMS function which is uniquely identified by its name and the type
 * of its argument.
 *
 * About the functions accepting any data type for their argument: if @argtype is not %NULL
 * then such an function will be a candidate, and if @argtype is %NULL
 * then only such an function will be a candidate.
 *
 * If several functions are found, then the function completely matching will be returned, or
 * an function where the argument type has the same GDA typa as the @argtype, or lastly an
 * function accepting any data type as argument.
 *
 * Returns: The function or NULL if not found
 */
GdaDictFunction *
gda_functions_get_by_name_arg (GdaDict *dict, const gchar *funcname, const GSList *argtypes)
{
	GdaDictRegisterStruct *reg;

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);
	return gda_functions_get_by_name_arg_in_list (dict, reg->assumed_objects, funcname, argtypes);
}


GdaDictFunction *
gda_functions_get_by_name_arg_in_list (GdaDict *dict, GSList *functions, 
				       const gchar *funcname, const GSList *argtypes)
{
	GdaDictFunction *func = NULL; /* prefectely matching function */
	GdaDictFunction *anytypefunc = NULL; /* matching because it accepts any data type for one of its args */
	GdaDictFunction *gdatypefunc = NULL; /* matching because its arg type is the same gda type as requested for one of its args */
	GSList *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;

	GdaServerProviderInfo *sinfo = NULL;
	GdaConnection *cnc;

	cnc = gda_dict_get_connection (dict);
        if (cnc)
                sinfo = gda_connection_get_infos (cnc);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (funcname, -1);
	else
		cmpstr = (gchar *) funcname;

	list = functions;
	while (list && !func) {
		gboolean argsok = TRUE;
		gboolean func_any_type = FALSE;
		gboolean func_gda_type = FALSE;
		GSList *funcargs, *list2;
		GdaDictFunction *tmp = NULL;

		/* arguments comparison */
		list2 = (GSList *) argtypes;
		funcargs = (GSList *) gda_dict_function_get_arg_types (GDA_DICT_FUNCTION (list->data));
		while (funcargs && list2 && argsok) {
			gboolean tmpok = FALSE;

			if (funcargs->data == list2->data)
				tmpok = TRUE;
			else {
				if (!funcargs->data) {
					tmpok = TRUE;
					func_any_type = TRUE;
				}
				else {
					if (funcargs->data && list2->data &&
					    sinfo && sinfo->implicit_data_types_casts &&
					    (gda_dict_type_get_gda_type (funcargs->data) == 
					     gda_dict_type_get_gda_type (list2->data))) {
						tmpok = TRUE;
						func_gda_type = TRUE;
					}
				}
			}
			
			if (!tmpok)
				argsok = FALSE;
			funcargs = g_slist_next (funcargs);
			list2 = g_slist_next (list2);
		}
		if (list2 || funcargs)
			argsok = FALSE;

		/* names comparison */
		if (argsok) {
			if (LC_NAMES (dict)) {
				cmpstr2 = g_utf8_strdown (gda_object_get_name (GDA_OBJECT (list->data)), -1);
				if (!strcmp (cmpstr2, cmpstr))
					tmp = GDA_DICT_FUNCTION (list->data);
				g_free (cmpstr2);
			}
			else
				if (!strcmp (cmpstr, gda_object_get_name (GDA_OBJECT (list->data))))
					tmp = GDA_DICT_FUNCTION (list->data);
		}

		if (tmp) {
			if (func_any_type)
				anytypefunc = tmp;
			else {
				if (func_gda_type)
					gdatypefunc = tmp;
				else
					func = tmp;
			}
		}
		
		list = g_slist_next (list);
	}

	if (!func && gdatypefunc)
		func = gdatypefunc;
	if (!func && anytypefunc)
		func = anytypefunc;

	if (LC_NAMES (dict))
		g_free (cmpstr);

	return func;	
}

/**
 * gda_dict_get_function_by_dbms_id
 * @dict: a #GdaDict object
 * @dbms_id: 
 *
 * To find a DBMS functions which is uniquely identified by its name and the type
 * of its argument.
 *
 * Returns: The function or NULL if not found
 */
GdaDictFunction *
gda_functions_get_by_dbms_id (GdaDict *dict, const gchar *dbms_id)
{
	GdaDictFunction *func = NULL;
	GSList *list;
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (dbms_id && *dbms_id, NULL);

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);

	list = reg->assumed_objects;
	while (list && !func) {
		gchar *str = gda_dict_function_get_dbms_id (GDA_DICT_FUNCTION (list->data));
		if (!str || ! *str) {
			str = (gchar *) gda_dict_function_get_sqlname (GDA_DICT_FUNCTION (list->data));
			g_error ("Function %p (%s) has no dbms_id", list->data, str);
		}
		if (str && !strcmp (dbms_id, str))
			func = GDA_DICT_FUNCTION (list->data);
		g_free (str);
		list = g_slist_next (list);
	}

	return func;
}


GSList *
gda_functions_get_by_name (GdaDict *dict, const gchar *funcname)
{
	GSList *retval = NULL, *list;
	gchar *cmpstr = NULL;
	gchar *cmpstr2;
	GdaDictRegisterStruct *reg;

	g_return_val_if_fail (GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (dict->priv, NULL);
	g_return_val_if_fail (funcname && *funcname, NULL);

	reg = gda_dict_get_object_type_registration (dict, GDA_TYPE_DICT_FUNCTION);
	g_assert (reg);

	if (LC_NAMES (dict))
		cmpstr = g_utf8_strdown (funcname, -1);
	else
		cmpstr = (gchar *) funcname;

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

	return retval;
}
