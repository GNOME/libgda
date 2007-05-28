/* GDA - SQL console
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Vivien Malerba <malerba@gnome-db.org>
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

#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include "tools-input.h"
#include "command-exec.h"

/* options */
gchar *pass = NULL;
gchar *user = NULL;

gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

gchar *single_command = NULL;
gchar *commandsfile = NULL;

gboolean list_configs = FALSE;
gboolean list_providers = FALSE;

gchar *outfile = NULL;


static GOptionEntry entries[] = {
        { "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
        { "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
        { "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},
        { "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
        { "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },

        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { "command", 'C', 0, G_OPTION_ARG_STRING, &single_command, "Run only single command (SQL or internal) and exit", "command" },
        { "commands-file", 'f', 0, G_OPTION_ARG_STRING, &commandsfile, "Execute commands from file, then exit", "filename" },
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
        { NULL }
};

typedef struct {
	GdaClient *client;
	GdaConnection *cnc;
	GdaDict *dict;
	GdaInternalCommandsList *internal_commands;

	FILE *input_stream;
	FILE *output_stream;
} MainData;

static void     compute_prompt (MainData *data, GString *string, gboolean in_command);
static gboolean set_output_file (MainData *data, const gchar *file, GError **error);
static gboolean set_input_file (MainData *data, const gchar *file, GError **error);
static void     output_data_model (MainData *data, GdaDataModel *model);
static gboolean open_connection (MainData *data, const gchar *dsn,
				 const gchar *provider,  const gchar *direct, 
				 const gchar *user, const gchar *pass, GError **error);
static GdaDataModel *list_all_sections (MainData *data);
static GdaDataModel *list_all_providers (MainData *data);

/* commands manipulation */
static GdaInternalCommandsList  *build_internal_commands_list (MainData *data);
static gboolean                  command_is_complete (const gchar *command);
static GdaInternalCommandResult *command_execute (MainData *data, gchar *command, GError **error);


int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	MainData *data;
	int exit_status = EXIT_SUCCESS;
	GString *prompt = g_string_new ("");
	gchar *filename;

	context = g_option_context_new (_("Gda SQL console"));        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_warning ("Can't parse arguments: %s", error->message);
		exit_status = EXIT_FAILURE;
		goto cleanup;
        }
        g_option_context_free (context);
        gda_init ("Gda author dictionary file", PACKAGE_VERSION, argc, argv);
	data = g_new0 (MainData, 1);

	/* output file */
	if (outfile) {
		if (! set_output_file (data, outfile, &error)) {
			g_warning ("Can't set output file as '%s': %s", outfile,
				   error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* treat here lists of providers and defined DSN */
	if (list_providers) {
		GdaDataModel *model = list_all_providers (data);
		output_data_model (data, model);
		g_object_unref (model);
		goto cleanup;
	}
	if (list_configs) {
		GdaDataModel *model = list_all_sections (data);
		output_data_model (data, model);
		g_object_unref (model);
		goto cleanup;
	}

	/* commands file */
	if (commandsfile) {
		if (! set_input_file (data, commandsfile, &error)) {
			g_warning ("Can't read file '%s': %s", commandsfile,
				   error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* check connection parameters coherence */
	if (direct && dsn) {
                g_fprintf (stderr, _("DSN and connection string are exclusive\n"));
		exit_status = EXIT_FAILURE;
                goto cleanup;
        }

        if (!direct && !dsn) {
                g_fprintf (stderr, 
			   _("You must specify a connection to open either as a DSN or a connection string\n"));
                exit_status = EXIT_FAILURE;
                goto cleanup;
        }

        if (direct && !prov) {
                g_fprintf (stderr, _("You must specify a provider when using a connection string\n"));
                exit_status = EXIT_FAILURE;
                goto cleanup;
        }

	/* open connection */
	if (!open_connection (data, dsn, prov, direct, user, pass, &error)) {
		g_warning (_("Can't open connection: %s"), error && error->message ? error->message : _("No detail"));
		exit_status = EXIT_FAILURE;
                goto cleanup;
	}

	/* create dictionary */
	data->dict = gda_dict_new ();
	if (dsn) {
		filename = gda_dict_compute_xml_filename (data->dict, dsn, NULL, &error);
                if (!filename)
                        g_fprintf (stderr, 
				   _("Could not compute output XML file name: %s\n"), 
				   error && error->message ? error->message: _("No detail"));
		else {
			gda_dict_set_xml_filename (data->dict, filename);
			g_free (filename);
			if (!gda_dict_load (data->dict, &error)) 
				g_fprintf (stderr, _("Could not load dictionary file (you'll have to synchronize manually): %s\n"),
					   error && error->message ? error->message: _("No detail"));
		}
        }
	if (error) {
		g_error_free (error);
		error = NULL;
	}
	gda_dict_set_connection (data->dict, data->cnc);

	/* build internal command s list */
	data->internal_commands = build_internal_commands_list (data);
	

	/* loop over commands */
	init_history ();
	GString *st_cmde = NULL;
	for (;;) {
		gchar *cmde;
		compute_prompt (data, prompt, st_cmde == NULL ? FALSE : TRUE);
		if (data->input_stream) {
			cmde = input_from_stream (data->input_stream);
			if (!cmde && !commandsfile) {
				/* go back to console after file is over */
				set_input_file (data, NULL, NULL);
				cmde = input_from_console (prompt->str);
			}
		}
		else
			cmde = input_from_console (prompt->str);

		if (!cmde) {
			save_history ();
			if (!data->output_stream)
				g_print ("\n");
			goto cleanup;
		}
		g_strchug (cmde);
		if (*cmde) {
			if (!st_cmde)
				st_cmde = g_string_new (cmde);
			else {
				g_string_append_c (st_cmde, ' ');
				g_string_append (st_cmde, cmde);
			}
			if (command_is_complete (st_cmde->str)) {
				/* execute command */
				GdaInternalCommandResult *res;
				FILE *to_stream;
				if (data && data->output_stream)
					to_stream = data->output_stream;
				else
					to_stream = stdout;

				res = command_execute (data, st_cmde->str, &error);
				if (!res) {
					g_fprintf (to_stream,
						   "ERROR: %s\n", 
						   error && error->message ? error->message : _("No detail"));
					if (error) {
						g_error_free (error);
						error = NULL;
					}
				}
				else {
					switch (res->type) {
					case GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL:
						output_data_model (data, res->u.model);
						break;
					case GDA_INTERNAL_COMMAND_RESULT_PLIST: {
						GSList *list;
						GString *string;
						
						string = g_string_new ("");
						list = res->u.plist->parameters;
						while (list) {
							gchar *str;
							const GValue *value;
							value = gda_parameter_get_value (GDA_PARAMETER (list->data));
							str = gda_value_stringify ((GValue *) value);
							g_string_append_printf (string, "%s => %s\n",
										gda_object_get_name (GDA_OBJECT (list->data)), str);
							g_free (str);
							list = g_slist_next (list);
						}
						g_fprintf (to_stream, "%s", string->str);
						g_string_free (string, TRUE);
						break;
					}
					case GDA_INTERNAL_COMMAND_RESULT_TXT: 
						g_fprintf (to_stream, "%s", res->u.txt->str);
						break;
					case GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT: 
						g_print ("%s\n", res->u.txt->str);
						break;
					case GDA_INTERNAL_COMMAND_RESULT_EMPTY:
						break;
					default:
						TO_IMPLEMENT;
						break;
					}
					gda_internal_command_exec_result_free (res);
				}

				g_string_free (st_cmde, TRUE);
				st_cmde = NULL;
			}
		}
		g_free (cmde);
	}
	
	/* cleanups */
 cleanup:
	if (data->cnc)
		g_object_unref (data->cnc);
	if (data->client)
		g_object_unref (data->client);
	set_input_file (data, NULL, NULL); 
	set_output_file (data, NULL, NULL); 
	g_string_free (prompt, TRUE);

	g_free (data);

	return 0;
}

/*
 * command_is_complete
 *
 * Checks if @command can be executed, or if more data is required
 */
static gboolean
command_is_complete (const gchar *command)
{
	if (!command || !(*command))
		return FALSE;
	if (*command == '\\') {
		/* internal command */
		return TRUE;
	}
	else {
		if (command [strlen (command) - 1] == ';')
			return TRUE;
		else
			return FALSE;
	}
}


/*
 * command_execute
 */
static GdaInternalCommandResult *execute_external_command (MainData *data, const gchar *command, GError **error);
static GdaInternalCommandResult *
command_execute (MainData *data, gchar *command, GError **error)
{
	if (!command || !(*command))
		return NULL;
	if (*command == '\\') 
		return gda_internal_command_execute (data->internal_commands, 
						     data->cnc, data->dict, command, error);
	else {
		if (!data->cnc) {
			g_set_error (error, 0, 0, 
				     _("No connection specified"));
			return NULL;
		}
		if (!gda_connection_is_opened (data->cnc)) {
			g_set_error (error, 0, 0, 
				     _("Connection closed"));
			return NULL;
		}
			
		return execute_external_command (data, command, error);
	}
}

/*
 * execute_external_command
 *
 * Executes an SQL statement as understood by the DBMS
 */
static GdaInternalCommandResult *
execute_external_command (MainData *data, const gchar *command, GError **error)
{
	GdaInternalCommandResult *res;
	GdaQuery *query;
	GdaParameterList *plist;
	GdaObject *obj;

	res = g_new0 (GdaInternalCommandResult, 1);
	query = gda_query_new_from_sql (data->dict, command, NULL);
	plist = gda_query_get_parameter_list (query);
	if (plist && plist->parameters) {
		/* fill parameters with some defined parameters */
		TO_IMPLEMENT;
	}
	obj = gda_query_execute (query, plist, FALSE, error);
	if (!obj) {
		g_free (res);
		res = NULL;
	}
	else {
		if (GDA_IS_DATA_MODEL (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = GDA_DATA_MODEL (obj);
		}
		else if (GDA_IS_PARAMETER_LIST (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_PLIST;
			res->u.plist = GDA_PARAMETER_LIST (obj);
		}
		else
			g_assert_not_reached ();
	}
	g_object_unref (query);
	if (plist)
		g_object_unref (plist);

	return res;
}



static void
compute_prompt (MainData *data, GString *string, gboolean in_command)
{
	g_assert (string);
	if (in_command)
		g_string_assign (string, "   > ");
	else
		g_string_assign (string, "gda> ");
}

/*
 * Change the output file, set to %NULL to be back on stdout
 */
static gboolean
set_output_file (MainData *data, const gchar *file, GError **error)
{
	if (data->output_stream) {
		fclose (data->output_stream);
		data->output_stream = NULL;
	}

	if (file) {
		data->output_stream = g_fopen (file, "w");
		if (!data->output_stream) {
			g_set_error (error, 0, 0,
				     _("Can't open file '%s' for writing: %s\n"), 
				     file,
				     strerror (errno));
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * Change the input file, set to %NULL to be back on stdin
 */
static gboolean
set_input_file (MainData *data, const gchar *file, GError **error)
{
	if (data->input_stream) {
		fclose (data->input_stream);
		data->input_stream = NULL;
	}

	if (file) {
		data->input_stream = g_fopen (file, "r");
		if (!data->input_stream) {
			g_set_error (error, 0, 0,
				     _("Can't open file '%s' for reading: %s\n"), 
				     file,
				     strerror (errno));
			return FALSE;
		}
	}

	return TRUE;
}


/*
 * Opens connection
 */
static gboolean
open_connection (MainData *data, const gchar *dsn, const gchar *provider, const gchar *direct, 
		 const gchar *user, const gchar *pass,
		 GError **error)
{
	GdaConnection *newcnc = NULL;

	if (!data->client)
		data->client = gda_client_new ();
	
	if (dsn) {
                GdaDataSourceInfo *info = NULL;
                info = gda_config_find_data_source (dsn);
                if (!info)
                        g_set_error (error, 0, 0,
				     _("DSN '%s' is not declared"), dsn);
                else {
                        newcnc = gda_client_open_connection (data->client, info->name,
							     user ? user : info->username,
							     pass ? pass : ((info->password) ? info->password : ""),
							     0, error);
                        gda_data_source_info_free (info);
                }
        }
        else {
		newcnc = gda_client_open_connection_from_string (data->client, provider, direct,
								 user, pass, 0, error);
        }

	if (newcnc && data->cnc) 
		g_object_unref (data->cnc);
	data->cnc = newcnc;

	return newcnc ? TRUE : FALSE;
}

/* 
 * Dumps the data model contents onto @data->output
 */
static void
output_data_model (MainData *data, GdaDataModel *model)
{
	gchar *str;
	FILE *to_stream;
	str = gda_data_model_dump_as_string (model);
       
	if (data && data->output_stream)
		to_stream = data->output_stream;
	else
		to_stream = stdout;
	g_fprintf (to_stream, "%s", str);
	g_free (str);
}

/*
 * Lists all the sections in the config files (local to the user and global) in the index page
 */
static GdaDataModel *
list_all_sections (MainData *data)
{
        GList *sections;
        GList *l;
	GdaDataModel *model;

	model = gda_data_model_array_new_with_g_types (5,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("DSN"));
	gda_data_model_set_column_title (model, 1, _("Provider"));
	gda_data_model_set_column_title (model, 2, _("Description"));
	gda_data_model_set_column_title (model, 3, _("Connection string"));
	gda_data_model_set_column_title (model, 4, _("Username"));

        sections = gda_config_list_sections ("/apps/libgda/Datasources");
        for (l = sections; l; l = l->next) {
                gchar *section = (gchar *) l->data;
		GValue *value;
		gint row;

		row = gda_data_model_append_row (model, NULL);
		value = gda_value_new_from_string (section, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);

		GList *keys;
		gchar *root;
		
		/* list keys in dsn */ 
		root = g_strdup_printf ("/apps/libgda/Datasources/%s", section);
		keys = gda_config_list_keys (root);
		if (keys) {
			GList *l;
			gint col;

			for (l = keys; l ; l = l->next) {
				gchar *str, *tmp;
				gchar *key = (gchar *) l->data;
				
				tmp = g_strdup_printf ("%s/%s", root, key);
				str = gda_config_get_string (tmp);
				value =  gda_value_new_from_string (str, G_TYPE_STRING);
				g_free (tmp);
				
				col = -1;
				if (!strcmp (key, "DSN"))
					col = 3;
				else if (!strcmp (key, "Description"))
					col = 2;
				else if (!strcmp (key, "Username"))
					col = 4;
				else if (!strcmp (key, "Provider"))
					col = 1;
				if (col >= 0)
					gda_data_model_set_value_at (model, col, row, value, NULL);
				gda_value_free (value);
			}
			gda_config_free_list (keys);
		}
		g_free (root);
        }
        gda_config_free_list (sections);

	return model;
}

/*
 * make a list of all the providers in the index page
 */
static GdaDataModel *
list_all_providers (MainData *data)
{
        GList *providers;
	GdaDataModel *model;

	model = gda_data_model_array_new_with_g_types (4,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Provider"));
	gda_data_model_set_column_title (model, 1, _("Description"));
	gda_data_model_set_column_title (model, 2, _("DSN parameters"));
	gda_data_model_set_column_title (model, 3, _("File"));

        for (providers = gda_config_get_provider_list (); providers; providers = providers->next) {
                GdaProviderInfo *info = (GdaProviderInfo *) providers->data;

		GValue *value;
		gint row;

		row = gda_data_model_append_row (model, NULL);

		value = gda_value_new_from_string( info->id, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);

		value = gda_value_new_from_string (info->description, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 1, row, value, NULL);
		gda_value_free (value);

		if (info->gda_params) {
			GSList *params;
			GString *string = g_string_new ("");
			for (params = info->gda_params->parameters; 
			     params; params = params->next) {
				gchar *tmp;
				
				g_object_get (G_OBJECT (params->data), "string_id", &tmp, NULL);
				if (params != info->gda_params->parameters)
					g_string_append (string, ", ");
				g_string_append (string, tmp);
				g_free (tmp);
			}
			value = gda_value_new_from_string (string->str, G_TYPE_STRING);
			g_string_free (string, TRUE);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
		}

		value = gda_value_new_from_string (info->location, G_TYPE_STRING);
		gda_data_model_set_value_at (model, 3, row, value, NULL);
		gda_value_free (value);
        }

	return model;
}

static gchar **args_as_string_func (const gchar *str);

static GdaInternalCommandResult *extra_command_set_output (GdaConnection *cnc, GdaDict *dict, 
							   const gchar **args,
							   GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_set_input (GdaConnection *cnc, GdaDict *dict, 
							  const gchar **args,
							  GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_echo (GdaConnection *cnc, GdaDict *dict, 
						     const gchar **args,
						     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_qecho (GdaConnection *cnc, GdaDict *dict, 
						      const gchar **args,
						      GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_list_dsn (GdaConnection *cnc, GdaDict *dict, 
							 const gchar **args,
							 GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_list_providers (GdaConnection *cnc, GdaDict *dict, 
							       const gchar **args,
							       GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_change_cnc_dsn (GdaConnection *cnc, GdaDict *dict, 
							       const gchar **args,
							       GError **error, MainData *data);

static GdaInternalCommandsList *
build_internal_commands_list (MainData *data)
{
	GdaInternalCommandsList *commands = g_new0 (GdaInternalCommandsList, 1);
	GdaInternalCommand *c;

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "s";
	c->description = _("Show commands history");
	c->args = NULL;
	c->command_func = gda_internal_command_history;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Dictionary");
	c->name = "dict_sync";
	c->description = _("Synchronize dictionary with database structure");
	c->args = NULL;
	c->command_func = gda_internal_command_dict_sync;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Dictionary");
	c->name = "dict_save";
	c->description = _("Save dictionary");
	c->args = NULL;
	c->command_func = gda_internal_command_dict_save;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = "dt";
	c->description = _("List all tables and views");
	c->args = NULL;
	c->command_func = gda_internal_command_list_tables_views;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	/* specific commands */
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = _("c DSN [USER [PASSWORD]]");
	c->description = _("Connect to another defined data source (DSN, see \\dsn_list)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc)extra_command_change_cnc_dsn;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "dsn_list";
	c->description = _("List configured data sources (DSN)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_dsn;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "providers_list";
	c->description = _("List installed database providers");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_providers;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = _("i FILE");
	c->description = _("Execute commands from file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_input;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = _("o [FILE]");
	c->description = _("Send output to a file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_output;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = _("echo [TEXT]");
	c->description = _("Send output to stdout");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_echo;
	c->user_data = data;
	c->arguments_delimiter_func = args_as_string_func;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = _("qecho [TEXT]");
	c->description = _("Send output to output stream");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_qecho;
	c->user_data = data;
	c->arguments_delimiter_func = args_as_string_func;
	commands->commands = g_slist_prepend (commands->commands, c);

	/* comes last */
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "?";
	c->description = _("List all available commands");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) gda_internal_command_help;
	c->user_data = commands;
	c->arguments_delimiter_func = NULL;
	commands->commands = g_slist_prepend (commands->commands, c);

	return commands;
}

static GdaInternalCommandResult *
extra_command_set_output (GdaConnection *cnc, GdaDict *dict, 
			  const gchar **args,
			  GError **error, MainData *data)
{
	if (set_output_file (data, args[0], error)) {
		GdaInternalCommandResult *res;
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static GdaInternalCommandResult *
extra_command_set_input (GdaConnection *cnc, GdaDict *dict, 
			 const gchar **args,
			 GError **error, MainData *data)
{
	if (set_input_file (data, args[0], error)) {
		GdaInternalCommandResult *res;
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static GdaInternalCommandResult *
extra_command_echo (GdaConnection *cnc, GdaDict *dict, 
		    const gchar **args,
		    GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
	res->u.txt = g_string_new (args[0]);
	return res;
}

static GdaInternalCommandResult *
extra_command_qecho (GdaConnection *cnc, GdaDict *dict, 
		     const gchar **args,
		     GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (args[0]);
	return res;
}

static 
GdaInternalCommandResult *extra_command_list_dsn (GdaConnection *cnc, GdaDict *dict, 
						  const gchar **args,
						  GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = list_all_sections (data);
	return res;
}

static 
GdaInternalCommandResult *extra_command_list_providers (GdaConnection *cnc, GdaDict *dict, 
							const gchar **args,
							GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = list_all_providers (data);
	return res;
}

static 
GdaInternalCommandResult *extra_command_change_cnc_dsn (GdaConnection *cnc, GdaDict *dict, 
							const gchar **args,
							GError **error, MainData *data)
{
	const gchar *user = NULL, *pass = NULL;

	if (args[1]) {
		user = args[1];
		if (args[2])
			pass = args[2];
	}
	if (open_connection (data, args[0], NULL, NULL, user, pass, error)) {
		GdaInternalCommandResult *res;
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static gchar **
args_as_string_func (const gchar *str)
{
	return g_strsplit (str, " ", 2);
}
