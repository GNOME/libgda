/*
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include <gtk/gtk.h>
#include "vtable-dialog.h"

#define SPACING 3

struct _VtableDialogPrivate {
	TConnection *tcnc;
	GtkWidget *tname_entry;
	GtkWidget *tname_replace;
};

static void vtable_dialog_class_init (VtableDialogClass *klass);
static void vtable_dialog_init       (VtableDialog *dlg, VtableDialogClass *klass);
static void vtable_dialog_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;


/*
 * VtableDialog class implementation
 */

static void
vtable_dialog_class_init (VtableDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = vtable_dialog_dispose;
}


static void
vtable_dialog_init (VtableDialog *dlg, G_GNUC_UNUSED VtableDialogClass *klass)
{
	dlg->priv = g_new0 (VtableDialogPrivate, 1);
}

static void
vtable_dialog_dispose (GObject *object)
{
	VtableDialog *dlg = (VtableDialog *) object;

	/* free memory */
	if (dlg->priv) {
		if (dlg->priv->tcnc)
			g_object_unref (dlg->priv->tcnc);
		g_free (dlg->priv);
		dlg->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
vtable_dialog_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo columns = {
			sizeof (VtableDialogClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) vtable_dialog_class_init,
			NULL,
			NULL,
			sizeof (VtableDialog),
			0,
			(GInstanceInitFunc) vtable_dialog_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "VtableDialog", &columns, 0);
	}
	return type;
}

/**
 * vtable_dialog_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
vtable_dialog_new (GtkWindow *parent, TConnection *tcnc)
{
	VtableDialog *dlg;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	dlg = VTABLE_DIALOG (g_object_new (VTABLE_DIALOG_TYPE, NULL));
	dlg->priv->tcnc = g_object_ref (tcnc);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (dlg), SPACING * 2);
	gtk_window_set_title (GTK_WINDOW (dlg), _("Define LDAP search as a virtual table"));

	GtkWidget *dcontents;
	GtkWidget *label, *entry, *grid, *button;
	gchar *str;
	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
	label = gtk_label_new (NULL);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	str = g_markup_printf_escaped ("<b>%s:</b>\n<small>%s</small>",
				       _("Name of the virtual LDAP table to create"),
				       _("Everytime data is selected from the virtual table which will "
					 "be created, the LDAP search will be executed and data "
					 "returned as the contents of the table."));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (dcontents), label, FALSE, FALSE, SPACING);

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), SPACING);
	gtk_grid_set_row_spacing (GTK_GRID (grid), SPACING);
	gtk_box_pack_start (GTK_BOX (dcontents), grid, FALSE, FALSE, SPACING);

	label = gtk_label_new (_("Table name:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	entry = gtk_entry_new ();
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	dlg->priv->tname_entry = entry;

	label = gtk_label_new (_("Replace if exists:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	button = gtk_check_button_new ();
	gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 1);
	dlg->priv->tname_replace = button;

	gtk_widget_show_all (dcontents);
	gtk_dialog_add_buttons (GTK_DIALOG (dlg),
				_("_Ok"), GTK_RESPONSE_OK,
				_("_Cancel"), GTK_RESPONSE_CANCEL, NULL);

	return (GtkWidget*) dlg;
}

/**
 * vtable_dialog_get_table_name:
 *
 */
const gchar *
vtable_dialog_get_table_name (VtableDialog *dlg)
{
	g_return_val_if_fail (IS_VTABLE_DIALOG (dlg), NULL);
	return gtk_entry_get_text (GTK_ENTRY (dlg->priv->tname_entry));
}

/**
 * vtable_dialog_get_replace_if_exists:
 */
gboolean
vtable_dialog_get_replace_if_exists (VtableDialog *dlg)
{
	g_return_val_if_fail (IS_VTABLE_DIALOG (dlg), FALSE);
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dlg->priv->tname_replace));
}
