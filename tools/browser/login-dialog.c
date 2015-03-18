/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include "login-dialog.h"
#include "ui-support.h"

/* 
 * Main static functions 
 */
static void login_dialog_class_init (LoginDialogClass * class);
static void login_dialog_init (LoginDialog *dialog);
static void login_dialog_dispose (GObject *object);

static TConnection *real_open_connection (const GdaDsnInfo *cncinfo, GError **error);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _LoginDialogPrivate
{
	GtkWidget *login;
	GtkWidget *spinner;
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
		static GMutex registering;
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
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_DIALOG, "LoginDialog", &info, 0);
		g_mutex_unlock (&registering);
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
	char *markup;
	GtkWidget *dcontents;

	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	dialog->priv = g_new0 (LoginDialogPrivate, 1);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("C_onnect"), GTK_RESPONSE_ACCEPT,
				_("_Cancel"), GTK_RESPONSE_REJECT, NULL);

	gtk_box_set_spacing (GTK_BOX (dcontents), 5);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);

	GdkPixbuf *pix;
	pix = gdk_pixbuf_new_from_resource ("/images/gda-browser-non-connected.png", NULL);
	gtk_window_set_icon (GTK_WINDOW (dialog), pix);
	g_object_unref (pix);

	/* label and spinner */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); 
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
	gtk_box_pack_start (GTK_BOX (dcontents), hbox, FALSE, FALSE, 0);
	
	wid = gtk_image_new_from_resource ("/images/gda-browser-non-connected-big.png");
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	markup = g_markup_printf_escaped ("<big><b>%s\n</b></big>%s\n",
					  _("Connection opening:"),
					  _("Select a named data source, or specify\nparameters "
					    "to open a connection to a \nnon defined data source"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 12);
	gtk_widget_show_all (hbox);
	
	dialog->priv->spinner = gtk_spinner_new ();
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
		g_free (dialog->priv);
		dialog->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}

/**
 * login_dialog_new:
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
 * login_dialog_run_open_connection:
 * @dialog: a #GdaLogin object
 * @retry: if set to %TRUE, then this method returns only when either a connection has been opened or the
 *         user gave up
 * @error: a place to store errors, or %NULL
 *
 * Displays the dialog and returns a new #TConnection, or %NULL
 *
 * Return: (transfer none): a new #TConnection, or %NULL
 */
TConnection *
login_dialog_run_open_connection (LoginDialog *dialog, gboolean retry, GError **error)
{
	TConnection *tcnc = NULL;

	g_return_val_if_fail (LOGIN_IS_DIALOG (dialog), NULL);

	gtk_widget_show (GTK_WIDGET (dialog));
	while (1) {
		gint result;
		result = gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_show (dialog->priv->spinner);
		gtk_spinner_start (GTK_SPINNER (dialog->priv->spinner));
		gtk_widget_set_sensitive (dialog->priv->login, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, FALSE);
		
		if (result == GTK_RESPONSE_ACCEPT) {
			const GdaDsnInfo *info;
			GError *lerror = NULL;

			info = gdaui_login_get_connection_information (GDAUI_LOGIN (dialog->priv->login));
			tcnc = real_open_connection (info, &lerror);
			gtk_spinner_stop (GTK_SPINNER (dialog->priv->spinner));
			if (tcnc)
				goto out;
			else {
				if (retry) {
					ui_show_error (GTK_WINDOW (dialog), _("Could not open connection:\n%s"),
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
		gtk_spinner_stop (GTK_SPINNER (dialog->priv->spinner));
		gtk_widget_hide (dialog->priv->spinner);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT, TRUE);
	}

 out:
	gtk_widget_hide (GTK_WIDGET (dialog));
	return tcnc;
}

/**
 * real_open_connection:
 * @cncinfo: a pointer to a #GdaDsnInfo, describing the connection to open
 * @error:
 *
 * Opens a connection in a sub thread while running a main loop
 *
 * Returns: (transfer none): a new #TConnection, or %NULL if an error occurred
 */
static TConnection *
real_open_connection (const GdaDsnInfo *cncinfo, GError **error)
{
#ifdef DUMMY
	sleep (5);
	g_set_error (error, GDA_TOOLS_INTERNAL_COMMAND_ERROR, "%s", "Dummy error!");
	return NULL;
#endif

	TConnection *tcnc;
	if (cncinfo->name)
		tcnc = t_connection_open (NULL, cncinfo->name, cncinfo->auth_string, FALSE, error);
	else {
		gchar *tmp;
		tmp = g_strdup_printf ("%s://%s", cncinfo->provider, cncinfo->cnc_string);
		tcnc = t_connection_open (NULL, tmp, cncinfo->auth_string, FALSE, error);
		g_free (tmp);
	}
	
	return tcnc;
}
