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
#include <string.h>
#include <libgdaui/libgdaui.h>
#include "support.h"
#include "browser-core.h"
#include "browser-connection.h"
#include "browser-window.h"
#include "login-dialog.h"
#include "auth-dialog.h"

int
main (int argc, char *argv[])
{
	gboolean have_loop = FALSE;

	gdaui_init ();
	gtk_init (&argc, &argv);

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

