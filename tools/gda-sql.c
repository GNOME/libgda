/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Nathan <nbinont@yahoo.ca>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012 Daniel Mustieles <daniel.mustieles@gmail.com>
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

#include "tool.h"
#include "tool-utils.h"
#include "gda-sql.h"
#include <virtual/libgda-virtual.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include "tool-input.h"
#include "tool-output.h"
#include "config-info.h"
#include "command-exec.h"
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <sys/types.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-blob-op.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef G_OS_WIN32
#include <signal.h>
typedef void (*sighandler_t)(int);
#include <pwd.h>
#else
#include <stdlib.h>
#include <windows.h>
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif

#ifdef HAVE_LIBSOUP
#include "web-server.h"
#endif

/* options */
gboolean show_version = FALSE;

gchar *single_command = NULL;
gchar *commandsfile = NULL;
gboolean interactive = FALSE;

gboolean list_configs = FALSE;
gboolean list_providers = FALSE;

gboolean list_data_files = FALSE;
gchar *purge_data_files = NULL;

gchar *outfile = NULL;
gboolean has_threads;

#ifdef HAVE_LIBSOUP
gint http_port = -1;
gchar *auth_token = NULL;
#endif

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Show version information and exit", NULL },	
        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { "command", 'C', 0, G_OPTION_ARG_STRING, &single_command, "Run only single command (SQL or internal) and exit", "command" },
        { "commands-file", 'f', 0, G_OPTION_ARG_STRING, &commandsfile, "Execute commands from file, then exit (except if -i specified)", "filename" },
	{ "interactive", 'i', 0, G_OPTION_ARG_NONE, &interactive, "Keep the console opened after executing a file (-f option)", NULL },
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
#ifdef HAVE_LIBSOUP
	{ "http-port", 's', 0, G_OPTION_ARG_INT, &http_port, "Run embedded HTTP server on specified port", "port" },
	{ "http-token", 't', 0, G_OPTION_ARG_STRING, &auth_token, "Authentication token (required to authenticate clients)", "token phrase" },
#endif
        { "data-files-list", 0, 0, G_OPTION_ARG_NONE, &list_data_files, "List files used to hold information related to each connection", NULL },
        { "data-files-purge", 0, 0, G_OPTION_ARG_STRING, &purge_data_files, "Remove some files used to hold information related to each connection. Criteria has to be in 'all', 'non-dsn', or 'non-exist-dsn'", "criteria"},
        { NULL, 0, 0, 0, NULL, NULL, NULL }
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


/* structure to hold program's data */
typedef struct {
	SqlConsole *term_console;
	GSList *settings; /* list all the ConnectionSetting */
	ToolCommandGroup *limit_commands;
	ToolCommandGroup *all_commands;

	FILE *input_stream;

	GString *partial_command;

	GdaSet *options;
	GHashTable *parameters; /* key = name, value = G_TYPE_STRING GdaHolder */
#define LAST_DATA_MODEL_NAME "_"
	GHashTable *mem_data_models; /* key = name, value = the named GdaDataModel, ref# kept in hash table */

#ifdef HAVE_LIBSOUP
	WebServer *server;
#endif
} MainData;

MainData *main_data;
GString *prompt = NULL;
GMainLoop *main_loop = NULL;
gboolean exit_requested = FALSE;

static ConnectionSetting *get_current_connection_settings (SqlConsole *console);
static char   **completion_func (const char *text, const gchar *line, int start, int end);
static void     compute_prompt (SqlConsole *console, GString *string, gboolean in_command,
				gboolean for_readline, ToolOutputFormat format);
static gboolean set_output_file (const gchar *file, GError **error);
static gboolean set_input_file (const gchar *file, GError **error);
static gchar   *data_model_to_string (SqlConsole *console, GdaDataModel *model);
static void     output_data_model (GdaDataModel *model);
static void     output_string (const gchar *str);
static ConnectionSetting *open_connection (SqlConsole *console, const gchar *cnc_name, const gchar *cnc_string,
					   GError **error);
static void connection_settings_free (ConnectionSetting *cs);

static gboolean treat_line_func (const gchar *cmde, gboolean *out_cmde_exec_ok);
static const char *prompt_func (void);

static GdaSet *make_options_set_from_gdasql_options (const gchar *context);
static void compute_term_color_attribute (void);


/* commands manipulation */
static void               build_commands (MainData *md);
static gboolean           command_is_complete (const gchar *command);
static ToolCommandResult *command_execute (SqlConsole *console,
					   const gchar *command,
					   GdaStatementModelUsage usage, GError **error);

static void                display_result (ToolCommandResult *res);

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	MainData *data;
	int exit_status = EXIT_SUCCESS;
	gboolean show_welcome = TRUE;
	GValue *value;
	GdaHolder *opt;
	prompt = g_string_new ("");

	context = g_option_context_new (_("[DSN|connection string]..."));        
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_ignore_unknown_options (context, TRUE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_fprintf  (stderr, "Can't parse arguments: %s\n", error->message);
		return EXIT_FAILURE;
        }
        g_option_context_free (context);

	if (show_version) {
		g_print (_("GDA SQL console version " PACKAGE_VERSION "\n"));
		return 0;
	}

	g_setenv ("GDA_CONFIG_SYNCHRONOUS", "1", TRUE);
	setlocale (LC_ALL, "");
        gda_init ();

	has_threads = g_thread_supported ();
	data = g_new0 (MainData, 1);
	data->options = gda_set_new_inline (3,
					    "csv_names_on_first_line", G_TYPE_BOOLEAN, FALSE,
					    "csv_quote", G_TYPE_STRING, "\"",
					    "csv_separator", G_TYPE_STRING, ",");
#ifdef HAVE_LDAP
#define DEFAULT_LDAP_ATTRIBUTES "cn"
	opt = gda_holder_new_string ("ldap_dn", "dn");
	value = gda_value_new (G_TYPE_STRING);
	g_value_set_string (value, _("Defines how the DN column is handled for LDAP searched (among \"dn\", \"rdn\" and \"none\")"));
	gda_holder_set_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION, value, NULL);
	gda_set_add_holder (data->options, opt);
	g_object_unref (opt);

	opt = gda_holder_new_string ("ldap_attributes", DEFAULT_LDAP_ATTRIBUTES);
	g_value_set_string (value, _("Defines the LDAP attributes which are fetched by default by LDAP commands"));
	gda_holder_set_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION, value, NULL);
	gda_value_free (value);
	gda_set_add_holder (data->options, opt);
	g_object_unref (opt);
#endif

	value = gda_value_new (G_TYPE_STRING);
	g_value_set_string (value, _("Set to TRUE when the 1st line of a CSV file holds column names"));
	opt = gda_set_get_holder (data->options, "csv_names_on_first_line");
	gda_holder_set_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION, value, NULL);
	g_value_set_string (value, "NAMES_ON_FIRST_LINE");
	gda_holder_set_attribute (opt, "csv", value, NULL);

	g_value_set_string (value, _("Quote character for CSV format"));
	opt = gda_set_get_holder (data->options, "csv_quote");
	gda_holder_set_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION, value, NULL);
	g_value_set_string (value, "QUOTE");
	gda_holder_set_attribute (opt, "csv", value, NULL);

	g_value_set_string (value, _("Separator character for CSV format"));
	opt = gda_set_get_holder (data->options, "csv_separator");
	gda_holder_set_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION, value, NULL);
	g_value_set_string (value, "SEPARATOR");
	gda_holder_set_attribute (opt, "csv", value, NULL);
	gda_value_free (value);

	data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	data->mem_data_models = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	main_data = data;

	/* build internal command s list */
	build_commands (data);
	data->term_console = gda_sql_console_new ("TERM");
	data->term_console->command_group = main_data->all_commands;
	compute_term_color_attribute ();

	/* output file */
	if (outfile) {
		if (! set_output_file (outfile, &error)) {
			g_print ("Can't set output file as '%s': %s\n", outfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* treat here lists of providers and defined DSN */
	if (list_providers) {
		if (argc == 2) {
			single_command = g_strdup_printf (".lp %s", argv[1]);
			argc = 1;
			show_welcome = FALSE;
		}
		else {
			GdaDataModel *model = config_info_list_all_providers ();
			output_data_model (model);
			g_object_unref (model);
			goto cleanup;
		}
	}
	if (list_configs) {
		if (argc == 2) {
			single_command = g_strdup_printf (".l %s", argv[1]);
			argc = 1;
			show_welcome = FALSE;
		}
		else {
			GdaDataModel *model = config_info_list_all_dsn ();
			output_data_model (model);
			g_object_unref (model);
			goto cleanup;
		}
	}
	if (list_data_files) {
		gchar *confdir;
		GdaDataModel *model;

		confdir = config_info_compute_dict_directory ();
		g_print (_("All files are in the directory: %s\n"), confdir);
		g_free (confdir);
		model = config_info_list_data_files (&error);
		if (model) {
			output_data_model (model);
			g_object_unref (model);
		}
		else
			g_print (_("Can't get the list of files used to store information about each connection: %s\n"),
				 error->message);	
		goto cleanup;
	}
	if (purge_data_files) {
		gchar *tmp;
		tmp = config_info_purge_data_files (purge_data_files, &error);
		if (tmp) {
			output_string (tmp);
			g_free (tmp);
		}
		if (error)
			g_print (_("Error while purging files used to store information about each connection: %s\n"),
				 error->message);	
		goto cleanup;
	}

	/* commands file */
	if (commandsfile) {
		if (! set_input_file (commandsfile, &error)) {
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
	if (show_welcome && !data->term_console->output_stream) {
#ifdef G_OS_WIN32
		HANDLE wHnd;
		SMALL_RECT windowSize = {0, 0, 139, 49};

		wHnd = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTitle ("GDA SQL console, version " PACKAGE_VERSION);
		SetConsoleWindowInfo (wHnd, TRUE, &windowSize);
#endif
		gchar *c1, *c2, *c3, *c4;
		tool_output_color_print (TOOL_COLOR_BOLD, main_data->term_console->output_format,
			     _("Welcome to the GDA SQL console, version " PACKAGE_VERSION));
		g_print ("\n\n");
		c1 = tool_output_color_string (TOOL_COLOR_BOLD, main_data->term_console->output_format, ".copyright");
		c2 = tool_output_color_string (TOOL_COLOR_BOLD, main_data->term_console->output_format, ".?");
		c3 = tool_output_color_string (TOOL_COLOR_BOLD, main_data->term_console->output_format, ".help");
		c4 = tool_output_color_string (TOOL_COLOR_BOLD, main_data->term_console->output_format, ".q");
		g_print (_("Type: %s to show usage and distribution terms\n"
			   "      %s or %s for help with internal commands\n"
			   "      %s (or CTRL-D) to quit (the '.' can be replaced by a '\\')\n"
			   "      or any SQL query terminated by a semicolon\n\n"), c1, c2, c3, c4);
		g_free (c1);
		g_free (c2);
		g_free (c3);
		g_free (c4);
	}

	/* open connections if specified */
	gint i;
	for (i = 1; i < argc; i++) {
		/* open connection */
		ConnectionSetting *cs;
		GdaDsnInfo *info = NULL;
		gchar *str;

		if (*argv[i] == '-')
			continue;
#define SHEBANG "#!"
#define SHEBANGLEN 2

		/* try to see if argv[i] corresponds to a file starting with SHEBANG */
		FILE *file;
		file = fopen (argv[i], "r");
		if (file) {
			char buffer [SHEBANGLEN + 1];
			size_t nread;
			nread = fread (buffer, 1, SHEBANGLEN, file);
			if (nread == SHEBANGLEN) {
				buffer [SHEBANGLEN] = 0;
				if (!strcmp (buffer, SHEBANG)) {
					if (! set_input_file (argv[i], &error)) {
						g_print ("Can't read file '%s': %s\n", commandsfile,
							 error->message);
						exit_status = EXIT_FAILURE;
						fclose (file);
						goto cleanup;
					}
					fclose (file);
					continue;
				}
			}
			fclose (file);
		}

                info = gda_config_get_dsn_info (argv[i]);
		if (info)
			str = g_strdup (info->name);
		else
			str = g_strdup_printf ("c%d", i-1);
		if (!data->term_console->output_stream) {
			gchar *params, *prov, *user;
			gda_connection_string_split (argv[i], &params, &prov, &user, NULL);
			g_print (_("Opening connection '%s' for: "), str);
			if (prov)
				g_print ("%s://", prov);
			if (user)
				g_print ("%s@", user);
			if (params)
				g_print ("%s", params);
			g_print ("\n");
			g_free (params);
			g_free (prov);
			g_free (user);
		}
		cs = open_connection (data->term_console, str, argv[i], &error);
		config_info_modify_argv (argv[i]);
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
		if (!data->term_console->output_stream) {
			gchar *params, *prov, *user;
			gda_connection_string_split (envstr, &params, &prov, &user, NULL);
			g_print (_("Opening connection '%s' (GDA_SQL_CNC environment variable): "), str);
			if (prov)
				g_print ("%s://", prov);
			if (user)
				g_print ("%s@", user);
			if (params)
				g_print ("%s", params);
			g_print ("\n");
			g_free (params);
			g_free (prov);
			g_free (user);
		}
		cs = open_connection (data->term_console, str, envstr, &error);
		g_free (str);
		if (!cs) {
			g_print (_("Can't open connection defined by GDA_SQL_CNC: %s\n"),
				 error && error->message ? error->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

#ifdef HAVE_LIBSOUP
	/* start HTTP server if requested */
	if (http_port > 0) {
		main_data->server = web_server_new (http_port, auth_token);
		if (!main_data->server) {
			g_print (_("Can't run HTTP server on port %d\n"), http_port);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}
#endif

	/* process commands which need to be executed as specified by the command line args */
	if (single_command) {
		treat_line_func (single_command, NULL);
		if (!data->term_console->output_stream)
			g_print ("\n");
		goto cleanup;
	}

	if (data->input_stream) {
		gchar *cmde;
		for (;;) {
			cmde = input_from_stream (data->input_stream);
			if (cmde) {
				gboolean command_ok;
				treat_line_func (cmde, &command_ok);
				g_free (cmde);
				if (! command_ok)
					break;
			}
			else
				break;
		}
		if (interactive && !cmde && isatty (fileno (stdin)))
			set_input_file (NULL, NULL);
		else {
			if (!data->term_console->output_stream)
				g_print ("\n");
			goto cleanup;
		}
	}

	if (exit_requested)
		goto cleanup;

	/* set up interactive commands */
	setup_sigint_handler ();
	init_input ((TreatLineFunc) treat_line_func, prompt_func, NULL);
	tool_input_set_completion_func (data->term_console->command_group, completion_func, ".\\");

	/* run main loop */
	main_loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (main_loop);		
	g_main_loop_unref (main_loop);

 cleanup:
	/* cleanups */
	g_slist_foreach (data->settings, (GFunc) connection_settings_free, NULL);
	set_input_file (NULL, NULL); 
	set_output_file (NULL, NULL); 
	end_input ();

	g_free (data);

	return exit_status;
}

static void
compute_term_color_attribute (void)
{
	g_assert (main_data);
	main_data->term_console->output_format &= ~(TOOL_OUTPUT_FORMAT_COLOR_TERM);
	if (!main_data->term_console->output_stream || isatty (fileno (main_data->term_console->output_stream))) {
		const gchar *term;
		main_data->term_console->output_format |= TOOL_OUTPUT_FORMAT_COLOR_TERM;
		term = g_getenv ("TERM");
                if (term && !strcmp (term, "dumb"))
			main_data->term_console->output_format ^= TOOL_OUTPUT_FORMAT_COLOR_TERM;
	}
}

static const char *
prompt_func (void)
{
	/* compute a new prompt */
	compute_prompt (NULL, prompt, main_data->partial_command == NULL ? FALSE : TRUE, TRUE,
			TOOL_OUTPUT_FORMAT_DEFAULT |
			(main_data->term_console->output_format & TOOL_OUTPUT_FORMAT_COLOR_TERM));
	return (char*) prompt->str;
}

/* @cmde is stolen here */
static gboolean
treat_line_func (const gchar *cmde, gboolean *out_cmde_exec_ok)
{
	gchar *loc_cmde = NULL;

	if (out_cmde_exec_ok)
		*out_cmde_exec_ok = TRUE;

	if (!cmde) {
		save_history (NULL, NULL);
		if (!main_data->term_console->output_stream)
			g_print ("\n");
		exit_requested = TRUE;
		goto exit;
	}
	
	loc_cmde = g_strdup (cmde);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		add_to_history (loc_cmde);
		if (!main_data->partial_command) {
			/* enable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_PARTIAL_COMMAND;
			main_data->partial_command = g_string_new (loc_cmde);
		}
		else {
			g_string_append_c (main_data->partial_command, '\n');
			g_string_append (main_data->partial_command, loc_cmde);
		}
		if (command_is_complete (main_data->partial_command->str)) {
			/* execute command */
			ToolCommandResult *res;
			FILE *to_stream;
			GError *error = NULL;
			
			if ((*main_data->partial_command->str != '\\') && (*main_data->partial_command->str != '.')) {
				if (main_data->term_console->current) {
					if (!main_data->term_console->current->query_buffer)
						main_data->term_console->current->query_buffer = g_string_new ("");
					g_string_assign (main_data->term_console->current->query_buffer,
							 main_data->partial_command->str);
				}
			}
			
			if (main_data->term_console->output_stream)
				to_stream = main_data->term_console->output_stream;
			else
				to_stream = stdout;
			res = command_execute (main_data->term_console, main_data->partial_command->str,
					       GDA_STATEMENT_MODEL_RANDOM_ACCESS, &error);
			
			if (!res) {
				if (!error ||
				    (error->domain != GDA_SQL_PARSER_ERROR) ||
				    (error->code != GDA_SQL_PARSER_EMPTY_SQL_ERROR)) {
					g_fprintf (to_stream, "%sERROR:%s ",
						   tool_output_color_s (TOOL_COLOR_RED, main_data->term_console->output_format),
						   tool_output_color_s (TOOL_COLOR_RESET, main_data->term_console->output_format));
					g_fprintf (to_stream,
						   "%s\n", 
						   error && error->message ? error->message : _("No detail"));
					if (out_cmde_exec_ok)
						*out_cmde_exec_ok = FALSE;
				}
				if (error) {
					g_error_free (error);
					error = NULL;
				}
			}
			else {
				display_result (res);
				if (res->type == TOOL_COMMAND_RESULT_EXIT) {
					tool_command_result_free (res);
					exit_requested = TRUE;
					goto exit;
				}
				tool_command_result_free (res);
			}
			g_string_free (main_data->partial_command, TRUE);
			main_data->partial_command = NULL;
			
			/* disable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_DISABLED;
		}
	}
	g_free (loc_cmde);
	return TRUE;

 exit:
	g_free (loc_cmde);
	if (main_loop)
		g_main_loop_quit (main_loop);
	return FALSE;
}

static void
display_result (ToolCommandResult *res)
{
	switch (res->type) {
	case TOOL_COMMAND_RESULT_TXT_STDOUT: 
		g_print ("%s", res->u.txt->str);
		if (res->u.txt->str [strlen (res->u.txt->str) - 1] != '\n')
			g_print ("\n");
		fflush (NULL);
		break;
	case TOOL_COMMAND_RESULT_EMPTY:
		break;
	case TOOL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		for (list = res->u.multiple_results; list; list = list->next)
			display_result ((ToolCommandResult *) list->data);
		break;
	}
	case TOOL_COMMAND_RESULT_EXIT:
		break;
	default: {
		gchar *str;
		str = tool_output_result_to_string (res, main_data->term_console->output_format,
						    main_data->term_console->output_stream,
						    main_data->options);
		output_string (str);
		g_free (str);
	}
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
			sigint_handler_status = SIGINT_HANDLER_DISABLED;
		}
		/* show a new prompt */
		compute_prompt (NULL, prompt, FALSE, FALSE,
				TOOL_OUTPUT_FORMAT_DEFAULT |
				(main_data->term_console->output_format & TOOL_OUTPUT_FORMAT_COLOR_TERM));
		g_print ("\n%s", prompt->str);
		compute_prompt (NULL, prompt, FALSE, TRUE,
				TOOL_OUTPUT_FORMAT_DEFAULT |
				(main_data->term_console->output_format & TOOL_OUTPUT_FORMAT_COLOR_TERM));
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
		gint i, len;
		len = strlen (command);
		for (i = len - 1; i > 0; i--) {
			if ((command [i] != ' ') && (command [i] != '\n') && (command [i] != '\r'))
				break;
		}
		if (command [i] == ';')
			return TRUE;
		else
			return FALSE;
	}
}


/*
 * command_execute
 */
static ToolCommandResult *execute_sql_command (SqlConsole *console, const gchar *command,
					       GdaStatementModelUsage usage,
					       GError **error);
static ToolCommandResult *
command_execute (SqlConsole *console, const gchar *command, GdaStatementModelUsage usage, GError **error)
{
	ConnectionSetting *cs;

	g_assert (console);
	cs = get_current_connection_settings (console);
	if (!command || !(*command))
                return NULL;
        if ((*command == '\\') || (*command == '.'))
		return tool_command_group_execute (console->command_group, command + 1, console->output_format,
						   console, error);

	else if (*command == '#') {
		/* nothing to do */
		ToolCommandResult *res;
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
        else {
                if (!cs) {
                        g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
				     "%s", _("No connection specified"));
                        return NULL;
                }
                if (!gda_connection_is_opened (cs->cnc)) {
                        g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_CONNECTION_CLOSED_ERROR,
				     "%s", _("Connection closed"));
                        return NULL;
                }

                return execute_sql_command (console, command, usage, error);
        }
}

/*
 * execute_sql_command
 *
 * Executes an SQL statement as understood by the DBMS
 */
static ToolCommandResult *
execute_sql_command (SqlConsole *console, const gchar *command,
		     GdaStatementModelUsage usage, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaBatch *batch;
	const GSList *stmt_list;
	GdaStatement *stmt;
	GdaSet *params;
	GObject *obj;
	const gchar *remain = NULL;
	ConnectionSetting *cs;

	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	batch = gda_sql_parser_parse_string_as_batch (cs->parser, command, &remain, error);
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
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("More than one SQL statement"));
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
			GdaHolder *h_in_data = g_hash_table_lookup (main_data->parameters, gda_holder_get_id (h));
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
						g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
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
				}
			}
			else {
				if (! gda_holder_is_valid (h)) {
					g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
						     _("No internal parameter named '%s' required by query"), 
						     gda_holder_get_id (h));
					g_free (res);
					res = NULL;
					goto cleanup;
				}
			}
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->was_in_transaction_before_exec = gda_connection_get_transaction_status (cs->cnc) ? TRUE : FALSE;
	res->cnc = g_object_ref (cs->cnc);
	obj = gda_connection_statement_execute (cs->cnc, stmt, params, usage, NULL, error);
	if (!obj) {
		g_free (res);
		res = NULL;
	}
	else {
		if (GDA_IS_DATA_MODEL (obj)) {
			res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = GDA_DATA_MODEL (obj);
		}
		else if (GDA_IS_SET (obj)) {
			res->type = TOOL_COMMAND_RESULT_SET;
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

static ConnectionSetting *
get_current_connection_settings (SqlConsole *console)
{
	if (console->current) {
		g_assert (g_slist_find (main_data->settings, console->current));
		return console->current;
	}
	else
		return NULL;
}

/**
 * clears and modifies @string to hold a prompt.
 */
static void
compute_prompt (SqlConsole *console, GString *string, gboolean in_command, gboolean for_readline, ToolOutputFormat format)
{
	gchar *prefix = NULL;
	ConnectionSetting *cs;
	g_assert (string);
	gchar suffix = '>';

	if (!console)
		console = main_data->term_console;
	g_string_set_size (string, 0);

	if (exit_requested)
		return;

	if (format & TOOL_OUTPUT_FORMAT_COLOR_TERM) {
		const gchar *color;
		color = tool_output_color_s (TOOL_COLOR_BOLD, format);
		if (color && *color) {
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_START_IGNORE);
#endif
			g_string_append (string, color);
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_END_IGNORE);
#endif
		}
	}
	cs = get_current_connection_settings (console);
	if (cs) {
		prefix = cs->name;
		if (cs->cnc) {
			GdaTransactionStatus *ts;
			ts = gda_connection_get_transaction_status (cs->cnc);
			if (ts)
				suffix='[';
		}
	}
	else
		prefix = "gda";

	if (in_command) {
		gint i, len;
		len = strlen (prefix);
		for (i = 0; i < len; i++)
			g_string_append_c (string, ' ');
		g_string_append_c (string, suffix);
		g_string_append_c (string, ' ');		
	}
	else 
		g_string_append_printf (string, "%s%c ", prefix, suffix);

	if (format & TOOL_OUTPUT_FORMAT_COLOR_TERM) {
		const gchar *color;
		color = tool_output_color_s (TOOL_COLOR_RESET, TOOL_OUTPUT_FORMAT_COLOR_TERM);
		if (color && *color) {
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_START_IGNORE);
#endif
			g_string_append (string, color);
#ifdef HAVE_READLINE
			if (for_readline)
				g_string_append_c (string, RL_PROMPT_END_IGNORE);
#endif
		}
	}
}

/*
 * Check that the @arg string can safely be passed to a shell
 * to be executed, i.e. it does not contain dangerous things like "rm -rf *"
 */
// coverity[ +tainted_string_sanitize_content : arg-0 ]
static gboolean
check_shell_argument (const gchar *arg)
{
	const gchar *ptr;
	g_assert (arg);

	/* check for starting spaces */
	for (ptr = arg; *ptr == ' '; ptr++);
	if (!*ptr)
		return FALSE; /* only spaces is not allowed */

	/* check for the rest */
	for (; *ptr; ptr++) {
		if (! g_ascii_isalnum (*ptr) && (*ptr != G_DIR_SEPARATOR))
			return FALSE;
	}
	return TRUE;
}

/*
 * Change the output file, set to %NULL to be back on stdout
 */
static gboolean
set_output_file (const gchar *file, GError **error)
{
	if (main_data->term_console->output_stream) {
		if (main_data->term_console->output_is_pipe) {
			pclose (main_data->term_console->output_stream);
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_DFL);
#endif
		}
		else
			fclose (main_data->term_console->output_stream);
		main_data->term_console->output_stream = NULL;
		main_data->term_console->output_is_pipe = FALSE;
	}

	if (file) {
		gchar *copy = g_strdup (file);
		g_strchug (copy);

		if (*copy != '|') {
			/* output to a file */
			main_data->term_console->output_stream = g_fopen (copy, "w");
			if (!main_data->term_console->output_stream) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     _("Can't open file '%s' for writing: %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
			main_data->term_console->output_is_pipe = FALSE;
		}
		else {
			/* output to a pipe */
			if (check_shell_argument (copy+1)) {
				main_data->term_console->output_stream = popen (copy+1, "w");
				if (!main_data->term_console->output_stream) {
					g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
						     _("Can't open pipe '%s': %s"), 
						     copy,
						     strerror (errno));
					g_free (copy);
					return FALSE;
				}
			}
			else {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     _("Can't open pipe '%s': %s"),
					     copy + 1,
					     "program name must only contain alphanumeric characters");
				g_free (copy);
				return FALSE;
			}
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_IGN);
#endif
			main_data->term_console->output_is_pipe = TRUE;
		}
		g_free (copy);
	}

	compute_term_color_attribute ();
	return TRUE;
}

/*
 * Change the input file, set to %NULL to be back on stdin
 */
static gboolean
set_input_file (const gchar *file, GError **error)
{
	if (main_data->input_stream) {
		fclose (main_data->input_stream);
		main_data->input_stream = NULL;
	}

	if (file) {
		if (*file == '~') {
			gchar *tmp;
			tmp = g_strdup_printf ("%s%s", g_get_home_dir (), file+1);
			main_data->input_stream = g_fopen (tmp, "r");
			g_free (tmp);
		}
		else
			main_data->input_stream = g_fopen (file, "r");
		if (!main_data->input_stream) {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
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
find_connection_from_name (const gchar *name)
{
	ConnectionSetting *cs = NULL;
	GSList *list;
	for (list = main_data->settings; list; list = list->next) {
		if (!strcmp (name, ((ConnectionSetting *) list->data)->name)) {
			cs = (ConnectionSetting *) list->data;
			break;
		}
	}
	return cs;
}

typedef struct {
	ConnectionSetting *cs;
	gboolean cannot_lock;
	gboolean result;
	GError   *error;
} MetaUpdateData;

static gpointer thread_start_update_meta_store (MetaUpdateData *data);
static void thread_ok_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);
static void thread_cancelled_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);
static void conn_closed_cb (GdaConnection *cnc, gpointer data);

static gchar* read_hidden_passwd ();
static void user_password_needed (GdaDsnInfo *info, const gchar *real_provider,
				  gboolean *out_need_username, gboolean *out_need_password,
				  gboolean *out_need_password_if_user);

/*
 * Open a connection
 */
static ConnectionSetting*
open_connection (SqlConsole *console, const gchar *cnc_name, const gchar *cnc_string, GError **error)
{
	GdaConnection *newcnc = NULL;
	ConnectionSetting *cs = NULL;
	gchar *real_cnc_string;

	g_assert (console);

	if (cnc_name && ! connection_name_is_valid (cnc_name)) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Connection name '%s' is invalid"), cnc_name);
		return NULL;
	}
	
	GdaDsnInfo *info;
	gboolean need_user, need_pass, need_password_if_user;
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
		real_cnc_string = g_strdup_printf ("%s://DB_DIR=%s;EXTRA_FUNCTIONS=TRUE;DB_NAME=%s", pname, e1, e2);
		g_free (e1);
		g_free (e2);
		gda_connection_string_split (real_cnc_string, &real_cnc, &real_provider, &user, &pass);
	}
	else {
		gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
		real_cnc_string = g_strdup (cnc_string);
	}

	info = gda_config_get_dsn_info (real_cnc);
	user_password_needed (info, real_provider, &need_user, &need_pass, &need_password_if_user);

	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed connection string '%s'"), cnc_string);
		g_free (real_cnc_string);
		return NULL;
	}

	if ((!user || !pass) && info && info->auth_string) {
		GdaQuarkList* ql;
		const gchar *tmp;
		ql = gda_quark_list_new_from_string (info->auth_string);
		if (!user) {
			tmp = gda_quark_list_find (ql, "USERNAME");
			if (tmp)
				user = g_strdup (tmp);
		}
		if (!pass) {
			tmp = gda_quark_list_find (ql, "PASSWORD");
			if (tmp)
				pass = g_strdup (tmp);
		}
		gda_quark_list_free (ql);
	}
	if (need_user && ((user && !*user) || !user)) {
		gchar buf[80], *ptr;
		g_print (_("\tUsername for '%s': "), cnc_name);
		if (! fgets (buf, 80, stdin)) {
			g_free (real_cnc);
			g_free (user);
			g_free (pass);
			g_free (real_provider);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
				     _("No username for '%s'"), cnc_string);
			g_free (real_cnc_string);
			return NULL;
		}
		for (ptr = buf; *ptr; ptr++) {
			if (*ptr == '\n') {
				*ptr = 0;
				break;
			}
		}
		g_free (user);
		user = g_strdup (buf);
	}
	if (user)
		need_pass = need_password_if_user;
	if (need_pass && ((pass && !*pass) || !pass)) {
		gchar *tmp;
		g_print (_("\tPassword for '%s': "), cnc_name);
		tmp = read_hidden_passwd ();
		g_print ("\n");
		if (! tmp) {
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
		pass = tmp;
	}

	if (user || pass) {
		GString *string;
		string = g_string_new ("");
		if (user) {
			gchar *enc;
			enc = gda_rfc1738_encode (user);
			g_string_append_printf (string, "USERNAME=%s", enc);
			g_free (enc);
		}
		if (pass) {
			gchar *enc;
			enc = gda_rfc1738_encode (pass);
			if (user)
				g_string_append_c (string, ';');
			g_string_append_printf (string, "PASSWORD=%s", enc);
			g_free (enc);
		}
		real_auth_string = g_string_free (string, FALSE);
	}
	
	if (info && !real_provider)
		newcnc = gda_connection_open_from_dsn (real_cnc_string, real_auth_string,
						       GDA_CONNECTION_OPTIONS_THREAD_SAFE |
						       GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);
	else 
		newcnc = gda_connection_open_from_string (NULL, real_cnc_string, real_auth_string,
							  GDA_CONNECTION_OPTIONS_THREAD_SAFE |
							  GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);

	g_free (real_cnc_string);
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);
	g_free (real_auth_string);

	if (newcnc) {
		gchar *dict_file_name = NULL;
		gchar *cnc_string;
		const gchar *rootname;
		gint i;
		g_object_set (G_OBJECT (newcnc), "execution-timer", TRUE, NULL);
		g_object_get (G_OBJECT (newcnc),
			      "cnc-string", &cnc_string, NULL);
		dict_file_name = config_info_compute_dict_file_name (info, cnc_string);
		g_free (cnc_string);

		cs = g_new0 (ConnectionSetting, 1);
		if (cnc_name && *cnc_name)
			rootname = cnc_name;
		else
			rootname = "c";
		if (gda_sql_get_connection (rootname)) {
			for (i = 0; ; i++) {
				gchar *tmp;
				tmp = g_strdup_printf ("%s%d", rootname, i);
				if (gda_sql_get_connection (tmp))
					g_free (tmp);
				else {
					cs->name = tmp;
					break;
				}
			}
		}
		else
			cs->name = g_strdup (rootname);
		cs->parser = gda_connection_create_parser (newcnc);
		if (!cs->parser)
			cs->parser = gda_sql_parser_new ();
		cs->cnc = newcnc;
		cs->query_buffer = NULL;
		cs->threader = NULL;
		cs->meta_job_id = 0;

		main_data->settings = g_slist_append (main_data->settings, cs);
		console->current = cs;

		/* show date format */
		GDateDMY order[3];
		gchar sep;
		if (gda_connection_get_date_format (cs->cnc, &order[0], &order[1], &order[2], &sep, NULL)) {
			g_print (_("Date format for this connection will be: %s%c%s%c%s, where YYYY is the year, MM the month and DD the day\n"),
				 (order [0] == G_DATE_DAY) ? "DD" : ((order [0] == G_DATE_MONTH) ? "MM" : "YYYY"), sep,
				 (order [1] == G_DATE_DAY) ? "DD" : ((order [1] == G_DATE_MONTH) ? "MM" : "YYYY"), sep,
				 (order [2] == G_DATE_DAY) ? "DD" : ((order [2] == G_DATE_MONTH) ? "MM" : "YYYY"));
		}

		/* dictionay related work */
		GdaMetaStore *store;
		gboolean update_store = FALSE;

		if (dict_file_name) {
			if (! g_file_test (dict_file_name, G_FILE_TEST_EXISTS))
				update_store = TRUE;
			store = gda_meta_store_new_with_file (dict_file_name);
			g_print (_("All the information related to the '%s' connection will be stored "
				   "in the '%s' file\n"),
				 cs->name, dict_file_name);
		}
		else {
			store = gda_meta_store_new (NULL);
			if (store)
				update_store = TRUE;
		}

		config_info_update_meta_store_properties (store, newcnc);

		g_object_set (G_OBJECT (cs->cnc), "meta-store", store, NULL);
		if (update_store) {
			GError *lerror = NULL;
			
			if (has_threads) {
				MetaUpdateData *thdata;
				cs->threader = (GdaThreader*) gda_threader_new ();
				thdata = g_new0 (MetaUpdateData, 1);
				thdata->cs = cs;
				thdata->cannot_lock = FALSE;
				cs->meta_job_id = gda_threader_start_thread (cs->threader, 
									     (GThreadFunc) thread_start_update_meta_store,
									     thdata,
									     (GdaThreaderFunc) thread_ok_cb_update_meta_store,
									     (GdaThreaderFunc) thread_cancelled_cb_update_meta_store, 
									     &lerror);
				if (cs->meta_job_id == 0) {
					if (!console->output_stream) 
						g_print (_("Error getting meta data in background: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
			}
			else {
				if (!console->output_stream) {
					g_print (_("Getting database schema information for connection '%s', this may take some time... "),
						 cs->name);
					fflush (stdout);
				}
				
				if (!gda_connection_update_meta_store (cs->cnc, NULL, &lerror)) {
					if (!console->output_stream) 
						g_print (_("error: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				else
					if (!console->output_stream) 
						g_print (_("Done.\n"));
			}
		}
		if (store)
			g_object_unref (store);
		g_free (dict_file_name);
	}

	if (cs) {
		g_signal_connect (cs->cnc, "conn-closed",
				  G_CALLBACK (conn_closed_cb), NULL);
	}

	return cs;
}

static void
user_password_needed (GdaDsnInfo *info, const gchar *real_provider,
		      gboolean *out_need_username, gboolean *out_need_password,
		      gboolean *out_need_password_if_user)
{
	GdaProviderInfo *pinfo = NULL;
	if (out_need_username)
		*out_need_username = FALSE;
	if (out_need_password)
		*out_need_password = FALSE;
	if (out_need_password_if_user)
		*out_need_password_if_user = FALSE;

	if (real_provider)
		pinfo = gda_config_get_provider_info (real_provider);
	else if (info)
		pinfo = gda_config_get_provider_info (info->provider);

	if (pinfo && pinfo->auth_params) {
		if (out_need_password) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "PASSWORD");
			if (h && gda_holder_get_not_null (h))
				*out_need_password = TRUE;
		}
		if (out_need_username) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "USERNAME");
			if (h && gda_holder_get_not_null (h))
				*out_need_username = TRUE;
		}
		if (out_need_password_if_user) {
			if (gda_set_get_holder (pinfo->auth_params, "PASSWORD") &&
			    gda_set_get_holder (pinfo->auth_params, "USERNAME"))
				*out_need_password_if_user = TRUE;
		}
	}
}

static gchar*
read_hidden_passwd (void)
{
	gchar *p, password [100];

#ifdef HAVE_TERMIOS_H
	int fail;
	struct termios termio;
	
	fail = tcgetattr (0, &termio);
	if (fail)
		return NULL;
	
	termio.c_lflag &= ~ECHO;
	fail = tcsetattr (0, TCSANOW, &termio);
	if (fail)
		return NULL;
#else
#ifdef G_OS_WIN32
	HANDLE  t = NULL;
        LPDWORD t_orig = NULL;

	/* get a new handle to turn echo off */
	t_orig = (LPDWORD) malloc (sizeof (DWORD));
	t = GetStdHandle (STD_INPUT_HANDLE);
	
	/* save the old configuration first */
	GetConsoleMode (t, t_orig);
	
	/* set to the new mode */
	SetConsoleMode (t, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
#endif
#endif
	
	p = fgets (password, sizeof (password) - 1, stdin);

#ifdef HAVE_TERMIOS_H
	termio.c_lflag |= ECHO;
	tcsetattr (0, TCSANOW, &termio);
#else
#ifdef G_OS_WIN32
	SetConsoleMode (t, *t_orig);
	fflush (stdout);
	free (t_orig);
#endif
#endif
	
	if (!p)
		return NULL;

	for (p = password; *p; p++) {
		if (*p == '\n') {
			*p = 0;
			break;
		}
	}
	p = g_strdup (password);
	memset (password, 0, sizeof (password));

	return p;
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
thread_ok_cb_update_meta_store (G_GNUC_UNUSED GdaThreader *threader, G_GNUC_UNUSED guint job, MetaUpdateData *data)
{
	data->cs->meta_job_id = 0;
	if (data->cannot_lock) {
		GError *lerror = NULL;
		if (!main_data->term_console->output_stream) {
			g_print (_("Getting database schema information for connection '%s', this may take some time... "),
				 data->cs->name);
			fflush (stdout);
		}

		if (!gda_connection_update_meta_store (data->cs->cnc, NULL, &lerror)) {
			if (!main_data->term_console->output_stream) 
				g_print (_("error: %s\n"), 
					 lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
		else
			if (!main_data->term_console->output_stream) 
				g_print (_("Done.\n"));
	}
	if (data->error)
		g_error_free (data->error);

	g_free (data);
}

static void
thread_cancelled_cb_update_meta_store (G_GNUC_UNUSED GdaThreader *threader, G_GNUC_UNUSED guint job,
				       MetaUpdateData *data)
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
	if (cs->cnc) {
		g_signal_handlers_disconnect_by_func (cs->cnc,
						      G_CALLBACK (conn_closed_cb), NULL);
		g_object_unref (cs->cnc);
	}
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
output_data_model (GdaDataModel *model)
{
	gchar *str;
	str = data_model_to_string (main_data->term_console, model);
	output_string (str);
	g_free (str);
}

static GdaSet *
make_options_set_from_gdasql_options (const gchar *context)
{
	GdaSet *expopt = NULL;
	GSList *list, *nlist = NULL;
	for (list = main_data->options->holders; list; list = list->next) {
		GdaHolder *param = GDA_HOLDER (list->data);
		const GValue *cvalue;
		cvalue = gda_holder_get_attribute (param, context);
		if (!cvalue)
			continue;

		GdaHolder *nparam;
		const GValue *cvalue2;
		cvalue2 = gda_holder_get_value (param);
		nparam = gda_holder_new (G_VALUE_TYPE (cvalue2));
		g_object_set ((GObject*) nparam, "id", g_value_get_string (cvalue), NULL);
		g_assert (gda_holder_set_value (nparam, cvalue2, NULL));
		nlist = g_slist_append (nlist, nparam);
	}
	if (nlist) {
		expopt = gda_set_new (nlist);
		g_slist_free (nlist);
	}
	return expopt;
}

static gchar *
data_model_to_string (SqlConsole *console, GdaDataModel *model)
{
	static gboolean env_set = FALSE;
	ToolOutputFormat of;

	g_assert (console);
	if (!GDA_IS_DATA_MODEL (model))
		return NULL;

	g_hash_table_insert (main_data->mem_data_models, g_strdup (LAST_DATA_MODEL_NAME), g_object_ref (model));

	if (!env_set) {
		if (! getenv ("GDA_DATA_MODEL_DUMP_TITLE"))
			g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "Yes", TRUE);
		if (! getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY"))
			g_setenv ("GDA_DATA_MODEL_NULL_AS_EMPTY", "Yes", TRUE);
		if (! console->output_stream || isatty (fileno (console->output_stream))) {
			if (! getenv ("GDA_DATA_MODEL_DUMP_TRUNCATE"))
				g_setenv ("GDA_DATA_MODEL_DUMP_TRUNCATE", "-1", TRUE);
		}
		env_set = TRUE;
	}
	
	of = console->output_format;
	if (of & TOOL_OUTPUT_FORMAT_DEFAULT) {
		gchar *tmp;
		GdaSet *options;
		options = gda_set_new_inline (1, "MAX_WIDTH", G_TYPE_INT, -1);
		tmp = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_TABLE, NULL, 0, NULL, 0, options);
		g_object_unref (options);
		if (GDA_IS_DATA_SELECT (model)) {
			gchar *tmp2, *tmp3;
			gdouble etime;
			g_object_get ((GObject*) model, "execution-delay", &etime, NULL);
			tmp2 = g_strdup_printf ("%s: %.03f s", _("Execution delay"), etime);
			tmp3 = g_strdup_printf ("%s\n%s", tmp, tmp2);
			g_free (tmp);
			g_free (tmp2);
			return tmp3;
		}
		else
			return tmp;
	}
	else if (of & TOOL_OUTPUT_FORMAT_XML)
		return gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							NULL, 0,
							NULL, 0, NULL);
	else if (of & TOOL_OUTPUT_FORMAT_CSV) {
		gchar *retval;
		GdaSet *optexp;
		optexp = make_options_set_from_gdasql_options ("csv");
		retval = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
							  NULL, 0,
							  NULL, 0, optexp);
		if (optexp)
			g_object_unref (optexp);
		return retval;
	}
	else if (of & TOOL_OUTPUT_FORMAT_HTML) {
		xmlBufferPtr buffer;
		xmlNodePtr top, div, table, node, row_node, col_node, header, meta;
		gint ncols, nrows, i, j;
		gchar *str;

		top = xmlNewNode (NULL, BAD_CAST "html");
		header = xmlNewChild (top, NULL, BAD_CAST "head", NULL);
		meta = xmlNewChild (header, NULL, BAD_CAST "meta", NULL);
		xmlSetProp (meta, BAD_CAST "http-equiv", BAD_CAST "content-type");
		xmlSetProp (meta, BAD_CAST "content", BAD_CAST "text/html; charset=UTF-8");
		div = xmlNewChild (top, NULL, BAD_CAST "body", NULL);
		table = xmlNewChild (div, NULL, BAD_CAST "table", NULL);
		xmlSetProp (table, BAD_CAST "border", BAD_CAST "1");
		
		if (g_object_get_data (G_OBJECT (model), "name"))
			xmlNewTextChild (table, NULL, BAD_CAST "caption", g_object_get_data (G_OBJECT (model), "name"));

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

		node = xmlNewChild (div, NULL, BAD_CAST "p", NULL);
		str = g_strdup_printf (ngettext ("(%d row)", "(%d rows)", nrows), nrows);
		xmlNodeSetContent (node, BAD_CAST str);
		g_free (str);

		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, top, 0, 1);
		str = g_strdup ((gchar *) xmlBufferContent (buffer));
		xmlBufferFree (buffer);
		xmlFreeNode (top);
		return str;
	}
	else
		TO_IMPLEMENT;

	return NULL;
}

static void
output_string (const gchar *str)
{
	FILE *to_stream;
	gboolean append_nl = FALSE;
	gint length;
	static gint force_no_pager = -1;

	if (!str)
		return;

	if (force_no_pager < 0) {
		/* still unset... */
		if (getenv (TOOL_NO_PAGER))
			force_no_pager = 1;
		else
			force_no_pager = 0;	
	}

	length = strlen (str);
	if (*str && (str[length - 1] != '\n'))
		append_nl = TRUE;

	if (main_data->term_console->output_stream)
		to_stream = main_data->term_console->output_stream;
	else
		to_stream = stdout;

	if (!force_no_pager && isatty (fileno (to_stream))) {
		/* use pager */
		FILE *pipe;
		const char *pager;
#ifndef G_OS_WIN32
		sighandler_t phandler;
#endif
		pager = getenv ("PAGER");
		if (!pager)
			pager = "more";
		if (!check_shell_argument (pager)) {
			g_warning ("Invalid PAGER value: must only contain alphanumeric characters");
			return;
		}
		else
			pipe = popen (pager, "w");
#ifndef G_OS_WIN32
		phandler = signal (SIGPIPE, SIG_IGN);
#endif
		if (append_nl)
			g_fprintf (pipe, "%s\n", str);
		else
			g_fprintf (pipe, "%s", str);
		pclose (pipe);
#ifndef G_OS_WIN32
		signal (SIGPIPE, phandler);
#endif
	}
	else {
		if (append_nl)
			g_fprintf (to_stream, "%s\n", str);
		else
			g_fprintf (to_stream, "%s", str);
	}
}

static ToolCommandResult *extra_command_copyright (ToolCommand *command, guint argc, const gchar **argv,
						   SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_option (ToolCommand *command, guint argc, const gchar **argv,
						SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_quit (ToolCommand *command, guint argc, const gchar **argv,
					      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_cd (ToolCommand *command, guint argc, const gchar **argv,
					    SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_set_output (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_set_output_format (ToolCommand *command, guint argc, const gchar **argv,
							   SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_set_input (ToolCommand *command, guint argc, const gchar **argv,
						   SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_echo (ToolCommand *command, guint argc, const gchar **argv,
					      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_qecho (ToolCommand *command, guint argc, const gchar **argv,
					       SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_list_dsn (ToolCommand *command, guint argc, const gchar **argv,
						  SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_create_dsn (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_remove_dsn (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_list_providers (ToolCommand *command, guint argc, const gchar **argv,
							SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_manage_cnc (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
static gchar **extra_command_manage_cnc_compl (const gchar *text);
static ToolCommandResult *extra_command_close_cnc (ToolCommand *command, guint argc, const gchar **argv,
						   SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_bind_cnc (ToolCommand *command, guint argc, const gchar **argv,
						  SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_edit_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_reset_buffer (ToolCommand *command, guint argc, const gchar **argv,
						      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_show_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_exec_buffer (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_write_buffer (ToolCommand *command, guint argc, const gchar **argv,
						      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_to_dict (ToolCommand *command, guint argc, const gchar **argv,
							      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_from_dict (ToolCommand *command, guint argc, const gchar **argv,
								SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_list_dict (ToolCommand *command, guint argc, const gchar **argv,
								SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_query_buffer_delete_dict (ToolCommand *command, guint argc, const gchar **argv,
								  SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_set (ToolCommand *command, guint argc, const gchar **argv,
					     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_unset (ToolCommand *command, guint argc, const gchar **argv,
					       SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_data_sets_list (ToolCommand *command, guint argc, const gchar **argv,
							SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_data_set_move (ToolCommand *command, guint argc, const gchar **argv,
						       SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_data_set_grep (ToolCommand *command, guint argc, const gchar **argv,
						       SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_data_set_show (ToolCommand *command, guint argc, const gchar **argv,
						       SqlConsole *console, GError **error);

static ToolCommandResult *extra_command_data_set_rm (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_data_set_import (ToolCommand *command, guint argc, const gchar **argv,
							 SqlConsole *console, GError **error);

static ToolCommandResult *extra_command_graph (ToolCommand *command, guint argc, const gchar **argv,
					       SqlConsole *console, GError **error);
#ifdef HAVE_LIBSOUP
static ToolCommandResult *extra_command_httpd (ToolCommand *command, guint argc, const gchar **argv,
					       SqlConsole *console, GError **error);
#endif
#ifdef NONE
static ToolCommandResult *extra_command_lo_update (ToolCommand *command, guint argc, const gchar **argv,
						   SqlConsole *console, GError **error);
#endif
static ToolCommandResult *extra_command_export (ToolCommand *command, guint argc, const gchar **argv,
						SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_set2 (ToolCommand *command, guint argc, const gchar **argv,
					      SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_pivot (ToolCommand *command, guint argc, const gchar **argv,
					       SqlConsole *console, GError **error);

static ToolCommandResult *extra_command_declare_fk (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);

static ToolCommandResult *extra_command_undeclare_fk (ToolCommand *command, guint argc, const gchar **argv,
						      SqlConsole *console, GError **error);
#ifdef HAVE_LDAP
static ToolCommandResult *extra_command_ldap_search (ToolCommand *command, guint argc, const gchar **argv,
						     SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_ldap_descr (ToolCommand *command, guint argc, const gchar **argv,
						    SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_ldap_mv (ToolCommand *command, guint argc, const gchar **argv,
						 SqlConsole *console, GError **error);
static ToolCommandResult *extra_command_ldap_mod (ToolCommand *command, guint argc, const gchar **argv,
						  SqlConsole *console, GError **error);
#endif

static void
build_commands (MainData *md)
{
	md->limit_commands = tool_command_group_new ();
	md->all_commands = tool_command_group_new ();

	ToolCommand *c;

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<FILE>]"), "s");
	c->description = _("Show commands history, or save it to file");
	c->command_func = (ToolCommandFunc) gda_internal_command_history;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<META DATA TYPE>]"), "meta");
	c->description = _("Force reading the database meta data (or part of the meta data, ex:\"tables\")");
	c->command_func = (ToolCommandFunc) gda_internal_command_dict_sync;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <fkname> <tableA>(<colA>,...) <tableB>(<colB>,...)"), "fkdeclare");
	c->description = _("Declare a new foreign key (not actually in database): tableA references tableB");
	c->command_func = (ToolCommandFunc) extra_command_declare_fk;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <fkname> <tableA> <tableB>"), "fkundeclare");
	c->description = _("Un-declare a foreign key (not actually in database)");
	c->command_func = (ToolCommandFunc) extra_command_undeclare_fk;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<TABLE>]"), "dt");
	c->description = _("List all tables (or named table)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_tables;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<VIEW>]"), "dv");
	c->description = _("List all views (or named view)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_views;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<SCHEMA>]"), "dn");
	c->description = _("List all schemas (or named schema)");
	c->command_func = (ToolCommandFunc) gda_internal_command_list_schemas;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<OBJ_NAME>|<SCHEMA>.*]"), "d");
	c->description = _("Describe object or full list of objects");
	c->command_func = (ToolCommandFunc) gda_internal_command_detail;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<TABLE1> [<TABLE2>...]]"), "graph");
	c->description = _("Create a graph of all or the listed tables");
	c->command_func = (ToolCommandFunc) extra_command_graph;
	tool_command_group_add (md->all_commands, c);

#ifdef HAVE_LIBSOUP
	c = g_new0 (ToolCommand, 1);
	c->group = _("Information");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<port> [<authentication token>]]"), "http");
	c->description = _("Start/stop embedded HTTP server (on given port or on 12345 by default)");
	c->command_func = (ToolCommandFunc) extra_command_httpd;
	tool_command_group_add (md->all_commands, c);
#endif

	/* specific commands */
	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [[<CNC_NAME>] [<DSN>|<CONNECTION STRING>]]"), "c");
	c->description = _("Opens a new connection or lists opened connections");
	c->command_func = (ToolCommandFunc) extra_command_manage_cnc;
	c->completion_func = extra_command_manage_cnc_compl;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<CNC_NAME>]"), "close");
	c->description = _("Close a connection");
	c->command_func = (ToolCommandFunc) extra_command_close_cnc;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <CNC NAME> <OBJ NAME> [<OBJ NAME> ...]"), "bind");
	c->description = _("Bind connections or datasets (<OBJ NAME>) into a single new one (allowing SQL commands to be executed across multiple connections and/or datasets)");
	c->command_func = (ToolCommandFunc) extra_command_bind_cnc;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<DSN>]"), "l");
	c->description = _("List all DSN (or named DSN's attributes)");
	c->command_func = (ToolCommandFunc) extra_command_list_dsn;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <DSN_NAME> <DSN_DEFINITION> [<DESCRIPTION>]"), "lc");
	c->description = _("Create (or modify) a DSN");
	c->command_func = (ToolCommandFunc) extra_command_create_dsn;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <DSN_NAME> [<DSN_NAME>...]"), "lr");
	c->description = _("Remove a DSN");
	c->command_func = (ToolCommandFunc) extra_command_remove_dsn;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("DSN (data sources) management");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<PROVIDER>]"), "lp");
	c->description = _("List all installed database providers (or named one's attributes)");
	c->command_func = (ToolCommandFunc) extra_command_list_providers;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <FILE>"), "i");
	c->description = _("Execute commands from file");
	c->command_func = (ToolCommandFunc) extra_command_set_input;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<FILE>]"), "o");
	c->description = _("Send output to a file or |pipe");
	c->command_func = (ToolCommandFunc) extra_command_set_output;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<TEXT>]"), "echo");
	c->description = _("Print TEXT or an empty line to standard output");
	c->command_func = (ToolCommandFunc) extra_command_echo;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<TEXT>]"), "qecho");
	c->description = _("Send TEXT or an empty line to current output stream");
	c->command_func = (ToolCommandFunc) extra_command_qecho;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = "q";
	c->description = _("Quit");
	c->command_func = (ToolCommandFunc) extra_command_quit;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<DIR>]"), "cd");
	c->description = _("Change the current working directory");
	c->command_func = (ToolCommandFunc) extra_command_cd;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = "copyright";
	c->description = _("Show usage and distribution terms");
	c->command_func = (ToolCommandFunc) extra_command_copyright;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("General");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<NAME> [<VALUE>]]"), "option");
	c->description = _("Set or show an option, or list all options ");
	c->command_func = (ToolCommandFunc) extra_command_option;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<FILE>]"), "e");
	c->description = _("Edit the query buffer (or file) with external editor");
	c->command_func = (ToolCommandFunc) extra_command_edit_buffer;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<FILE>]"), "qr");
	c->description = _("Reset the query buffer (or load file into query buffer)");
	c->command_func = (ToolCommandFunc) extra_command_reset_buffer;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = "qp";
	c->description = _("Show the contents of the query buffer");
	c->command_func = (ToolCommandFunc) extra_command_show_buffer;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<FAVORITE_NAME>]"), "g");
	c->description = _("Execute contents of query buffer, or execute specified query favorite");
	c->command_func = (ToolCommandFunc) extra_command_exec_buffer;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <FILE>"), "qw");
	c->description = _("Write query buffer to file");
	c->command_func = (ToolCommandFunc) extra_command_write_buffer;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "qs");
	c->description = _("Save query buffer as favorite");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_to_dict;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "ql");
	c->description = _("Load a query favorite into query buffer");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_from_dict;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <FAVORITE_NAME>"), "qd");
	c->description = _("Delete a query favorite");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_delete_dict;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s"), "qa");
	c->description = _("List all query favorites");
	c->command_func = (ToolCommandFunc) extra_command_query_buffer_list_dict;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Input/Output");
	c->group_id = NULL;
	c->name = "H [HTML|XML|CSV|DEFAULT]";
	c->description = _("Set output format");
	c->command_func = (ToolCommandFunc) extra_command_set_output_format;
	tool_command_group_add (md->all_commands, c);

	/*
	  c = g_new0 (ToolCommand, 1);
	  c->group = _("Query buffer & query favorites");
	  c->group_id = NULL;
	  c->name = g_strdup_printf (_("%s <FILE> <TABLE> <BLOB_COLUMN> <ROW_CONDITION>"), "lo_update");
	  c->description = _("Import a blob into the database");
	  c->command_func = (ToolCommandFunc) extra_command_lo_update;
	  tool_command_group_add (md->all_commands, c);
	*/	

	c = g_new0 (ToolCommand, 1);
	c->group = _("Query buffer & query favorites");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<NAME>|<TABLE> <COLUMN> <ROW_CONDITION>] <FILE>"), "export");
	c->description = _("Export internal parameter or table's value to the FILE file");
	c->command_func = (ToolCommandFunc) extra_command_export;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<NAME> [<VALUE>|_null_]]"), "set");
	c->description = _("Set or show internal parameter, or list all if no parameter specified ");
	c->command_func = (ToolCommandFunc) extra_command_set;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s [<NAME>]"), "unset");
	c->description = _("Unset (delete) internal named parameter (or all parameters)");
	c->command_func = (ToolCommandFunc) extra_command_unset;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <NAME> [<FILE>|<TABLE> <COLUMN> <ROW_CONDITION>]"), "setex");
	c->description = _("Set internal parameter as the contents of the FILE file or from an existing table's value");
	c->command_func = (ToolCommandFunc) extra_command_set2;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Execution context");
	c->group_id = NULL;
	c->name = g_strdup_printf (_("%s <SELECT> <ROW_FIELDS> [<COLUMN_FIELDS> [<DATA_FIELDS> ...]]"), "pivot");
	c->description = _("Performs a statistical analysis on the data from SELECT, "
			   "using ROW_FIELDS and COLUMN_FIELDS criteria and optionally DATA_FIELDS for the data");
	c->command_func = (ToolCommandFunc) extra_command_pivot;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s"), "ds_list");
	c->description = _("Lists all the datasets kept in memory for reference");
	c->command_func = (ToolCommandFunc) extra_command_data_sets_list;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> <PATTERN>"), "ds_grep");
	c->description = _("Show a dataset's contents where lines match a regular expression");
	c->command_func = (ToolCommandFunc) extra_command_data_set_grep;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> [<COLUMN> [<COLUMN> ...]]"), "ds_show");
	c->description = _("Show a dataset's contents, showing only the specified columns if any specified");
	c->command_func = (ToolCommandFunc) extra_command_data_set_show;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> [<DATASET NAME> ...]"), "ds_rm");
	c->description = _("Remove one or more datasets");
	c->command_func = (ToolCommandFunc) extra_command_data_set_rm;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s <DATASET NAME> <DATASET NAME>"), "ds_mv");
	c->description = _("Rename a dataset, useful to rename the '_' dataset to keep it");
	c->command_func = (ToolCommandFunc) extra_command_data_set_move;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("Datasets' manipulations");
	c->group_id = "DATA";
	c->name = g_strdup_printf (_("%s CSV <FILE NAME>"), "ds_import");
	c->description = _("Import a dataset from a file");
	c->command_func = (ToolCommandFunc) extra_command_data_set_import;
	tool_command_group_add (md->limit_commands, c);
	tool_command_group_add (md->all_commands, c);

#ifdef HAVE_LDAP
	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <filter> [<base|onelevel|subtree> [<base DN>]]"), "ldap_search");
	c->description = _("Search LDAP entries");
	c->command_func = (ToolCommandFunc) extra_command_ldap_search;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> [\"all\"|\"set\"|\"unset\"]"), "ldap_descr");
	c->description = _("Shows attributes for the entry identified by its DN. If the "
			   "\"set\" 2nd parameter is passed, then all set attributes are show, if "
			   "the \"all\" 2nd parameter is passed, then the unset attributes are "
			   "also shown, and if the \"unset\" 2nd parameter "
			   "is passed, then only non set attributes are shown.");
	c->command_func = (ToolCommandFunc) extra_command_ldap_descr;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> <new DN>"), "ldap_mv");
	c->description = _("Renames an LDAP entry");
	c->command_func = (ToolCommandFunc) extra_command_ldap_mv;
	tool_command_group_add (md->all_commands, c);

	c = g_new0 (ToolCommand, 1);
	c->group = _("LDAP");
	c->group_id = "LDAP";
	c->name = g_strdup_printf (_("%s <DN> <OPERATION> [<ATTR>[=<VALUE>]] [<ATTR>=<VALUE> ...]"), "ldap_mod");
	c->description = _("Modifies an LDAP entry's attributes; <OPERATION> may be DELETE, REPLACE or ADD");
	c->command_func = (ToolCommandFunc) extra_command_ldap_mod;
	tool_command_group_add (md->all_commands, c);
#endif
}

static ToolCommandResult *
extra_command_set_output (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	g_assert (console);
	if (set_output_file (argv[0], error)) {
		ToolCommandResult *res;
		
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static ToolCommandResult *
extra_command_set_output_format (ToolCommand *command, guint argc, const gchar **argv,
				 SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *fmt = NULL;

	g_assert (console);
	if (argv[0] && *argv[0])
		fmt = argv[0];
	
	console->output_format = TOOL_OUTPUT_FORMAT_DEFAULT;

	if (fmt) {
		if ((*fmt == 'X') || (*fmt == 'x'))
			console->output_format = TOOL_OUTPUT_FORMAT_XML;
		else if ((*fmt == 'H') || (*fmt == 'h'))
			console->output_format = TOOL_OUTPUT_FORMAT_HTML;
		else if ((*fmt == 'D') || (*fmt == 'd'))
			console->output_format = TOOL_OUTPUT_FORMAT_DEFAULT;
		else if ((*fmt == 'C') || (*fmt == 'c'))
			console->output_format = TOOL_OUTPUT_FORMAT_CSV;
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown output format: '%s', reset to default"), fmt);
			goto out;
		}
	}

	if (!console->output_stream) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new ("");
		switch (console->output_format) {
		case TOOL_OUTPUT_FORMAT_DEFAULT:
			g_string_assign (res->u.txt, ("Output format is default\n"));
			break;
		case TOOL_OUTPUT_FORMAT_HTML:
			g_string_assign (res->u.txt, ("Output format is HTML\n"));
			break;
		case TOOL_OUTPUT_FORMAT_XML:
			g_string_assign (res->u.txt, ("Output format is XML\n"));
			break;
		case TOOL_OUTPUT_FORMAT_CSV:
			g_string_assign (res->u.txt, ("Output format is CSV\n"));
			break;
		default:
			TO_IMPLEMENT;
		}
	}
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}

 out:
	compute_term_color_attribute ();
	
	return res;
}

static gboolean
idle_read_input_stream (G_GNUC_UNUSED gpointer data)
{
	if (main_data->input_stream) {
		gchar *cmde;
		cmde = input_from_stream (main_data->input_stream);
		if (cmde) {
			gboolean command_ok;
			treat_line_func (cmde, &command_ok);
			g_free (cmde);
			if (command_ok)
				return TRUE; /* potentially some more work to do from the stream */
			else 
				goto stop;
		}
		else 
			goto stop;
	}

 stop:
	compute_prompt (NULL, prompt, main_data->partial_command == NULL ? FALSE : TRUE, FALSE,
			TOOL_OUTPUT_FORMAT_DEFAULT |
			(main_data->term_console->output_format & TOOL_OUTPUT_FORMAT_COLOR_TERM));
	g_print ("\n%s", prompt->str);
	fflush (NULL);
	set_input_file (NULL, NULL);
	return FALSE; /* stop calling this function */
}

static ToolCommandResult *
extra_command_set_input (ToolCommand *command, guint argc, const gchar **argv,
			 SqlConsole *console, GError **error)
{
	g_assert (console);
	if (set_input_file (argv[0], error)) {
		ToolCommandResult *res;
		
		g_idle_add ((GSourceFunc) idle_read_input_stream, NULL);

		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static ToolCommandResult *
extra_command_echo (ToolCommand *command, guint argc, const gchar **argv,
		    SqlConsole *console, GError **error)
{
	ToolCommandResult *res;
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
	res->u.txt = g_string_new (argv[0]);
	if (argv[0][strlen (argv[0]) - 1] != '\n')
		g_string_append_c (res->u.txt, '\n');
	return res;
}

static ToolCommandResult *
extra_command_qecho (ToolCommand *command, guint argc, const gchar **argv,
		     SqlConsole *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (argv[0]);
	return res;
}

static ToolCommandResult *
extra_command_list_dsn (ToolCommand *command, guint argc, const gchar **argv,
			SqlConsole *console, GError **error)
{
	ToolCommandResult *res;
	GList *list = NULL;
	GdaDataModel *dsn_list = NULL, *model = NULL;

	if (argv[0]) {
		/* details about a DSN */
		GdaDataModel *model;
		model = config_info_detail_dsn (argv[0], error);
		if (model) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
			return res;
		}
		else
			return NULL;
	}
	else {
		gint i, nrows;
		
		dsn_list = gda_config_list_dsn ();
		nrows = gda_data_model_get_n_rows (dsn_list);

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
		
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
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

static ToolCommandResult *
extra_command_create_dsn (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDsnInfo newdsn;
	gchar *real_cnc, *real_provider, *user, *pass;

	if (!argv[0] || !argv[1]) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing arguments"));
		return NULL;
	}

	newdsn.name = (gchar *) argv [0];
	gda_connection_string_split ((gchar *) argv[1], &real_cnc, &real_provider, &user, &pass);
	newdsn.provider = real_provider;
	newdsn.description = (gchar*) argv[2];
	
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
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing provider name"));
	}
	else if (gda_config_define_dsn (&newdsn, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}

	g_free (real_cnc);
	g_free (real_provider);
	g_free (user);
	g_free (pass);

	return res;
}

static ToolCommandResult *
extra_command_remove_dsn (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	ToolCommandResult *res;
	gint i;

	if (!argv[0]) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DSN name"));
		return NULL;
	}
	for (i = 0; argv [i]; i++) {
		if (! gda_config_remove_dsn (argv[i], error))
			return NULL;
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;
	return res;	
}

static ToolCommandResult *
extra_command_list_providers (ToolCommand *command, guint argc, const gchar **argv,
			      SqlConsole *console, GError **error)
{
	ToolCommandResult *res;
	GdaDataModel *model;

	if (argv[0])
		model = config_info_detail_provider (argv[0], error);
	else
		model = config_info_list_all_providers ();
		
	if (model) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
	return NULL;
}

static void
vconnection_hub_foreach_cb (G_GNUC_UNUSED GdaConnection *cnc, const gchar *ns, GString *string)
{
	if (string->len > 0)
		g_string_append_c (string, '\n');
	g_string_append_printf (string, "namespace %s", ns);
}

static gchar **
extra_command_manage_cnc_compl (const gchar *text)
{
	GArray *array = NULL;
	gsize len;
	gint i, ndsn;
	len = strlen (text);

	/* complete with DSN list to open connections */
	ndsn = gda_config_get_nb_dsn ();
	for (i = 0; i < ndsn; i++) {
		GdaDsnInfo *info;
		info = gda_config_get_dsn_info_at_index (i);
		if (!len || !strncmp (info->name, text, len)) {
			if (!array)
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
			gchar *tmp;
			tmp = g_strdup (info->name);
			g_array_append_val (array, tmp);
		}
	}

	/* complete with opened connections */
	GSList *list;
	for (list = main_data->settings; list; list = list->next) {
		ConnectionSetting *cs = (ConnectionSetting *) list->data;
		if (!len || !strncmp (cs->name, text, len)) {
			if (!array)
				array = g_array_new (TRUE, FALSE, sizeof (gchar*));
			gchar *tmp;
			tmp = g_strdup (cs->name);
			g_array_append_val (array, tmp);
		}
	}

	if (array)
		return (gchar**) g_array_free (array, FALSE);
	else
		return NULL;
}

static 
ToolCommandResult *
extra_command_manage_cnc (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	g_assert (console);

	if (argc > 2) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Too many arguments"));
		return NULL;
	}

	/* arguments:
	 * 0 = connection name
	 * 1 = DSN or direct string
	 */
	if (argv[0]) {
		const gchar *dsn = NULL;
		if (!argv[1]) {
			/* try to switch to an existing connection */
			ConnectionSetting *cs;

			cs = find_connection_from_name (argv[0]);
			if (cs) {
				ToolCommandResult *res;
				
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
				console->current = cs;
				return res;
			}
			else {
				if (*argv [0] == '~') {
					/* find connection for which we want the meta store's connection */
					if (*(argv[0] + 1)) {
						cs = find_connection_from_name (argv[0] + 1);
						if (!cs) {
							g_set_error (error, GDA_TOOLS_ERROR,
								     GDA_TOOLS_NO_CONNECTION_ERROR,
								     _("No connection named '%s' found"), argv[0] + 1);
							return NULL;
						}
					}
					else
						cs = console->current;

					if (!cs) {
						g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
							     "%s", _("No current connection"));
						return NULL;
					}

					/* find if requested connection already exists */
					ConnectionSetting *ncs = NULL;
					if (* (cs->name) == '~') 
						ncs = find_connection_from_name (console->current->name + 1);
					else {
						gchar *tmp;
						tmp = g_strdup_printf ("~%s", cs->name);
						ncs = find_connection_from_name (tmp);
						g_free (tmp);
					}
					if (ncs) {
						ToolCommandResult *res;
						
						res = g_new0 (ToolCommandResult, 1);
						res->type = TOOL_COMMAND_RESULT_EMPTY;
						console->current = ncs;
						return res;
					}

					/* open a new connection */
					ncs = g_new0 (ConnectionSetting, 1);
					GdaMetaStore *store;
					ToolCommandResult *res;
					
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
					
					main_data->settings = g_slist_append (main_data->settings, ncs);
					console->current = ncs;

					GError *lerror = NULL;
					if (!console->output_stream) {
						g_print (_("Getting database schema information, "
							   "this may take some time... "));
						fflush (stdout);
					}
					if (!gda_connection_update_meta_store (ncs->cnc, NULL, &lerror)) {
						if (!console->output_stream) 
							g_print (_("error: %s\n"), 
								 lerror && lerror->message ? lerror->message : _("No detail"));
						if (lerror)
							g_error_free (lerror);
					}
					else
						if (!console->output_stream) 
							g_print (_("Done.\n"));
					
					res = g_new0 (ToolCommandResult, 1);
					res->type = TOOL_COMMAND_RESULT_EMPTY;
					return res;
				}

				dsn = argv[0];
			}
		}
		else
			dsn = argv[1];

		if (dsn) {
			/* open a new connection */
			ConnectionSetting *cs;

			cs = find_connection_from_name (argv[0]);
			if (cs) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     _("A connection named '%s' already exists"), argv[0]);
				return NULL;
			}

			cs = open_connection (console, argv[0], dsn, error);
			if (cs) {
				ToolCommandResult *res;
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
				return res;
			}
			else
				return NULL;
		}
		else
			return NULL;
	}
	else {
		/* list opened connections */
		GdaDataModel *model;
		GSList *list;
		ToolCommandResult *res;

		if (! main_data->settings) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_TXT;
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

		for (list = main_data->settings; list; list = list->next) {
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
 
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
}

static void
handle_close_cnc (SqlConsole *console, ConnectionSetting *cs, GdaConnection *cnc)
{
	g_assert (console);
	if (!cs) {
		/* handle closed @cnc */
		GSList *list;
		for (list = main_data->settings; list; list = list->next) {
			if (((ConnectionSetting *) list->data)->cnc == cnc) {
				cs = (ConnectionSetting *) list->data;
				break;
			}
		}
	}

	if (console->current == cs) {
		gint index;
		ConnectionSetting *ncs = NULL;
		index = g_slist_index (main_data->settings, cs);
		if (index == 0)
			ncs = g_slist_nth_data (main_data->settings, index + 1);
		else
			ncs = g_slist_nth_data (main_data->settings, index - 1);
		console->current = ncs;
	}
	main_data->settings = g_slist_remove (main_data->settings, cs);
	connection_settings_free (cs);
}

static void
conn_closed_cb (GdaConnection *cnc, G_GNUC_UNUSED gpointer data)
{
	handle_close_cnc (NULL, NULL, cnc);
}

static 
ToolCommandResult *
extra_command_close_cnc (ToolCommand *command, guint argc, const gchar **argv,
			 SqlConsole *console, GError **error)
{
	if (argc == 0) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     _("No connection specified"));
		return NULL;
	}
	else {
		guint i;
		for (i = 0; i < argc; i++) {
			ConnectionSetting *cs = NULL;
			if (argv[i] && *argv[i])
				cs = find_connection_from_name (argv[0]);
			if (!cs) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
					     _("No connection named '%s' found"), argv[0]);
				return NULL;
			}

			handle_close_cnc (console, cs, NULL);
		}
	}
	
	ToolCommandResult *res;
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;
	return res;
}

static ToolCommandResult *
extra_command_bind_cnc (ToolCommand *command, guint argc, const gchar **argv,
			SqlConsole *console, GError **error)
{
	ConnectionSetting *cs = NULL;
	gint i, nargv = g_strv_length ((gchar **) argv);
	static GdaVirtualProvider *vprovider = NULL;
	GdaConnection *virtual;
	GString *string;

	g_assert (console);
	if (nargv < 2) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing required connection names"));
		return NULL;
	}

	/* check for connections existance */
	cs = find_connection_from_name (argv[0]);
	if (cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("A connection named '%s' already exists"), argv[0]);
		return NULL;
	}
	if (! connection_name_is_valid (argv[0])) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Connection name '%s' is invalid"), argv[0]);
		return NULL;
	}
	for (i = 1; i < nargv; i++) {
		cs = find_connection_from_name (argv[i]);
		if (!cs) {
			GdaDataModel *src;
			src = g_hash_table_lookup (main_data->mem_data_models, argv[i]);
			if (!src) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     _("No connection or dataset named '%s' found"), argv[i]);
				return NULL;
			}
		}
	}

	/* actual virtual connection creation */
	if (!vprovider) 
		vprovider = gda_vprovider_hub_new ();
	g_assert (vprovider);

	virtual = gda_virtual_connection_open_extended (vprovider, GDA_CONNECTION_OPTIONS_THREAD_SAFE |
							GDA_CONNECTION_OPTIONS_AUTO_META_DATA, NULL);
	if (!virtual) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not create virtual connection"));
		return NULL;
	}
	g_object_set (G_OBJECT (virtual), "execution-timer", TRUE, NULL);
	gda_connection_get_meta_store (virtual); /* force create of meta store */

	/* add existing connections to virtual connection */
	string = g_string_new (_("Bound connections are as:"));
	for (i = 1; i < nargv; i++) {
		cs = find_connection_from_name (argv[i]);
		if (cs) {
			if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual),
						      cs->cnc, argv[i], error)) {
				g_object_unref (virtual);
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, "\n   ");
			/* Translators: this string indicates that all the tables in connection named in the
			 * 1st "%s" will appear in the SQL namespace named as the 2nd "%s" */
			g_string_append_printf (string, _("%s in the '%s' namespace"), argv[i], argv[i]);
		}
		else {
			GdaDataModel *src;
			src = g_hash_table_lookup (main_data->mem_data_models, argv[i]);
			if (! gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (virtual),
								    src, argv[i], error)) {
				g_object_unref (virtual);
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, "\n   ");
			/* Translators: this string indicates that the dataset named in the 1st "%s"
			 * will appear as the table named as the 2nd "%s" */
			g_string_append_printf (string, _("%s mapped to the %s table"), argv[i],
						gda_vconnection_data_model_get_table_name (GDA_VCONNECTION_DATA_MODEL (virtual), src));
		}
        }

	cs = g_new0 (ConnectionSetting, 1);
	cs->name = g_strdup (argv[0]);
	cs->cnc = virtual;
	cs->parser = gda_connection_create_parser (virtual);
	cs->query_buffer = NULL;
	cs->threader = NULL;
	cs->meta_job_id = 0;

	main_data->settings = g_slist_append (main_data->settings, cs);
	console->current = cs;

	ToolCommandResult *res;
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT;
	res->u.txt = string;
	return res;
}

static ToolCommandResult *
extra_command_copyright (ToolCommand *command, guint argc, const gchar **argv,
			 SqlConsole *console, GError **error)
{
	ToolCommandResult *res;
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT;
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

static ToolCommandResult *
extra_command_option (ToolCommand *command, guint argc, const gchar **argv,
		      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *oname = NULL;
	const gchar *value = NULL;

	if (argv[0] && *argv[0]) {
		oname = argv[0];
		if (argv[1] && *argv[1])
			value = argv[1];
	}

	if (oname) {
		GdaHolder *opt = gda_set_get_holder (main_data->options, oname);
		if (opt) {
			if (value) {
				if (! gda_holder_set_value_str (opt, NULL, value, error))
					return NULL;
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_SET;
				res->u.set = gda_set_new (NULL);
				gda_set_add_holder (res->u.set, gda_holder_copy (opt));
			}
		}
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("No option named '%s'"), oname);
			return NULL;
		}
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		GSList *list;
		model = gda_data_model_array_new_with_g_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		gda_data_model_set_column_title (model, 2, _("Description"));
		g_object_set_data (G_OBJECT (model), "name", _("List of options"));
		for (list = main_data->options->holders; list; list = list->next) {
			gint row;
			gchar *str;
			GValue *value;
			GdaHolder *opt;
			opt = GDA_HOLDER (list->data);
			row = gda_data_model_append_row (model, NULL);

			value = gda_value_new_from_string (gda_holder_get_id (opt), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);

			str = gda_holder_get_value_str (opt, NULL);
			value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			value = (GValue*) gda_holder_get_attribute (opt, GDA_ATTRIBUTE_DESCRIPTION);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
		}

		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}

	return res;
}

static ToolCommandResult *
extra_command_quit (ToolCommand *command, guint argc, const gchar **argv,
		    SqlConsole *console, GError **error)
{
	ToolCommandResult *res;

	g_assert (console);
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EXIT;
	return res;
}

static ToolCommandResult *
extra_command_cd (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	const gchar *dir = NULL;
#define DIR_LENGTH 256
	static char start_dir[DIR_LENGTH];
	static gboolean init_done = FALSE;

	g_assert (console);
	if (!init_done) {
		init_done = TRUE;
		memset (start_dir, 0, DIR_LENGTH);
		if (getcwd (start_dir, DIR_LENGTH) != NULL) {
			/* default to $HOME */
#ifdef G_OS_WIN32
			TO_IMPLEMENT;
			strncpy (start_dir, "/", 2);
#else
			struct passwd *pw;
			
			pw = getpwuid (geteuid ());
			if (!pw) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     _("Could not get home directory: %s"), strerror (errno));
				return NULL;
			}
			else {
				gsize l = strlen (pw->pw_dir);
				if (l > DIR_LENGTH - 1)
					strncpy (start_dir, "/", 2);
				else
					strncpy (start_dir, pw->pw_dir, l + 1);
			}
#endif
		}
	}

	if (argv[0]) 
		dir = argv[0];
	else 
		dir = start_dir;

	if (dir) {
		if (chdir (dir) == 0) {
			ToolCommandResult *res;

			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
			res->u.txt = g_string_new ("");
			g_string_append_printf (res->u.txt, _("Working directory is now: %s"), dir);
			return res;
		}
		else
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
				     _("Could not change working directory to '%s': %s"),
				     dir, strerror (errno));
	}

	return NULL;
}

static ToolCommandResult *
extra_command_edit_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   SqlConsole *console, GError **error)
{
	gchar *filename = NULL;
	static gchar *editor_name = NULL;
	gchar *edit_command = NULL;
	gint systemres;
	ToolCommandResult *res = NULL;

	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		goto end_of_command;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");

	if (argv[0] && *argv[0])
		filename = (gchar *) argv[0];
	else {
		/* use a temp file */
		gint fd;
		fd = g_file_open_tmp (NULL, &filename, error);
		if (fd < 0)
			goto end_of_command;
		if (write (fd, console->current->query_buffer->str, 
			   console->current->query_buffer->len) != (gint)console->current->query_buffer->len) {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
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
	edit_command = g_strdup_printf ("%s\"%s\" \"%s\"%s", SYSTEMQUOTE, editor_name, filename, SYSTEMQUOTE);
#else
	edit_command = g_strdup_printf ("exec %s '%s'", editor_name, filename);
#endif

	systemres = system (edit_command);
	if (systemres == -1) {
                g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     _("could not start editor '%s'"), editor_name);
		goto end_of_command;
	}
        else if (systemres == 127) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not start /bin/sh"));
		goto end_of_command;
	}
	else {
		if (! argv[0]) {
			gchar *str;
			
			if (!g_file_get_contents (filename, &str, NULL, error))
				goto end_of_command;
			g_string_assign (console->current->query_buffer, str);
			g_free (str);
		}
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;

 end_of_command:

	g_free (edit_command);
	if (! argv[0]) {
		g_unlink (filename);
		g_free (filename);
	}

	return res;
}

static ToolCommandResult *
extra_command_reset_buffer (ToolCommand *command, guint argc, const gchar **argv,
			    SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");
	else 
		g_string_assign (console->current->query_buffer, "");

	if (argv[0]) {
		const gchar *filename = NULL;
		gchar *str;

		filename = argv[0];
		if (!g_file_get_contents (filename, &str, NULL, error))
			return NULL;

		g_string_assign (console->current->query_buffer, str);
		g_free (str);
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static ToolCommandResult *
extra_command_show_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (console->current->query_buffer->str);

	return res;
}

static ToolCommandResult *
extra_command_exec_buffer (ToolCommand *command, guint argc, const gchar **argv,
			   SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		/* load named query buffer first */
		res = extra_command_query_buffer_from_dict (command, argc, argv, console, error);
		if (!res)
			return NULL;
		tool_command_result_free (res);
		res = NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");
	if (*console->current->query_buffer->str != 0)
		res = command_execute (console, console->current->query_buffer->str,
				       GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}

	return res;
}

static ToolCommandResult *
extra_command_write_buffer (ToolCommand *command, guint argc, const gchar **argv,
			    SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");
	if (!argv[0]) 
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing FILE to write to"));
	else {
		if (g_file_set_contents (argv[0], console->current->query_buffer->str, -1, error)) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
	}

	return res;
}

#define QUERY_BUFFERS_TABLE_NAME "gda_sql_query_buffers"
#define QUERY_BUFFERS_TABLE_SELECT					\
	"SELECT name, sql FROM " QUERY_BUFFERS_TABLE_NAME " ORDER BY name"
#define QUERY_BUFFERS_TABLE_SELECT_ONE					\
	"SELECT sql FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"
#define QUERY_BUFFERS_TABLE_DELETE					\
	"DELETE FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"

static ToolCommandResult *
extra_command_query_buffer_list_dict (ToolCommand *command, guint argc, const gchar **argv,
				      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDataModel *retmodel;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	/* initialize returned data model */
	retmodel = gda_data_model_array_new_with_g_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gda_data_model_set_column_title (retmodel, 0, _("Favorite name"));
	gda_data_model_set_column_title (retmodel, 1, _("Comments"));
	gda_data_model_set_column_title (retmodel, 2, _("SQL"));

	GdaMetaStore *mstore;
	mstore = gda_connection_get_meta_store (console->current->cnc);

	/* Use tools favorites */
	if (! console->current->favorites)
		console->current->favorites = gda_tools_favorites_new (mstore);

	GSList *favlist, *list;
	GError *lerror = NULL;
	favlist = gda_tools_favorites_list (console->current->favorites, 0, GDA_TOOLS_FAVORITES_QUERIES,
					    ORDER_KEY_QUERIES, &lerror);
	if (lerror) {
		g_propagate_error (error, lerror);
		g_object_unref (retmodel);
		return NULL;
	}
	for (list = favlist; list; list = list->next) {
		ToolsFavoritesAttributes *att = (ToolsFavoritesAttributes*) list->data;
		gint i;
		GValue *value = NULL;
		i = gda_data_model_append_row (retmodel, error);
		if (i == -1)
			goto cleanloop;
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), att->name);
		if (! gda_data_model_set_value_at (retmodel, 0, i, value, error))
			goto cleanloop;
		g_value_set_string (value, att->descr);
		if (! gda_data_model_set_value_at (retmodel, 1, i, value, error))
			goto cleanloop;
		g_value_set_string (value, att->contents);
		if (! gda_data_model_set_value_at (retmodel, 2, i, value, error))
			goto cleanloop;
		gda_value_free (value);
		continue;
	cleanloop:
		gda_value_free (value);
		gda_tools_favorites_free_list (favlist);
		g_object_unref (retmodel);
		return NULL;
	}
	if (favlist)
		gda_tools_favorites_free_list (favlist);

	/* Use query buffer which used to be stored differently in previous versions of GdaSql:
	 * in the "gda_sql_query_buffers" table */
	GdaStatement *sel_stmt = NULL;
	GdaDataModel *model;
	sel_stmt = gda_sql_parser_parse_string (console->current->parser, 
						QUERY_BUFFERS_TABLE_SELECT, NULL, NULL);
	g_assert (sel_stmt);
	GdaConnection *store_cnc;
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	model = gda_connection_statement_execute_select (store_cnc, sel_stmt, NULL, NULL);
	g_object_unref (sel_stmt);
	if (model) {
		gint r, nrows;
		nrows = gda_data_model_get_n_rows (model);
		for (r = 0; r < nrows; r++) {
			const GValue *cvalue = NULL;
			gint i;
			i = gda_data_model_append_row (retmodel, NULL);
			if (i == -1)
				break;
			cvalue = gda_data_model_get_value_at (model, 0, r, NULL);
			if (!cvalue)
				break;
			gda_data_model_set_value_at (retmodel, 0, i, cvalue, NULL);

			cvalue = gda_data_model_get_value_at (model, 1, r, NULL);
			if (!cvalue)
				break;
			gda_data_model_set_value_at (retmodel, 2, i, cvalue, NULL);
		}
		g_object_unref (model);
	}
	
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = retmodel;

	return res;
}

static ToolCommandResult *
extra_command_query_buffer_to_dict (ToolCommand *command, guint argc, const gchar **argv,
				    SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");

	if (*console->current->query_buffer->str != 0) {
		/* find a suitable name */
		gchar *qname;
		if (argv[0] && *argv[0]) 
			qname = g_strdup ((gchar *) argv[0]);
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Missing query buffer name"));
			return NULL;
		}

		/* Use tools favorites */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (console->current->cnc);
		if (! console->current->favorites)
			console->current->favorites = gda_tools_favorites_new (mstore);

		ToolsFavoritesAttributes att;
		att.id = -1;
		att.type = GDA_TOOLS_FAVORITES_QUERIES;
		att.name = qname;
		att.descr = NULL;
		att.contents = console->current->query_buffer->str;

		if (! gda_tools_favorites_add (console->current->favorites, 0,
					       &att, ORDER_KEY_QUERIES, G_MAXINT, error)) {
			g_free (qname);
			return NULL;
		}
		g_free (qname);

		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Query buffer is empty"));

	return res;
}

static ToolCommandResult *
extra_command_query_buffer_from_dict (ToolCommand *command, guint argc, const gchar **argv,
				      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");

	if (argv[0] && *argv[0]) {
		/* Use tools favorites */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (console->current->cnc);
		if (! console->current->favorites)
			console->current->favorites = gda_tools_favorites_new (mstore);

		ToolsFavoritesAttributes att;
		gint favid;
		favid = gda_tools_favorites_find_by_name (console->current->favorites, 0,
							  GDA_TOOLS_FAVORITES_QUERIES,
							  argv[0], &att, NULL);      
		if (favid >= 0) {
			g_string_assign (console->current->query_buffer, att.contents);
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
		else {
			/* query retrieval */
			static GdaStatement *sel_stmt = NULL;
			static GdaSet *sel_params = NULL;
			GdaDataModel *model;
			const GValue *cvalue;
			GError *lerror = NULL;

			g_set_error (&lerror, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
				     "%s", _("Could not find favorite"));
			if (!sel_stmt) {
				sel_stmt = gda_sql_parser_parse_string (console->current->parser, 
									QUERY_BUFFERS_TABLE_SELECT_ONE, NULL, NULL);
				g_assert (sel_stmt);
				g_assert (gda_statement_get_parameters (sel_stmt, &sel_params, NULL));
			}

			if (! gda_set_set_holder_value (sel_params, error, "name", argv[0])) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			GdaConnection *store_cnc;
			store_cnc = gda_meta_store_get_internal_connection (mstore);
			model = gda_connection_statement_execute_select (store_cnc, sel_stmt, sel_params, NULL);
			if (!model) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			if ((gda_data_model_get_n_rows (model) == 1) &&
			    (cvalue = gda_data_model_get_value_at (model, 0, 0, NULL))) {
				g_string_assign (console->current->query_buffer, g_value_get_string (cvalue));
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
			}
			g_object_unref (model);
		}
	}
	else
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing query buffer name"));
		
	return res;
}

static ToolCommandResult *
extra_command_query_buffer_delete_dict (ToolCommand *command, guint argc, const gchar **argv,
					SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (!console->current->query_buffer) 
		console->current->query_buffer = g_string_new ("");

	if (argv[0] && *argv[0]) {
		/* Use tools favorites */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (console->current->cnc);
		if (! console->current->favorites)
			console->current->favorites = gda_tools_favorites_new (mstore);

		ToolsFavoritesAttributes att;
		GError *lerror = NULL;
		att.id = -1;
		att.type = GDA_TOOLS_FAVORITES_QUERIES;
		att.name = (gchar*) argv[0];
		att.descr = NULL;
		att.contents = NULL;

		if (! gda_tools_favorites_delete (console->current->favorites, 0,
						  &att, &lerror)) {		
			/* query retrieval */
			static GdaStatement *del_stmt = NULL;
			static GdaSet *del_params = NULL;
			if (!del_stmt) {
				del_stmt = gda_sql_parser_parse_string (console->current->parser, 
									QUERY_BUFFERS_TABLE_DELETE, NULL, NULL);
				g_assert (del_stmt);
				g_assert (gda_statement_get_parameters (del_stmt, &del_params, NULL));
			}

			if (! gda_set_set_holder_value (del_params, NULL, "name", argv[0])) {
				g_propagate_error (error, lerror);
				return NULL;
			}

			GdaConnection *store_cnc;
			store_cnc = gda_meta_store_get_internal_connection (mstore);
			if (gda_connection_statement_execute_non_select (store_cnc, del_stmt, del_params,
									 NULL, NULL) > 0)
				g_clear_error (&lerror);
			else {
				g_propagate_error (error, lerror);
				return NULL;
			}
		}
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing query buffer name"));
		
	return res;
}

static void foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model);
static ToolCommandResult *
extra_command_set (ToolCommand *command, guint argc, const gchar **argv,
		   SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *value = NULL;

	g_assert (console);
	if (!console->current) {
                g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

	if (argv[0] && *argv[0]) {
		pname = argv[0];
		if (argv[1] && *argv[1])
			value = argv[1];
	}

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
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

					prov = gda_connection_get_provider (console->current->cnc);
					dh = gda_server_provider_get_data_handler_g_type (prov,
											  console->current->cnc,
											  gda_holder_get_g_type (param));
					gvalue = gda_data_handler_get_value_from_str (dh, value, gda_holder_get_g_type (param));
					if (! gda_holder_take_value (param, gvalue, error))
						return NULL;
				}

				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_SET;
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
				g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_EMPTY;
			}
			else 
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
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
		g_hash_table_foreach (main_data->parameters, (GHFunc) foreach_param_set, model);
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
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

static void foreach_data_model (const gchar *name, GdaDataModel *keptmodel, GdaDataModel *model);
static ToolCommandResult *
extra_command_data_sets_list (ToolCommand *command, guint argc, const gchar **argv,
			      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	GdaDataModel *model;
	model = gda_data_model_array_new_with_g_types (2,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("dimensions (columns x rows)"));
	g_object_set_data (G_OBJECT (model), "name", _("List of kept data"));
	g_hash_table_foreach (main_data->mem_data_models, (GHFunc) foreach_data_model, model);
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static void
foreach_data_model (const gchar *name, GdaDataModel *keptmodel, GdaDataModel *model)
{
	gint row;
	gchar *str;
	GValue *value;
	row = gda_data_model_append_row (model, NULL);

	value = gda_value_new_from_string (name, G_TYPE_STRING);
	gda_data_model_set_value_at (model, 0, row, value, NULL);
	gda_value_free (value);

	str = g_strdup_printf ("%d x %d",
			       gda_data_model_get_n_columns (keptmodel),
			       gda_data_model_get_n_rows (keptmodel));
	value = gda_value_new_from_string (str, G_TYPE_STRING);
	g_free (str);
	gda_data_model_set_value_at (model, 1, row, value, NULL);
	gda_value_free (value);
}

static ToolCommandResult *
extra_command_data_set_grep (ToolCommand *command, guint argc, const gchar **argv,
			     SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *model_name = NULL;
	const gchar *pattern = NULL;

	if (argv[0] && *argv[0]) {
		model_name = argv[0];
		if (argv[1] && *argv[1])
			pattern = argv[1];
	}
	if (!model_name) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src, *model;
	gint nbfields, nbrows, i;

	src = g_hash_table_lookup (main_data->mem_data_models, model_name);
	if (!src) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), model_name);
		return NULL;
	}
	nbfields = gda_data_model_get_n_columns (src);
	nbrows = gda_data_model_get_n_rows (src);
        model = gda_data_model_array_new (nbfields);
	if (g_object_get_data (G_OBJECT (src), "name"))
                g_object_set_data_full (G_OBJECT (model), "name", g_strdup (g_object_get_data (G_OBJECT (src), "name")), g_free);
        if (g_object_get_data (G_OBJECT (src), "descr"))
                g_object_set_data_full (G_OBJECT (model), "descr", g_strdup (g_object_get_data (G_OBJECT (src), "descr")), g_free);
        for (i = 0; i < nbfields; i++) {
		GdaColumn *copycol, *srccol;
                gchar *colid;

                srccol = gda_data_model_describe_column (src, i);
                copycol = gda_data_model_describe_column (model, i);

                g_object_get (G_OBJECT (srccol), "id", &colid, NULL);
                g_object_set (G_OBJECT (copycol), "id", colid, NULL);
                g_free (colid);
                gda_column_set_description (copycol, gda_column_get_description (srccol));
                gda_column_set_name (copycol, gda_column_get_name (srccol));
                gda_column_set_dbms_type (copycol, gda_column_get_dbms_type (srccol));
                gda_column_set_g_type (copycol, gda_column_get_g_type (srccol));
                gda_column_set_position (copycol, gda_column_get_position (srccol));
                gda_column_set_allow_null (copycol, gda_column_get_allow_null (srccol));
	}

	for (i = 0; i < nbrows; i++) {
		gint nrid, j;
		gboolean add_row = TRUE;
		const GValue *cvalue;
		GRegex *regex = NULL;

		if (pattern && *pattern) {
			if (!regex) {
				GRegexCompileFlags flags = G_REGEX_OPTIMIZE | G_REGEX_CASELESS;
				regex = g_regex_new ((const gchar*) pattern, flags, 0, error);
				if (!regex) {
					g_object_unref (model);
					return NULL;
				}
			}

			add_row = FALSE;
			for (j = 0; j < nbfields; j++) {
				cvalue = gda_data_model_get_value_at (src, j, i, error);
				if (!cvalue) {
					if (regex)
						g_regex_unref (regex);
					g_object_unref (model);
					return NULL;
				}
				gchar *str;
				str = gda_value_stringify (cvalue);
				if (g_regex_match (regex, str, 0, NULL)) {
					add_row = TRUE;
					g_free (str);
					break;
				}
				g_free (str);
			}
		}
		if (!add_row)
			continue;

		nrid = gda_data_model_append_row (model, error);
		if (nrid < 0) {
			if (regex)
				g_regex_unref (regex);
			g_object_unref (model);
			return NULL;
		}

		for (j = 0; j < nbfields; j++) {
			cvalue = gda_data_model_get_value_at (src, j, i, error);
			if (!cvalue) {
				if (regex)
					g_regex_unref (regex);
				g_object_unref (model);
				return NULL;
			}
			if (! gda_data_model_set_value_at (model, j, nrid, cvalue, error)) {
				if (regex)
					g_regex_unref (regex);
				g_object_unref (model);
				return NULL;
			}
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static ToolCommandResult *
extra_command_data_set_show (ToolCommand *command, guint argc, const gchar **argv,
			      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *model_name = NULL;

	if (argv[0] && *argv[0])
		model_name = argv[0];

	if (!model_name) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src;
	src = g_hash_table_lookup (main_data->mem_data_models, model_name);
	if (!src) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), model_name);
		return NULL;
	}

	if (argv[1] && *argv[1]) {
		GArray *cols;
		gint i;
		cols = g_array_new (FALSE, FALSE, sizeof (gint));
		for (i = 1; argv[i] && *argv[i]; i++) {
			const gchar *cname = argv[i];
			gint pos;
			pos = gda_data_model_get_column_index (src, cname);
			if (pos < 0) {
				long int li;
				char *end;
				li = strtol (cname, &end, 10);
				if (!*end && (li >= 0) && (li < G_MAXINT))
					pos = (gint) li;
			}
			if (pos < 0) {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     _("Could not identify column '%s'"), cname);
				g_array_free (cols, TRUE);
				return NULL;
			}
			g_array_append_val (cols, pos);
		}

		GdaDataModel *model;
		model = (GdaDataModel*) gda_data_model_array_copy_model_ext (src, cols->len, (gint*) cols->data,
									     error);
		g_array_free (cols, TRUE);
		if (model) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
		}
		else
			return NULL;
	}
	else {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = g_object_ref (src);
	}

	return res;
}

static ToolCommandResult *
extra_command_data_set_rm (ToolCommand *command, guint argc, const gchar **argv,
			   SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	guint i;

	if (!argv[0] ||  !(*argv[0])) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}
	for (i = 0; argv[i]; i++) {
		GdaDataModel *src;
		src = g_hash_table_lookup (main_data->mem_data_models, argv[i]);
		if (!src) {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("Could not find dataset named '%s'"), argv[i]);
			return NULL;
		}
	}

	for (i = 0; argv[i]; i++)
		g_hash_table_remove (main_data->mem_data_models, argv[i]);

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static ToolCommandResult *
extra_command_data_set_import (ToolCommand *command, guint argc, const gchar **argv,
			       SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *type = NULL, *file_name = NULL;

	if (argv[0] && *argv[0]) {
		type = argv[0];
		if (g_ascii_strcasecmp (type, "csv")) {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown import format '%s'"), argv[0]);
			return NULL;
		}
		if (argv[1] && *argv[1])
			file_name = argv[1];
	}

	if (!type || !file_name) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *model;
	GdaSet *impopt;
	impopt = make_options_set_from_gdasql_options ("csv");
	model = gda_data_model_import_new_file (file_name, TRUE, impopt);
	if (impopt)
		g_object_unref (impopt);
	if (!model) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     _("Could not import file '%s'"), file_name);
		return NULL;
	}
	GSList *list;
	list = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model));
	if (list) {
		g_propagate_error (error, g_error_copy ((GError*) list->data));
		return NULL;
	}

	g_hash_table_insert (main_data->mem_data_models, g_strdup (LAST_DATA_MODEL_NAME), model);
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

static ToolCommandResult *
extra_command_data_set_move (ToolCommand *command, guint argc, const gchar **argv,
			     SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *old_name = NULL;
	const gchar *new_name = NULL;

	if (argv[0] && *argv[0]) {
		old_name = argv[0];
		if (argv[1] && *argv[1])
			new_name = argv[1];
	}
	if (!old_name || !*old_name || !new_name || !*new_name) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing argument"));
		return NULL;
	}

	GdaDataModel *src;

	src = g_hash_table_lookup (main_data->mem_data_models, old_name);
	if (!src) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Could not find dataset named '%s'"), old_name);
		return NULL;
	}

	g_hash_table_insert (main_data->mem_data_models, g_strdup (new_name), g_object_ref (src));
	if (strcmp (old_name, LAST_DATA_MODEL_NAME))
		g_hash_table_remove (main_data->mem_data_models, old_name);

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;

	return res;
}

typedef struct {
	gchar *fkname;
	gchar *table;
	gchar *ref_table;
	GArray *columns;
	GArray *ref_columns;
} FkDeclData;

static void
fk_decl_data_free (FkDeclData *data)
{
	g_free (data->fkname);
	g_free (data->table);
	g_free (data->ref_table);
	if (data->columns) {
		gint i;
		for (i = 0; i < data->columns->len; i++)
			g_free (g_array_index (data->columns, gchar*, i));
		g_array_free (data->columns, TRUE);
	}
	if (data->ref_columns) {
		gint i;
		for (i = 0; i < data->ref_columns->len; i++)
			g_free (g_array_index (data->ref_columns, gchar*, i));
		g_array_free (data->ref_columns, TRUE);
	}
	g_free (data);
}

static FkDeclData *
parse_fk_decl_spec (const gchar *spec, gboolean columns_required, GError **error)
{
	FkDeclData *decldata;
	decldata = g_new0 (FkDeclData, 1);

	gchar *ptr, *dspec, *start, *subptr, *substart;

	if (!spec || !*spec) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key declaration specification"));
		return NULL;
	}
	dspec = g_strstrip (g_strdup (spec));

	/* FK name */
	for (ptr = dspec; *ptr && !g_ascii_isspace (*ptr); ptr++) {
		if (! g_ascii_isalnum (*ptr) && (*ptr != '_'))
			goto onerror;
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	decldata->fkname = g_strstrip (g_strdup (dspec));
#ifdef GDA_DEBUG_NO
	g_print ("KFNAME [%s]\n", decldata->fkname);
#endif

	/* table name */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("TABLE [%s]\n", decldata->table);
#endif

	if (columns_required) {
		/* columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("COLS [%s]\n", start);
#endif
		substart = start;
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->columns)
				decldata->columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->columns)
			goto onerror;
	}

	/* ref table */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->ref_table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("REFTABLE [%s]\n", decldata->ref_table);
#endif

	if (columns_required) {
		/* ref columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("REF COLS [%s]\n", start);
#endif
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->ref_columns)
				decldata->ref_columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->ref_columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->ref_columns)
			goto onerror;
	}

	g_free (dspec);

	if (columns_required && (decldata->columns->len != decldata->ref_columns->len))
		goto onerror;

	return decldata;

 onerror:
	fk_decl_data_free (decldata);
	g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
		     "%s", _("Malformed foreign key declaration specification"));
	return NULL;
}

/*
 * decomposes @table and sets the the out* variables
 */
static gboolean
fk_decl_analyse_table_name (const gchar *table, GdaMetaStore *mstore,
			    gchar **out_catalog, gchar **out_schema, gchar **out_table,
			    GError **error)
{
	gchar **id_array;
	gint size = 0, l;

	id_array = gda_sql_identifier_split (table);
	if (id_array) {
		size = g_strv_length (id_array) - 1;
		g_assert (size >= 0);
		l = size;
		*out_table = g_strdup (id_array[l]);
		l--;
		if (l >= 0) {
			*out_schema = g_strdup (id_array[l]);
			l--;
			if (l >= 0)
				*out_catalog = g_strdup (id_array[l]);
		}
		g_strfreev (id_array);
	}
	else {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Malformed table name specification '%s'"), table);
		return FALSE;
	}
	return TRUE;
}

static ToolCommandResult *
extra_command_declare_fk (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (console->current->cnc);
		if (! (decldata = parse_fk_decl_spec (argv[0], TRUE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gchar **colnames, **ref_colnames;
		gint l;
		gboolean allok;
		colnames = g_new0 (gchar *, decldata->columns->len);
		ref_colnames = g_new0 (gchar *, decldata->columns->len);
		for (l = 0; l < decldata->columns->len; l++) {
			colnames [l] = g_array_index (decldata->columns, gchar*, l);
			ref_colnames [l] = g_array_index (decldata->ref_columns, gchar*, l);
		}

		allok = gda_meta_store_declare_foreign_key (mstore, NULL,
							    decldata->fkname,
							    catalog, schema, table,
							    ref_catalog, ref_schema, ref_table,
							    decldata->columns->len,
							    colnames, ref_colnames, error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		g_free (colnames);
		g_free (ref_colnames);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key name argument"));
	return res;
}

static ToolCommandResult *
extra_command_undeclare_fk (ToolCommand *command, guint argc, const gchar **argv,
			    SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection opened"));
		return NULL;
	}

	if (argv[0] && *argv[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (console->current->cnc);
		if (! (decldata = parse_fk_decl_spec (argv[0], FALSE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gboolean allok;
		allok = gda_meta_store_undeclare_foreign_key (mstore, NULL,
							      decldata->fkname,
							      catalog, schema, table,
							      ref_catalog, ref_schema, ref_table,
							      error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing foreign key name argument"));
	return res;
}

static const GValue *
get_table_value_at_cell (SqlConsole *console, GError **error,
			 const gchar *table, const gchar *column, const gchar *row_cond,
			 GdaDataModel **out_model_of_value)
{
	const GValue *retval = NULL;

	*out_model_of_value = NULL;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rcolumn;
	
	rtable = gda_sql_identifier_quote (table, console->current->cnc, NULL, FALSE, FALSE);
	rcolumn = gda_sql_identifier_quote (column, console->current->cnc, NULL, FALSE, FALSE);
	sql = g_strdup_printf ("SELECT %s FROM %s WHERE %s", rcolumn, rtable, row_cond);
	g_free (rtable);
	g_free (rcolumn);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (console->current->parser, sql, &remain, error);
	if (!stmt) {
		g_free (sql);
		return NULL;
	}
	if (remain) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong row condition"));
		g_free (sql);
		return NULL;
	}
	g_object_unref (stmt);

	/* execute statement */
	ToolCommandResult *tmpres;
	tmpres = execute_sql_command (NULL, sql, GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	g_free (sql);
	if (!tmpres)
		return NULL;
	gboolean errorset = FALSE;
	if (tmpres->type == TOOL_COMMAND_RESULT_DATA_MODEL) {
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
	tool_command_result_free (tmpres);

	if (!retval && !errorset)
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("No unique row identified"));

	return retval;
}

static ToolCommandResult *
extra_command_set2 (ToolCommand *command, guint argc, const gchar **argv,
		    SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *filename = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *row_cond = NULL;
	gint whichargv = 0;
	
	if (!console->current) {
                g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

        if (argv[0] && *argv[0]) {
                pname = argv[0];
                if (argv[1] && *argv[1]) {
			if (argv[2] && *argv[2]) {
				table = argv[1];
				column = argv[2];
				if (argv[3] && *argv[3]) {
					row_cond = argv[3];
					if (argv [4]) {
						g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
							     "%s", _("Too many arguments"));
						return NULL;
					}
					whichargv = 1;
				}
			}
			else {
				filename = argv[1];
				whichargv = 2;
			}
		}
        }

	if (whichargv == 1) {
		/* param from an existing blob */
		const GValue *value;
		GdaDataModel *model = NULL;
		value = get_table_value_at_cell (console, error, table,
						 column, row_cond, &model);
		if (value) {
			GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
			if (param) 
				g_hash_table_remove (main_data->parameters, pname);
		
			param = gda_holder_new (G_VALUE_TYPE (value));
			g_assert (gda_holder_set_value (param, value, NULL));
			g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
		if (model)
			g_object_unref (model);
	}
	else if (whichargv == 2) {
		/* param from filename */
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		GValue *bvalue;
		if (param) 
			g_hash_table_remove (main_data->parameters, pname);
		
		param = gda_holder_new (GDA_TYPE_BLOB);
		bvalue = gda_value_new_blob_from_file (filename);
		g_assert (gda_holder_take_value (param, bvalue, NULL));
		g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}
	else 
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong number of arguments"));

	return res;
}

static ToolCommandResult *
extra_command_pivot (ToolCommand *command, guint argc, const gchar **argv,
		     SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	ConnectionSetting *cs;
	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	const gchar *select = NULL;
	const gchar *row_fields = NULL;
	const gchar *column_fields = NULL;

        if (argv[0] && *argv[0]) {
                select = argv[0];
                if (argv[1] && *argv[1]) {
			row_fields = argv [1];
			if (argv[2] && *argv[2])
				column_fields = argv[2];
		}
        }

	if (!select) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing data on which to operate"));
		return NULL;
	}
	if (!row_fields) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing row fields specifications"));
		return NULL;
	}
	if (column_fields && !strcmp (column_fields, "-"))
		column_fields = NULL;

	/* execute SELECT */
	gboolean was_in_trans;
	ToolCommandResult *tmpres;

	was_in_trans = gda_connection_get_transaction_status (cs->cnc) ? TRUE : FALSE;

	tmpres = command_execute (console, select, GDA_STATEMENT_MODEL_CURSOR_FORWARD, error);
	if (!tmpres)
		return NULL;
	if (tmpres->type != TOOL_COMMAND_RESULT_DATA_MODEL) {
		tool_command_result_free (tmpres);
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong SELECT argument"));
		return NULL;
	}

	GdaDataPivot *pivot;
	gdouble etime = 0.;
	//g_object_get ((GObject*) tmpres->u.model, "execution-delay", &etime, NULL);
	pivot = (GdaDataPivot*) gda_data_pivot_new (tmpres->u.model);
	tool_command_result_free (tmpres);

	if (! gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_ROW,
					row_fields, NULL, error)) {
		g_object_unref (pivot);
		return NULL;
	}

	if (column_fields &&
	    ! gda_data_pivot_add_field (pivot, GDA_DATA_PIVOT_FIELD_COLUMN,
					column_fields, NULL, error)) {
		g_object_unref (pivot);
		return NULL;
	}

	GTimer *timer;
	timer = g_timer_new ();

	gint i;
	for (i = 3; argv[i] && *argv[i]; i++) {
		const gchar *df = argv[i];
		const gchar *alias = "count";
		GdaDataPivotAggregate agg = GDA_DATA_PIVOT_COUNT;
		if (*df == '[') {
			const gchar *tmp;
			for (tmp = df; *tmp && *tmp != ']'; tmp++);
			if (!*tmp) {
				g_timer_destroy (timer);
				g_object_unref (pivot);
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     "%s", _("Wrong data field argument"));
				return NULL;
			}
			df++;
			if (! g_ascii_strncasecmp (df, "sum", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_SUM;
				alias = "sum";
			}
			else if (! g_ascii_strncasecmp (df, "avg", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_AVG;
				alias = "avg";
			}
			else if (! g_ascii_strncasecmp (df, "max", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_MAX;
				alias = "max";
			}
			else if (! g_ascii_strncasecmp (df, "min", 3) && (df[3] == ']')) {
				agg = GDA_DATA_PIVOT_MIN;
				alias = "min";
			}
			else if (! g_ascii_strncasecmp (df, "count", 5) && (df[5] == ']')) {
				agg = GDA_DATA_PIVOT_COUNT;
				alias = "count";
			}
			else {
				g_timer_destroy (timer);
				g_object_unref (pivot);
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     "%s", _("Wrong data field argument"));
				return NULL;
			}
			df = tmp+1;
		}
		gchar *tmp;
		tmp = g_strdup_printf ("%s_%s", alias, df);
		if (! gda_data_pivot_add_data (pivot, agg, df, tmp, error)) {
			g_free (tmp);
			g_timer_destroy (timer);
			g_object_unref (pivot);
			return NULL;
		}
		g_free (tmp);
	}

	if (! gda_data_pivot_populate (pivot, error)) {
		g_timer_destroy (timer);
		g_object_unref (pivot);
		return NULL;
	}

	etime += g_timer_elapsed (timer, NULL);
	g_timer_destroy (timer);
	//g_object_set (pivot, "execution-delay", etime, NULL);

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->was_in_transaction_before_exec = was_in_trans;
	res->u.model = (GdaDataModel*) pivot;
	return res;
}

#ifdef HAVE_LDAP
static ToolCommandResult *
extra_command_ldap_search (ToolCommand *command, guint argc, const gchar **argv,
			   SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	ConnectionSetting *cs;
	GdaDataModel *model, *wrapper;
	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (cs->cnc)) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *filter = NULL;
	gchar *lfilter = NULL;
	const gchar *scope = NULL;
	const gchar *base_dn = NULL;
	GdaLdapSearchScope lscope = GDA_LDAP_SEARCH_SUBTREE;

        if (argv[0] && *argv[0]) {
                filter = argv[0];
                if (argv[1]) {
			scope = argv [1];
			if (!g_ascii_strcasecmp (scope, "base"))
				lscope = GDA_LDAP_SEARCH_BASE;
			else if (!g_ascii_strcasecmp (scope, "onelevel"))
				lscope = GDA_LDAP_SEARCH_ONELEVEL;
			else if (!g_ascii_strcasecmp (scope, "subtree"))
				lscope = GDA_LDAP_SEARCH_SUBTREE;
			else {
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
					     _("Unknown search scope '%s'"), scope);
				return NULL;
			}
			if (argv[2])
				base_dn = argv[2];
		}
        }

	if (!filter) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing filter which to operate"));
		return NULL;
	}

	if (*filter != '(')
		lfilter = g_strdup_printf ("(%s)", filter);

	GdaHolder *param;
	const gchar *ldap_attributes;
	ldap_attributes = g_value_get_string (gda_set_get_holder_value (main_data->options, "ldap_attributes"));
	model = gda_data_model_ldap_new (cs->cnc, base_dn, lfilter ? lfilter : filter,
					 ldap_attributes ? ldap_attributes : DEFAULT_LDAP_ATTRIBUTES,
					 lscope);
	g_free (lfilter);
	param = gda_set_get_holder (main_data->options, "ldap_dn");
	wrapper = gda_data_access_wrapper_new (model);
	g_object_unref (model);

	if (param) {
		const GValue *cvalue;
		gchar *tmp = NULL;
		cvalue = gda_holder_get_value (param);
		if (cvalue)
			tmp = gda_value_stringify (cvalue);
		if (tmp) {
			if (!g_ascii_strcasecmp (tmp, "rdn"))
				g_object_set ((GObject *) model, "use-rdn", TRUE, NULL);
			else if (!g_ascii_strncasecmp (tmp, "no", 2)) {
				gint nbcols;
				nbcols = gda_data_model_get_n_columns (wrapper);
				if (nbcols > 1) {
					gint *map, i;
					map = g_new (gint, nbcols - 1);
					for (i = 0; i < nbcols - 1; i++)
						map [i] = i+1;
					gda_data_access_wrapper_set_mapping (GDA_DATA_ACCESS_WRAPPER (wrapper),
									     map, nbcols - 1);
					g_free (map);
				}
			}
			g_free (tmp);
		}
	}

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = wrapper;
	return res;
}

static ToolCommandResult *
extra_command_ldap_mv (ToolCommand *command, guint argc, const gchar **argv,
		       SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	ConnectionSetting *cs;
	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (cs->cnc)) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *current_dn = NULL;
	const gchar *new_dn = NULL;

        if (argv[0] && *argv[0]) {
                current_dn = argv[0];
                if (argv[1])
			new_dn = argv [1];
        }

	if (!current_dn || !new_dn) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing current DN or new DN specification"));
		return NULL;
	}

	if (gda_ldap_rename_entry (GDA_LDAP_CONNECTION (cs->cnc), current_dn, new_dn, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

/*
 * parses text as <att_name>[=<att_value>]:
 *  - if value_req is %TRUE then the format has to be <att_name>=<att_value>
 *  - if value_req is %TRUE then the format has to be <att_name>=<att_value> or <att_name>
 *
 * if returned value is %TRUE, then @out_attr_name and @out_value are allocated
 */
static gboolean
parse_ldap_attr (const gchar *spec, gboolean value_req,
		 gchar **out_attr_name, GValue **out_value)
{
	const gchar *ptr;
	g_return_val_if_fail (spec && *spec, FALSE);
	*out_attr_name = NULL;
	*out_value = NULL;

	for (ptr = spec; *ptr && (*ptr != '='); ptr++);
	if (!*ptr) {
		if (value_req)
			return FALSE;
		else {
			*out_attr_name = g_strstrip (g_strdup (spec));
			return TRUE;
		}
	}
	else {
		/* copy attr name part */
		*out_attr_name = g_new (gchar, ptr - spec + 1);
		memcpy (*out_attr_name, spec, ptr - spec);
		(*out_attr_name) [ptr - spec] = 0;
		g_strstrip (*out_attr_name);

		/* move on to the attr value */
		ptr++;
		if (!*ptr) {
			/* no value ! */
			g_free (*out_attr_name);
			*out_attr_name = NULL;
			return FALSE;
		}
		else {
			gchar *tmp;
			*out_value = gda_value_new (G_TYPE_STRING);
			tmp = g_strdup (ptr);
			g_value_take_string (*out_value, tmp);
			return TRUE;
		}
	}
}

static ToolCommandResult *
extra_command_ldap_mod (ToolCommand *command, guint argc, const gchar **argv,
			SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	ConnectionSetting *cs;
	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (cs->cnc)) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *dn = NULL;
	const gchar *op = NULL;
	GdaLdapModificationType optype;
        if (argv[0] && *argv[0]) {
                dn = argv[0];
                if (argv[1])
			op = argv [1];
        }

	if (!dn) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DN of LDAP entry"));
		return NULL;
	}
	if (!op) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing operation to perform on LDAP entry's attributes"));
		return NULL;
	}

	if (! g_ascii_strncasecmp (op, "DEL", 3))
		optype = GDA_LDAP_MODIFICATION_ATTR_DEL;
	else if (! g_ascii_strncasecmp (op, "REPL", 4))
		optype = GDA_LDAP_MODIFICATION_ATTR_REPL;
	else if (! g_ascii_strncasecmp (op, "ADD", 3))
		optype = GDA_LDAP_MODIFICATION_ATTR_ADD;
	else {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     _("Unknown operation '%s' to perform on LDAP entry's attributes"), op);
		return NULL;
	}

	GdaLdapEntry *entry;
	guint i;

	entry = gda_ldap_entry_new (dn);
	for (i = 2; argv[i]; i++) {
		gchar *att_name;
		GValue *att_value;
		gboolean vreq = TRUE;
		if (optype == GDA_LDAP_MODIFICATION_ATTR_DEL)
			vreq = FALSE;
		if (parse_ldap_attr (argv[i], vreq, &att_name, &att_value)) {
			gda_ldap_entry_add_attribute (entry, TRUE, att_name, 1, &att_value);
			g_free (att_name);
			gda_value_free (att_value);
		}
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("Wrong attribute value specification '%s'"), argv[i]);
			return NULL;
		}
	}

	if (gda_ldap_modify_entry (GDA_LDAP_CONNECTION (cs->cnc), optype, entry, NULL, error)) {
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}
	gda_ldap_entry_free (entry);
	return res;
}


typedef struct {
	GValue *attr_name;
	GValue *req;
	GArray *values; /* array of #GValue */
} AttRow;

static gint
att_row_cmp (AttRow *r1, AttRow *r2)
{
	return strcmp (g_value_get_string (r1->attr_name), g_value_get_string (r2->attr_name));
}

static ToolCommandResult *
extra_command_ldap_descr (ToolCommand *command, guint argc, const gchar **argv,
			  SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	ConnectionSetting *cs;
	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No connection specified"));
		return NULL;
	}

	if (! GDA_IS_LDAP_CONNECTION (cs->cnc)) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
			     "%s", _("Connection is not an LDAP connection"));
		return NULL;
	}

	const gchar *dn = NULL;
	GHashTable *attrs_hash = NULL;
	gint options = 0; /* 0 => show attributes specified with ".option ldap_attributes"
			   * 1 => show all attributes
			   * 2 => show non set attributes only
			   * 3 => show all set attributes only
			   */

        if (!argv[0] || !*argv[0]) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing DN (Distinguished name) argument"));		
		return NULL;
        }
	dn = argv[0];
	if (argv [1] && *argv[1]) {
		if (!g_ascii_strcasecmp (argv [1], "all"))
			options = 1;
		else if (!g_ascii_strcasecmp (argv [1], "unset"))
			options = 2;
		else if (!g_ascii_strcasecmp (argv [1], "set"))
			options = 3;
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     _("Unknown '%s' argument"), argv[1]);
			return NULL;
		}
	}

	GdaLdapEntry *entry;
	GdaDataModel *model;
	entry = gda_ldap_describe_entry (GDA_LDAP_CONNECTION (cs->cnc), dn, error);
	if (!entry) {
		if (error && !*error)
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
				     _("Could not find entry with DN '%s'"), dn);
		return NULL;
	}

	model = gda_data_model_array_new_with_g_types (3,
						       G_TYPE_STRING,
						       G_TYPE_BOOLEAN,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Attribute"));
	gda_data_model_set_column_title (model, 1, _("Required?"));
	gda_data_model_set_column_title (model, 2, _("Value"));
	g_object_set_data (G_OBJECT (model), "name", _("LDAP entry's Attributes"));

	/* parse attributes to use */
	const gchar *ldap_attributes;
	ldap_attributes = g_value_get_string (gda_set_get_holder_value (main_data->options, "ldap_attributes"));
	if ((options == 0) && ldap_attributes) {
		gchar **array;
		guint i;
		array = g_strsplit (ldap_attributes, ",", 0);
		for (i = 0; array [i]; i++) {
			gchar *tmp;
			for (tmp = array [i]; *tmp && (*tmp != ':'); tmp++);
			*tmp = 0;
			g_strstrip (array [i]);
			if (!attrs_hash)
				attrs_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
								    NULL);
			g_hash_table_insert (attrs_hash, array [i], (gpointer) 0x01);
		}
		g_free (array);
	}

	/* Prepare rows */
	GSList *rows_list = NULL; /* sorted list of #AttRow, used to create the data model */
	GHashTable *rows_hash; /* key = att name, value = #AttRow */	
	rows_hash = g_hash_table_new (g_str_hash, g_str_equal);

	/* 1st pass: all actual entry's attributes */
	if (options != 2) {
		guint i;
		for (i = 0; i < entry->nb_attributes; i++) {
			GdaLdapAttribute *attr;
			guint j;
			attr = (GdaLdapAttribute*) entry->attributes[i];

			if (attrs_hash && !g_hash_table_lookup (attrs_hash, attr->attr_name))
				continue;

			AttRow *row;
			row = g_new0 (AttRow, 1);
			g_value_set_string ((row->attr_name = gda_value_new (G_TYPE_STRING)),
					    attr->attr_name);
			row->req = NULL;
			row->values = g_array_new (FALSE, FALSE, sizeof (GValue*));
			for (j = 0; j < attr->nb_values; j++) {
				GValue *copy;
				copy = gda_value_copy (attr->values [j]);
				g_array_append_val (row->values, copy);
			}

			g_hash_table_insert (rows_hash, (gpointer) g_value_get_string (row->attr_name), row);
			rows_list = g_slist_insert_sorted (rows_list, row, (GCompareFunc) att_row_cmp);
		}
	}

	/* 2nd pass: use the class's attributes */
	GSList *attrs_list, *list;
	attrs_list = gda_ldap_entry_get_attributes_list (GDA_LDAP_CONNECTION (cs->cnc), entry);
	for (list = attrs_list; list; list = list->next) {
		GdaLdapAttributeDefinition *def;
		def = (GdaLdapAttributeDefinition*) list->data;
		GdaLdapAttribute *vattr;
		vattr = g_hash_table_lookup (entry->attributes_hash, def->name);
		if (! (((options == 0) && vattr) ||
		       ((options == 3) && vattr) ||
		       (options == 1) ||
		       ((options == 2) && !vattr)))
			continue;

		if (attrs_hash && !g_hash_table_lookup (attrs_hash, def->name))
			continue;

		AttRow *row;
		row = g_hash_table_lookup (rows_hash, def->name);
		if (row) {
			if (!row->req)
				g_value_set_boolean ((row->req = gda_value_new (G_TYPE_BOOLEAN)),
						     def->required);
		}
		else {
			row = g_new0 (AttRow, 1);
			g_value_set_string ((row->attr_name = gda_value_new (G_TYPE_STRING)),
					    def->name);
			g_value_set_boolean ((row->req = gda_value_new (G_TYPE_BOOLEAN)),
					     def->required);
			row->values = NULL;
			g_hash_table_insert (rows_hash, (gpointer) g_value_get_string (row->attr_name), row);
			rows_list = g_slist_insert_sorted (rows_list, row, (GCompareFunc) att_row_cmp);
		}
	}
	gda_ldap_attributes_list_free (attrs_list);

	if (attrs_hash)
		g_hash_table_destroy (attrs_hash);

	/* store all rows in data model */
	GValue *nvalue = NULL;
	nvalue = gda_value_new_null ();
	for (list = rows_list; list; list = list->next) {
		AttRow *row;
		GList *vlist;
		guint i;
		row = (AttRow*) list->data;
		vlist = g_list_append (NULL, row->attr_name);
		if (row->req)
			vlist = g_list_append (vlist, row->req);
		else
			vlist = g_list_append (vlist, nvalue);
		if (row->values) {
			for (i = 0; i < row->values->len; i++) {
				GValue *value;
				value = g_array_index (row->values, GValue*, i);
				vlist = g_list_append (vlist, value);
				g_assert (gda_data_model_append_values (model, vlist, NULL) != -1);
				gda_value_free (value);
				vlist = g_list_remove (vlist, value);
			}
			g_array_free (row->values, TRUE);
		}
		else {
			vlist = g_list_append (vlist, nvalue);
			g_assert (gda_data_model_append_values (model, vlist, NULL) != -1);
		}
		g_list_free (vlist);
		gda_value_free (row->attr_name);
		gda_value_free (row->req);
		g_free (row);
	}
	g_slist_free (rows_list);


	gda_ldap_entry_free (entry);
	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

#endif /* HAVE_LDAP */

static ToolCommandResult *
extra_command_export (ToolCommand *command, guint argc, const gchar **argv,
		      SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	const gchar *pname = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;
	gint whichargv = 0;
	
	if (!console->current) {
                g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

        if (argv[0] && *argv[0]) {
                table = argv[0];
                pname = argv[0];
                if (argv[1] && *argv[1]) {
                        column = argv[1];
			filename = argv[1];
			if (argv[2] && *argv[2]) {
				row_cond = argv[2];
				if (argv[3] && *argv[3]) {
					filename = argv[3];
					if (argv [4]) {
						g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
							     "%s", _("Too many arguments"));
						return NULL;
					}
					else
						whichargv = 1;
				}
			}
			else {
				whichargv = 2;
			}
		}
        }

	const GValue *value = NULL;
	GdaDataModel *model = NULL;

	if (whichargv == 1) 
		value = get_table_value_at_cell (console, error, table, column,
						 row_cond, &model);
	else if (whichargv == 2) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		if (!pname) 
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
				     _("No parameter named '%s' defined"), pname);
		else
			value = gda_holder_get_value (param);
	}
	else 
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong number of arguments"));

	if (value) {
		/* to file through this blob */
		gboolean done = FALSE;

		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			const GdaBlob *fblob = gda_value_get_blob (value);
			if (gda_blob_op_write (tblob->op, (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     "%s", _("Could not write file"));
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
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     "%s", _("Could not write file"));
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
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
	}
	if (model)
		g_object_unref (model);

				
	return res;
}


static ToolCommandResult *
extra_command_unset (ToolCommand *command, guint argc, const gchar **argv,
		     SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;
	const gchar *pname = NULL;

	if (argv[0] && *argv[0]) 
		pname = argv[0];

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		if (param) {
			g_hash_table_remove (main_data->parameters, pname);
			res = g_new0 (ToolCommandResult, 1);
			res->type = TOOL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
				     _("No parameter named '%s' defined"), pname);
	}
	else {
		g_hash_table_destroy (main_data->parameters);
		main_data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_EMPTY;
	}
		
	return res;
}

#define DO_UNLINK(x) g_unlink(x)
static void
graph_func_child_died_cb (GPid pid, G_GNUC_UNUSED gint status, gchar *fname)
{
	DO_UNLINK (fname);
	g_free (fname);
	g_spawn_close_pid (pid);
}

static gchar *
create_graph_from_meta_struct (G_GNUC_UNUSED GdaConnection *cnc, GdaMetaStruct *mstruct, GError **error)
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

static ToolCommandResult *
extra_command_graph (ToolCommand *command, guint argc, const gchar **argv,
		     SqlConsole *console, GError **error)
{
	gchar *result;
	GdaMetaStruct *mstruct;

	g_assert (console);
	if (!console->current) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
		return NULL;
	}

	mstruct = gda_internal_command_build_meta_struct (console->current->cnc, argv, error);
	if (!mstruct)
		return NULL;
	
	result = create_graph_from_meta_struct (console->current->cnc, mstruct, error);
	g_object_unref (mstruct);
	if (result) {
		ToolCommandResult *res;
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (result);
		g_free (result);
		return res;
	}
	else 
		return NULL;
}



#ifdef HAVE_LIBSOUP
static ToolCommandResult *
extra_command_httpd (ToolCommand *command, guint argc, const gchar **argv,
		     SqlConsole *console, GError **error)
{
	ToolCommandResult *res = NULL;

	if (main_data->server) {
		/* stop server */
		g_object_unref (main_data->server);
		main_data->server = NULL;
		res = g_new0 (ToolCommandResult, 1);
		res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (_("HTTPD server stopped"));
	}
	else {
		/* start new server */
		gint port = 12345;
		const gchar *auth_token = NULL;
		if (argv[0] && *argv[0]) {
			gchar *ptr;
			port = (gint) strtol (argv[0], &ptr, 10);
			if (ptr && *ptr)
				port = -1;
			if (argv[1] && *argv[1]) {
				auth_token = argv[1];
			}
		}
		if (port > 0) {
			main_data->server = web_server_new (port, auth_token);
			if (!main_data->server) 
				g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_INTERNAL_COMMAND_ERROR,
					     "%s", _("Could not start HTTPD server"));
			else {
				res = g_new0 (ToolCommandResult, 1);
				res->type = TOOL_COMMAND_RESULT_TXT_STDOUT;
				res->u.txt = g_string_new (_("HTTPD server started"));
			}
		}
		else
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Invalid port specification"));
	}

	return res;
}
#endif

#ifdef NONE
static ToolCommandResult *
extra_command_lo_update (ToolCommand *command, guint argc, const gchar **argv,
			 SqlConsole *console, GError **error)
{
	ToolCommandResult *res;

	const gchar *table = NULL;
        const gchar *blob_col = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;

	if (!cnc) {
                g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_NO_CONNECTION_ERROR,
			     "%s", _("No current connection"));
                return NULL;
        }

        if (argv[0] && *argv[0]) {
                filename = argv[0];
                if (argv[1] && *argv[1]) {
                        table = argv[1];
			if (argv[2] && *argv[2]) {
				blob_col = argv[2];
				if (argv[3] && *argv[3]) {
					row_cond = argv[3];
					if (argv [4]) {
						g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
							     "%s", _("Too many arguments"));
						return NULL;
					}
				}
			}
		}
        }
	if (!row_cond) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing arguments"));
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
	
	rtable = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);
	rblob_col = gda_sql_identifier_quote (blob_col, cnc, NULL, FALSE, FALSE);
	sql = g_strdup_printf ("UPDATE %s SET %s = ##blob::GdaBlob WHERE %s", rtable, rblob_col, row_cond);
	g_free (rtable);
	g_free (rblob_col);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (console->current->parser, sql, &remain, error);
	g_free (sql);
	if (!stmt)
		return NULL;
	if (remain) {
		g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Wrong row condition"));
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

	res = g_new0 (ToolCommandResult, 1);
	res->type = TOOL_COMMAND_RESULT_EMPTY;
	return res;
}
#endif


static char **
completion_func (G_GNUC_UNUSED const char *text, const gchar *line, int start, int end)
{
	ConnectionSetting *cs = main_data->term_console->current;
	char **array;
	gchar **gda_compl;
	gint i, nb_compl;

	if (!cs)
		return NULL;
	gda_compl = gda_completion_list_get (cs->cnc, line, start, end);
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
}


const GSList *
gda_sql_get_all_connections (void)
{
	return main_data->settings;
}

const ConnectionSetting *
gda_sql_get_connection (const gchar *name)
{
	return find_connection_from_name (name);
}

const ConnectionSetting *
gda_sql_get_current_connection (void)
{
	return main_data->term_console->current;
}

gchar *
gda_sql_console_execute (SqlConsole *console, const gchar *command, GError **error, ToolOutputFormat format)
{
	gchar *loc_cmde = NULL;
	gchar *retstr = NULL;

	g_assert (console);
	loc_cmde = g_strdup (command);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		if (command_is_complete (loc_cmde)) {
			/* execute command */
			ToolCommandResult *res;
			
			res = command_execute (console, loc_cmde,
					       GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
			
			if (res) {
				ToolOutputFormat of = console->output_format;
				if (res->type == TOOL_COMMAND_RESULT_DATA_MODEL)
					console->output_format = TOOL_OUTPUT_FORMAT_HTML;

				retstr = tool_output_result_to_string (res, format, console->output_stream,
								       main_data->options);
				console->output_format = of;
				tool_command_result_free (res);
			}
		}
		else {
			g_set_error (error, GDA_TOOLS_ERROR, GDA_TOOLS_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Command is incomplete"));
		}
	}
	g_free (loc_cmde);

	return retstr;
}

SqlConsole *
gda_sql_console_new (const gchar *id)
{
	SqlConsole *c;

	c = g_new0 (SqlConsole, 1);
	c->output_format = TOOL_OUTPUT_FORMAT_DEFAULT;
	if (id)
		c->id = g_strdup (id);
	if (main_data->term_console)
		c->current = main_data->term_console->current;
	c->command_group = main_data->limit_commands; /* limit to a subset of all commands */
	c->output_stream = NULL;
	return c;
}

void
gda_sql_console_free (SqlConsole *console)
{
	g_free (console->id);
	g_free (console);
}

gchar *
gda_sql_console_compute_prompt (SqlConsole *console, ToolOutputFormat format)
{
	GString *string;

	string = g_string_new ("");
	compute_prompt (console, string, FALSE, TRUE, format);

	return g_string_free (string, FALSE);
}
