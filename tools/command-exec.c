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
			"table_type LIKE '%TABLE%' AND table_short_name = table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type LIKE '%TABLE%' AND table_short_name = table_name";
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
			"table_type = 'VIEW' AND table_short_name = table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type='VIEW' AND table_short_name = table_name";
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

GdaInternalCommandResult *
gda_internal_command_detail (GdaConnection *cnc, const gchar **args,
			     GError **error, gpointer data)
{
	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	if (!args[0]) {
		TO_IMPLEMENT;
		return NULL;
	}

	TO_IMPLEMENT;
#ifdef OLD_CODE
	GdaDictTable *table;
	GdaDictDatabase *db = gda_dict_get_database (dict);
	g_assert (db);
	table = gda_dict_database_get_table_by_name (db, args[0]);
	if (table) {
		GdaInternalCommandResult *global_res, *res;

		global_res = g_new0 (GdaInternalCommandResult, 1);
		global_res->type = GDA_INTERNAL_COMMAND_RESULT_MULTIPLE;
		global_res->u.multiple_results = NULL;

		/*
		 * First part: description of the table
		 */
		GdaDataModel *model;
		GSList *fields, *list;		
		gchar *str;
		model = gda_data_model_array_new_with_g_types (4,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Column"));
		gda_data_model_set_column_title (model, 1, _("Type"));
		gda_data_model_set_column_title (model, 2, _("Modifiers"));
		gda_data_model_set_column_title (model, 3, _("Description"));
		str = g_strdup_printf (_("Description of table '%s'"), args[0]);
		gda_object_set_name (GDA_OBJECT (model), str);
		g_free (str);

		fields = gda_entity_get_fields (GDA_ENTITY (table));
		for (list = fields; list; list = list->next) {
			gint row;
			const gchar *cstr;
			GString *string;
			GValue *value;
			row = gda_data_model_append_row (model, NULL);
			
			/* column */
			cstr = gda_object_get_name (GDA_OBJECT (list->data));
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);

			/* type */
			GdaDictType *type;
			GType gtype;
			type = gda_entity_field_get_dict_type (GDA_ENTITY_FIELD (list->data));
			gtype = gda_entity_field_get_g_type (GDA_ENTITY_FIELD (list->data));
			if (type) {
				gint len = gda_dict_field_get_length (GDA_DICT_FIELD (list->data));
				if (len >= 0) {
				    if (gtype == G_TYPE_STRING)
					    str = g_strdup_printf ("%s (%d)", gda_dict_type_get_sqlname (type), len);
				    else if (gtype == GDA_TYPE_NUMERIC) 
					    str = g_strdup_printf ("%s (%d,%d)", 
								   gda_dict_type_get_sqlname (type), len,
								   gda_dict_field_get_scale (GDA_DICT_FIELD (list->data)));
				    else
					    str = g_strdup (gda_dict_type_get_sqlname (type)); 
				} else
					str = g_strdup (gda_dict_type_get_sqlname (type));
			}
			else
				str = g_strdup_printf ("(%s)", g_type_name (gtype));
			value = gda_value_new_from_string (str, G_TYPE_STRING);
			/*g_print ("%s => %s (%s)\n", gda_object_get_name (GDA_OBJECT (list->data)),
			  str, g_type_name (gtype));*/
			g_free (str);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			/* modifiers */
			gboolean start = TRUE;
			string = g_string_new ("");
#ifdef BE_CORRECT
			gint attrs = gda_dict_field_get_attributes (GDA_DICT_FIELD (list->data));
			if (attrs & FIELD_AUTO_INCREMENT) {
				g_string_append (string, "AUTO INCREMENT");
				start = FALSE;
			}
#else
			if (g_object_get_data (G_OBJECT (list->data), "raw_extra_attributes")) {
				g_string_append (string, 
						 (gchar *) g_object_get_data (G_OBJECT (list->data), "raw_extra_attributes"));
				start = FALSE;
			}
#endif
			if (! gda_dict_field_is_null_allowed (GDA_DICT_FIELD (list->data))) {
				if (!start) 
					g_string_append (string, ", ");
				else
					start = FALSE;
				g_string_append (string, "NOT NULL");
			}
			const GValue *def_val;
			def_val = gda_dict_field_get_default_value (GDA_DICT_FIELD (list->data));
			if (def_val && !gda_value_is_null (def_val)) {
				if (!start) 
					g_string_append (string, ", ");
				else
					start = FALSE;
				str = gda_value_stringify (def_val);
				g_string_append_printf (string, "DEFAULT %s", str);
				g_free (str);
			}
			value = gda_value_new_from_string (string->str, G_TYPE_STRING);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
			g_string_free (string, TRUE);
				
			/* description */
			cstr = gda_object_get_description (GDA_OBJECT (list->data));
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}
		g_slist_free (fields);

		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		global_res->u.multiple_results = g_slist_append (global_res->u.multiple_results, res);

		/*
		 * Second part: description of the constraints
		 */
		{
			GString *string = NULL;
			GSList *constraints, *list;

			constraints = gda_dict_table_get_constraints (table);
			for (list = constraints; list; list = list->next) {
				GdaDictConstraint *ct = GDA_DICT_CONSTRAINT (list->data);
				GSList *fields_list, *fl;

				if (gda_dict_constraint_get_constraint_type (ct) == CONSTRAINT_NOT_NULL)
					continue;

				if (!string)
					string = g_string_new ("");
				else
					g_string_append_c (string, '\n');
				g_string_append (string, _("Constraint"));
				if (gda_object_get_name (GDA_OBJECT (ct)))
					g_string_append_printf (string, " %s:\n", gda_object_get_name (GDA_OBJECT (ct)));
				else
					g_string_append (string, ":\n");
				switch (gda_dict_constraint_get_constraint_type (ct)) {
				case CONSTRAINT_PRIMARY_KEY:
					g_string_append (string, " PRIMARY KEY (");
					fields_list = gda_dict_constraint_pkey_get_fields (ct);
					for (fl = fields_list; fl; fl = fl->next) {
						if (fl != fields_list)
							g_string_append (string, ", ");
						g_string_append (string, gda_object_get_name (GDA_OBJECT (fl->data)));
					}
					g_string_append_c (string, ')');
					g_slist_free (fields_list);
					break;
				case CONSTRAINT_FOREIGN_KEY: {
					GdaDictTable *ref_table;
					ref_table = gda_dict_constraint_fkey_get_ref_table (ct);
					g_string_append (string, " FOREIGN KEY (");
					fields_list = gda_dict_constraint_fkey_get_fields (ct);
					for (fl = fields_list; fl; fl = fl->next) {
						GdaDictConstraintFkeyPair *pair = (GdaDictConstraintFkeyPair*) fl->data;
						if (fl != fields_list)
							g_string_append (string, ", ");
						g_string_append (string, gda_object_get_name (GDA_OBJECT (pair->fkey)));
					}
					g_string_append_c (string, ')');

					g_string_append (string, " REFERENCES ");
					g_string_append_printf (string, "%s (", 
								gda_object_get_name (GDA_OBJECT (ref_table)));
					for (fl = fields_list; fl; fl = fl->next) {
						GdaDictConstraintFkeyPair *pair = (GdaDictConstraintFkeyPair*) fl->data;
						if (fl != fields_list)
							g_string_append (string, ", ");
						g_string_append (string, gda_object_get_name (GDA_OBJECT (pair->ref_pkey)));
					}
					g_string_append_c (string, ')');
					g_slist_free (fields_list);
					/* FIXME: need the actions */
					break;
				}
				case CONSTRAINT_UNIQUE:
					g_string_append (string, " UNIQUE (");
					fields_list = gda_dict_constraint_unique_get_fields (ct);
					for (fl = fields_list; fl; fl = fl->next) {
						if (fl != fields_list)
							g_string_append (string, ", ");
						g_string_append (string, gda_object_get_name (GDA_OBJECT (fl->data)));
					}
					g_string_append_c (string, ')');
					g_slist_free (fields_list);
					break;
				case CONSTRAINT_NOT_NULL: 
					/* Dont' display anything here */
					break;
				case CONSTRAINT_CHECK_EXPR:
					break;
				case CONSTRAINT_CHECK_IN_LIST:
					break;
				case CONSTRAINT_CHECK_SETOF_LIST:
					break;
				default:
					break;
				}
			}
			g_slist_free (constraints);

			if (!string && ! gda_dict_table_is_view (table))
				string = g_string_new (_("No constraint for this table."));
			if (string) {
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
				res->u.txt = string;
				global_res->u.multiple_results = g_slist_append (global_res->u.multiple_results, res);
			}
		}

		/*
		 * 3rd part: SQL definition
		 */
		{
			GString *string = NULL;
			if (gda_dict_table_is_view (table)) {
				GdaDataModel *schema;
				GdaParameterList *plist;
				plist = gda_parameter_list_new_inline (dict, "name", G_TYPE_STRING, 
								       gda_object_get_name (GDA_OBJECT (table)), NULL);
				schema = gda_connection_get_schema (cnc, GDA_CONNECTION_SCHEMA_VIEWS, plist, NULL);
				g_object_unref (plist);
				if (schema) {
					const GValue *cvalue;
					cvalue = gda_data_model_get_value_at (schema, 3, 0);
					if (cvalue && !gda_value_is_null (cvalue) && 
					    g_value_get_string (cvalue) && *g_value_get_string (cvalue)) {
						string = g_string_new (_("View definition:"));
						g_string_append (string, "\n ");
						g_string_append (string, g_value_get_string (cvalue));
					}
					g_object_unref (schema);
				}
			}
			if (string) {
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
				res->u.txt = string;
				global_res->u.multiple_results = g_slist_append (global_res->u.multiple_results, res);
			}
		}
		

		return global_res;
	}
#endif

	g_set_error (error, 0, 0,
		     _("No object named '%s' found"), args[0]);
	return NULL;
}
