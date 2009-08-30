/* 
 * Copyright (C) 2009 The GNOME Foundation.
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
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

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

/* Perspectives' factories */
#include "schema-browser/perspective-main.h"
#include "dummy-perspective/perspective-main.h"


extern BrowserCoreInitFactories browser_core_init_factories;

GSList *
main_browser_core_init_factories (void)
{
	GSList *factories = NULL;
	factories = g_slist_append (factories, schema_browser_perspective_get_factory ());
	factories = g_slist_append (factories, dummy_perspective_get_factory ());
	return factories;
}

/* options */
gchar *perspective = NULL;

static GOptionEntry entries[] = {
        { "perspective", 'p', 0, G_OPTION_ARG_STRING, &perspective, "Perspective", "default perspective "
	  "to use when opening windows"},
	{ NULL }
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

	gdaui_init ();
	gtk_init (&argc, &argv);
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
		}
	}
	
	if (have_loop)
		/* application loop */
		gtk_main ();
	else if (browser_core_exists ())
		g_object_unref (browser_core_get ());

	return 0;
}

