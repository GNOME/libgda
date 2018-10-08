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

#include <libgda/gda-config.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda-ui/gdaui-login.h>
#include "gdaui-login-dialog.h"
#include <glib/gi18n-lib.h>

struct _GdauiLoginDialogPrivate {
	GtkWidget *notice;
	GtkWidget *login;
};

static void gdaui_login_dialog_class_init   (GdauiLoginDialogClass *klass);
static void gdaui_login_dialog_init         (GdauiLoginDialog *dialog,
						GdauiLoginDialogClass *klass);
static void gdaui_login_dialog_set_property (GObject *object,
						guint paramid,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_login_dialog_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);
static void gdaui_login_dialog_finalize     (GObject *object);

enum {
	PROP_0
};

static GObjectClass *parent_class = NULL;

/*
 * GdauiLoginDialog class implementation
 */

static void
gdaui_login_dialog_class_init (GdauiLoginDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = gdaui_login_dialog_set_property;
	object_class->get_property = gdaui_login_dialog_get_property;
	object_class->finalize = gdaui_login_dialog_finalize;

}

static void
login_changed_cb (GdauiLogin *login, G_GNUC_UNUSED gboolean valid, GdauiLoginDialog *dialog)
{
	gchar *str;
	const GdaDsnInfo *cinfo;

	cinfo = gdaui_login_get_connection_information (login);
	if (cinfo->name && gda_config_dsn_needs_authentication (cinfo->name)) {
		str = g_strdup_printf ("<b>%s:</b>\n%s", _("Connection opening"),
				       _("Fill in the following authentication elements\n"
					 "to open a connection"));
		gtk_label_set_markup (GTK_LABEL (dialog->priv->notice), str);
		g_free (str);
		gtk_widget_show (dialog->priv->login);
	}
	else {
		str = g_strdup_printf ("<b>%s:</b>\n%s", _("Connection opening"),
				       _("No authentication required,\n"
					 "confirm connection opening"));
		gtk_label_set_markup (GTK_LABEL (dialog->priv->notice), str);
		g_free (str);
		gtk_widget_hide (dialog->priv->login);
	}
}

static void
gdaui_login_dialog_init (GdauiLoginDialog *dialog, G_GNUC_UNUSED GdauiLoginDialogClass *klass)
{
	GtkWidget *hbox, *vbox, *image, *label;
	GtkWidget *nb;
	GtkWidget *dcontents;

	g_return_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog));

	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	dialog->priv = g_new0 (GdauiLoginDialogPrivate, 1);
        
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("C_onnect"), GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
        gtk_box_set_spacing (GTK_BOX (dcontents), 12);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (dcontents), hbox, TRUE, TRUE, 0);

	image = gtk_image_new_from_icon_name ("dialog-password", GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 5);

	nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), nb, TRUE, TRUE, 0);
	gtk_widget_show (nb);
	g_object_set_data (G_OBJECT (dialog), "main_part", nb);	

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_show (vbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (nb), vbox, NULL);
	gtk_widget_show (vbox);

	label = gtk_label_new ("");
	dialog->priv->notice = label;

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_selectable (GTK_LABEL (label), FALSE);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	dialog->priv->login = gdaui_login_new (NULL);
	g_object_set (G_OBJECT (dialog->priv->login), "mode",
		      GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE |
		      GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE, NULL);
	gtk_widget_show (dialog->priv->login);
	gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->login, TRUE, TRUE, 0);
	g_signal_connect (G_OBJECT (dialog->priv->login), "changed",
			  G_CALLBACK (login_changed_cb), dialog);

	GdkPixbuf *icon;
	icon = gdk_pixbuf_new_from_resource ("/images/gdaui-generic.png", NULL);
	if (icon) {
		gtk_window_set_icon (GTK_WINDOW (dialog), icon);
		g_object_unref (icon);
	}

}

static void
gdaui_login_dialog_set_property (GObject *object,
				    guint param_id,
				    G_GNUC_UNUSED const GValue *value,
				    GParamSpec *pspec)
{
	GdauiLoginDialog *dialog = (GdauiLoginDialog *) object;

	g_return_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog));

	switch (param_id) {
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_login_dialog_get_property (GObject *object,
				    guint param_id,
				    G_GNUC_UNUSED GValue *value,
				    GParamSpec *pspec)
{
	GdauiLoginDialog *dialog = (GdauiLoginDialog *) object;

	g_return_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog));

	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_login_dialog_finalize (GObject *object)
{
	GdauiLoginDialog *dialog = (GdauiLoginDialog *) object;

	g_return_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog));

	/* free memory */
	g_free (dialog->priv);
	dialog->priv = NULL;

	parent_class->finalize (object);
}

GType
gdaui_login_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiLoginDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_login_dialog_class_init,
			NULL,
			NULL,
			sizeof (GdauiLoginDialog),
			0,
			(GInstanceInitFunc) gdaui_login_dialog_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "GdauiLoginDialog", &info, 0);
	}
	return type;
}

/**
 * gdaui_login_dialog_new
 * @title: (nullable): title of the dialog, or %NULL
 * @parent: (nullable): transient parent of the dialog, or %NULL
 *
 * Creates a new login dialog widget.
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_login_dialog_new (const gchar *title, GtkWindow *parent)
{
	GdauiLoginDialog *dialog;

	dialog = g_object_new (GDAUI_TYPE_LOGIN_DIALOG, "transient-for", parent, "title", title, NULL);

	return GTK_WIDGET (dialog);
}

/**
 * gdaui_login_dialog_run
 * @dialog:
 *
 * Shows the login dialog and waits for the user to enter its username and
 * password and perform an action on the dialog.
 *
 * Returns: TRUE if the user wants to connect
 */
gboolean
gdaui_login_dialog_run (GdauiLoginDialog *dialog)
{
	g_return_val_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog), FALSE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
		return TRUE;

	return FALSE;
}

/**
 * gdaui_login_dialog_get_login_widget
 * @dialog: A #GdauiLoginDialog widget.
 *
 * Get the #GdauiLogin widget contained in a #GdauiLoginDialog.
 *
 * Returns: the login widget contained in the dialog.
 */
GdauiLogin *
gdaui_login_dialog_get_login_widget (GdauiLoginDialog *dialog)
{
	g_return_val_if_fail (GDAUI_IS_LOGIN_DIALOG (dialog), NULL);
	return GDAUI_LOGIN (dialog->priv->login);
}

