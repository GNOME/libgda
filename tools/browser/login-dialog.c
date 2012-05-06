/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "login-dialog.h"
#include "browser-spinner.h"
#include "support.h"
#include <libgda/thread-wrapper/gda-thread-wrapper.h>

/* 
 * Main static functions 
 */
static void login_dialog_class_init (LoginDialogClass * class);
static void login_dialog_init (LoginDialog *dialog);
static void login_dialog_dispose (GObject *object);

static GdaConnection *real_open_connection (GdaThreadWrapper *wrapper, const GdaDsnInfo *cncinfo, GError **error);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _LoginDialogPrivate
{
	GtkWidget *login;
	GtkWidget *spinner;
	GdaThreadWrapper *wrapper;
};

/* module error */
GQuark login_dialog_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("login_dialog_error");
        return quark;
}

GType
login_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (LoginDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) login_dialog_class_init,
			NULL,
			NULL,
			sizeof (LoginDialog),
			0,
			(GInstanceInitFunc) login_dialog_init,
			0
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_DIALOG, "LoginDialog", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}


static void
login_dialog_class_init (LoginDialogClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* virtual functions */
	object_class->dispose = login_dialog_dispose;
}

static void
login_contents_changed_cb (G_GNUC_UNUSED GdauiLogin *login, gboolean is_valid, LoginDialog *dialog)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, is_valid);
	if (is_valid)
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

static void
login_dialog_init (LoginDialog *dialog)
{
	GtkWidget *label, *hbox, *wid;
	char *markup, *str;
	GtkWidget *dcontents;

	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	dialog->priv = g_new0 (LoginDialogPrivate, 1);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CONNECT,
				GTK_RESPONSE_ACCEPT,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_REJECT, NULL);

	gtk_box_set_spacing (GTK_BOX (dcontents), 5);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);

	str = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-browser-non-connected.png", NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (dialog), str, NULL);
	g_free (str);

	/* label and spinner */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); 
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
	gtk_box_pack_start (GTK_BOX (dcontents), hbox, FALSE, FALSE, 0);
	
	str = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-browser-non-connected-big.png", NULL);
	wid = gtk_image_new_from_file (str);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	markup = g_markup_printf_escaped ("<big><b>%s\n</b></big>%s\n",
					  _("Connection opening:"),
					  _("Select a named data source, or specify\nparameters "
					    "to open a connection to a \nnon defined data source"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 12);
	gtk_widget_show_all (hbox);
	
	dialog->priv->spinner = browser_spinner_new ();
	gtk_container_add (GTK_CONTAINER (hbox), dialog->priv->spinner);

	/* login (not shown) */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
	gtk_box_pack_start (GTK_BOX (dcontents), hbox, FALSE, FALSE, 10);
	label = gtk_label_new ("    ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	
	dialog->priv->login = gdaui_login_new (NULL);
	gdaui_login_set_mode (GDAUI_LOGIN (dialog->priv->login),
			      GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE);
	g_signal_connect (dialog->priv->login, "changed",
			  G_CALLBACK (login_contents_changed_cb), dialog);
	gtk_container_add (GTK_CONTAINER (hbox), dialog->priv->login);
	gtk_widget_show_all (hbox);
}

static void
login_dialog_dispose (GObject *object)
{
	LoginDialog *dialog;
	dialog = LOGIN_DIALOG (object);
	if (dialog->priv) {
		if (dialog->priv->wrapper)
			g_object_unref (dialog->priv->wrapper);
		g_free (dialog->priv);
		dialog->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}

/**
 * login_dialog_new
 *
 * Creates a new dialog dialog
 *
 * Returns: a new #LoginDialog object
 */
LoginDialog *
login_dialog_new (GtkWindow *parent)
{
	return (LoginDialog*) g_object_new (LOGIN_TYPE_DIALOG, "title", _("Connection opening"),
					    "transient-for", parent,
					    "resizable", FALSE, NULL);
}

/**
 * login_dialog_run
 * @dialog: a #GdaLogin object
 * @retry: if set to %TRUE, then this method returns only when either a connection has been opened or the
 *         user gave up
 * @error: a place to store errors, or %NULL
 *
 * Displays the dialog and returns a new #GdaConnection, or %NULL
 *
 * Return: a new #GdaConnection, or %NULL
 */
GdaConnection *
login_dialog_run (LoginDialog *dialog, gboolean retry, GError **error)
{
	GdaConnection *cnc = NULL;

	g_return_val_if_fail (LOGIN_IS_DIALOG (dialog), NULL);

	gtk_widget_show (GTK_WIDGET (dialog));
	while (1) {
		gint result;
		result = gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_show (dialog->priv->spinner);
		browser_spinner_start (BROWSER_SPINNER (dialog->priv->spinner));
		gtk_widget_set_sensitive (dialog->priv->login, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, FALSE);
		
		if (result == GTK_RESPONSE_ACCEPT) {
			const GdaDsnInfo *info;
			GError *lerror = NULL;
			
			if (!dialog->priv->wrapper)
				dialog->priv->wrapper = gda_thread_wrapper_new ();

			info = gdaui_login_get_connection_information (GDAUI_LOGIN (dialog->priv->login));
			cnc = real_open_connection (dialog->priv->wrapper, info, &lerror);
			browser_spinner_stop (BROWSER_SPINNER (dialog->priv->spinner));
			if (cnc)
				goto out;
			else {
				if (retry) {
					browser_show_error (GTK_WINDOW (dialog), _("Could not open connection:\n%s"),
							    lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				else {
					g_propagate_error (error, lerror);
					goto out;
				}
			}
		}
		else {
			/* cancelled connection opening */
			g_set_error (error, LOGIN_DIALOG_ERROR, LOGIN_DIALOG_CANCELLED_ERROR,
				     "%s", _("Cancelled by the user"));
			goto out;
		}
		
		gtk_widget_set_sensitive (dialog->priv->login, TRUE);
		browser_spinner_stop (BROWSER_SPINNER (dialog->priv->spinner));
		gtk_widget_hide (dialog->priv->spinner);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, TRUE);
	}

 out:
	gtk_widget_hide (GTK_WIDGET (dialog));
	return cnc;
}

/*
 * executed in a sub thread
 */
static GdaConnection *
sub_thread_open_cnc (GdaDsnInfo *info, GError **error)
{
#ifndef DUMMY
	GdaConnection *cnc;
	if (info->name)
		cnc = gda_connection_open_from_dsn (info->name, info->auth_string,
						    GDA_CONNECTION_OPTIONS_THREAD_SAFE |
						    GDA_CONNECTION_OPTIONS_AUTO_META_DATA,
						    error);
	else
		cnc = gda_connection_open_from_string (info->provider, info->cnc_string,
						       info->auth_string,
						       GDA_CONNECTION_OPTIONS_THREAD_SAFE |
						       GDA_CONNECTION_OPTIONS_AUTO_META_DATA,
						       error);
	return cnc;
#else
	sleep (5);
	g_set_error (error, 0, 0, "%s", "Oooo");
	return NULL;
#endif
}

static void
dsn_info_free (GdaDsnInfo *info)
{
	g_free (info->name);
	g_free (info->provider);
	g_free (info->cnc_string);
	g_free (info->auth_string);
	g_free (info);
}

typedef struct {
	guint cncid;
	GMainLoop *loop;
	GError **error;
	GdaThreadWrapper *wrapper;

	/* out */
	GdaConnection *cnc;
} MainloopData;

static gboolean
check_for_cnc (MainloopData *data)
{
	GdaConnection *cnc;
	GError *lerror = NULL;
	cnc = gda_thread_wrapper_fetch_result (data->wrapper, FALSE, data->cncid, &lerror);
	if (cnc || (!cnc && lerror)) {
		/* waiting is finished! */
		data->cnc = cnc;
		if (lerror)
			g_propagate_error (data->error, lerror);
		g_main_loop_quit (data->loop);
		return FALSE;
	}
	return TRUE;
}

/**
 * real_open_connection
 * @cncinfo: a pointer to a #GdaDsnInfo, describing the connection to open
 * @error:
 *
 * Opens a connection in a sub thread while running a main loop
 *
 * Returns: a new #GdaConnection, or %NULL if an error occurred
 */
static GdaConnection *
real_open_connection (GdaThreadWrapper *wrapper, const GdaDsnInfo *cncinfo, GError **error)
{
	
	GdaDsnInfo *info;
	guint cncid;

	info = g_new0 (GdaDsnInfo, 1);
	if (cncinfo->name)
		info->name = g_strdup (cncinfo->name);
	if (cncinfo->provider)
		info->provider = g_strdup (cncinfo->provider);
	if (cncinfo->cnc_string)
		info->cnc_string = g_strdup (cncinfo->cnc_string);
	if (cncinfo->auth_string)
		info->auth_string = g_strdup (cncinfo->auth_string);
	
	cncid = gda_thread_wrapper_execute (wrapper,
					    (GdaThreadWrapperFunc) sub_thread_open_cnc,
					    (gpointer) info,
					    (GDestroyNotify) dsn_info_free,
					    error);
	if (cncid == 0)
		return NULL;

	GMainLoop *loop;
	MainloopData data;

	loop = g_main_loop_new (NULL, FALSE);
	
	data.cncid = cncid;
	data.error = error;
	data.loop = loop;
	data.cnc = NULL;
	data.wrapper = wrapper;

	g_timeout_add (200, (GSourceFunc) check_for_cnc, &data);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	if (data.cnc)
		g_object_set (data.cnc, "monitor-wrapped-in-mainloop", TRUE, NULL);
	return data.cnc;
}
