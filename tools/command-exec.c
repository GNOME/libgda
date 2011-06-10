/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef GDA_DISABLE_DEPRECATED
#include "command-exec.h"
#include <glib/gi18n-lib.h>
#include <string.h>
#include "tools-input.h"
#ifdef HAVE_HISTORY
#include <readline/history.h>
#endif
#include <sql-parser/gda-statement-struct-util.h>
#include "tools-utils.h"

/*
 *  gda_internal_command_arg_remove_quotes
 *
 * If @str has simple or double quotes, remove those quotes, otherwise does nothing.
 *
 * Returns: @str
 */
gchar *
gda_internal_command_arg_remove_quotes (gchar *str)
{
	glong total;
        gchar *ptr;
        glong offset = 0;
        char delim;

        if (!str)
                return NULL;
        delim = *str;
        if ((delim != '\'') && (delim != '"'))
                return str;


        total = strlen (str);
        if (str[total-1] == delim) {
                /* string is correctly terminated by a double quote */
                g_memmove (str, str+1, total-2);
                total -=2;
        }
        else {
                /* string is _not_ correctly terminated by a double quote */
                g_memmove (str, str+1, total-1);
                total -=1;
        }
        str[total] = 0;

        ptr = (gchar *) str;
        while (offset < total) {
                /* we accept the "''" as a synonym of "\'" */
                if (*ptr == delim) {
                        if (*(ptr+1) == delim) {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                *str = 0;
                                return str;
                        }
                }
                if (*ptr == '\\') {
                        if (*(ptr+1) == '\\') {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                if (*(ptr+1) == delim) {
                                        *ptr = delim;
                                        g_memmove (ptr+1, ptr+2, total - offset);
                                        offset += 2;
                                }
                                else {
                                        *str = 0;
                                        return str;
                                }
			}
                }
                else
                        offset ++;

                ptr++;
        }

        return str;
}

/*
 * gda_internal_command_exec_result_free
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
		if (! res->was_in_transaction_before_exec &&
		    res->cnc &&
		    gda_connection_get_transaction_status (res->cnc))
			gda_connection_rollback_transaction (res->cnc, NULL, NULL);
		    
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
	if (res->cnc)
		g_object_unref (res->cnc);
	g_free (res);
}

/*
 * Small tokenizer
 */
#define T_ILLEGAL                         1
#define T_SPACE                           2
#define T_TEXTUAL                         3
#define T_WORD                            4
#define T_STRING                          5
static int
getToken (const char *z, gint *tok_type)
{
	*tok_type = T_SPACE;
	int i, c;

	switch (*z) {
	case ' ': case '\t': case '\n': case '\f': case '\r': {
		for (i=1; g_ascii_isspace (z[i]); i++){}
		*tok_type = T_SPACE;
		return i;
	}
	case '\'':
        case '"': {
                char delim = z[0];
                for (i = 1; (c = z[i]) != 0; i++) {
                        if (c == delim) {
                                if (z[i+1] == delim)
                                        i++;
                                else
                                        break;
                        }
                        else if (c == '\\') {
                                if (z[i+1] == delim)
                                        i++;
                        }
                }
                if (c) {
                        if (delim == '"')
                                *tok_type = T_TEXTUAL;
                        else
                                *tok_type = T_STRING;
			return i+1;
                }
                else {
                        *tok_type = T_ILLEGAL;
                        return 0;
                }
                break;
        }
	default: 
		for (i=0; z[i] && !g_ascii_isspace (z[i]); i++){}
		*tok_type = T_WORD;
		return i;
	}
}

/* default function to split arguments */
gchar **
default_gda_internal_commandargs_func (const gchar *string)
{
	gchar **array = NULL;
	GSList *parsed_list = NULL;
	gchar *ptr;
	gint nparsed, token_type;

	ptr = (gchar *) string; 
	nparsed = getToken (ptr, &token_type);
	while (nparsed > 0) {
		gint local_nparsed = nparsed;
		gchar *local_ptr = ptr;
		switch (token_type) {
		case T_SPACE:
		case T_ILLEGAL:
			break;
		case T_TEXTUAL:
		case T_STRING:
		default: {
			gchar hold;
			hold = local_ptr[local_nparsed];
			local_ptr[local_nparsed] = 0;
			parsed_list = g_slist_append (parsed_list, g_strdup (local_ptr));
			local_ptr[local_nparsed] = hold;
			break;
		}
		}
		ptr += nparsed;
		nparsed = getToken (ptr, &token_type);
	}
	
	if (parsed_list) {
		GSList *list;
		gint i;
		array = g_new0 (gchar*, g_slist_length (parsed_list) + 1);
		for (i = 0, list = parsed_list; list; i++, list = list->next) {
			array [i] = (gchar *) list->data;
			/*g_print ("array [%d] = %s\n", i, array [i]);*/
		}
		g_slist_free (parsed_list);
	}

	return array;
}

GdaInternalCommandResult *
gda_internal_command_help (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			   G_GNUC_UNUSED const gchar **args, G_GNUC_UNUSED GError **error,
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

		if (console && command->limit_to_main)
			continue;

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
gda_internal_command_history (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			      GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;

	if (console) {
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		TO_IMPLEMENT;
		return res;
	}

	GString *string;
#ifdef HAVE_HISTORY
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
gda_internal_command_dict_sync (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
				GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
		return NULL;
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	if (args[0] && *args[0]) {
		GdaMetaContext context;
		memset (&context, 0, sizeof (context));
		if (*args[0] == '_')
			context.table_name = (gchar*) args[0];
		else
			context.table_name = g_strdup_printf ("_%s", args[0]);
		if (!gda_connection_update_meta_store (cnc, &context, error)) {
			g_free (res);
			res = NULL;
		}
		if (*args[0] != '_')
			g_free (context.table_name);
	}
	else if (!gda_connection_update_meta_store (cnc, NULL, error)) {
		g_free (res);
		res = NULL;
	}

	return res;
}

GdaInternalCommandResult *
gda_internal_command_list_tables (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
				  GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_short_name=##tname::string AND "
			"table_type LIKE '%TABLE%' "
			"ORDER BY table_schema, table_name";

		gchar *tmp = gda_sql_identifier_prepare_for_compare (g_strdup (args[0]));
		g_value_take_string (v = gda_value_new (G_TYPE_STRING), tmp);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type LIKE '%TABLE%' "
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
gda_internal_command_list_views (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
				 GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		GValue *v;
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_short_name=##tname::string AND "
			"table_type = 'VIEW' "
			"ORDER BY table_schema, table_name";

		g_value_set_string (v = gda_value_new (G_TYPE_STRING), args[0]);
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, "tname", v, NULL);
		gda_value_free (v);
	}
	else {
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner, table_comments as Description "
			"FROM _tables WHERE table_type='VIEW' "
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
gda_internal_command_list_schemas (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
				   GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
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

GdaMetaStruct *
gda_internal_command_build_meta_struct (GdaConnection *cnc, const gchar **args, GError **error)
{
	GdaMetaStruct *mstruct;
	gint index;
	const gchar *arg;
	GdaMetaStore *store;
	GSList *objlist;

	store = gda_connection_get_meta_store (cnc);
	mstruct = gda_meta_struct_new (store, GDA_META_STRUCT_FEATURE_ALL);

	if (!args[0]) {
		GSList *list;
		/* use all tables or views visible by default */
		if (!gda_meta_struct_complement_default (mstruct, error))
			goto onerror;
		list = gda_meta_struct_get_all_db_objects (mstruct);
		if (!list) {
			/* use all tables or views visible or not by default */
			if (!gda_meta_struct_complement_all (mstruct, error))
				goto onerror;
		}
		else
			g_slist_free (list);
	}

	for (index = 0, arg = args[0]; arg; index++, arg = args[index]) {
		GValue *v;
		g_value_set_string (v = gda_value_new (G_TYPE_STRING), arg);

		/* see if we have the form <schema_name>.*, to list all the objects in a given schema */
		if (g_str_has_suffix (arg, ".*") && (*arg != '.')) {
			gchar *str;

			str = g_strdup (arg);
			str[strlen (str) - 2] = 0;
			g_value_take_string (v, str);

			if (!gda_meta_struct_complement_schema (mstruct, NULL, v, error))
				goto onerror;
		}
		else {
			/* try to find it as a table or view */
			if (g_str_has_suffix (arg, "=") && (*arg != '=')) {
				GdaMetaDbObject *dbo;
				gchar *str;
				str = g_strdup (arg);
				str[strlen (str) - 1] = 0;
				g_value_take_string (v, str);
				dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, 
								  NULL, NULL, v, error);
				if (dbo)
					gda_meta_struct_complement_depend (mstruct, dbo, error);
				else
					goto onerror;
			}
			else if (!gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, NULL, NULL, v, error))
				goto onerror;
		}
	}

	objlist = gda_meta_struct_get_all_db_objects (mstruct);
	if (!objlist) {
		g_set_error (error, 0, 0, "%s", 
			     _("No object found"));
		goto onerror;
	}
	g_slist_free (objlist);
	gda_meta_struct_sort_db_objects (mstruct, GDA_META_SORT_ALHAPETICAL, NULL);
	return mstruct;

 onerror:
	g_object_unref (mstruct);
	return NULL;
}

static void
meta_table_column_foreach_attribute_func (const gchar *att_name, const GValue *value, GString **string)
{
	if (!strcmp (att_name, GDA_ATTRIBUTE_AUTO_INCREMENT) && 
	    (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN) && 
	    g_value_get_boolean (value)) {
		if (*string) {
			g_string_append (*string, ", ");
			g_string_append (*string, _("Auto increment"));
		}
		else
			*string = g_string_new (_("Auto increment"));
	}
}

GdaInternalCommandResult *
gda_internal_command_detail (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
			     GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
		return NULL;
	}

	if (!args[0] || !*args[0]) {
		/* FIXME: include indexes and sequences when they are present in the information schema */
		/* displays all tables, views, indexes and sequences which are "directly visible" */
		const gchar *sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
			"table_owner as Owner FROM _tables WHERE table_short_name = table_name "
			"ORDER BY table_schema, table_name";
		model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, NULL);

		if (model) {
			/* if no row, then return all the objects from all the schemas */
			if (gda_data_model_get_n_rows (model) == 0) {
				g_object_unref (model);
				sql = "SELECT table_schema AS Schema, table_name AS Name, table_type as Type, "
					"table_owner as Owner FROM _tables "
					"ORDER BY table_schema, table_name";
				model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, NULL);
			}
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
		}
		else {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		return res;
	}

	GdaMetaStruct *mstruct;
	GSList *dbo_list, *tmplist;
	mstruct = gda_internal_command_build_meta_struct (cnc, args, error);
	if (!mstruct)
		return NULL;
	
	/* compute the number of known database objects */
	gint nb_objects = 0;
	tmplist = gda_meta_struct_get_all_db_objects (mstruct);
	for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
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
		for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
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
				g_value_set_string (val, "TABLE");
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
		g_slist_free (tmplist);
		return res;
	}
	else if (nb_objects == 0) {
		g_set_error (error, 0, 0, "%s", _("No object found"));
		g_slist_free (tmplist);
		return NULL;
	}

	/* 
	 * Information about a single object
	 */
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_MULTIPLE;
	res->u.multiple_results = NULL;
	GdaMetaDbObject *dbo;

	for (dbo_list = tmplist; dbo_list; dbo_list = dbo_list->next) {
		dbo = GDA_META_DB_OBJECT (dbo_list->data);
		if (dbo->obj_type == GDA_META_DB_UNKNOWN)
			dbo = NULL;
		else
			break;
	}
	g_assert (dbo);
	g_slist_free (tmplist);

	if ((dbo->obj_type == GDA_META_DB_VIEW) || (dbo->obj_type == GDA_META_DB_TABLE)) {
		GdaInternalCommandResult *subres;
		GdaMetaTable *mt = GDA_META_TABLE (dbo);
		GSList *list;

		model = gda_data_model_array_new (5);
		gda_data_model_set_column_title (model, 0, _("Column"));
		gda_data_model_set_column_title (model, 1, _("Type"));
		gda_data_model_set_column_title (model, 2, _("Nullable"));
		gda_data_model_set_column_title (model, 3, _("Default"));
		gda_data_model_set_column_title (model, 4, _("Extra"));
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
			GString *string = NULL;

			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_name);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->column_type);
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->nullok ? _("yes") : _("no"));
			values = g_list_append (values, val);
			g_value_set_string ((val = gda_value_new (G_TYPE_STRING)), tcol->default_value);
			values = g_list_append (values, val);

			gda_meta_table_column_foreach_attribute (tcol, 
					      (GdaAttributesManagerFunc) meta_table_column_foreach_attribute_func, &string);
			if (string) {
				g_value_take_string ((val = gda_value_new (G_TYPE_STRING)), string->str);
				g_string_free (string, FALSE);
			}
			else
				val = gda_value_new_null ();
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
			GdaMetaView *mv = GDA_META_VIEW (dbo);
			
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
				"AND constraint_type != 'FOREIGN KEY' "
				"ORDER BY constraint_type DESC, constraint_name";

			g_value_set_string ((catalog = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
			g_value_set_string ((schema = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
			g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
			model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), sql, error, 
							"tc", catalog, "ts", schema, "tname", name, NULL);
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				GString *string = NULL;
				const GValue *cvalue;
				const gchar *str = NULL;

				cvalue = gda_data_model_get_value_at (model, 0, i, error);
				if (!cvalue) {
					gda_internal_command_exec_result_free (res);
					res = NULL;
					goto out;
				}
				str = g_value_get_string (cvalue);
				if (*str == 'P') {
					/* primary key */
					GdaDataModel *cols;
					cvalue = gda_data_model_get_value_at (model, 1, i, error);
					if (!cvalue) {
						gda_internal_command_exec_result_free (res);
						res = NULL;
						goto out;
					}
					string = g_string_new (_("Primary key"));
					g_string_append_printf (string, " '%s'", g_value_get_string (cvalue));
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
						if (cnrows > 0) {
							g_string_append (string, " (");
							for (j = 0; j < cnrows; j++) {
								if (j > 0)
									g_string_append (string, ", ");
								cvalue = gda_data_model_get_value_at (cols, 0, j, error);
								if (!cvalue) {
									gda_internal_command_exec_result_free (res);
									res = NULL;
									g_object_unref (cols);
									g_string_free (string, TRUE);
									goto out;
								}
								g_string_append (string, g_value_get_string (cvalue));
							}
							g_string_append_c (string, ')');
						}
						g_object_unref (cols);
					}
				}
				else if (*str == 'F') {
					/* foreign key, not handled here */
				}
				else if (*str == 'U') {
					/* Unique constraint */
					GdaDataModel *cols;
					cvalue = gda_data_model_get_value_at (model, 1, i, error);
					if (!cvalue) {
						gda_internal_command_exec_result_free (res);
						res = NULL;
						goto out;
					}
					string = g_string_new (_("Unique"));
					g_string_append_printf (string, " '%s'", g_value_get_string (cvalue));
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
						if (cnrows > 0) {
							g_string_append (string, " (");
							for (j = 0; j < cnrows; j++) {
								if (j > 0)
									g_string_append (string, ", ");
								cvalue = gda_data_model_get_value_at (cols, 0, j, error);
								if (!cvalue) {
									gda_internal_command_exec_result_free (res);
									res = NULL;
									g_object_unref (cols);
									g_string_free (string, TRUE);
									goto out;
								}
								g_string_append (string, g_value_get_string (cvalue));
							}
							g_string_append_c (string, ')');
						}
						g_object_unref (cols);
					}
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

			/* foreign key constraints */
			GSList *list;
			for (list = mt->fk_list; list; list = list->next) {
				GString *string;
				GdaMetaTableForeignKey *fk;
				gint i;
				fk = (GdaMetaTableForeignKey*) list->data;
				if (GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED (fk))
					string = g_string_new (_("Declared foreign key"));
				else
					string = g_string_new (_("Foreign key"));
				g_string_append_printf (string, " '%s' (", fk->fk_name);
				for (i = 0; i < fk->cols_nb; i++) {
					if (i != 0)
						g_string_append (string, ", ");
					g_string_append (string, fk->fk_names_array [i]);
				}
				g_string_append (string, ") ");
				if (fk->depend_on->obj_short_name)
					/* To translators: the term "references" is the verb
					 * "to reference" in the context of foreign keys where
					 * "table A REFERENCES table B" */
					g_string_append_printf (string, _("references %s"),
								fk->depend_on->obj_short_name);
				else
					/* To translators: the term "references" is the verb
					 * "to reference" in the context of foreign keys where
					 * "table A REFERENCES table B" */
					g_string_append_printf (string, _("references %s.%s"),
								fk->depend_on->obj_schema,
								fk->depend_on->obj_name);
				g_string_append (string, " (");
				for (i = 0; i < fk->cols_nb; i++) {
					if (i != 0)
						g_string_append (string, ", ");
					g_string_append (string, fk->ref_pk_names_array [i]);
				}
				g_string_append (string, ")");

				g_string_append (string, "\n  ");
				g_string_append (string, _("Policy on UPDATE"));
				g_string_append (string, ": ");
				g_string_append (string, tools_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY (fk)));
				g_string_append (string, "\n  ");
				g_string_append (string, _("Policy on DELETE"));
				g_string_append (string, ": ");
				g_string_append (string, tools_utils_fk_policy_to_string (GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY (fk)));

				subres = g_new0 (GdaInternalCommandResult, 1);
				subres->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
				subres->u.txt = string;
				res->u.multiple_results = g_slist_append (res->u.multiple_results,
									  subres);
			}

			return res;
		}
	}
	else 
		TO_IMPLEMENT;

 out:
	g_object_unref (mstruct);
	return res;
}
