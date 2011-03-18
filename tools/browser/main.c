/* 
 * Copyright (C) 2009 - 2011 The GNOME Foundation.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <libgda-ui/libgda-ui.h>
#include "support.h"
#include "browser-core.h"
#include "browser-connection.h"
#include "browser-window.h"
#include "login-dialog.h"
#include "auth-dialog.h"
#include "browser-stock-icons.h"
#include "../config-info.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/* Perspectives' factories */
#include "schema-browser/perspective-main.h"
#include "query-exec/perspective-main.h"
#include "data-manager/perspective-main.h"
/* #include "dummy-perspective/perspective-main.h" */

extern BrowserCoreInitFactories browser_core_init_factories;

GSList *
main_browser_core_init_factories (void)
{
	GSList *factories = NULL;
	factories = g_slist_append (factories, schema_browser_perspective_get_factory ());
	factories = g_slist_append (factories, query_exec_perspective_get_factory ());
	factories = g_slist_append (factories, data_manager_perspective_get_factory ());
	/* factories = g_slist_append (factories, dummy_perspective_get_factory ()); */
	return factories;
}

static void
output_data_model (GdaDataModel *model)
{
	if (! getenv ("GDA_DATA_MODEL_DUMP_TITLE"))
		g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "Yes", TRUE);
	if (! getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY"))
		g_setenv ("GDA_DATA_MODEL_NULL_AS_EMPTY", "Yes", TRUE);
	if (isatty (fileno (stdout))) {
		if (! getenv ("GDA_DATA_MODEL_DUMP_TRUNCATE"))
			g_setenv ("GDA_DATA_MODEL_DUMP_TRUNCATE", "-1", TRUE);
	}
	gda_data_model_dump (model, NULL);	
}


/* options */
gchar *perspective = NULL;
gboolean list_configs = FALSE;
gboolean list_providers = FALSE;
gboolean list_data_files = FALSE;
gchar *purge_data_files = NULL;

static GOptionEntry entries[] = {
        { "perspective", 'p', 0, G_OPTION_ARG_STRING, &perspective, "Perspective", "default perspective "
	  "to use when opening windows"},
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
        { "data-files-list", 0, 0, G_OPTION_ARG_NONE, &list_data_files, "List files used to hold information related to each connection", NULL },
        { "data-files-purge", 0, 0, G_OPTION_ARG_STRING, &purge_data_files, "Remove some files used to hold information related to each connection", "criteria"},
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

int
main (int argc, char *argv[])
{
	GOptionContext *context;
        GError *error = NULL;
	gboolean have_loop = FALSE;

	/* set factories function */
	browser_core_init_factories = main_browser_core_init_factories;

	context = g_option_context_new (_("[DSN|connection string]..."));
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        g_option_context_set_ignore_unknown_options (context, TRUE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_fprintf  (stderr, "Can't parse arguments: %s\n", error->message);
                return EXIT_FAILURE;
        }
        g_option_context_free (context);

	/* treat here lists of providers and defined DSN */
	if (list_providers) {
		gda_init ();
		setlocale (LC_ALL, "");
		GdaDataModel *model;
		if (argc == 2)
			model = config_info_detail_provider (argv[1], &error);
		else
			model = config_info_list_all_providers ();

		if (model) {
			output_data_model (model);
			g_object_unref (model);
		}
		else {
			g_print (_("Error: %s\n"), 
				 error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
		return 0;
	}
	if (list_configs) {
		gda_init ();
		setlocale (LC_ALL, "");
		GdaDataModel *model = config_info_list_all_dsn ();
		output_data_model (model);
		g_object_unref (model);
		return 0;
	}
	if (list_data_files) {
		gchar *confdir;
		GdaDataModel *model;

		gda_init ();
		setlocale (LC_ALL, "");
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
		return 0;
	}
	if (purge_data_files) {
		gchar *tmp;

		gda_init ();
		setlocale (LC_ALL, "");
		tmp = config_info_purge_data_files (purge_data_files, &error);
		if (tmp) {
			g_print ("%s\n", tmp);
			g_free (tmp);
		}
		if (error)
			g_print (_("Error while purging files used to store information about each connection: %s\n"),
				 error->message);	
		return 0;
	}

	gtk_init (&argc, &argv);
	gdaui_init ();

#ifdef HAVE_MAC_INTEGRATION
	theApp = g_object_new (GTK_TYPE_OSX_APPLICATION, NULL);
#endif
	browser_stock_icons_init ();

	browser_core_set_default_factory (perspective);

	if (argc == 1) {
		GError *error = NULL;
		if (browser_connection_open (&error))
			have_loop = TRUE;
		else
			g_print ("Connection NOT opened: %s\n",
				 error && error->message ? error->message : _("No detail"));
	}
	else {
		AuthDialog *dialog;
		GError *error = NULL;
		gint i;

		dialog = auth_dialog_new (NULL);
		
		for (i = 1; i < argc; i++) {
			if (! auth_dialog_add_cnc_string (dialog, argv[i], &error)) {
				gtk_widget_destroy (GTK_WIDGET (dialog));
				dialog = NULL;
				g_print ("Connection NOT opened: %s\n",
					 error && error->message ? error->message : _("No detail"));
				break;
			}
		}
		
		if (dialog) {
			have_loop = auth_dialog_run (dialog);
			if (have_loop) {
				/* create a window for each opened connection */
				const GSList *cnclist, *list;

				cnclist = auth_dialog_get_connections (dialog);
				for (list = cnclist; list; list = list->next) {
					AuthDialogConnection *ad = (AuthDialogConnection*) list->data;
					if (ad->cnc) {
						BrowserConnection *bcnc;
						BrowserWindow *bwin;
						bcnc = browser_connection_new (ad->cnc);
						bwin = browser_window_new (bcnc, NULL);
						
						browser_core_take_window (bwin);
						browser_core_take_connection (bcnc);
					}
					else {
						g_print ("Connection NOT opened: %s\n",
							 ad->cnc_open_error && ad->cnc_open_error->message ? 
							 ad->cnc_open_error->message : _("No detail"));
						have_loop = FALSE;
						break;
					}
				}
			}
			gtk_widget_destroy (dialog);
		}
	}
	
	/*g_print ("Main (GTK+) THREAD is %p\n", g_thread_self ());*/
	if (have_loop)
		/* application loop */
		gtk_main ();
	else if (browser_core_exists ())
		g_object_unref (browser_core_get ());

	return 0;
}

