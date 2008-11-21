/* GDA - SQL console
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
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
#include <sql-parser/gda-sql-parser.h>
#include <virtual/libgda-virtual.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include "tools-input.h"
#include "command-exec.h"
#include <unistd.h>
#include <sys/types.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-blob-op.h>

#ifndef G_OS_WIN32
#include <signal.h>
#include <pwd.h>
#else
#include <stdlib.h>
#include <windows.h>
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif

/* options */
gboolean ask_pass = FALSE;

gchar *single_command = NULL;
gchar *commandsfile = NULL;
gboolean interractive = FALSE;

gboolean list_configs = FALSE;
gboolean list_providers = FALSE;

gchar *outfile = NULL;
gboolean has_threads;

static GOptionEntry entries[] = {
        { "no-password-ask", 'p', 0, G_OPTION_ARG_NONE, &ask_pass, "Don't ask for a password when it is empty", NULL },

        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { "command", 'C', 0, G_OPTION_ARG_STRING, &single_command, "Run only single command (SQL or internal) and exit", "command" },
        { "commands-file", 'f', 0, G_OPTION_ARG_STRING, &commandsfile, "Execute commands from file, then exit (except if -i specified)", "filename" },
	{ "interractive", 'i', 0, G_OPTION_ARG_NONE, &interractive, "Keep the console opened after executing a file (-f option)", NULL },
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
        { NULL }
};

/* interruption handling */
#ifndef G_OS_WIN32
struct sigaction old_sigint_handler; 
#endif
static void sigint_handler (int sig_num);
static void setup_sigint_handler (void);
typedef enum {
	SIGINT_HANDLER_DISABLED = 0,
	SIGINT_HANDLER_PARTIAL_COMMAND,
} SigintHandlerCode;
static SigintHandlerCode sigint_handler_status = SIGINT_HANDLER_DISABLED;

typedef enum {
	OUTPUT_FORMAT_DEFAULT = 0,
	OUTPUT_FORMAT_HTML,
	OUTPUT_FORMAT_XML,
	OUTPUT_FORMAT_CSV
} OutputFormat;

/*
 * structure representing an opened connection
 */
typedef struct {
	gchar         *name;
	GdaConnection *cnc;
	GdaSqlParser  *parser;
	GString       *query_buffer;

	GdaThreader   *threader;
	guint          meta_job_id;
} ConnectionSetting;

/* structure to hold program's data */
typedef struct {
	GSList *settings; /* list all the CncSetting */
	ConnectionSetting *current; /* current connection setting to which commands are sent */
	GdaInternalCommandsList *internal_commands;

	FILE *input_stream;
	FILE *output_stream;
	gboolean output_is_pipe;
	OutputFormat output_format;

	GString *partial_command;

	GHashTable *parameters; /* key = name, value = G_TYPE_STRING GdaParameter */
} MainData;
MainData *main_data;
GString *prompt = NULL;

static gchar   *read_a_line (MainData *data);
static char   **completion_func (const char *text, int start, int end);
static void     compute_prompt (MainData *data, GString *string, gboolean in_command);
static gboolean set_output_file (MainData *data, const gchar *file, GError **error);
static gboolean set_input_file (MainData *data, const gchar *file, GError **error);
static void     output_data_model (MainData *data, GdaDataModel *model);
static void     output_string (MainData *data, const gchar *str);
static ConnectionSetting *open_connection (MainData *data, const gchar *cnc_name, const gchar *cnc_string,
					   GError **error);
static void connection_settings_free (ConnectionSetting *cs);
static GdaDataModel *list_all_dsn (MainData *data);
static GdaDataModel *list_all_providers (MainData *data);

/* commands manipulation */
static GdaInternalCommandsList  *build_internal_commands_list (MainData *data);
static gboolean                  command_is_complete (const gchar *command);
static GdaInternalCommandResult *command_execute (MainData *data, const gchar *command, GError **error);
static void                      display_result (MainData *data, GdaInternalCommandResult *res);

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	MainData *data;
	int exit_status = EXIT_SUCCESS;
	prompt = g_string_new ("");

	context = g_option_context_new (_("[DSN|connection string]..."));        
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_fprintf  (stderr, "Can't parse arguments: %s\n", error->message);
		return EXIT_FAILURE;
        }
        g_option_context_free (context);
        gda_init ();
	has_threads = g_thread_supported ();
	data = g_new0 (MainData, 1);
	data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	main_data = data;
	ask_pass = !ask_pass;

	/* output file */
	if (outfile) {
		if (! set_output_file (data, outfile, &error)) {
			g_print ("Can't set output file as '%s': %s\n", outfile,
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
		GdaDataModel *model = list_all_dsn (data);
		output_data_model (data, model);
		g_object_unref (model);
		goto cleanup;
	}

	/* commands file */
	if (commandsfile) {
		if (! set_input_file (data, commandsfile, &error)) {
			g_print ("Can't read file '%s': %s\n", commandsfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}
	else {
		/* check if stdin is a term */
		if (!isatty (fileno (stdin))) 
			data->input_stream = stdin;
	}

	/* welcome message */
	if (!data->output_stream) {
#ifdef G_OS_WIN32
		HANDLE wHnd;
		SMALL_RECT windowSize = {0, 0, 139, 49};

		wHnd = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTitle ("Gda console, version " PACKAGE_VERSION);
		SetConsoleWindowInfo (wHnd, TRUE, &windowSize);
#endif
		g_print (_("Welcome to the GDA SQL console, version " PACKAGE_VERSION));
		g_print ("\n\n");
		g_print (_("Type: .copyright to show usage and distribution terms\n"
			   "      .? for help with internal commands\n"
			   "      .q (or CTRL-D) to quit\n"
			   "      (the '.' can be replaced by a '\\')\n"
			   "      or any query terminated by a semicolon\n\n"));
	}

	/* open connections if specified */
	gint i;
	for (i = 1; i < argc; i++) {
		/* open connection */
		ConnectionSetting *cs;
		GdaDsnInfo *info = NULL;
		gchar *str;

                info = gda_config_get_dsn_info (argv[i]);
		if (info)
			str = g_strdup (info->name);
		else
			str = g_strdup_printf ("c%d", i-1);
		if (!data->output_stream) 
			g_print (_("Opening connection '%s' for: %s\n"), str, argv[i]);
		cs = open_connection (data, str, argv[i], &error);
		g_free (str);
		if (!cs) {
			g_print (_("Can't open connection %d: %s\n"), i,
				 error && error->message ? error->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}
	if (getenv ("GDA_SQL_CNC")) {
		ConnectionSetting *cs;
		gchar *str;
		const gchar *envstr = getenv ("GDA_SQL_CNC");
		str = g_strdup_printf ("c%d", i-1);
		if (!data->output_stream) 
			g_print (_("Opening connection '%s' for: %s (GDA_SQL_CNC environment variable)\n"), 
				   str, envstr);
		cs = open_connection (data, str, envstr, &error);
		g_free (str);
		if (!cs) {
			g_print (_("Can't open connection defined by GDA_SQL_CNC: %s\n"),
				 error && error->message ? error->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* build internal command s list */
	data->internal_commands = build_internal_commands_list (data);

	/* loop over commands */
	setup_sigint_handler ();
	init_input ();
	set_completion_func (completion_func);
	init_history ();
	for (;;) {
		gchar *cmde;

		/* run any pending iterations */
		while (g_main_context_iteration (NULL, FALSE));

		cmde = read_a_line (data);
		if (!cmde) {
			save_history (NULL, NULL);
			if (!data->output_stream)
				g_print ("\n");
			goto cleanup;
		}

		g_strchug (cmde);
		if (*cmde) {
			if (!data->partial_command) {
				/* enable SIGINT handling */
				sigint_handler_status = SIGINT_HANDLER_PARTIAL_COMMAND;
				data->partial_command = g_string_new (cmde);
			}
			else {
				g_string_append_c (data->partial_command, ' ');
				g_string_append (data->partial_command, cmde);
			}
			if (command_is_complete (data->partial_command->str)) {
				/* execute command */
				GdaInternalCommandResult *res;
				FILE *to_stream;

				if ((*data->partial_command->str != '\\') && (*data->partial_command->str != '.')) {
					if (data->current) {
						if (!data->current->query_buffer)
							data->current->query_buffer = g_string_new ("");
						g_string_assign (data->current->query_buffer, data->partial_command->str);
					}
				}

				if (data && data->output_stream)
					to_stream = data->output_stream;
				else
					to_stream = stdout;
				res = command_execute (data, data->partial_command->str, &error);
				
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
					display_result (data, res);
					if (res->type == GDA_INTERNAL_COMMAND_RESULT_EXIT) {
						gda_internal_command_exec_result_free (res);
						goto cleanup;
					}
					gda_internal_command_exec_result_free (res);
				}
				g_string_free (data->partial_command, TRUE);
				data->partial_command = NULL;
				
				/* disable SIGINT handling */
				sigint_handler_status = SIGINT_HANDLER_DISABLED;
			}
		}
		g_free (cmde);
	}
	
	/* cleanups */
 cleanup:
	g_slist_foreach (data->settings, (GFunc) connection_settings_free, NULL);
	set_input_file (data, NULL, NULL); 
	set_output_file (data, NULL, NULL); 

	g_free (data);

	return EXIT_SUCCESS;
}

static void
display_result (MainData *data, GdaInternalCommandResult *res)
{
	switch (res->type) {
	case GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL:
		output_data_model (data, res->u.model);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_SET: {
		GSList *list;
		GString *string;
		xmlNodePtr node;
		xmlBufferPtr buffer;
		switch (data->output_format) {
		case OUTPUT_FORMAT_DEFAULT:
			string = g_string_new ("");
			for (list = res->u.set->holders; list; list = list->next) {
				gchar *str;
				const GValue *value;
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				str = gda_value_stringify ((GValue *) value);
				g_string_append_printf (string, "%s => %s\n", 
							gda_holder_get_id (GDA_HOLDER (list->data)), str);
				g_free (str);
			}
			output_string (data, string->str);
			g_string_free (string, TRUE);
			break;
		case OUTPUT_FORMAT_XML: {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "parameters");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "parameter");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			output_string (data, (gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			break;
		}
		case OUTPUT_FORMAT_HTML: {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "ul");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "li");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			output_string (data, (gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			break;
		}
		case OUTPUT_FORMAT_CSV: 
			string = g_string_new ("");
			for (list = res->u.set->holders; list; list = list->next) {
				gchar *str;
				const GValue *value;
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				str = gda_value_stringify ((GValue *) value);
				g_string_append_printf (string, "%s,%s\n", 
							gda_holder_get_id (GDA_HOLDER (list->data)), str);
				g_free (str);
			}
			output_string (data, string->str);
			g_string_free (string, TRUE);
			break;
		default:
			TO_IMPLEMENT;
			break;
		}
		break;
	}
	case GDA_INTERNAL_COMMAND_RESULT_TXT: {
		xmlNodePtr node;
		xmlBufferPtr buffer;
		switch (data->output_format) {
		case OUTPUT_FORMAT_DEFAULT:
		case OUTPUT_FORMAT_CSV:
			output_string (data, res->u.txt->str);
			break;
		case OUTPUT_FORMAT_XML:
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "txt");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			output_string (data, (gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			break;
		case OUTPUT_FORMAT_HTML: 
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "p");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			output_string (data, (gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			break;
		default:
			TO_IMPLEMENT;
			break;
		}
		break;
	}
	case GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT: 
		g_print ("%s", res->u.txt->str);
		fflush (NULL);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_EMPTY:
		break;
	case GDA_INTERNAL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		for (list = res->u.multiple_results; list; list = list->next)
			display_result (data, (GdaInternalCommandResult *) list->data);
		break;
	}
	case GDA_INTERNAL_COMMAND_RESULT_EXIT:
		break;
	default:
		TO_IMPLEMENT;
		break;
	}
}

/*
 * SIGINT handling
 */

static void 
setup_sigint_handler (void) 
{
#ifndef G_OS_WIN32
	struct sigaction sac;
	memset (&sac, 0, sizeof (sac));
	sigemptyset (&sac.sa_mask);
	sac.sa_handler = sigint_handler;
	sac.sa_flags = SA_RESTART;
	sigaction (SIGINT, &sac, &old_sigint_handler);
#endif
}

#ifndef G_OS_WIN32
static void
sigint_handler (int sig_num)
{
	if (sigint_handler_status == SIGINT_HANDLER_PARTIAL_COMMAND) {
		if (main_data->partial_command) {
			g_string_free (main_data->partial_command, TRUE);
			main_data->partial_command = NULL;
		}
		/* show a new prompt */
		compute_prompt (main_data, prompt, main_data->partial_command == NULL ? FALSE : TRUE);
		g_print ("\n%s", prompt->str);
		fflush (NULL);
	}
	else {
		g_print ("\n");
		exit (EXIT_SUCCESS);
	}

	if (old_sigint_handler.sa_handler)
		old_sigint_handler.sa_handler (sig_num);
}
#endif

/*
 * read_a_line
 *
 * Read a line to be processed
 */
static gchar *read_a_line (MainData *data)
{
	gchar *cmde;

	if (single_command) {
		if (*single_command) {
			cmde = single_command;
			single_command = "";
			return cmde;
		}
		else
			return NULL;
	}
	compute_prompt (data, prompt, data->partial_command == NULL ? FALSE : TRUE);
	if (data->input_stream) {
		cmde = input_from_stream (data->input_stream);
		if (interractive && !cmde && isatty (fileno (stdin))) {
			/* go back to console after file is over */
			set_input_file (data, NULL, NULL);
			cmde = input_from_console (prompt->str);
		}
	}
	else
		cmde = input_from_console (prompt->str);

	return cmde;
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
	if (single_command)
		return TRUE;
	if ((*command == '\\') || (*command == '.')) {
		/* internal command */
		return TRUE;
	}
	else if (*command == '#') {
		/* comment, to be ignored */
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
command_execute (MainData *data, const gchar *command, GError **error)
{
	if (!command || !(*command))
                return NULL;
        if ((*command == '\\') || (*command == '.')) {
                if (data->current)
                        return gda_internal_command_execute (data->internal_commands,
                                                             data->current->cnc, command, error);
                else
                        return gda_internal_command_execute (data->internal_commands, NULL, command, error);
        }
	else if (*command == '#') {
		/* nothing to do */
		GdaInternalCommandResult *res;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
        else {
                if (!data->current) {
                        g_set_error (error, 0, 0,
                                     _("No connection specified"));
                        return NULL;
                }
                if (!gda_connection_is_opened (data->current->cnc)) {
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
	GdaInternalCommandResult *res = NULL;
	GdaBatch *batch;
	const GSList *stmt_list;
	GdaStatement *stmt;
	GdaSet *params;
	GObject *obj;
	const gchar *remain = NULL;

	batch = gda_sql_parser_parse_string_as_batch (data->current->parser, command, &remain, error);
	if (!batch)
		return NULL;
	if (remain) {
		g_object_unref (batch);
		return NULL;
	}
	
	stmt_list = gda_batch_get_statements (batch);
	if (!stmt_list) {
		g_object_unref (batch);
		return NULL;
	}

	if (stmt_list->next) {
		g_set_error (error, 0, 0,
			     _("More than one SQL statement"));
		g_object_unref (batch);
		return NULL;
	}
		
	stmt = GDA_STATEMENT (stmt_list->data);
	g_object_ref (stmt);
	g_object_unref (batch);

	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	/* fill parameters with some defined parameters */
	if (params && params->holders) {
		GSList *list;
		for (list = params->holders; list; list = list->next) {
			GdaHolder *h = GDA_HOLDER (list->data);
			GdaHolder *h_in_data = g_hash_table_lookup (data->parameters, gda_holder_get_id (h));
			if (h_in_data) {
				const GValue *cvalue;
				GValue *value;

				cvalue = gda_holder_get_value (h_in_data);
				if (cvalue && (G_VALUE_TYPE (cvalue) == gda_holder_get_g_type (h))) {
					if (!gda_holder_set_value (h, cvalue, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
				else if (cvalue && (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)) {
					if (!gda_holder_set_value (h, cvalue, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
				else {
					gchar *str;
					str = gda_value_stringify (cvalue);
					value = gda_value_new_from_string (str, gda_holder_get_g_type (h));
					g_free (str);
					if (! value) {
						g_set_error (error, 0, 0,
							     _("Could not interpret the '%s' parameter's value"), 
							     gda_holder_get_id (h));
						g_free (res);
						res = NULL;
						goto cleanup;
					}
					else if (! gda_holder_take_value (h, value, error)) {
						gda_value_free (value);
						g_free (res);
						res = NULL;
						goto cleanup;
					}
					g_free (str);
				}
			}
			else {
				if (! gda_holder_is_valid (h)) {
					g_set_error (error, 0, 0,
						     _("No internal parameter named '%s' required by query"), 
						     gda_holder_get_id (h));
					g_free (res);
					res = NULL;
					goto cleanup;
				}
			}
		}
	}

	res = g_new0 (GdaInternalCommandResult, 1);	
	obj = gda_connection_statement_execute (data->current->cnc, stmt, params, 
						GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!obj) {
		g_free (res);
		res = NULL;
	}
	else {
		if (GDA_IS_DATA_MODEL (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = GDA_DATA_MODEL (obj);
		}
		else if (GDA_IS_SET (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_SET;
			res->u.set = GDA_SET (obj);
		}
		else
			g_assert_not_reached ();
	}

 cleanup:
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	return res;
}



static void
compute_prompt (MainData *data, GString *string, gboolean in_command)
{
	gchar *prefix;
	g_assert (string);

	g_string_set_size (string, 0);
	if (data->current)
		prefix = data->current->name;
	else
		prefix = "gda";

	if (in_command) {
		gint i, len;
		len = strlen (prefix);
		for (i = 0; i < len; i++)
			g_string_append_c (string, ' ');
		g_string_append_c (string, '>');
		g_string_append_c (string, ' ');		
	}
	else 
		g_string_append_printf (string, "%s> ", prefix);
}

/*
 * Change the output file, set to %NULL to be back on stdout
 */
static gboolean
set_output_file (MainData *data, const gchar *file, GError **error)
{
	if (data->output_stream) {
		if (data->output_is_pipe) {
			pclose (data->output_stream);
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_DFL);
#endif
		}
		else
			fclose (data->output_stream);
		data->output_stream = NULL;
		data->output_is_pipe = FALSE;
	}

	if (file) {
		gchar *copy = g_strdup (file);
		g_strchug (copy);

		if (*copy != '|') {
			/* output to a file */
			data->output_stream = g_fopen (copy, "w");
			if (!data->output_stream) {
				g_set_error (error, 0, 0,
					     _("Can't open file '%s' for writing: %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
			data->output_is_pipe = FALSE;
		}
		else {
			/* output to a pipe */
			data->output_stream = popen (copy+1, "w");
			if (!data->output_stream) {
				g_set_error (error, 0, 0,
					     _("Can't open pipe '%s': %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_IGN);
#endif
			data->output_is_pipe = TRUE;
		}
		g_free (copy);
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
		if (*file == '~') {
			gchar *tmp;
			tmp = g_strdup_printf ("%s%s", g_get_home_dir (), file+1);
			data->input_stream = g_fopen (tmp, "r");
			g_free (tmp);
		}
		else
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

static gboolean
connection_name_is_valid (const gchar *name)
{
	const gchar *ptr;
	if (!name || !*name)
		return FALSE;
	if (! g_ascii_isalpha (*name) && (*name != '_'))
		return FALSE;
	for (ptr = name; *ptr; ptr++)
		if (!g_ascii_isalnum (*ptr) && (*ptr != '_'))
			return FALSE;
	return TRUE;
}

static ConnectionSetting *
find_connection_from_name (MainData *data, const gchar *name)
{
	ConnectionSetting *cs = NULL;
	GSList *list;
	for (list = data->settings; list; list = list->next) {
		if (!strcmp (name, ((ConnectionSetting *) list->data)->name)) {
			cs = (ConnectionSetting *) list->data;
			break;
		}
	}
	return cs;
}

typedef struct {
	MainData *data;
	ConnectionSetting *cs;
	gboolean cannot_lock;
	gboolean result;
	GError   *error;
} MetaUpdateData;

static gpointer thread_start_update_meta_store (MetaUpdateData *data);
static void thread_ok_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);
static void thread_cancelled_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);

/*
 * Open a connection
 */
static ConnectionSetting*
open_connection (MainData *data, const gchar *cnc_name, const gchar *cnc_string, GError **error)
{
	GdaConnection *newcnc = NULL;
	ConnectionSetting *cs = NULL;
	static gint cncindex = 0;
	gchar *real_cnc_string;

	if (cnc_name && ! connection_name_is_valid (cnc_name)) {
		g_set_error (error, 0, 0,
			     _("Connection name '%s' is invalid"), cnc_name);
		return NULL;
	}
	
	GdaDsnInfo *info;
	gchar *user, *pass, *real_cnc, *real_provider, *real_auth_string = NULL;

	/* if cnc string is a regular file, then use it with SQLite */
	if (g_file_test (cnc_string, G_FILE_TEST_IS_REGULAR)) {
		gchar *path, *file, *e1, *e2;
		const gchar *pname = "SQLite";
		
		path = g_path_get_dirname (cnc_string);
		file = g_path_get_basename (cnc_string);
		if (g_str_has_suffix (file, ".mdb")) {
			pname = "MSAccess";
			file [strlen (file) - 4] = 0;
		}
		else if (g_str_has_suffix (file, ".db"))
			file [strlen (file) - 3] = 0;
		e1 = gda_rfc1738_encode (path);
		e2 = gda_rfc1738_encode (file);
		g_free (path);
		g_free (file);
		real_cnc_string = g_strdup_printf ("%s://DB_DIR=%s;DB_NAME=%s", pname, e1, e2);
		g_free (e1);
		g_free (e2);
		gda_connection_string_split (real_cnc_string, &real_cnc, &real_provider, &user, &pass);
	}
	else {
		gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
		real_cnc_string = g_strdup (cnc_string);
	}
	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed connection string '%s'"), cnc_string);
		g_free (real_cnc_string);
		return NULL;
	}

	if (ask_pass) {
		if (user && !*user) {
			gchar buf[80];
			g_print (_("\tUsername for '%s': "), cnc_name);
			if (scanf ("%80s", buf) == -1) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
					     _("No username for '%s'"), cnc_string);
				g_free (real_cnc_string);
				return NULL;
			}
			g_free (user);
			user = g_strdup (buf);
		}
		if (pass && !*pass) {
			gchar buf[80];
			g_print (_("\tPassword for '%s': "), cnc_name);
			if (scanf ("%80s", buf) == -1) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
					     _("No password for '%s'"), cnc_string);
				g_free (real_cnc_string);
				return NULL;
			}
			g_free (pass);
			pass = g_strdup (buf);
		}
		if (user || pass) {
			gchar *s1;
			s1 = gda_rfc1738_encode (user);
			if (pass) {
				gchar *s2;
				s2 = gda_rfc1738_encode (pass);
				real_auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
				g_free (s2);
			}
			else
				real_auth_string = g_strdup_printf ("USERNAME=%s", s1);
			g_free (s1);
		}
	}
	
	info = gda_config_get_dsn_info (real_cnc);
	if (info && !real_provider)
		newcnc = gda_connection_open_from_dsn (real_cnc_string, real_auth_string, 0, error);
	else 
		newcnc = gda_connection_open_from_string (NULL, real_cnc_string, real_auth_string, 0, error);
	
	g_free (real_cnc_string);
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);
	g_free (real_auth_string);

	if (newcnc) {
		cs = g_new0 (ConnectionSetting, 1);
		if (cnc_name && *cnc_name) 
			cs->name = g_strdup (cnc_name);
		else
			cs->name = g_strdup_printf ("c%d", cncindex);
		cncindex++;
		cs->parser = gda_connection_create_parser (newcnc);
		if (!cs->parser)
			cs->parser = gda_sql_parser_new ();
		cs->cnc = newcnc;
		cs->query_buffer = NULL;
		cs->threader = NULL;
		cs->meta_job_id = 0;

		data->settings = g_slist_append (data->settings, cs);
		data->current = cs;

		GdaMetaStore *store;
		gboolean update_store = FALSE;

		if (info) {
			gchar *filename;
#define LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S ".libgda"
			filename = g_strdup_printf ("%s%sgda-sql-%s.db", 
						    g_get_home_dir (), LIBGDA_USER_CONFIG_DIR G_DIR_SEPARATOR_S,
						    info->name);
			if (! g_file_test (filename, G_FILE_TEST_EXISTS))
				update_store = TRUE;
			store = gda_meta_store_new_with_file (filename);
			g_free (filename);
		}
		else {
			store = gda_meta_store_new (NULL);
			update_store = TRUE;
		}

		g_object_set (G_OBJECT (cs->cnc), "meta-store", store, NULL);
		if (update_store) {
			GError *lerror = NULL;
			
			if (has_threads) {
				MetaUpdateData *thdata;
				cs->threader = (GdaThreader*) gda_threader_new ();
				thdata = g_new0 (MetaUpdateData, 1);
				thdata->data = data;
				thdata->cs = cs;
				thdata->cannot_lock = FALSE;
				cs->meta_job_id = gda_threader_start_thread (cs->threader, 
									     (GThreadFunc) thread_start_update_meta_store,
									     thdata,
									     (GdaThreaderFunc) thread_ok_cb_update_meta_store,
									     (GdaThreaderFunc) thread_cancelled_cb_update_meta_store, 
									     &lerror);
				if (cs->meta_job_id == 0) {
					if (!data->output_stream) 
						g_print (_("Error getting meta data in background: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
			}
			else {
				if (!data->output_stream) {
					g_print (_("Getting database schema information for connection '%s', this may take some time... "),
						 cs->name);
					fflush (stdout);
				}
				
				if (!gda_connection_update_meta_store (cs->cnc, NULL, &lerror)) {
					if (!data->output_stream) 
						g_print (_("error: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				else
					if (!data->output_stream) 
						g_print (_("Done.\n"));
			}
		}
		g_object_unref (store);
	}

	return cs;
}

static gpointer
thread_start_update_meta_store (MetaUpdateData *data)
{
	/* test if the connection can be locked, which means that multiple threads can access it at the same time.
	 * If that is not possible, then quit the thread while positionning data->cannot_lock to TRUE so the
	 * meta data update can be done while back in the main thread */
	if (gda_lockable_trylock (GDA_LOCKABLE (data->cs->cnc))) {
		gda_lockable_unlock (GDA_LOCKABLE (data->cs->cnc));
		data->result = gda_connection_update_meta_store (data->cs->cnc, NULL, &(data->error));
	}
	else
		data->cannot_lock = TRUE;
	return NULL;
}

static void
thread_ok_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data)
{
	data->cs->meta_job_id = 0;
	if (data->cannot_lock) {
		GError *lerror = NULL;
		if (!data->data->output_stream) {
			g_print (_("Getting database schema information for connection '%s', this may take some time... "),
				 data->cs->name);
			fflush (stdout);
		}

		if (!gda_connection_update_meta_store (data->cs->cnc, NULL, &lerror)) {
			if (!data->data->output_stream) 
				g_print (_("error: %s\n"), 
					 lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
		else
			if (!data->data->output_stream) 
				g_print (_("Done.\n"));
	}
	if (data->error)
		g_error_free (data->error);

	g_free (data);
}

static void
thread_cancelled_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data)
{
	data->cs->meta_job_id = 0;
	if (data->error)
		g_error_free (data->error);
	g_free (data);
}

/* free the connection settings */
static void
connection_settings_free (ConnectionSetting *cs)
{
	g_free (cs->name);
	if (cs->cnc)
		g_object_unref (cs->cnc);
	if (cs->parser)
		g_object_unref (cs->parser);
	if (cs->query_buffer)
		g_string_free (cs->query_buffer, TRUE);
	if (cs->threader) {
		if (cs->meta_job_id)
			gda_threader_cancel (cs->threader, cs->meta_job_id);
		g_object_unref (cs->threader);
	}
	g_free (cs);
}

/* 
 * Dumps the data model contents onto @data->output
 */
static void
output_data_model (MainData *data, GdaDataModel *model)
{
	gchar *str;
	static gboolean env_set = FALSE;

	if (!env_set) {
		g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "Yes", TRUE);
		g_setenv ("GDA_DATA_MODEL_NULL_AS_EMPTY", "Yes", TRUE);
		env_set = TRUE;
	}

	switch (data->output_format) {
	case OUTPUT_FORMAT_DEFAULT:
		str = gda_data_model_dump_as_string (model);
		output_string (data, str);
		g_free (str);
		break;
	case OUTPUT_FORMAT_XML:
		str = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
						       NULL, 0,
						       NULL, 0, NULL);
		output_string (data, str);
		g_free (str);
		break;
	case OUTPUT_FORMAT_CSV:
		str = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
						       NULL, 0,
						       NULL, 0, NULL);
		output_string (data, str);
		g_free (str);
		break;
	case OUTPUT_FORMAT_HTML: {
		xmlBufferPtr buffer;
		xmlNodePtr html, table, node, row_node, col_node;
		gint ncols, nrows, i, j;
		gchar *str;

		html = xmlNewNode (NULL, BAD_CAST "div");

		table = xmlNewChild (html, NULL, BAD_CAST "table", NULL);
		xmlSetProp (table, BAD_CAST "border", BAD_CAST "1");
		
		if (g_object_get_data (G_OBJECT (model), "name")) {
			node = xmlNewChild (table, NULL, BAD_CAST "caption", NULL);
			xmlNewTextChild (node, NULL, BAD_CAST "big", g_object_get_data (G_OBJECT (model), "name"));
		}

		ncols = gda_data_model_get_n_columns (model);
		nrows = gda_data_model_get_n_rows (model);

		row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
		for (j = 0; j < ncols; j++) {
			const gchar *cstr;
			cstr = gda_data_model_get_column_title (model, j);
			col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "th", BAD_CAST cstr);
			xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "center");
		}

		for (i = 0; i < nrows; i++) {
			row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
			xmlSetProp (row_node, BAD_CAST "valign", BAD_CAST "top");
			for (j = 0; j < ncols; j++) {
				const GValue *value;
				value = gda_data_model_get_value_at (model, j, i, NULL);
				if (!value) {
					col_node = xmlNewChild (row_node, NULL, BAD_CAST "td", BAD_CAST "ERROR");
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
				}
				else {
					str = gda_value_stringify (value);
					col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "td", BAD_CAST str);
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
					g_free (str);
				}
			}
		}

		node = xmlNewChild (html, NULL, BAD_CAST "p", NULL);
		g_strdup_printf (ngettext ("(%d row)", "(%d rows)", nrows), nrows);
		xmlNodeSetContent (node, BAD_CAST str);
		g_free (str);

	        xmlNewChild (html, NULL, BAD_CAST "br", NULL);

		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, html, 0, 1);
		output_string (data, (gchar *) xmlBufferContent (buffer));
		xmlBufferFree (buffer);
		xmlFreeNode (html);

		break;
	}
	default: 
		break;
	}
}

static void
output_string (MainData *data, const gchar *str)
{
	FILE *to_stream;
	gboolean append_nl = FALSE;
	gint length;
	static gint force_no_pager = -1;
	
	if (!str)
		return;

	if (force_no_pager < 0) {
		/* still unset... */
		if (getenv ("GDA_NO_PAGER"))
			force_no_pager = 1;
		else
			force_no_pager = 0;	
	}

	length = strlen (str);
	if (str[length - 1] != '\n')
		append_nl = TRUE;

	if (data && data->output_stream)
		to_stream = data->output_stream;
	else
		to_stream = stdout;

	if (!force_no_pager && isatty (fileno (to_stream))) {
		/* use pager */
		FILE *pipe;
		const char *pager;
		
		pager = getenv ("PAGER");
		if (!pager)
			pager = "more";
		pipe = popen (pager, "w");
#ifndef G_OS_WIN32
		signal (SIGPIPE, SIG_IGN);
#endif
		if (append_nl)
			g_fprintf (pipe, "%s\n", str);
		else
			g_fprintf (pipe, "%s", str);
		pclose (pipe);
#ifndef G_OS_WIN32
		signal(SIGPIPE, SIG_DFL);
#endif
	}
	else {
		if (append_nl)
			g_fprintf (to_stream, "%s\n", str);
		else
			g_fprintf (to_stream, "%s", str);
	}
}

/*
 * Lists all the sections in the config files (local to the user and global) in the index page
 */
static GdaDataModel *
list_all_dsn (MainData *data)
{
	return gda_config_list_dsn ();
}

/*
 * make a list of all the providers in the index page
 */
static GdaDataModel *
list_all_providers (MainData *data)
{
	return gda_config_list_providers ();
}

static gchar **args_as_string_func (const gchar *str);
static gchar **args_as_string_set (const gchar *str);

static GdaInternalCommandResult *extra_command_copyright (GdaConnection *cnc, const gchar **args,
							  GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_quit (GdaConnection *cnc, const gchar **args,
						     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_cd (GdaConnection *cnc, const gchar **args,
						   GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_set_output (GdaConnection *cnc, const gchar **args,
							   GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_set_output_format (GdaConnection *cnc, const gchar **args,
								  GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_set_input (GdaConnection *cnc, const gchar **args,
							  GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_echo (GdaConnection *cnc, const gchar **args,
						     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_qecho (GdaConnection *cnc, const gchar **args,
						      GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_list_dsn (GdaConnection *cnc, const gchar **args,
							 GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_create_dsn (GdaConnection *cnc, const gchar **args,
							   GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_remove_dsn (GdaConnection *cnc, const gchar **args,
							   GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_list_providers (GdaConnection *cnc, const gchar **args,
							       GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_manage_cnc (GdaConnection *cnc, const gchar **args,
							   GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_close_cnc (GdaConnection *cnc, const gchar **args,
							  GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_bind_cnc (GdaConnection *cnc, const gchar **args,
							 GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_edit_buffer (GdaConnection *cnc, const gchar **args,
							    GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_reset_buffer (GdaConnection *cnc, const gchar **args,
							     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_show_buffer (GdaConnection *cnc, const gchar **args,
							    GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_exec_buffer (GdaConnection *cnc, const gchar **args,
							    GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_write_buffer (GdaConnection *cnc, const gchar **args,
							     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_query_buffer_to_dict (GdaConnection *cnc, const gchar **args,
								     GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_query_buffer_from_dict (GdaConnection *cnc, const gchar **args,
								       GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_query_buffer_list_dict (GdaConnection *cnc, const gchar **args,
								       GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_query_buffer_delete_dict (GdaConnection *cnc, const gchar **args,
									 GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_set (GdaConnection *cnc, const gchar **args,
						    GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_unset (GdaConnection *cnc, const gchar **args,
						      GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_graph (GdaConnection *cnc, const gchar **args,
						      GError **error, MainData *data);

static GdaInternalCommandResult *extra_command_lo_update (GdaConnection *cnc, const gchar **args,
							  GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_export (GdaConnection *cnc, const gchar **args,
						       GError **error, MainData *data);
static GdaInternalCommandResult *extra_command_set2 (GdaConnection *cnc, const gchar **args,
						     GError **error, MainData *data);

static GdaInternalCommandsList *
build_internal_commands_list (MainData *data)
{
	GdaInternalCommandsList *commands = g_new0 (GdaInternalCommandsList, 1);
	GdaInternalCommand *c;

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [FILE]"), "s");
	c->description = _("Show commands history, or save it to file");
	c->args = NULL;
	c->command_func = gda_internal_command_history;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = "meta";
	c->description = _("Force reading the database meta data");
	c->args = NULL;
	c->command_func = gda_internal_command_dict_sync;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [TABLE]"), "dt");
	c->description = _("List all tables (or named table)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_tables;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [VIEW]"), "dv");
	c->description = _("List all views (or named view)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_views;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [SCHEMA]"), "dn");
	c->description = _("List all schemas (or named schema)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_schemas;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [OBJ_NAME|SCHEMA.*]"), "d");
	c->description = _("Describe object or full list of objects");
	c->args = NULL;
	c->command_func = gda_internal_command_detail;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [TABLE1 [TABLE2...]]"), "graph");
	c->description = _("Create a graph of all or the listed tables");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_graph;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	/* specific commands */
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [CNC_NAME [DSN [USER [PASSWORD]]]]"), "c");
	c->description = _("Connect to another defined data source (DSN, see \\l)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_manage_cnc;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [CNC_NAME]"), "close");
	c->description = _("Close a connection");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_close_cnc;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s CNC_NAME CNC_NAME1 CNC_NAME2 [CNC_NAME ...]"), "bind");
	c->description = _("Bind several connections together into the CNC_NAME virtual connection");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc)extra_command_bind_cnc;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [DSN]"), "l");
	c->description = _("List all DSN (or named DSN's attributes)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_dsn;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s DSN_NAME DSN_DEFINITION [DESCRIPTION]"), "lc");
	c->description = _("Create (or modify) a DSN");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_create_dsn;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s DSN_NAME [DSN_NAME...]"), "lr");
	c->description = _("Remove a DSN");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_remove_dsn;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [PROVIDER]"), "lp");
	c->description = _("List all installed database providers (or named one's attributes)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_providers;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s FILE"), "i");
	c->description = _("Execute commands from file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_input;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [FILE]"), "o");
	c->description = _("Send output to a file or |pipe");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_output;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [TEXT]"), "echo");
	c->description = _("Send output to stdout");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_echo;
	c->user_data = data;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [TEXT]"), "qecho");
	c->description = _("Send output to output stream");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_qecho;
	c->user_data = data;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "q";
	c->description = _("Quit");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_quit;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [DIR]"), "cd");
	c->description = _("Change the current working directory");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_cd;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "copyright";
	c->description = _("Show usage and distribution terms");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_copyright;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [FILE]"), "e");
	c->description = _("Edit the query buffer (or file) with external editor");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_edit_buffer;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [FILE]"), "qr");
	c->description = _("Reset the query buffer (fill buffer with contents of file)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_reset_buffer;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = "qp";
	c->description = _("Show the contents of the query buffer");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_show_buffer;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [QUERY_BUFFER_NAME]"), "g");
	c->description = _("Execute contents of query buffer, or named query buffer");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_exec_buffer;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s FILE"), "qw");
	c->description = _("Write query buffer to file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_write_buffer;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "qs");
	c->description = _("Save query buffer to dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_to_dict;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "ql");
	c->description = _("Load query buffer from dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_from_dict;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "qd");
	c->description = _("Delete query buffer from dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_delete_dict;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s"), "qa");
	c->description = _("List all saved query buffers in dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_list_dict;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME [VALUE|_null_]]"), "set");
	c->description = _("Set or show internal parameter, or list all if no parameters");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set;
	c->user_data = data;
	c->arguments_delimiter_func = args_as_string_set;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME]"), "unset");
	c->description = _("Unset (delete) internal named parameter (or all parameters)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_unset;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Formatting");
	c->name = "H [HTML|XML|CSV|DEFAULT]";
	c->description = _("Set output format");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_output_format;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	/*
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s FILE TABLE BLOB_COLUMN ROW_CONDITION"), "lo_update");
	c->description = _("Import a blob into the database");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_lo_update;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);
	*/

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME|TABLE COLUMN ROW_CONDITION] FILE"), "export");
	c->description = _("Export internal parameter or table's value to the FILE file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_export;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s NAME [FILE|TABLE COLUMN ROW_CONDITION]"), "setex");
	c->description = _("Set internal parameter as the contents of the FILE file or from an existing table's value");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set2;
	c->user_data = data;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
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
extra_command_set_output (GdaConnection *cnc, const gchar **args,
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
extra_command_set_output_format (GdaConnection *cnc, const gchar **args,
				 GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	const gchar *format = NULL;

	if (args[0] && *args[0])
		format = args[0];
	
	data->output_format = OUTPUT_FORMAT_DEFAULT;
	if (format) {
		if ((*format == 'X') || (*format == 'x'))
			data->output_format = OUTPUT_FORMAT_XML;
		else if ((*format == 'H') || (*format == 'h'))
			data->output_format = OUTPUT_FORMAT_HTML;
		else if ((*format == 'D') || (*format == 'd'))
			data->output_format = OUTPUT_FORMAT_DEFAULT;
		else if ((*format == 'C') || (*format == 'c'))
			data->output_format = OUTPUT_FORMAT_CSV;
		else {
			g_set_error (error, 0, 0,
				     _("Unknown output format: '%s', reset to default"), format);
			return NULL;
		}
	}

	if (!data->output_stream) {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new ("");
		switch (data->output_format) {
		case OUTPUT_FORMAT_DEFAULT:
			g_string_assign (res->u.txt, ("Output format is default\n"));
			break;
		case OUTPUT_FORMAT_HTML:
			g_string_assign (res->u.txt, ("Output format is HTML\n"));
			break;
		case OUTPUT_FORMAT_XML:
			g_string_assign (res->u.txt, ("Output format is XML\n"));
			break;
		case OUTPUT_FORMAT_CSV:
			g_string_assign (res->u.txt, ("Output format is CSV\n"));
			break;
		default:
			TO_IMPLEMENT;
		}
	}
	else {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	return res;
}

static GdaInternalCommandResult *
extra_command_set_input (GdaConnection *cnc, const gchar **args,
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
extra_command_echo (GdaConnection *cnc, const gchar **args,
		    GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
	res->u.txt = g_string_new (args[0]);
	return res;
}

static GdaInternalCommandResult *
extra_command_qecho (GdaConnection *cnc, const gchar **args,
		     GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (args[0]);
	return res;
}

static GdaInternalCommandResult *
extra_command_list_dsn (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *dsn_list, *model = NULL;
	gint i, nrows;
	GList *list = NULL;

	dsn_list = gda_config_list_dsn ();
	nrows = gda_data_model_get_n_rows (dsn_list);

	if (args[0]) {
		/* details about a DSN */
		for (i = 0; i < nrows; i++) {
			const GValue *value;
			value = gda_data_model_get_value_at (dsn_list, 0, i, error);
			if (!value)
				goto onerror;

			if (!strcmp (g_value_get_string (value), args[0])) {
				gint j;
				model = gda_data_model_array_new_with_g_types (2,
									       G_TYPE_STRING,
									       G_TYPE_STRING);
				gda_data_model_set_column_title (model, 0, _("Attribute"));
				gda_data_model_set_column_title (model, 1, _("Value"));
				g_object_set_data_full (G_OBJECT (model), "name", 
							g_strdup_printf (_("DSN '%s' description"), args[0]),
							g_free);
				
				for (j = 0; j < 6; j++) {
					GValue *tmpvalue;
					if (gda_data_model_append_row (model, error) == -1) 
						goto onerror;
					
					g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)),
							    gda_data_model_get_column_title (dsn_list, j));
					if (! gda_data_model_set_value_at (model, 0, j, tmpvalue, error))
						goto onerror;
					gda_value_free (tmpvalue);
									 
					value = gda_data_model_get_value_at (dsn_list, j, i, error);
					if (!value ||
					    ! gda_data_model_set_value_at (model, 1, j, value, error))
						goto onerror;
				}
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
				res->u.model = model;
				g_object_unref (dsn_list);
				return res;
			}
		}
		g_object_unref (dsn_list);
		g_set_error (error, 0, 0,
			     _("Could not find any DSN named '%s'"), args[0]);
		return NULL;
	}
	else {
		model = gda_data_model_array_new_with_g_types (3,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("DSN"));
		gda_data_model_set_column_title (model, 1, _("Description"));
		gda_data_model_set_column_title (model, 2, _("Provider"));
		g_object_set_data (G_OBJECT (model), "name", _("DSN list"));
		
		for (i =0; i < nrows; i++) {
			const GValue *value;
			list = NULL;
			value = gda_data_model_get_value_at (dsn_list, 0, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (NULL, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 2, i, error);
			if (!value) 
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 1, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			
			if (gda_data_model_append_values (model, list, error) == -1)
				goto onerror;
			
			g_list_foreach (list, (GFunc) gda_value_free, NULL);
			g_list_free (list);
		}
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		g_object_unref (dsn_list);
		
		return res;
	}

 onerror:
	if (list) {
		g_list_foreach (list, (GFunc) gda_value_free, NULL);
		g_list_free (list);
	}
	g_object_unref (dsn_list);
	g_object_unref (model);
	return NULL;
}

static GdaInternalCommandResult *
extra_command_create_dsn (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;
	GdaDsnInfo newdsn;
	gchar *real_cnc, *real_provider, *user, *pass;

	if (!args[0] || !args[1]) {
		g_set_error (error, 0, 0,
			     _("Missing arguments"));
		return NULL;
	}

	newdsn.name = (gchar *) args [0];
	gda_connection_string_split ((gchar *) args[1], &real_cnc, &real_provider, &user, &pass);
	newdsn.provider = real_provider;
	newdsn.description = (gchar*) args[2];
	
	newdsn.cnc_string = real_cnc;
	newdsn.auth_string = NULL;
	if (user) {
		gchar *tmp;
		tmp = gda_rfc1738_encode (user);
		newdsn.auth_string = g_strdup_printf ("USERNAME=%s", tmp);
		g_free (tmp);
	}
	newdsn.is_system = FALSE;

	if (!newdsn.provider) {
		g_set_error (error, 0, 0,
			     _("Missing provider name"));
	}
	else if (gda_config_define_dsn (&newdsn, error)) {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}

	g_free (real_cnc);
	g_free (real_provider);
	g_free (user);
	g_free (pass);

	return res;
}

static GdaInternalCommandResult *
extra_command_remove_dsn (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	gint i;

	if (!args[0]) {
		g_set_error (error, 0, 0,
			     _("Missing DSN name"));
		return NULL;
	}
	for (i = 0; args [i]; i++) {
		if (! gda_config_remove_dsn (args[i], error))
			return NULL;
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;	
}

static GdaInternalCommandResult *
extra_command_list_providers (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *prov_list, *model = NULL;
	gint i, nrows;
	GList *list = NULL;

	prov_list = gda_config_list_providers ();
	nrows = gda_data_model_get_n_rows (prov_list);

	if (args[0]) {
		/* details about a provider */
		for (i = 0; i < nrows; i++) {
			const GValue *value;
			value = gda_data_model_get_value_at (prov_list, 0, i, error);
			if (!value)
				goto onerror;

			if (!strcmp (g_value_get_string (value), args[0])) {
				gint j;
				model = gda_data_model_array_new_with_g_types (2,
									       G_TYPE_STRING,
									       G_TYPE_STRING);
				gda_data_model_set_column_title (model, 0, _("Attribute"));
				gda_data_model_set_column_title (model, 1, _("Value"));
				g_object_set_data_full (G_OBJECT (model), "name", 
							g_strdup_printf (_("Provider '%s' description"), args[0]),
							g_free);
				
				for (j = 0; j < 5; j++) {
					GValue *tmpvalue;
					if (gda_data_model_append_row (model, error) == -1) 
						goto onerror;
					
					g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)),
							    gda_data_model_get_column_title (prov_list, j));
					if (! gda_data_model_set_value_at (model, 0, j, tmpvalue, error))
						goto onerror;
					gda_value_free (tmpvalue);
									 
					value = gda_data_model_get_value_at (prov_list, j, i, error);
					if (!value ||
					    ! gda_data_model_set_value_at (model, 1, j, value, error))
						goto onerror;
				}
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
				res->u.model = model;
				g_object_unref (prov_list);
				return res;
			}
		}
		g_object_unref (prov_list);
		g_set_error (error, 0, 0,
			     _("Could not find any provider named '%s'"), args[0]);
		return NULL;
	}
	else {
		model = gda_data_model_array_new_with_g_types (2,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Provider"));
		gda_data_model_set_column_title (model, 1, _("Description"));
		g_object_set_data (G_OBJECT (model), "name", _("Installed providers list"));
		
		for (i =0; i < nrows; i++) {
			const GValue *value;
			list = NULL;
			value = gda_data_model_get_value_at (prov_list, 0, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			value = gda_data_model_get_value_at (prov_list, 1, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			
			if (gda_data_model_append_values (model, list, error) == -1)
				goto onerror;
			
			g_list_foreach (list, (GFunc) gda_value_free, NULL);
			g_list_free (list);
		}
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		g_object_unref (prov_list);
		
		return res;
	}

 onerror:
	if (list) {
		g_list_foreach (list, (GFunc) gda_value_free, NULL);
		g_list_free (list);
	}
	g_object_unref (prov_list);
	g_object_unref (model);
	return NULL;
}

static void
vconnection_hub_foreach_cb (GdaConnection *cnc, const gchar *ns, GString *string)
{
	if (string->len > 0)
		g_string_append_c (string, '\n');
	g_string_append_printf (string, "namespace %s", ns);
}

static 
GdaInternalCommandResult *
extra_command_manage_cnc (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	/* arguments:
	 * 0 = connection name
	 * 1 = DSN or direct string
	 * 2 = user
	 * 3 = password
	 */
	if (args[0]) {
		if (args [1]) {
			/* open a new connection */
			const gchar *user = NULL, *pass = NULL;
			ConnectionSetting *cs;
			
			cs = find_connection_from_name (data, args[0]);
			if (cs) {
				g_set_error (error, 0, 0,
					     _("A connection named '%s' already exists"), args[0]);
				return NULL;
			}

			if (args[2]) {
				user = args[2];
				if (args[3])
					pass = args[3];
			}
			cs = open_connection (data, args[0], args[1], error);
			if (cs) {
				GdaInternalCommandResult *res;
				
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
				return res;
			}
			else
				return NULL;
		}
		else {
			/* switch to another already opened connection */
			ConnectionSetting *cs;

			cs = find_connection_from_name (data, args[0]);
			if (cs) {
				GdaInternalCommandResult *res;
				
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
				data->current = cs;
				return res;
			}
			else {
				if (*args [0] == '~') {
					if (*(args[0] + 1)) {
						cs = find_connection_from_name (data, args[0] + 1);
						if (!cs) {
							g_set_error (error, 0, 0,
								     _("No connection named '%s' found"), args[0] + 1);
							return NULL;
						}
					}
					else if (!data->current) {
						g_set_error (error, 0, 0,
							     _("No current connection"));
						return NULL;
					}
					else {
						if (* (data->current->name) == '~') 
							cs = find_connection_from_name (data, data->current->name + 1);
						if (!cs) {
							gchar *tmp;
							tmp = g_strdup_printf ("~%s", data->current->name);
							cs = find_connection_from_name (data, tmp);
							g_free (tmp);
						}
						if (cs) {
							GdaInternalCommandResult *res;
							
							res = g_new0 (GdaInternalCommandResult, 1);
							res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
							data->current = cs;
							return res;
						}

						cs = data->current;
					}

					ConnectionSetting *ncs = g_new0 (ConnectionSetting, 1);
					GdaMetaStore *store;
					GdaInternalCommandResult *res;
					
					ncs->name = g_strdup_printf ("~%s", cs->name);
					store = gda_connection_get_meta_store (cs->cnc);
					ncs->cnc = gda_meta_store_get_internal_connection (store);
					g_object_ref (ncs->cnc);
					ncs->parser = gda_connection_create_parser (ncs->cnc);
					if (!cs->parser)
						cs->parser = gda_sql_parser_new ();
					ncs->query_buffer = NULL;
					ncs->threader = NULL;
					ncs->meta_job_id = 0;
					
					data->settings = g_slist_append (data->settings, ncs);
					data->current = ncs;

					GError *lerror = NULL;
					if (!data->output_stream) {
						g_print (_("Getting database schema information, "
							   "this may take some time... "));
						fflush (stdout);
					}
					if (!gda_connection_update_meta_store (ncs->cnc, NULL, &lerror)) {
						if (!data->output_stream) 
							g_print (_("error: %s\n"), 
								 lerror && lerror->message ? lerror->message : _("No detail"));
						if (lerror)
							g_error_free (lerror);
					}
					else
						if (!data->output_stream) 
							g_print (_("Done.\n"));
					
					res = g_new0 (GdaInternalCommandResult, 1);
					res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
					return res;
				}

				g_set_error (error, 0, 0,
					     _("No connection named '%s' found"), args[0]);
				return NULL;
			}
		}
	}
	else {
		/* list opened connections */
		GdaDataModel *model;
		GSList *list;
		GdaInternalCommandResult *res;

		if (! data->settings) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
			res->u.txt = g_string_new (_("No opened connection"));
			return res;
		}

		model = gda_data_model_array_new_with_g_types (4,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Provider"));
		gda_data_model_set_column_title (model, 2, _("DSN or connection string"));
		gda_data_model_set_column_title (model, 3, _("Username"));
		g_object_set_data (G_OBJECT (model), "name", _("List of opened connections"));

		for (list = data->settings; list; list = list->next) {
			ConnectionSetting *cs = (ConnectionSetting *) list->data;
			GValue *value;
			gint row;
			const gchar *cstr;
			GdaServerProvider *prov;
			
			row = gda_data_model_append_row (model, NULL);
			
			value = gda_value_new_from_string (cs->name, G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);
			
			prov = gda_connection_get_provider (cs->cnc);
			if (GDA_IS_VPROVIDER_HUB (prov))
				value = gda_value_new_from_string ("", G_TYPE_STRING);
			else
				value = gda_value_new_from_string (gda_connection_get_provider_name (cs->cnc), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			if (GDA_IS_VPROVIDER_HUB (prov)) {
				GString *string = g_string_new ("");
				gda_vconnection_hub_foreach (GDA_VCONNECTION_HUB (cs->cnc), 
							     (GdaVConnectionHubFunc) vconnection_hub_foreach_cb, string);
				value = gda_value_new_from_string (string->str, G_TYPE_STRING);
				g_string_free (string, TRUE);
			}
			else {
				cstr = gda_connection_get_dsn (cs->cnc);
				if (cstr)
					value = gda_value_new_from_string (cstr, G_TYPE_STRING);
				else
					value = gda_value_new_from_string (gda_connection_get_cnc_string (cs->cnc), G_TYPE_STRING);
			}
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);

			/* only get USERNAME from the the authentication string */
			GdaQuarkList* ql;
			cstr = gda_connection_get_authentication (cs->cnc);
			ql = gda_quark_list_new_from_string (cstr);
			cstr = gda_quark_list_find (ql, "USERNAME");
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_quark_list_free (ql);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}
 
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
}

static 
GdaInternalCommandResult *
extra_command_close_cnc (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	ConnectionSetting *cs;
	if (args[0] && *args[0]) {
		cs = find_connection_from_name (data, args[0]);
		if (!cs) {
			g_set_error (error, 0, 0,
				     _("No connection named '%s' found"), args[0]);
			return NULL;
		}
	}
	else {
		/* close current connection */
		if (! data->current) {
			g_set_error (error, 0, 0,
				     _("No currently opened connection"));
			return NULL;
		}
		cs = data->current;
	}
	
	gint pos;
	pos = g_slist_index (data->settings, cs);
	data->current = g_slist_nth_data (data->settings, pos - 1);
	if (! data->current)
		data->current = g_slist_nth_data (data->settings, pos + 1);

	data->settings = g_slist_remove (data->settings, cs);
	connection_settings_free (cs);

	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;
}

static GdaInternalCommandResult *
extra_command_bind_cnc (GdaConnection *cnc, const gchar **args, GError **error, MainData *data)
{
	ConnectionSetting *cs = NULL;
	gint i, nargs = g_strv_length ((gchar **) args);
	static GdaVirtualProvider *vprovider = NULL;
	GdaConnection *virtual;
	GString *string;

	if (nargs < 3) {
		g_set_error (error, 0, 0,
				     _("Missing required connection names"));
		return NULL;
	}

	/* check for connections existance */
	cs = find_connection_from_name (data, args[0]);
	if (cs) {
		g_set_error (error, 0, 0,
			     _("A connection named '%s' already exists"), args[0]);
		return NULL;
	}
	if (! connection_name_is_valid (args[0])) {
		g_set_error (error, 0, 0,
			     _("Connection name '%s' is invalid"), args[0]);
		return NULL;
	}
	for (i = 1; i < nargs; i++) {
		cs = find_connection_from_name (data, args[i]);
		if (!cs) {
			g_set_error (error, 0, 0,
				     _("No connection named '%s' found"), args[i]);
			return NULL;
		}
	}

	/* actual virtual connection creation */
	if (!vprovider) 
		vprovider = gda_vprovider_hub_new ();
	g_assert (vprovider);

	virtual = gda_virtual_connection_open (vprovider, NULL);
	if (!virtual) {
		g_set_error (error, 0, 0, _("Could not create virtual connection"));
		return NULL;
	}

	/* add existing connections to virtual connection */
	string = g_string_new (_("Bound connections are as:"));
	for (i = 1; i < nargs; i++) {
		cs = find_connection_from_name (data, args[i]);
		if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), cs->cnc, args[i], error)) {
			g_object_unref (virtual);
			g_string_free (string, TRUE);
			return NULL;
		}
		g_string_append_printf (string, "\n   %s in the '%s' namespace", args[i], args[i]);
        }

	cs = g_new0 (ConnectionSetting, 1);
	cs->name = g_strdup (args[0]);
	cs->cnc = virtual;
	cs->parser = gda_connection_create_parser (virtual);
	cs->query_buffer = NULL;
	cs->threader = NULL;
	cs->meta_job_id = 0;

	data->settings = g_slist_append (data->settings, cs);
	data->current = cs;

	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = string;
	return res;
}

static GdaInternalCommandResult *
extra_command_copyright (GdaConnection *cnc, const gchar **args,
			 GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new ("This program is free software; you can redistribute it and/or modify\n"
				   "it under the terms of the GNU General Public License as published by\n"
				   "the Free Software Foundation; either version 2 of the License, or\n"
				   "(at your option) any later version.\n\n"
				   "This program is distributed in the hope that it will be useful,\n"
				   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
				   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
				   "GNU General Public License for more details.\n");
	return res;
}

static GdaInternalCommandResult *
extra_command_quit (GdaConnection *cnc, const gchar **args,
		    GError **error, MainData *data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EXIT;
	return res;
}

static GdaInternalCommandResult *
extra_command_cd (GdaConnection *cnc, const gchar **args,
		  GError **error, MainData *data)
{
	const gchar *dir = NULL;
#define DIR_LENGTH 256
	static char start_dir[DIR_LENGTH];
	static gboolean init_done = FALSE;

	if (!init_done) {
		init_done = TRUE;
		memset (start_dir, 0, DIR_LENGTH);
		if (getcwd (start_dir, DIR_LENGTH) <= 0) {
			/* default to $HOME */
#ifdef G_OS_WIN32
			TO_IMPLEMENT;
			strncpy (start_dir, "/", 2);
#else
			struct passwd *pw;
			
			pw = getpwuid (geteuid ());
			if (!pw) {
				g_set_error (error, 0, 0, _("Could not get home directory: %s"), strerror (errno));
				return NULL;
			}
			else {
				gint l = strlen (pw->pw_dir);
				if (l > DIR_LENGTH - 1)
					strncpy (start_dir, "/", 2);
				else
					strncpy (start_dir, pw->pw_dir, l + 1);
			}
#endif
		}
	}

	if (args[0]) 
		dir = args[0];
	else 
		dir = start_dir;

	if (dir) {
		if (chdir (dir) == 0) {
			GdaInternalCommandResult *res;

			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
			res->u.txt = g_string_new ("");
			g_string_append_printf (res->u.txt, _("Working directory is now: %s"), dir);
			return res;
		}
		else
			g_set_error (error, 0, 0, _("Could not change working directory to '%s': %s"),
				     dir, strerror (errno));
	}

	return NULL;
}

static GdaInternalCommandResult *
extra_command_edit_buffer (GdaConnection *cnc, const gchar **args,
			   GError **error, MainData *data)
{
	gchar *filename = NULL;
	static gchar *editor_name = NULL;
	gchar *command = NULL;
	gint systemres;
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		goto end_of_command;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0])
		filename = (gchar *) args[0];
	else {
		/* use a temp file */
		gint fd;
		fd = g_file_open_tmp (NULL, &filename, error);
		if (fd < 0)
			goto end_of_command;
		if (write (fd, data->current->query_buffer->str, 
			   data->current->query_buffer->len) != data->current->query_buffer->len) {
			g_set_error (error, 0, 0,
				     _("Could not write to temporary file '%s': %s"),
				     filename, strerror (errno));
			close (fd);
			goto end_of_command;
		}
		close (fd);
	}

	if (!editor_name) {
		editor_name = getenv("GDA_SQL_EDITOR");
		if (!editor_name)
			editor_name = getenv("EDITOR");
		if (!editor_name)
			editor_name = getenv("VISUAL");
		if (!editor_name) {
#ifdef G_OS_WIN32
			editor_name = "notepad.exe";
#else
			editor_name = "vi";
#endif
		}
	}
#ifdef G_OS_WIN32
#ifndef __CYGWIN__
#define SYSTEMQUOTE "\""
#else
#define SYSTEMQUOTE ""
#endif
	command = g_strdup_printf ("%s\"%s\" \"%s\"%s", SYSTEMQUOTE, editor_name, filename, SYSTEMQUOTE);
#else
	command = g_strdup_printf ("exec %s '%s'", editor_name, filename);
#endif

	systemres = system (command);
	if (systemres == -1) {
                g_set_error (error, 0, 0,
			     _("could not start editor '%s'"), editor_name);
		goto end_of_command;
	}
        else if (systemres == 127) {
		g_set_error (error, 0, 0,
			     _("Could not start /bin/sh"));
		goto end_of_command;
	}
	else {
		if (! args[0]) {
			gchar *str;
			
			if (!g_file_get_contents (filename, &str, NULL, error))
				goto end_of_command;
			g_string_assign (data->current->query_buffer, str);
			g_free (str);
		}
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

 end_of_command:

	g_free (command);
	if (! args[0]) {
		g_unlink (filename);
		g_free (filename);
	}

	return res;
}

static GdaInternalCommandResult *
extra_command_reset_buffer (GdaConnection *cnc, const gchar **args,
			    GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");
	else 
		g_string_assign (data->current->query_buffer, "");

	if (args[0]) {
		const gchar *filename = NULL;
		gchar *str;

		filename = args[0];
		if (!g_file_get_contents (filename, &str, NULL, error))
			return NULL;

		g_string_assign (data->current->query_buffer, str);
		g_free (str);
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	return res;
}

static GdaInternalCommandResult *
extra_command_show_buffer (GdaConnection *cnc, const gchar **args,
			   GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (data->current->query_buffer->str);

	return res;
}

static GdaInternalCommandResult *
extra_command_exec_buffer (GdaConnection *cnc, const gchar **args,
			   GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		/* load named query buffer first */
		res = extra_command_query_buffer_from_dict (cnc, args, error, data);
		if (!res)
			return NULL;
		gda_internal_command_exec_result_free (res);
		res = NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");
	if (*data->current->query_buffer->str != 0)
		res = command_execute (data, data->current->query_buffer->str, error);
	else {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}

	return res;
}

static GdaInternalCommandResult *
extra_command_write_buffer (GdaConnection *cnc, const gchar **args,
			    GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");
	if (!args[0]) 
		g_set_error (error, 0, 0, 
			     _("Missing FILE to write to"));
	else {
		if (g_file_set_contents (args[0], data->current->query_buffer->str, -1, error)) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}

	return res;
}

#define QUERY_BUFFERS_TABLE_NAME "gda_sql_query_buffers"
#define QUERY_BUFFERS_TABLE_DESC \
	"<table name=\"" QUERY_BUFFERS_TABLE_NAME "\"> "			    \
	"   <column name=\"id\" type=\"gint\" pkey=\"TRUE\" autoinc=\"TRUE\"/>"	    \
	"   <column name=\"name\"/>"				    \
	"   <column name=\"sql\"/>"				    \
	"</table>"
#define QUERY_BUFFERS_TABLE_INSERT \
	"INSERT INTO " QUERY_BUFFERS_TABLE_NAME " (name, sql) VALUES (##name::string, ##sql::string)"
#define QUERY_BUFFERS_TABLE_SELECT \
	"SELECT name, sql FROM " QUERY_BUFFERS_TABLE_NAME " ORDER BY name"
#define QUERY_BUFFERS_TABLE_SELECT_ONE \
	"SELECT sql FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"
#define QUERY_BUFFERS_TABLE_DELETE \
	"DELETE FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"

static GdaInternalCommandResult *
extra_command_query_buffer_list_dict (GdaConnection *cnc, const gchar **args,
				      GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	/* Meta store's init */
	GdaMetaStore *mstore;
	mstore = gda_connection_get_meta_store (data->current->cnc);
	if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
		g_set_error (error, 0, 0,
			     _("Can't initialize dictionary to store query buffers"));
		return NULL;
	}
		
	/* actual list retreival */
	static GdaStatement *sel_stmt = NULL;
	GdaDataModel *model;
	if (!sel_stmt) {
		sel_stmt = gda_sql_parser_parse_string (data->current->parser, 
							QUERY_BUFFERS_TABLE_SELECT, NULL, NULL);
		g_assert (sel_stmt);
	}

	GdaConnection *store_cnc;
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	model = gda_connection_statement_execute_select (store_cnc, sel_stmt, NULL, error);
	if (!model)
		return NULL;

	gda_data_model_set_column_title (model, 0, _("Query buffer name"));
	gda_data_model_set_column_title (model, 1, _("SQL"));
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_to_dict (GdaConnection *cnc, const gchar **args,
				    GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");
	if (*data->current->query_buffer->str != 0) {
		/* find a suitable name */
		gchar *qname;
		if (args[0] && *args[0]) 
			qname = g_strdup ((gchar *) args[0]);
		else {
			g_set_error (error, 0, 0,
				     _("Missing query buffer name"));
			return NULL;
		}

		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0,
				     _("Can't initialize dictionary to store query buffers"));
			g_free (qname);
			return NULL;
		}
		
		/* actual store of the statement */
		static GdaStatement *ins_stmt = NULL;
		static GdaSet *ins_params = NULL;
		if (!ins_stmt) {
			ins_stmt = gda_sql_parser_parse_string (data->current->parser, 
								QUERY_BUFFERS_TABLE_INSERT, NULL, NULL);
			g_assert (ins_stmt);
			g_assert (gda_statement_get_parameters (ins_stmt, &ins_params, NULL));
		}

		if (! gda_set_set_holder_value (ins_params, error, "name", qname) ||
		    ! gda_set_set_holder_value (ins_params, error, "sql", data->current->query_buffer->str)) {
			g_free (qname);
			return NULL;
		}
		g_free (qname);
		
		GdaConnection *store_cnc;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		if (gda_connection_statement_execute_non_select (store_cnc, ins_stmt, ins_params,
								 NULL, error) == -1)
			return NULL;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, 0, 0,
			     _("Query buffer is empty"));

	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_from_dict (GdaConnection *cnc, const gchar **args,
				      GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0]) {
		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0,
				     _("Can't initialize dictionary to store query buffers"));
			return NULL;
		}
		
		/* query retreival */
		static GdaStatement *sel_stmt = NULL;
		static GdaSet *sel_params = NULL;
		GdaDataModel *model;
		const GValue *cvalue;
		if (!sel_stmt) {
			sel_stmt = gda_sql_parser_parse_string (data->current->parser, 
								QUERY_BUFFERS_TABLE_SELECT_ONE, NULL, NULL);
			g_assert (sel_stmt);
			g_assert (gda_statement_get_parameters (sel_stmt, &sel_params, NULL));
		}

		if (! gda_set_set_holder_value (sel_params, error, "name", args[0]))
			return NULL;

		GdaConnection *store_cnc;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		model = gda_connection_statement_execute_select (store_cnc, sel_stmt, sel_params, error);
		if (!model)
			return NULL;
		
		if ((gda_data_model_get_n_rows (model) == 1) &&
		    (cvalue = gda_data_model_get_value_at (model, 0, 0, NULL))) {
			g_string_assign (data->current->query_buffer, g_value_get_string (cvalue));
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, 0, 0,
				     _("Could not find query buffer named '%s'"), args[0]);
		g_object_unref (model);
	}
	else
		g_set_error (error, 0, 0,
			     _("Missing query buffer name"));
		
	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_delete_dict (GdaConnection *cnc, const gchar **args,
					GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	if (!data->current) {
		g_set_error (error, 0, 0, _("No connection opened"));
		return NULL;
	}

	if (!data->current->query_buffer) 
		data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0]) {
		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0,
				     _("Can't initialize dictionary to store query buffers"));
			return NULL;
		}
		
		/* query retreival */
		static GdaStatement *del_stmt = NULL;
		static GdaSet *del_params = NULL;
		if (!del_stmt) {
			del_stmt = gda_sql_parser_parse_string (data->current->parser, 
								QUERY_BUFFERS_TABLE_DELETE, NULL, NULL);
			g_assert (del_stmt);
			g_assert (gda_statement_get_parameters (del_stmt, &del_params, NULL));
		}

		if (! gda_set_set_holder_value (del_params, error, "name", args[0]))
			return NULL;

		GdaConnection *store_cnc;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		if (gda_connection_statement_execute_non_select (store_cnc, del_stmt, del_params,
								 NULL, error) == -1)
			return NULL;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, 0, 0,
			     _("Missing query buffer name"));
		
	return res;
}

static void foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model);
static GdaInternalCommandResult *
extra_command_set (GdaConnection *cnc, const gchar **args,
		   GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *value = NULL;

	if (args[0] && *args[0]) {
		pname = args[0];
		if (args[1] && *args[1])
			value = args[1];
	}

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (data->parameters, pname);
		if (param) {
			if (value) {
				/* set param's value */
				if (!strcmp (value, "_null_")) {
					if (! gda_holder_set_value (param, NULL, error))
						return NULL;
				}
				else {
					GdaServerProvider *prov;
					GdaDataHandler *dh;
					GValue *gvalue;

					prov = gda_connection_get_provider (data->current->cnc);
					dh = gda_server_provider_get_data_handler_g_type (prov, data->current->cnc,
											 gda_holder_get_g_type (param));
					gvalue = gda_data_handler_get_value_from_str (dh, value, gda_holder_get_g_type (param));
					if (! gda_holder_take_value (param, gvalue, error))
						return NULL;
				}

				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_SET;
				res->u.set = gda_set_new (NULL);
				gda_set_add_holder (res->u.set, gda_holder_copy (param));
			}
		}
		else {
			if (value) {
				/* create parameter */
				if (!strcmp (value, "_null_"))
					value = NULL;
				param = gda_holder_new_string (pname, value);
				g_hash_table_insert (data->parameters, g_strdup (pname), param);
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
			}
			else 
				g_set_error (error, 0, 0,
					     _("No parameter named '%s' defined"), pname);
		}
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		model = gda_data_model_array_new_with_g_types (2,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		g_object_set_data (G_OBJECT (model), "name", _("List of defined parameters"));
		g_hash_table_foreach (data->parameters, (GHFunc) foreach_param_set, model);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}
		
	return res;
}

static void 
foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model)
{
	gint row;
	gchar *str;
	GValue *value;
	row = gda_data_model_append_row (model, NULL);

	value = gda_value_new_from_string (pname, G_TYPE_STRING);
	gda_data_model_set_value_at (model, 0, row, value, NULL);
	gda_value_free (value);
			
	str = gda_holder_get_value_str (param, NULL);
	value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
	gda_data_model_set_value_at (model, 1, row, value, NULL);
	gda_value_free (value);
}

static const GValue *
get_table_value_at_cell (GdaConnection *cnc, GError **error, MainData *data,
			 const gchar *table, const gchar *column, const gchar *row_cond,
			 GdaDataModel **out_model_of_value)
{
	const GValue *retval = NULL;

	*out_model_of_value = NULL;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rcolumn;
	
	if (gda_sql_identifier_needs_quotes (table))
		rtable = gda_sql_identifier_add_quotes (table);
	else
		rtable = g_strdup (table);
	if (gda_sql_identifier_needs_quotes (column))
		rcolumn = gda_sql_identifier_add_quotes (column);
	else
		rcolumn = g_strdup (column);
	sql = g_strdup_printf ("SELECT %s FROM %s WHERE %s", rcolumn, rtable, row_cond);
	g_free (rtable);
	g_free (rcolumn);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (data->current->parser, sql, &remain, error);
	if (!stmt) {
		g_free (sql);
		return NULL;
	}
	if (remain) {
		g_set_error (error, 0, 0,
			     _("Wrong row condition"));
		g_free (sql);
		return NULL;
	}
	g_object_unref (stmt);

	/* execute statement */
	GdaInternalCommandResult *tmpres;
	tmpres = execute_external_command (data, sql, error);
	g_free (sql);
	if (!tmpres)
		return NULL;
	gboolean errorset = FALSE;
	if (tmpres->type == GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL) {
		GdaDataModel *model;
		model = tmpres->u.model;
		if (gda_data_model_get_n_rows (model) == 1) {
			retval = gda_data_model_get_value_at (model, 0, 0, error);
			if (!retval)
				errorset = TRUE;
			else
				*out_model_of_value = g_object_ref (model);
		}
	}
	gda_internal_command_exec_result_free (tmpres);

	if (!retval && !errorset)
		g_set_error (error, 0, 0,
			     _("No unique row identified"));

	return retval;
}

static GdaInternalCommandResult *
extra_command_set2 (GdaConnection *cnc, const gchar **args,
		    GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *filename = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *row_cond = NULL;
	gint whichargs = 0;

	if (!cnc) {
                g_set_error (error, 0, 0, _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                pname = args[0];
                if (args[1] && *args[1]) {
			if (args[2] && *args[2]) {
				table = args[1];
				column = args[2];
				if (args[3] && *args[3]) {
					row_cond = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0,
							     _("Too many arguments"));
						return NULL;
					}
					whichargs = 1;
				}
			}
			else {
				filename = args[1];
				whichargs = 2;
			}
		}
        }

	if (whichargs == 1) {
		/* param from an existing blob */
		const GValue *value;
		GdaDataModel *model = NULL;
		value = get_table_value_at_cell (cnc, error, data, table, column, row_cond, &model);
		if (value) {
			GdaHolder *param = g_hash_table_lookup (data->parameters, pname);
			if (param) 
				g_hash_table_remove (data->parameters, pname);
		
			param = gda_holder_new (G_VALUE_TYPE (value));
			g_assert (gda_holder_set_value (param, value, NULL));
			g_hash_table_insert (data->parameters, g_strdup (pname), param);
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		if (model)
			g_object_unref (model);
	}
	else if (whichargs == 2) {
		/* param from filename */
		GdaHolder *param = g_hash_table_lookup (data->parameters, pname);
		GValue *bvalue;
		if (param) 
			g_hash_table_remove (data->parameters, pname);
		
		param = gda_holder_new (GDA_TYPE_BLOB);
		bvalue = gda_value_new_blob_from_file (filename);
		g_assert (gda_holder_take_value (param, bvalue, NULL));
		g_hash_table_insert (data->parameters, g_strdup (pname), param);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else 
		g_set_error (error, 0, 0,
                             _("Wrong number of arguments"));

	return res;
}

static GdaInternalCommandResult *
extra_command_export (GdaConnection *cnc, const gchar **args,
		      GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;

	const gchar *pname = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;
	gint whichargs = 0;

	if (!cnc) {
                g_set_error (error, 0, 0, _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                table = args[0];
                pname = args[0];
                if (args[1] && *args[1]) {
                        column = args[1];
			filename = args[1];
			if (args[2] && *args[2]) {
				row_cond = args[2];
				if (args[3] && *args[3]) {
					filename = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0,
							     _("Too many arguments"));
						return NULL;
					}
					else
						whichargs = 1;
				}
			}
			else {
				whichargs = 2;
			}
		}
        }

	const GValue *value = NULL;
	GdaDataModel *model = NULL;

	if (whichargs == 1) 
		value = get_table_value_at_cell (cnc, error, data, table, column, row_cond, &model);
	else if (whichargs == 2) {
		GdaHolder *param = g_hash_table_lookup (data->parameters, pname);
		if (!pname) 
			g_set_error (error, 0, 0,
				     _("No parameter named '%s' defined"), pname);
		else
			value = gda_holder_get_value (param);
	}
	else 
		g_set_error (error, 0, 0,
                             _("Wrong number of arguments"));

	if (value) {
		/* to file through this blob */
		gboolean done = FALSE;

		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			const GdaBlob *fblob = gda_value_get_blob (value);
			if (gda_blob_op_write (tblob->op, (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, 0, 0,
					     _("Could not write file"));
			else
				done = TRUE;
			gda_value_free (vblob);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			GdaBlob *fblob = g_new0 (GdaBlob, 1);
			const GdaBinary *fbin = gda_value_get_binary (value);
			((GdaBinary *) fblob)->data = fbin->data;
			((GdaBinary *) fblob)->binary_length = fbin->binary_length;
			if (gda_blob_op_write (tblob->op, (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, 0, 0,
					     _("Could not write file"));
			else
				done = TRUE;
			g_free (fblob);
			gda_value_free (vblob);
		}
		else {
			gchar *str;
			str = gda_value_stringify (value);
			if (g_file_set_contents (filename, str, -1, error))
				done = TRUE;
			g_free (str);
		}
		
		if (done) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}
	if (model)
		g_object_unref (model);

				
	return res;
}


static GdaInternalCommandResult *
extra_command_unset (GdaConnection *cnc, const gchar **args,
		     GError **error, MainData *data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;

	if (args[0] && *args[0]) 
		pname = args[0];

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (data->parameters, pname);
		if (param) {
			g_hash_table_remove (data->parameters, pname);
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, 0, 0,
				     _("No parameter named '%s' defined"), pname);
	}
	else {
		g_hash_table_destroy (data->parameters);
		data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
		
	return res;
}

#define DO_UNLINK(x) g_unlink(x)
static void
graph_func_child_died_cb (GPid pid, gint status, gchar *fname)
{
	DO_UNLINK (fname);
	g_free (fname);
	g_spawn_close_pid (pid);
}

static gchar *
create_graph_from_meta_struct (GdaConnection *cnc, GdaMetaStruct *mstruct, GError **error)
{
#define FNAME "graph.dot"
	gchar *graph;

	graph = gda_meta_struct_dump_as_graph (mstruct, GDA_META_GRAPH_COLUMNS, error);
	if (!graph)
		return NULL;

	/* do something with the graph */
	gchar *result = NULL;
	if (g_file_set_contents (FNAME, graph, -1, error)) {
		const gchar *viewer;
		const gchar *format;
		
		viewer = g_getenv ("GDA_SQL_VIEWER_PNG");
		if (viewer)
			format = "png";
		else {
			viewer = g_getenv ("GDA_SQL_VIEWER_PDF");
			if (viewer)
				format = "pdf";
		}
		if (viewer) {
			static gint counter = 0;
			gchar *tmpname, *suffix;
			gchar *argv[] = {"dot", NULL, "-o",  NULL, FNAME, NULL};
			guint eventid = 0;

			suffix = g_strdup_printf (".gda_graph_tmp-%d", counter++);
			tmpname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
			g_free (suffix);
			argv[1] = g_strdup_printf ("-T%s", format);
			argv[3] = tmpname;
			if (g_spawn_sync (NULL, argv, NULL,
					  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
					  NULL, NULL,
					  NULL, NULL, NULL, NULL)) {
				gchar *vargv[3];
				GPid pid;
				vargv[0] = (gchar *) viewer;
				vargv[1] = tmpname;
				vargv[2] = NULL;
				if (g_spawn_async (NULL, vargv, NULL,
						   G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | 
						   G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD, 
						   NULL, NULL,
						   &pid, NULL)) {
					eventid = g_child_watch_add (pid, (GChildWatchFunc) graph_func_child_died_cb, 
								     tmpname);
				}
			}
			if (eventid == 0) {
				DO_UNLINK (tmpname);
				g_free (tmpname);
			}
			g_free (argv[1]);
			result = g_strdup_printf (_("Graph written to '%s'\n"), FNAME);
		}
		else 
			result = g_strdup_printf (_("Graph written to '%s'\n"
						    "Use 'dot' (from the GraphViz package) to create a picture, for example:\n"
						    "\tdot -Tpng -o graph.png %s\n"
						    "Note: set the GDA_SQL_VIEWER_PNG or GDA_SQL_VIEWER_PDF environment "
						    "variables to view the graph\n"), FNAME, FNAME);
	}
	g_free (graph);
	return result;
}

static GdaInternalCommandResult *
extra_command_graph (GdaConnection *cnc, const gchar **args,
		     GError **error, MainData *data)
{
	gchar *result;
	GdaMetaStruct *mstruct;

	if (!cnc) {
		g_set_error (error, 0, 0, _("No current connection"));
		return NULL;
	}

	mstruct = gda_internal_command_build_meta_struct (cnc, args, error);
	if (!mstruct)
		return NULL;
	
	result = create_graph_from_meta_struct (cnc, mstruct, error);
	g_object_unref (mstruct);
	if (result) {
		GdaInternalCommandResult *res;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (result);
		g_free (result);
		return res;
	}
	else 
		return NULL;
}

#ifdef NONE
static GdaInternalCommandResult *
extra_command_lo_update (GdaConnection *cnc, const gchar **args,
			 GError **error, MainData *data)
{
	GdaInternalCommandResult *res;

	const gchar *table = NULL;
        const gchar *blob_col = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;

	if (!cnc) {
                g_set_error (error, 0, 0, _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                filename = args[0];
                if (args[1] && *args[1]) {
                        table = args[1];
			if (args[2] && *args[2]) {
				blob_col = args[2];
				if (args[3] && *args[3]) {
					row_cond = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0,
							     _("Too many arguments"));
						return NULL;
					}
				}
			}
		}
        }
	if (!row_cond) {
		g_set_error (error, 0, 0,
                             _("Missing arguments"));
		return NULL;
	}

	g_print ("file: #%s#\n", filename);
	g_print ("table: #%s#\n", table);
	g_print ("col: #%s#\n", blob_col);
	g_print ("cond: #%s#\n", row_cond);
	TO_IMPLEMENT;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rblob_col;
	
	if (gda_sql_identifier_needs_quotes (table))
		rtable = gda_sql_identifier_add_quotes (table);
	else
		rtable = g_strdup (table);
	if (gda_sql_identifier_needs_quotes (blob_col))
		rblob_col = gda_sql_identifier_add_quotes (blob_col);
	else
		rblob_col = g_strdup (blob_col);
	sql = g_strdup_printf ("UPDATE %s SET %s = ##blob::GdaBlob WHERE %s", rtable, rblob_col, row_cond);
	g_free (rtable);
	g_free (rblob_col);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (data->current->parser, sql, &remain, error);
	g_free (sql);
	if (!stmt)
		return NULL;
	if (remain) {
		g_set_error (error, 0, 0,
			     _("Wrong row condition"));
		return NULL;
	}

	/* prepare execution environment */
	GdaSet *params;
	GdaHolder *h;
	GValue *blob;

	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	h = gda_set_get_holder (params, "blob");
	g_assert (h);
	blob = gda_value_new_blob_from_file (filename);
	if (!gda_holder_take_value (h, blob, error)) {
		gda_value_free (blob);
		g_object_unref (params);
		g_object_unref (stmt);
		return NULL;
	}

	/* execute statement */
	gint nrows;
	nrows = gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, error);
	g_object_unref (params);
	g_object_unref (stmt);
	if (nrows == -1)
		return NULL;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;
}
#endif


static gchar **
args_as_string_func (const gchar *str)
{
	return g_strsplit (str, " ", 2);
}

static gchar **
args_as_string_set (const gchar *str)
{
	return g_strsplit (str, " ", 3);
}



static char **
completion_func (const char *text, int start, int end)
{
#ifdef HAVE_READLINE
	ConnectionSetting *cs = main_data->current;
	char **array;
	gchar **gda_compl;
	gint i, nb_compl;

	if (!cs)
		return NULL;
	gda_compl = gda_completion_list_get (cs->cnc, rl_line_buffer, start, end);
	if (!gda_compl)
		return NULL;

	for (nb_compl = 0; gda_compl[nb_compl]; nb_compl++);

	array = malloc (sizeof (char*) * (nb_compl + 1));
	for (i = 0; i < nb_compl; i++) {
		char *str;
		gchar *gstr;
		gint l;

		gstr = gda_compl[i];
		l = strlen (gstr) + 1;
		str = malloc (sizeof (char) * l);
		memcpy (str, gstr, l);
		array[i] = str;
	}
	array[i] = NULL;
	g_strfreev (gda_compl);
	return array;
#else
	return NULL;
#endif
}
