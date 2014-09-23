/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <virtual/libgda-virtual.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include "base/base-tool-input.h"
#include "base/base-tool-output.h"
#include "common/t-app.h"
#include "common/t-utils.h"
#include "common/t-config-info.h"
#include "common/t-term-context.h"
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <sys/types.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-blob-op.h>
#include <thread-wrapper/gda-connect.h>
#include <sql-parser/gda-sql-parser.h>
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

#ifdef IS_BROWSER
  #include <libgda-ui/libgda-ui.h>
  #include <browser/browser-window.h>
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
#ifdef IS_BROWSER
  gchar *perspective = NULL;
#endif

#ifdef HAVE_LIBSOUP
gint http_port = -1;
gchar *auth_token = NULL;
#endif

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Show version information and exit", NULL },	
        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { "command", 'C', 0, G_OPTION_ARG_STRING, &single_command, "Run only single command (SQL or internal) and exit", "command" },
        { "commands-file", 'f', 0, G_OPTION_ARG_STRING, &commandsfile, "Execute commands from file, then exit (except if -i specified)", "filename" },
	{ "interactive", 'i', 0, G_OPTION_ARG_NONE, &interactive, "Keep the console opened", NULL },
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
#ifdef HAVE_LIBSOUP
	{ "http-port", 's', 0, G_OPTION_ARG_INT, &http_port, "Run embedded HTTP server on specified port", "port" },
	{ "http-token", 't', 0, G_OPTION_ARG_STRING, &auth_token, "Authentication token (required to authenticate clients)", "token phrase" },
#endif
        { "data-files-list", 0, 0, G_OPTION_ARG_NONE, &list_data_files, "List files used to hold information related to each connection", NULL },
        { "data-files-purge", 0, 0, G_OPTION_ARG_STRING, &purge_data_files, "Remove some files used to hold information related to each connection. Criteria has to be in 'all', 'non-dsn', or 'non-exist-dsn'", "criteria"},
#ifdef IS_BROWSER
	{ "perspective", 'p', 0, G_OPTION_ARG_STRING, &perspective, "Perspective", "default perspective "
			"to use when opening windows"},
#endif
        { NULL, 0, 0, 0, NULL, NULL, NULL }
};


static void output_data_model (GdaDataModel *model);

static gboolean
ticker (G_GNUC_UNUSED gpointer data)
{
	static int pos=0;
	char cursor[4]={'/','-','\\','|'};
	printf("%c\b", cursor[pos]);
	fflush(stdout);
	pos = (pos+1) % 4;
	return G_SOURCE_CONTINUE;
}

static void
cnc_status_changed_cb (TConnection *tcnc, GdaConnectionStatus status, gpointer data)
{
	TContext *term_console;
	term_console = t_app_get_term_console ();
	if (!term_console || (t_context_get_connection (term_console) != tcnc))
		return;

#ifdef DEBUG_NO
	gchar *status_str[] = {
                "CLOSED",
                "OPENING",
                "IDLE",
                "BUSY"
        };
	g_print ("Status is now %s\n", status_str [status]);
#endif
	static guint source_id = 0;
	static GMutex mutex;
	g_mutex_lock (&mutex);
	if (status == GDA_CONNECTION_STATUS_BUSY) {
		if (source_id == 0)
			source_id = g_timeout_add (100, (GSourceFunc) ticker, NULL);
	}
	else if (source_id > 0) {
		g_source_remove (source_id);
		source_id = 0;
	}
	 g_mutex_unlock (&mutex);
}

static void
free_strings_array (GArray *array)
{
	guint i;
	for (i = 0; i < array->len; i++) {
		gchar *str;
		str = g_array_index (array, gchar*, i);
		g_free (str);
	}
	g_array_free (array, TRUE);
}

static void
cnc_added_cb (TApp *app, TConnection *tcnc)
{
	gda_signal_connect (tcnc, "status-changed",
			    G_CALLBACK (cnc_status_changed_cb), NULL,
			    NULL, 0, NULL);

}

GThread *term_console_thread = NULL;
static int
command_line (GApplication *application, GApplicationCommandLine *cmdline)
{
	gchar **argv;
	int argc;
	argv = g_application_command_line_get_arguments (cmdline, &argc);

	GOptionContext *context;
	GError *error = NULL;
	int exit_status = EXIT_SUCCESS;
	gboolean is_interactive = TRUE; /* final interactivity status */
	gboolean nocnc = FALSE;
#ifdef IS_BROWSER
	gboolean ui = TRUE;
	is_interactive = FALSE;
#endif
	t_app_add_feature (T_APP_TERM_CONSOLE);

	/* options parsing */
	context = g_option_context_new (_("[DSN|connection string]..."));        
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_ignore_unknown_options (context, TRUE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_fprintf  (stderr, "Can't parse arguments: %s\n", error->message);
		exit_status = EXIT_FAILURE;
		t_app_request_quit ();
		goto out;
        }
        g_option_context_free (context);

	if (show_version) {
		g_application_command_line_print (cmdline, _("GDA SQL console version " PACKAGE_VERSION "\n"));
		t_app_request_quit ();
		goto out;
	}

	/* compute final interactivity status */
	if (interactive)
		is_interactive = TRUE;
	if (list_providers ||
	    list_configs ||
	    list_data_files ||
	    purge_data_files) {
		is_interactive = FALSE;
#ifdef IS_BROWSER
		ui = FALSE;
#endif
		nocnc = TRUE;
	}

	TContext *term_console;
	FILE *ostream = NULL;
	term_console = t_app_get_term_console ();
	g_assert (term_console);
	t_term_context_set_interactive (T_TERM_CONTEXT (term_console), is_interactive);

	/* output file */
	if (outfile) {
		if (! t_context_set_output_file (term_console, outfile, &error)) {
			g_print ("Can't set output file as '%s': %s\n", outfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			t_app_request_quit ();
			goto out;
		}
	}
	ostream = t_context_get_output_stream (term_console, NULL);

	/* welcome message */
	if (is_interactive && !ostream) {
#ifdef G_OS_WIN32
		HANDLE wHnd;
		SMALL_RECT windowSize = {0, 0, 139, 49};

		wHnd = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTitle ("GDA SQL console, version " PACKAGE_VERSION);
		SetConsoleWindowInfo (wHnd, TRUE, &windowSize);
#endif
		gchar *c1, *c2, *c3, *c4;
		ToolOutputFormat oformat = BASE_TOOL_OUTPUT_FORMAT_DEFAULT;
		oformat = t_context_get_output_format (term_console);

#ifdef IS_BROWSER
  #define PRGNAME "Gda Browser"
#else
  #define PRGNAME "Gda SQL"
#endif
		base_tool_output_color_print (BASE_TOOL_COLOR_BOLD, oformat,
					      _("Welcome to the " PRGNAME " console, version " PACKAGE_VERSION));
		g_print ("\n\n");
		c1 = base_tool_output_color_string (BASE_TOOL_COLOR_BOLD, oformat, ".copyright");
		c2 = base_tool_output_color_string (BASE_TOOL_COLOR_BOLD, oformat, ".?");
		c3 = base_tool_output_color_string (BASE_TOOL_COLOR_BOLD, oformat, ".help");
		c4 = base_tool_output_color_string (BASE_TOOL_COLOR_BOLD, oformat, ".q");
		g_print (_("Type: %s to show usage and distribution terms\n"
			   "      %s or %s for help with internal commands\n"
			   "      %s (or CTRL-D) to quit (the '.' can be replaced by a '\\')\n"
			   "      or any SQL query terminated by a semicolon\n\n"), c1, c2, c3, c4);
		g_free (c1);
		g_free (c2);
		g_free (c3);
		g_free (c4);
	}

	/* Handle command line options to display some information */
	if (list_providers) {
		if (argc == 2) {
			single_command = g_strdup_printf (".L %s", argv[1]);
			argc = 1;
		}
		else {
			GdaDataModel *model = t_config_info_list_all_providers ();
			output_data_model (model);
			g_object_unref (model);
		}
	}
	if (list_configs) {
		if (argc == 2) {
			single_command = g_strdup_printf (".l %s", argv[1]);
			argc = 1;
		}
		else {
			GdaDataModel *model = t_config_info_list_all_dsn ();
			output_data_model (model);
			g_object_unref (model);
		}
	}
	if (list_data_files) {
		gchar *confdir;
		GdaDataModel *model;

		confdir = t_config_info_compute_dict_directory ();
		g_print (_("All files are in the directory: %s\n"), confdir);
		g_free (confdir);
		model = t_config_info_list_data_files (&error);
		if (model) {
			output_data_model (model);
			g_object_unref (model);
		}
		else
			g_print (_("Can't get the list of files used to store information about each connection: %s\n"),
				 error->message);	
	}

	if (purge_data_files) {
		gchar *tmp;
		tmp = t_config_info_purge_data_files (purge_data_files, &error);
		if (tmp) {
			base_tool_output_output_string (ostream, tmp);
			g_free (tmp);
		}
		if (error)
			g_print (_("Error while purging files used to store information about each connection: %s\n"),
				 error->message);	
	}

	/* commands file */
	if (commandsfile) {
		if (! t_term_context_set_input_file (T_TERM_CONTEXT (term_console), commandsfile, &error)) {
			g_print ("Can't read file '%s': %s\n", commandsfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			t_app_request_quit ();
			goto out;
		}
	}
	else {
		/* check if stdin is a term */
		if (!isatty (fileno (stdin)))
			t_term_context_set_input_stream (T_TERM_CONTEXT (term_console), stdin);
	}

	/* recreating an argv[] array, one entry per connection to open, and we don't want to include there
	 * any SHEBANG command file */
	GArray *array;
	array = g_array_new (FALSE, FALSE, sizeof (gchar *));
#define SHEBANG "#!"
#define SHEBANGLEN 2
	gint i = 0;
	gboolean has_sh_arg = FALSE;
	for (i = 1; i < argc; i++) { /* we don't want argv[0] which is the program name */
		/* Check if argv[i] corresponds to a file starting with SHEBANG */
		FILE *file;
		gboolean arg_is_cncname = TRUE;
		file = fopen (argv[i], "r");
		if (file) {
			char buffer [SHEBANGLEN + 1];
			size_t nread;
			nread = fread (buffer, 1, SHEBANGLEN, file);
			if (nread == SHEBANGLEN) {
				buffer [SHEBANGLEN] = 0;
				if (!strcmp (buffer, SHEBANG)) {
					if (has_sh_arg) {
						g_print (_("More than one file to execute\n"));
						exit_status = EXIT_FAILURE;
						fclose (file);
						free_strings_array (array);
						t_app_request_quit ();
						goto out;
					}

					if (! t_term_context_set_input_file (T_TERM_CONTEXT (term_console),
									     argv[i], &error)) {
						g_print (_("Can't read file '%s': %s\n"), commandsfile,
							 error->message);
						exit_status = EXIT_FAILURE;
						fclose (file);
						free_strings_array (array);
						t_app_request_quit ();
						goto out;
					}
					fclose (file);
					has_sh_arg = TRUE;
					arg_is_cncname = FALSE;
				}
			}
			fclose (file);
		}

		if (arg_is_cncname) {
			gchar *tmp;
			tmp = g_strdup (argv[i]);
			g_array_append_val (array, tmp);
			t_config_info_modify_argv (argv[i]);
		}
	}

	if (getenv ("GDA_SQL_CNC")) {
		const gchar *cncname;
		cncname = getenv ("GDA_SQL_CNC");
		g_array_append_val (array, cncname);
	}

	gda_signal_connect (application, "connection-added",
			    G_CALLBACK (cnc_added_cb), NULL,
			    NULL, 0, NULL);

	/* open connections if specified */
	if (! nocnc) {
		if (! t_app_open_connections (array->len, (const gchar **) array->data, &error)) {
			g_print (_("Error opening connection: %s\n"), error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
			exit_status = EXIT_FAILURE;
			free_strings_array (array);
			t_app_request_quit ();
			goto out;
		}
	}

	GSList *cnclist;
	cnclist = (GSList*) t_app_get_all_connections ();
	if (cnclist) {
		cnclist = g_slist_last (cnclist);
		t_context_set_connection (term_console, T_CONNECTION (cnclist->data));
	}

	free_strings_array (array);
	array = NULL;

#ifdef HAVE_LIBSOUP
	/* start HTTP server if requested */
	/* FIXME: create a new TContext for that web server in another thread */
	if (http_port > 0) {
		GError *lerror = NULL;
		if (! t_app_start_http_server (http_port, auth_token, &lerror)) {
			g_print (_("Can't run HTTP server on port %d: %s\n"),
				 http_port, lerror && lerror->message ? lerror->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			t_app_request_quit ();
			goto out;
		}
	}
#endif

	/* process commands which need to be executed as specified by the command line args */
	if (single_command) {
		is_interactive = FALSE;
		t_term_context_treat_single_line (T_TERM_CONTEXT (term_console), single_command);
		if (!ostream)
			g_print ("\n");
	}

	if (is_interactive)
		term_console_thread = t_context_run (term_console);
	else {
#ifdef IS_BROWSER
		if (!ui)
			t_app_request_quit ();
#else
		t_app_request_quit ();
#endif
	}

#ifdef IS_BROWSER
	if (ui) {
		GtkBuilder *builder;
		builder = gtk_builder_new ();
		gtk_builder_add_from_string (builder,
					     "<interface>"
					     "  <menu id='app-menu'>"
					     "    <section>"
					     "      <item>"
					     "        <attribute name='label' translatable='yes'>_New Window</attribute>"
					     "        <attribute name='action'>app.new</attribute>"
					     "        <attribute name='accel'>&lt;Primary&gt;n</attribute>"
					     "      </item>"
					     "    </section>"
					     "    <section>"
					     "      <item>"
					     "        <attribute name='label' translatable='yes'>_About Bloatpad</attribute>"
					     "        <attribute name='action'>app.about</attribute>"
					     "      </item>"
					     "    </section>"
					     "    <section>"
					     "      <item>"
					     "        <attribute name='label' translatable='yes'>_Quit</attribute>"
					     "        <attribute name='action'>app.quit</attribute>"
					     "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
					     "      </item>"
					     "    </section>"
					     "  </menu>"
					     "  <menu id='menubar'>"
					     "    <submenu label='_Edit'>"
					     "      <item label='_Copy' action='win.copy'/>"
					     "      <item label='_Paste' action='win.paste'/>"
					     "    </submenu>"
					     "  </menu>"
					     "</interface>", -1, NULL);
		gtk_application_set_app_menu (GTK_APPLICATION (t_app_get ()),
					      G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
		gtk_application_set_menubar (GTK_APPLICATION (t_app_get ()),
					     G_MENU_MODEL (gtk_builder_get_object (builder, "menubar")));
		g_object_unref (builder);

		t_app_add_feature (T_APP_BROWSER);
		if (!is_interactive)
			t_app_remove_feature (T_APP_TERM_CONSOLE);
	}
#endif

 out:
	g_strfreev (argv);
	return exit_status;
}

int
main (int argc, char *argv[])
{
	g_setenv ("GDA_CONFIG_SYNCHRONOUS", "1", TRUE);
	setlocale (LC_ALL, "");
        gda_init ();

	GMainContext *mcontext;
	mcontext = g_main_context_ref_thread_default ();
	g_main_context_acquire (mcontext); /* the current thread owns the default GMainContext */
	gda_connection_set_main_context (NULL, NULL, mcontext);
	g_main_context_unref (mcontext);

#ifdef IS_BROWSER
	gtk_init (&argc, &argv);
        gdaui_init ();

  #ifdef HAVE_MAC_INTEGRATION
        theApp = g_object_new (GTK_TYPE_OSX_APPLICATION, NULL);
  #endif
#endif

	 g_set_application_name ("Bloatpad");

	/* TApp initialization */
	GApplication *app;
	t_app_setup (T_APP_NO_FEATURE);
	app = G_APPLICATION (t_app_get ());

	g_application_set_inactivity_timeout (app, 0);
	g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);

	g_application_run (app, argc, argv);

	/* cleanups */
	if (term_console_thread)
		g_thread_join (term_console_thread);
	t_app_cleanup ();

	return EXIT_SUCCESS;
}

/* 
 * Dumps the data model contents onto @data->output
 */
static void
output_data_model (GdaDataModel *model)
{
	gchar *str;
	FILE *ostream = NULL;
	ToolOutputFormat oformat = BASE_TOOL_OUTPUT_FORMAT_DEFAULT;
	TContext *console;

	console = t_app_get_term_console ();
	if (console) {
		ostream = t_context_get_output_stream (console, NULL);
		oformat = t_context_get_output_format (console);
	}
	str = base_tool_output_data_model_to_string (model, oformat, ostream, t_app_get_options ());
	base_tool_output_output_string (ostream, str);
	g_free (str);
	t_app_store_data_model (model, T_LAST_DATA_MODEL_NAME);
}
