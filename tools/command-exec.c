/* 
 * Copyright (C) 2007 The GNOME Foundation.
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
	case GDA_INTERNAL_COMMAND_RESULT_PLIST:
		if (res->u.plist)
			g_object_unref (res->u.plist);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_TXT:
	case GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT:
		if (res->u.txt)
			g_string_free (res->u.txt, TRUE);
		break;
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

	if (!command_str || (*command_str != '\\'))
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
			      GdaConnection *cnc, GdaDict *dict, const gchar *command_str, GError **error)
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
	res = command->command_func (cnc, dict, (const gchar **) &(args[1]), 
				     error, command->user_data);
	
 cleanup:
	if (args)
		g_strfreev (args);

	return res;
}

GdaInternalCommandResult *
gda_internal_command_help (GdaConnection *cnc, GdaDict *dict, const gchar **args, 
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
gda_internal_command_history (GdaConnection *cnc, GdaDict *dict, const gchar **args, GError **error, gpointer data)
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
gda_internal_command_dict_sync (GdaConnection *cnc, GdaDict *dict, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	if (!gda_dict_update_dbms_meta_data (dict, 0, NULL, error)) {
		g_free (res);
		res = NULL;
	}

	return res;
}

GdaInternalCommandResult *
gda_internal_command_dict_save (GdaConnection *cnc, GdaDict *dict, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	if (!gda_dict_save (dict, error)) {
		g_free (res);
		res = NULL;
	}

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_tables_views (GdaConnection *cnc, GdaDict *dict, const gchar **args, GError **error, gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDictDatabase* db = gda_dict_get_database (dict);

	GSList *tables, *list;
	GdaDataModel *model;
	GValue *value;
	gint row;
	const gchar *tname = NULL;

	if (args[0] && *args[0]) 
		tname = args[0];
	
	model = gda_data_model_array_new_with_g_types (4,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Type"));
	gda_data_model_set_column_title (model, 2, _("Owner"));
	gda_data_model_set_column_title (model, 3, _("Description"));
	tables = gda_dict_database_get_tables (db);
	for (list = tables; list; list = list->next) {
		const gchar *cstr;
		GdaDictTable *table = GDA_DICT_TABLE (list->data);
		
		cstr = gda_object_get_name (GDA_OBJECT (table));
		if (tname && strcmp (tname, cstr))
			continue;

		row = gda_data_model_append_row (model, NULL);
		value = gda_value_new_from_string (cstr, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);
		
		value = gda_value_new_from_string (gda_dict_table_is_view (table) ? _("View") : 
						   _("Table"), G_TYPE_STRING);
		gda_data_model_set_value_at (model, 1, row, value, NULL);
		gda_value_free (value);
		
		cstr = gda_object_get_owner (GDA_OBJECT (table));
		if (cstr)
			value = gda_value_new_from_string (cstr, G_TYPE_STRING);
		else
			value = gda_value_new_null ();
		gda_data_model_set_value_at (model, 2, row, value, NULL);
		gda_value_free (value);
		
		cstr = gda_object_get_description (GDA_OBJECT (table));
		if (cstr)
			value = gda_value_new_from_string (cstr, G_TYPE_STRING);
		else
			value = gda_value_new_null ();		
		gda_data_model_set_value_at (model, 3, row, value, NULL);
		gda_value_free (value);			
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_queries (GdaConnection *cnc, GdaDict *dict, 
				   const gchar **args,
				   GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	GSList *queries, *list;
	GdaDataModel *model;
	GValue *value;
	gint row;
	const gchar *qname = NULL;
	gboolean with_sql_def = FALSE;

	if (args[0] && *args[0]) {
		if (!strcmp (args[0], "+"))
			with_sql_def = TRUE;
		else {
			qname = args[0];
			if (args[1] && (*args[1] == '+'))
				with_sql_def = TRUE;
		}
	}

	if (qname) {
		GdaQuery *query = NULL;
		queries = gda_dict_get_queries (dict);
		for (list = queries; list; list = list->next) {
			const gchar *cstr;			
			cstr = gda_object_get_name (GDA_OBJECT (list->data));
			if (cstr && !strcmp (cstr, qname)) {
				query = GDA_QUERY (list->data);
				break;
			}
		}
		g_slist_free (queries);
		if (query) {
			gchar *str;
			GString *string = g_string_new ("");
			str = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL,
							  GDA_RENDERER_EXTRA_PRETTY_SQL | GDA_RENDERER_PARAMS_AS_DETAILED,
							  NULL);
			g_string_append_printf (string, _("%s\n"), str);
			g_free (str);
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
			res->u.txt = string;
		}
		else
			g_set_error (error, 0, 0,
				     _("Could not find query named '%s'"), qname);
	}
	else {
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
		queries = gda_dict_get_queries (dict);
		for (list = queries; list; list = list->next) {
			GdaQuery *query = GDA_QUERY (list->data);
			const gchar *cstr;
			row = gda_data_model_append_row (model, NULL);
			
			cstr = gda_object_get_name (GDA_OBJECT (query));
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);
			
			value = gda_value_new_from_string (gda_query_get_query_type_string (query), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);
			
			cstr = gda_object_get_description (GDA_OBJECT (query));
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);

			GdaParameterList *plist;
			plist = gda_query_get_parameter_list (query);
			if (plist && plist->parameters) {
				GSList *params;
				GString *string = g_string_new ("");
				/* fill parameters with some defined parameters */
				for (params = plist->parameters; params; params = params->next) {
					GdaParameter *param = GDA_PARAMETER (params->data);
					if (params != plist->parameters)
						g_string_append_c (string, '\n');
					g_string_append_printf (string, "%s (%s)", gda_object_get_name (GDA_OBJECT (param)),
								g_type_name (gda_parameter_get_g_type (param)));
				}
				value = gda_value_new_from_string (string->str, G_TYPE_STRING);
				g_string_free (string, TRUE);
			}
			else
				value = gda_value_new_from_string ("", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
			if (plist)
				g_object_unref (plist);
			if (with_sql_def) {
				gchar *str;
				str = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL,
								  GDA_RENDERER_EXTRA_PRETTY_SQL | GDA_RENDERER_PARAMS_AS_DETAILED,
								  NULL);
				value = gda_value_new_from_string (str, G_TYPE_STRING);
				gda_data_model_set_value_at (model, 4, row, value, NULL);
				gda_value_free (value);
			}
		}

		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}

	return res;
}
