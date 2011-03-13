/*
 * Copyright (C) 2000 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "dsn-properties-dialog.h"
#include "gdaui-dsn-editor.h"
#include "gdaui-login-dialog.h"
#include <string.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <gdk/gdk.h>

enum {
	REVERT_BUTTON,
	TEST_BUTTON,
	BROWSE_BUTTON
};

static gboolean
data_source_info_equal (const GdaDsnInfo *info1, const GdaDsnInfo *info2)
{
	#define str_is_equal(x,y) (((x) && (y) && !strcmp ((x),(y))) || (!(x) && (!y)))
	 if (!info1 && !info2)
                return TRUE;
        if (!info1 || !info2)
                return FALSE;

        if (! str_is_equal (info1->name, info2->name))
                return FALSE;
        if (! str_is_equal (info1->provider, info2->provider))
                return FALSE;
        if (! str_is_equal (info1->cnc_string, info2->cnc_string))
                return FALSE;
        if (! str_is_equal (info1->description, info2->description))
                return FALSE;
        if (! str_is_equal (info1->auth_string, info2->auth_string))
                return FALSE;
        return info1->is_system == info2->is_system;
}

static void
dsn_changed_cb (GdauiDsnEditor *config, gpointer user_data)
{
	gboolean changed;
	const GdaDsnInfo *dsn_info , *old_dsn_info;

	dsn_info = gdaui_dsn_editor_get_dsn (config);
	old_dsn_info = g_object_get_data (G_OBJECT (user_data), "old_dsn_info");

	changed = !data_source_info_equal (dsn_info, old_dsn_info);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data), REVERT_BUTTON, changed);

	gda_config_define_dsn (dsn_info, NULL);
}

static void 
data_source_info_free (GdaDsnInfo *info)
{
	g_free (info->provider); 
	g_free (info->cnc_string); 
	g_free (info->description);
	g_free (info->auth_string);
	g_free (info);
}

static void
delete_event_cb (GtkDialog *dlg, G_GNUC_UNUSED gpointer data)
{
	gtk_dialog_response (dlg, GTK_RESPONSE_OK);
}

void
dsn_properties_dialog (GtkWindow *parent, const gchar *dsn)
{
	GdaDsnInfo *dsn_info, *priv_dsn_info;
	GtkWidget *dialog, *props, *label, *hbox;
	GdkPixbuf *icon;
	gchar *str;
	gint result;
	GdaProviderInfo *pinfo;

	dsn_info = gda_config_get_dsn_info (dsn);
	if (!dsn_info)
		return;
	pinfo = gda_config_get_provider_info (dsn_info->provider);

	/* create the dialog */
	dialog = gtk_dialog_new_with_buttons (_("Data Source Properties"),
					      parent, GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("Test"), TEST_BUTTON,
					      _("Browse"), BROWSE_BUTTON,
					      GTK_STOCK_REVERT_TO_SAVED, REVERT_BUTTON,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 450, 300);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), TEST_BUTTON, pinfo ? TRUE : FALSE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), BROWSE_BUTTON, pinfo ? TRUE : FALSE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), REVERT_BUTTON, FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	priv_dsn_info = g_new0 (GdaDsnInfo, 1);
	priv_dsn_info->name = g_strdup (dsn_info->name);
	if (dsn_info->provider)
		priv_dsn_info->provider = g_strdup (dsn_info->provider);
	if (dsn_info->cnc_string)
		priv_dsn_info->cnc_string = g_strdup (dsn_info->cnc_string);
	if (dsn_info->description)
		priv_dsn_info->description = g_strdup (dsn_info->description);
	if (dsn_info->auth_string)
		priv_dsn_info->auth_string = g_strdup (dsn_info->auth_string);
	priv_dsn_info->is_system = dsn_info->is_system;
	
	g_object_set_data_full (G_OBJECT (dialog), "old_dsn_info", priv_dsn_info,
				(GDestroyNotify) data_source_info_free);

	gchar *path;
	path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-control-center.png", NULL);
	icon = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);
	if (icon) {
		gtk_window_set_icon (GTK_WINDOW (dialog), icon);
		g_object_unref (icon);
	}

	/* textual explanation */
	if (!dsn_info->is_system || gda_config_can_modify_system_config ())
		str = g_strdup_printf ("<b>%s:</b>\n<small>%s</small>",
				       _("Data Source Properties"),
				       _("Change the data source properties (the name can't be modified)."));
	else
		str = g_strdup_printf ("<b>%s:</b>\n<small>%s</small>",
				       _("Data Source Properties"),
				       _("For information only, this data source is a system wide data source\n"
					 "and you don't have the permission change it."));

	label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
        g_free (str);

	GtkWidget *dcontents;
	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (dcontents), 5);
	gtk_box_pack_start (GTK_BOX (dcontents), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (dcontents), hbox, FALSE, FALSE, 10);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* create data source settings page */
	props = gdaui_dsn_editor_new ();
	gdaui_dsn_editor_set_dsn (GDAUI_DSN_EDITOR (props), dsn_info);
	gtk_widget_show (props);
	g_signal_connect (G_OBJECT (props), "changed", G_CALLBACK (dsn_changed_cb), dialog);	
	gtk_box_pack_start (GTK_BOX (hbox), props, TRUE, TRUE, 0);

	/* handle "delete-event" */
	g_signal_connect (dialog, "delete-event",
			  G_CALLBACK (delete_event_cb), NULL);

	/* run the dialog */
	do {
		result = gtk_dialog_run (GTK_DIALOG (dialog));
		switch (result) {
		case REVERT_BUTTON:
			/* reverting changes... */
			gdaui_dsn_editor_set_dsn (GDAUI_DSN_EDITOR (props), priv_dsn_info);
			gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), REVERT_BUTTON, FALSE);
			break;
		case TEST_BUTTON:
		{
			GtkWidget *test_dialog = NULL;
			GdauiLogin* login = NULL;
			gchar *title;
			GdaConnection *cnc = NULL;
			gboolean auth_needed = gda_config_dsn_needs_authentication (dsn);

			if (auth_needed) {
				title = g_strdup_printf (_("Login for %s"), dsn);
				test_dialog = gdaui_login_dialog_new (title, GTK_WINDOW (dialog));
				g_free (title);
				login = gdaui_login_dialog_get_login_widget (GDAUI_LOGIN_DIALOG (test_dialog));
				g_object_set (G_OBJECT (login), "dsn", dsn, NULL);
			}

			if (!auth_needed || gdaui_login_dialog_run (GDAUI_LOGIN_DIALOG (test_dialog))) {
				GtkWidget *msgdialog;
				GError *error = NULL;
				const GdaDsnInfo *cinfo = NULL;

				if (login)
					cinfo = gdaui_login_get_connection_information (login);
				
				cnc = gda_connection_open_from_dsn (dsn,
								    cinfo ? cinfo->auth_string : NULL,
								    GDA_CONNECTION_OPTIONS_NONE, &error);
				if (cnc) {
					msgdialog = gtk_message_dialog_new_with_markup (test_dialog ? GTK_WINDOW (test_dialog) : parent, 
											GTK_DIALOG_MODAL,
											GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
											"<b>%s</b>", _("Connection successfully opened!"));
					gda_connection_close (cnc);
				}
				else {
					msgdialog = gtk_message_dialog_new_with_markup (test_dialog ? GTK_WINDOW (test_dialog) : parent, 
											GTK_DIALOG_MODAL,
											GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
											"<b>%s:</b>\n%s", _("Could not open connection"), 
											error->message ? error->message : _("Unknown error"));
					if (error)
						g_error_free (error);
				}
				
				gtk_dialog_run (GTK_DIALOG (msgdialog));
				gtk_widget_destroy (msgdialog);
			}
			if (test_dialog)
				gtk_widget_destroy (test_dialog);
		}
		break;
		case BROWSE_BUTTON:
		{
			GAppInfo *appinfo;
			GdkAppLaunchContext *context;
			GdkScreen *screen;
			gboolean sresult;
			GError *lerror = NULL;
			gchar *cmd;
#ifdef G_OS_WIN32
#define EXENAME "gda-browser-" GDA_ABI_VERSION ".exe"
#else
#define EXENAME "gda-browser-" GDA_ABI_VERSION
#endif
			/* run the gda-browser tool */
			cmd = gda_gbr_get_file_path (GDA_BIN_DIR, (char *) EXENAME, NULL);
			appinfo = g_app_info_create_from_commandline (cmd,
								      "Gda browser",
								      G_APP_INFO_CREATE_NONE,
								      NULL);
			g_free (cmd);
			
			screen = gtk_widget_get_screen (GTK_WIDGET (parent));
			context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
			gdk_app_launch_context_set_screen (context, screen);
			sresult = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), NULL);
			if (! sresult) {
				g_object_unref (appinfo);
				appinfo = g_app_info_create_from_commandline (EXENAME,
									      "Gda Control center",
									      G_APP_INFO_CREATE_NONE,
									      NULL);
				sresult = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), &lerror);
			}
			g_object_unref (context);
			g_object_unref (appinfo);

			if (!sresult) {
				GtkWidget *msgdialog;
				msgdialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog), GTK_DIALOG_MODAL,
										GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
										"<b>%s:</b>\n%s",
										_("Could not execute browser program"),
										lerror && lerror->message ? lerror->message : _("No detail"));
				if (lerror)
					g_error_free (lerror);
				gtk_dialog_run (GTK_DIALOG (msgdialog));
				gtk_widget_destroy (msgdialog);
			}
			else
				result = GTK_RESPONSE_OK; /* force closing of this dialog */
		}
		break;
		default:
			break;
		}
	}
	while (result != GTK_RESPONSE_OK);
	gtk_widget_destroy (dialog);
}
