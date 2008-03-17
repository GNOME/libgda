/* 
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "command-exec.h"
#include <glib/gi18n-lib.h>
#include <string.h>
#include "tools-input.h"
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif


/*
 * command_exec_result_free
 *
 * Clears the memory associated with @res
 */
void
gda_internal_command_exec_result_free (GdaInternalCommandResult *res)
{
	if (!res)
		return;
	switch (res->type) {
	case GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL:
		if (res->u.model)
			g_object_unref (res->u.model);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_SET:
		if (res->u.set)
			g_object_unref (res->u.set);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_TXT:
	case GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT:
		if (res->u.txt)
			g_string_free (res->u.txt, TRUE);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;

		for (list = res->u.multiple_results; list; list = list->next)
			gda_internal_command_exec_result_free ((GdaInternalCommandResult *) list->data);
		g_slist_free (res->u.multiple_results);
		break;
	}
	default:
		break;
	}
	g_free (res);
}

static GdaInternalCommand *
find_command (GdaInternalCommandsList *commands_list, const gchar *command_str, gboolean *command_complete)
{
	GdaInternalCommand *command = NULL;
	GSList *list;
	gint length;

	if (!command_str || ((*command_str != '\\') && (*command_str != '.')))
		return NULL;

	length = strlen (command_str + 1);
	for (list = commands_list->name_ordered; list; list = list->next) {
		command = (GdaInternalCommand*) list->data;
		if (!strncmp (command->name, command_str + 1, MIN (length, strlen (command->name)))) {
			gint l;
			gchar *ptr;
			for (ptr = command->name, l = 0; *ptr && (*ptr != ' '); ptr++, l++);
				
			if (length == l)
				break;
			else
				command = NULL;
		}
		else
			command = NULL;
	}

	/* FIXME */
	if (command_complete)
		*command_complete = TRUE;

	return command;
}

static gint
commands_compare_name (GdaInternalCommand *a, GdaInternalCommand *b)
{
	gint cmp, alength, blength;
	if (!a->name || !b->name) {
		g_warning (_("Invalid unnamed command"));
		if (!a->name) {
			if (b->name)
				return 1;
			else
				return 0;
		}
		else
			return -1;
	}
	alength = strlen (a->name);
	blength = strlen (b->name);
	cmp = strncmp (a->name, b->name, MIN (alength, blength));
	if (cmp == 0) 
		return blength - alength;
	else
		return cmp;
}

static gint
commands_compare_group (GdaInternalCommand *a, GdaInternalCommand *b)
{
	if (!a->group) {
		if (b->group)
			return 1;
		else
			return 0;
	}
	else {
		if (b->group) {
			gint cmp = strcmp (a->group, b->group);
			if (cmp)
				return cmp;
			else 
				return commands_compare_name (a, b);
		}
		else
			return -1;
	}
}

/* default function to split arguments */
static gchar **
default_gda_internal_commandargs_func (const gchar *string)
{
	gchar **array, **ptr;

	array = g_strsplit (string, " ", -1);
	for (ptr = array; *ptr; ptr++)
		g_strchug (*ptr);

	return array;
}


/*
 * gda_internal_command_execute
 *
 * Executes a command starting by \ such as:
 */
GdaInternalCommandResult *
gda_internal_command_execute (GdaInternalCommandsList *commands_list,
			      GdaConnection *cnc, const gchar *command_str, GError **error)
{
	GdaInternalCommand *command;
	gboolean command_complete;
	gchar **args;
	GdaInternalCommandResult *res = NULL;

	g_return_val_if_fail (commands_list, NULL);
	if (!commands_list->name_ordered) {
		GSList *list;

		for (list = commands_list->commands; list; list = list->next) {
			commands_list->name_ordered = 
				g_slist_insert_sorted (commands_list->name_ordered, list->data,
						       (GCompareFunc) commands_compare_name);
			commands_list->group_ordered = 
				g_slist_insert_sorted (commands_list->group_ordered, list->data,
						       (GCompareFunc) commands_compare_group);
		}
	}

	args = g_strsplit (command_str, " ", 2);
	command = find_command (commands_list, args[0], &command_complete);
	g_strfreev (args);
	args = NULL;
	if (!command) {
		g_set_error (error, 0, 0,
			     _("Unknown internal command"));
		goto cleanup;
	}

	if (!command->command_func) {
		g_set_error (error, 0, 0,
			     _("Internal command not correctly defined"));
		goto cleanup;
	}

	if (!command_complete) {
		g_set_error (error, 0, 0,
			     _("Incomplete internal command"));
		goto cleanup;
	}

	if (command->arguments_delimiter_func)
		args = command->arguments_delimiter_func (command_str);
	else
		args = default_gda_internal_commandargs_func (command_str);
	res = command->command_func (cnc, (const gchar **) &(args[1]), 
				     error, command->user_data);
	
 cleanup:
	if (args)
		g_strfreev (args);

	return res;
}

GdaInternalCommandResult *
gda_internal_command_help (GdaConnection *cnc, const gchar **args, 
			   GError **error,
			   GdaInternalCommandsList *clist)
{
	GdaInternalCommandResult *res;
	GSList *list;
	gchar *current_group = NULL;
	GString *string = g_string_new ("");
#define NAMESIZE 18

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;

	for (list = clist->group_ordered; list; list = list->next) {
		GdaInternalCommand *command = (GdaInternalCommand*) list->data;
		if (!current_group || strcmp (current_group, command->group)) {
			current_group = command->group;
			if (list != clist->group_ordered)
				g_string_append_c (string, '\n');
			g_string_append_printf (string, "%s\n", current_group);
			
		}
		g_string_append_printf (string, "  \\%s", command->name);
		if (strlen (command->name) < NAMESIZE) {
			gint i;
			for (i = strlen (command->name); i < NAMESIZE; i++)
				g_string_append_c (string, ' ');
		}
		else {
			g_string_append_c (string, '\n');
			gint i;
			for (i = 0; i < NAMESIZE + 3; i++)
				g_string_append_c (string, ' ');
		}
		g_string_append_printf (string, "%s\n", command->description);
	}
	res->u.txt = string;
	return res;
}

GdaInternalCommandResult *
gda_internal_command_history (GdaConnection *cnc, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;

	GString *string;
#ifdef HAVE_READLINE_HISTORY_H
	if (args[0]) {
		if (!save_history (args[0], error)) {
			g_free (res);
			res = NULL;
		}
		else
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else {
		HIST_ENTRY **hist_array = history_list ();
		string = g_string_new ("");
		if (hist_array) {
			HIST_ENTRY *current;
			gint index;
			for (index = 0, current = *hist_array; current; index++, current = hist_array [index]) {
				g_string_append (string, current->line);
				g_string_append_c (string, '\n');
			}
		}
		res->u.txt = string;
	}
#else
	string = g_string_new (_("History is not supported"));
	res->u.txt = string;
#endif
	
	return res;
}

GdaInternalCommandResult *
gda_internal_command_dict_sync (GdaConnection *cnc, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	if (!gda_connection_update_meta_store (cnc, NULL, error)) {
		g_free (res);
		res = NULL;
	}

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_tables (GdaConnection *cnc, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_name=##tname::string AND "
			"table_type LIKE '%TABLE%' AND table_short_name = table_name "
			"ORDER BY table_schema, table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name = table_name "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of tables"));
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_views (GdaConnection *cnc, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_name=##tname::string AND "
			"table_type = 'VIEW' AND table_short_name = table_name "
			"ORDER BY table_schema, table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type='VIEW' AND table_short_name = table_name "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of views"));
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_schemas (GdaConnection *cnc, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		GValue *v;
		const gchar *sql = "SELECT schema_name AS Schema, schema_owner AS Owner, "
			"CASE WHEN schema_internal THEN 'yes' ELSE 'no' END AS Internal "
			"FROM _schemata WHERE schema_name=##sname::string "
			"ORDER BY schema_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "sname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT schema_name AS Schema, schema_owner AS Owner, "
			"CASE WHEN schema_internal THEN 'yes' ELSE 'no' END AS Internal "
			"FROM _schemata ORDER BY schema_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error);
	}
	if (!model)
		return NULL;

	g_object_set_data (G_OBJECT (model), "name", _("List of schemas"));
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_queries (GdaConnection *cnc, const gchar **args,
				   GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;
	GdaDataModel *model;
	const gchar *qname = NULL;
	gboolean with_sql_def = FALSE;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		if (!strcmp (args[0], "+"))
			with_sql_def = TRUE;
		else {
			qname = args[0];
			if (args[1] && (*args[1] == '+'))
				with_sql_def = TRUE;
		}
	}

	if (with_sql_def)
		model = gda_data_model_array_new_with_g_types (5,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
	else
		model = gda_data_model_array_new_with_g_types (4,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Type"));
	gda_data_model_set_column_title (model, 2, _("Description"));
	gda_data_model_set_column_title (model, 3, _("Parameters"));
	if (with_sql_def)
		gda_data_model_set_column_title (model, 4, _("SQL"));
	g_object_set_data (G_OBJECT (model), "name", _("List of queries"));

	if (qname) {
		TO_IMPLEMENT;
	}
	else {
		

		TO_IMPLEMENT;
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaMetaStruct *
gda_internal_command_build_meta_struct (GdaConnection *cnc, const gchar **args, GError **error)
{
	GdaMetaStruct *mstruct;
	gint index;
	const gchar *arg;
	GdaMetaStore *store;

	store = gda_connection_get_meta_store (cnc);
	mstruct = gda_meta_struct_new (GDA_META_STRUCT_FEATURE_ALL);

	if (!args[0]) {
		/* use all tables or views */
		GdaDataModel *model;
		gint i, nrows;
		const gchar *sql = "SELECT t.table_catalog, t.table_schema, t.table_name, v.table_name FROM _tables as t LEFT JOIN _views as v ON (t.table_catalog=v.table_catalog AND t.table_schema=v.table_schema AND t.table_name=v.table_name) WHERE table_short_name != table_full_name";
		model = gda_meta_store_extract (store, sql, error, NULL);
		if (!model)
			return NULL;
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			const GValue *detv;
			detv = gda_data_model_get_value_at (model, 3, i);
			if (! gda_meta_struct_complement (mstruct, store, 
							  detv && !gda_value_is_null (detv) && 
							  g_value_get_string (detv) && *g_value_get_string (detv) ? 
							  GDA_META_DB_VIEW : GDA_META_DB_TABLE,
							  gda_data_model_get_value_at (model, 0, i),
							  gda_data_model_get_value_at (model, 1, i),
							  gda_data_model_get_value_at (model, 2, i), error)) 
				goto onerror;
		}
		g_object_unref (model);
	}

	for (index = 0, arg = args[0]; arg; index++, arg = args[index]) {
		GValue *v;
		g_value_set_string (v = gda_value_new (G_TYPE_STRING), arg);

		/* see if we have the form <schema_name>.*, to list all the objects in a given schema */
		if (g_str_has_suffix (arg, ".*") && (*arg != '.')) {
			gchar *str;
			GdaDataModel *model;
			gint i, nrows;

			str = g_strdup (arg);
			str[strlen (str) - 2] = 0;
			g_value_take_string (v, str);
			const gchar *sql = "SELECT t.table_catalog, t.table_schema, t.table_name, v.table_name FROM _tables as t LEFT JOIN _views as v ON (t.table_catalog=v.table_catalog AND t.table_schema=v.table_schema AND t.table_name=v.table_name) WHERE t.table_schema = ##ts::string";
			model = gda_meta_store_extract (store, sql, error, "ts", v, NULL);
			if (!model)
				return NULL;
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				const GValue *detv;
				detv = gda_data_model_get_value_at (model, 3, i);
				if (! gda_meta_struct_complement (mstruct, store, 
								  detv && !gda_value_is_null (detv) && 
								  g_value_get_string (detv) && *g_value_get_string (detv) ? 
								  GDA_META_DB_VIEW : GDA_META_DB_TABLE,
								  gda_data_model_get_value_at (model, 0, i),
								  gda_data_model_get_value_at (model, 1, i),
								  gda_data_model_get_value_at (model, 2, i), error)) 
					goto onerror;
			}
			g_object_unref (model);
		}
		else {
			/* try to find it as a table or view */
			if (!gda_meta_struct_complement (mstruct, store, GDA_META_DB_UNKNOWN, NULL, NULL, v, NULL)) {
				if (g_str_has_suffix (arg, "=") && (*arg != '=')) {
					GdaMetaDbObject *dbo;
					gchar *str;
					str = g_strdup (arg);
					str[strlen (str) - 1] = 0;
					g_value_take_string (v, str);
					dbo = gda_meta_struct_complement (mstruct, store, GDA_META_DB_TABLE, 
									  NULL, NULL, v, NULL);
					if (!dbo)
						dbo = gda_meta_struct_complement (mstruct, store, GDA_META_DB_VIEW, 
										  NULL, NULL, v, NULL);
					if (dbo && dbo->depend_list) {
						GSList *list, *dep_list;
						GValue *catalog, *schema, *name;

						dep_list = g_slist_copy (dbo->depend_list);
						for (list = dep_list; list; list = list->next) {
							dbo = GDA_META_DB_OBJECT (list->data);
							g_value_set_string ((catalog = gda_value_new (G_TYPE_STRING)), 
									    dbo->obj_catalog);
							g_value_set_string ((schema = gda_value_new (G_TYPE_STRING)), 
									    dbo->obj_schema);
							g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), 
									    dbo->obj_name);
							gda_meta_struct_complement (mstruct, store, dbo->obj_type,
										    catalog, schema, name, NULL);
							gda_value_free (catalog);
							gda_value_free (schema);
							gda_value_free (name);
						}
						g_slist_free (dep_list);
					}
				}
			}
		}
	}

	if (!mstruct->db_objects) {
		g_set_error (error, 0, 0,
			     _("No object found"));
		goto onerror;
	}
	gda_meta_struct_sort_db_objects (mstruct, GDA_META_SORT_ALHAPETICAL, NULL);
	return mstruct;

 onerror:
	g_object_unref (mstruct);
	return NULL;
}

GdaInternalCommandResult *
gda_internal_command_detail (GdaConnection *cnc, const gchar **args,
			     GError **error, gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (!args[0] || !*args[0]) {
		/* displays all tables, views, indexes and sequences which are "directly visible" */
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner FROM _tables WHERE table_short_name = table_name "
			"ORDER BY table_schema, table_name";
		/* FIXME: include indexes and sequences when they are present in the information schema */
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, NULL);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}

	GdaMetaStruct *mstruct;
	GSList *dbo_list;
	mstruct = gda_internal_command_build_meta_struct (cnc, args, error);
	if (!mstruct)
		return NULL;

	/* compute the number of known database objects */
	gint nb_objects = 0;
	for (dbo_list = mstruct->db_objects; dbo_list; dbo_list = dbo_list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
		if (dbo->obj_type != GDA_META_DB_UNKNOWN)
			nb_objects++;
	}

	/* if more than one object, then show a list */
	if (nb_objects > 1) {
		model = gda_data_model_array_new (4);
		gda_data_model_set_column_title (model, 0, _("Schema"));
		gda_data_model_set_column_title (model, 1, _("Name"));
		gda_data_model_set_column_title (model, 2, _("Type"));
		gda_data_model_set_column_title (model, 3, _("Owner"));
		for (dbo_list = mstruct->db_objects; dbo_list; dbo_list = dbo_list->next) {
			GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
			GList *values = NULL;
			GValue *val;
			
			if (dbo->obj_type == GDA_META_DB_UNKNOWN)
				continue;

			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
			values = g_list_append (values, val);

			val = gda_value_new (G_TYPE_STRING);
			switch (dbo->obj_type) {
			case GDA_META_DB_TABLE:
				g_value_set_string (val, "BASE TABLE");
				break;
			case GDA_META_DB_VIEW:
				g_value_set_string (val, "VIEW");
				break;
			default:
				g_value_set_string (val, "???");
				TO_IMPLEMENT;
			}
			values = g_list_append (values, val);
			if (dbo->obj_owner)
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), dbo->obj_owner);
			else
				g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), "");
			values = g_list_append (values, val);
			gda_data_model_append_values (model, values, NULL);
			g_list_foreach (values, (GFunc) gda_value_free, NULL);
			g_list_free (values);
		}
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
	else if (nb_objects == 0) {
		g_set_error (error, 0, 0, _("No object found"));
		return NULL;
	}

	/* 
	 * Information about a single object
	 */
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_MULTIPLE;
	res->u.multiple_results = NULL;
	GdaMetaDbObject *dbo;

	for (dbo_list = mstruct->db_objects; dbo_list; dbo_list = dbo_list->next) {
		dbo = GDA_META_DB_OBJECT (dbo_list->data);
		if (dbo->obj_type == GDA_META_DB_UNKNOWN)
			dbo = NULL;
		else
			break;
	}
	g_assert (dbo);

	if ((dbo->obj_type == GDA_META_DB_VIEW) || (dbo->obj_type == GDA_META_DB_TABLE)) {
		GdaInternalCommandResult *subres;
		GdaMetaTable *mt = GDA_META_DB_OBJECT_GET_TABLE (dbo);
		GSList *list;

		model = gda_data_model_array_new (4);
		gda_data_model_set_column_title (model, 0, _("Column"));
		gda_data_model_set_column_title (model, 1, _("Type"));
		gda_data_model_set_column_title (model, 2, _("Nullable"));
		gda_data_model_set_column_title (model, 3, _("Default"));
		if (dbo->obj_type == GDA_META_DB_VIEW)
			g_object_set_data_full (G_OBJECT (model), "name", 
						g_strdup_printf (_("List of columns for view '%s'"), 
								 dbo->obj_short_name), 	g_free);
		else
			g_object_set_data_full (G_OBJECT (model), "name", 
						g_strdup_printf (_("List of columns for table '%s'"), 
								 dbo->obj_short_name), g_free);
		for (list = mt->columns; list; list = list->next) {
			GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
			GList *values = NULL;
			GValue *val;
			
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_name);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_type);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->nullok ? _("yes") : _("no"));
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->default_value);
			values = g_list_append (values, val);
			gda_data_model_append_values (model, values, NULL);
			g_list_foreach (values, (GFunc) gda_value_free, NULL);
			g_list_free (values);
		}
		
		subres = g_new0 (GdaInternalCommandResult, 1);
		subres->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		subres->u.model = model;
		res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
		
		if (dbo->obj_type == GDA_META_DB_VIEW) {
			/* VIEW specific */
			GdaMetaView *mv = GDA_META_DB_OBJECT_GET_VIEW (dbo);
			
			subres = g_new0 (GdaInternalCommandResult, 1);
			subres->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
			subres->u.txt = g_string_new ("");
			g_string_append_printf (subres->u.txt, _("View definition: %s"), mv->view_def);
			res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
		}
		else {
			/* TABLE specific */
			GValue *catalog, *schema, *name;
			gint i, nrows;
			const gchar *sql = "SELECT constraint_type, constraint_name  "
				"FROM _table_constraints WHERE table_catalog = ##tc::string "
				"AND table_schema = ##ts::string AND table_name = ##tname::string "
				"ORDER BY constraint_type, constraint_name";

			g_value_set_string ((catalog = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
			g_value_set_string ((schema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
			g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
			model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, 
							"tc", catalog, "ts", schema, "tname", name, NULL);
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				GString *string = NULL;
				const GValue *cvalue;
				const gchar *str;

				cvalue = gda_data_model_get_value_at (model, 0, i);
				str = g_value_get_string (cvalue);
				if (*str == 'P') {
					/* primary key */
					GdaDataModel *cols;
					cvalue = gda_data_model_get_value_at (model, 1, i);
					string = g_string_new ("Primary key ");
					g_string_append_printf (string, "'%s'", g_value_get_string (cvalue));
					str = "SELECT column_name, ordinal_position "
						"FROM _key_column_usage WHERE table_catalog = ##tc::string "
						"AND table_schema = ##ts::string AND table_name = ##tname::string AND "
						"constraint_name = ##cname::string "
						"ORDER BY ordinal_position";
					
					cols = gda_meta_store_extract (gda_connection_get_meta_store (cnc), str, error, 
								       "tc", catalog, "ts", schema, "tname", name, "cname", cvalue, 
								       NULL);
					if (cols) {
						gint j, cnrows;
						cnrows = gda_data_model_get_n_rows (cols);
						for (j = 0; j < cnrows; j++) {
							if (j == 0)
								g_string_append (string, ": ");
							else
								g_string_append (string, ", ");
							g_string_append (string, 
									 g_value_get_string (gda_data_model_get_value_at (cols, 0, j)));
						}
						g_object_unref (cols);
					}
				}
				else if (*str == 'F') {
					/* foreign key */
					cvalue = gda_data_model_get_value_at (model, 1, i);
					string = g_string_new ("Foreign key ");
					g_string_append_printf (string, "'%s'", g_value_get_string (cvalue));
				}
				else if (*str == 'U') {
					/* Unique constraint */
					cvalue = gda_data_model_get_value_at (model, 1, i);
					string = g_string_new ("Unique ");
					g_string_append_printf (string, "'%s'", g_value_get_string (cvalue));
				}

				if (string) {
					subres = g_new0 (GdaInternalCommandResult, 1);
					subres->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
					subres->u.txt = string;
					res->u.multiple_results = g_slist_append (res->u.multiple_results, subres);
				}
			}

			gda_value_free (catalog);
			gda_value_free (schema);
			gda_value_free (name);

			g_object_unref (model);
			return res;
		}
	}
	else 
		TO_IMPLEMENT;

	g_object_unref (mstruct);
	return res;
}
