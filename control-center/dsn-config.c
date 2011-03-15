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
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda-ui/libgda-ui.h>
#include "dsn-config.h"
#include "dsn-properties-dialog.h"
#include "cc-utility.h"
#include "cc-gray-bar.h"

#define DSN_CONFIG_DATA "DSN_ConfigData"

typedef struct {
	GtkWidget *title;
	GtkWidget *dsn_list;
	GtkWidget *dialog;
} DsnConfigPrivate;

static void
free_private_data (gpointer data)
{
	DsnConfigPrivate *priv = (DsnConfigPrivate *) data;

	g_free (priv);
}

static void
list_double_clicked_cb (G_GNUC_UNUSED GdauiGrid *grid, G_GNUC_UNUSED gint row, gpointer user_data)
{
	dsn_config_edit_properties (GTK_WIDGET (user_data));
}

static void
list_popup_properties_cb (G_GNUC_UNUSED GtkWidget *menu, gpointer user_data)
{
	dsn_config_edit_properties (GTK_WIDGET (user_data));
}

static void
list_popup_delete_cb (G_GNUC_UNUSED GtkWidget *menu, gpointer user_data)
{
	dsn_config_delete (GTK_WIDGET (user_data));
}

static void
list_popup_cb (GdauiRawGrid *grid, GtkMenu *menu, gpointer user_data)
{
	GtkWidget *item_delete, *item_properties;
	gboolean ok;
	GArray *selection;

	item_delete = cc_new_menu_item (GTK_STOCK_DELETE,
					      TRUE,
					      G_CALLBACK (list_popup_delete_cb),
					      user_data);
	item_properties = cc_new_menu_item (GTK_STOCK_PROPERTIES,
						  TRUE,
						  G_CALLBACK (list_popup_properties_cb),
						  user_data);

	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (grid));
	ok = (selection != NULL);
	if (selection)
		g_array_free (selection, TRUE);
	gtk_widget_set_sensitive (item_delete, ok);
	gtk_widget_set_sensitive (item_properties, ok);

	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item_delete);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item_properties);
}

/*
 * Public functions
 */

GtkWidget *
dsn_config_new (void)
{
	DsnConfigPrivate *priv;
	GtkWidget *dsn;
	GtkWidget *box;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *sw;
	gchar *title;
	GdaDataModel *model;

	priv = g_new0 (DsnConfigPrivate, 1);
	dsn = cc_new_vbox_widget (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (dsn), 6);
	g_object_set_data_full (G_OBJECT (dsn), DSN_CONFIG_DATA, priv,
				(GDestroyNotify) free_private_data);

	/* title */
	title = g_strdup_printf ("<b>%s</b>\n%s", _("Data Sources"),
				 _("Configured data sources in the system"));
	priv->title = cc_gray_bar_new (title);
	g_free (title);

	gchar *path;
	path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gdaui-generic.png", NULL);
	cc_gray_bar_set_icon_from_file (CC_GRAY_BAR (priv->title), path);
	g_free (path);
	gtk_box_pack_start (GTK_BOX (dsn), priv->title, FALSE, FALSE, 0);
	gtk_widget_show (priv->title);

	/* create the data source list */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (dsn), sw, TRUE, TRUE, 0);
	
	model = gda_config_list_dsn ();
	priv->dsn_list = gdaui_raw_grid_new (model);
	gtk_tree_view_move_column_after (GTK_TREE_VIEW (priv->dsn_list),
					 gtk_tree_view_get_column (GTK_TREE_VIEW (priv->dsn_list), 1),
					 gtk_tree_view_get_column (GTK_TREE_VIEW (priv->dsn_list), 2));

	g_object_unref (model);
	g_object_set_data (G_OBJECT (dsn), "grid", priv->dsn_list);
 	gdaui_data_proxy_column_set_editable (GDAUI_DATA_PROXY (priv->dsn_list), 0, FALSE);
 	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->dsn_list), 3, FALSE);
 	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->dsn_list), 4, FALSE);
	g_object_set (priv->dsn_list, "info-cell-visible", FALSE, NULL);

	gtk_container_add (GTK_CONTAINER (sw), priv->dsn_list);
	
	gtk_widget_show_all (sw);
	g_signal_connect (priv->dsn_list, "double-clicked",
			  G_CALLBACK (list_double_clicked_cb), dsn);
	g_signal_connect (priv->dsn_list, "populate-popup",
			  G_CALLBACK (list_popup_cb), dsn);

	/* add tip */
	box = gtk_hbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_box_pack_start (GTK_BOX (dsn), box, FALSE, FALSE, 0);
	gtk_widget_show (box);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
        gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

	label = gtk_label_new (_("Data sources are the means by which database "
				 "connections are identified: all "
				 "the information needed to open a connection to "
				 "a specific database using a 'provider' is referenced using "
				 "a unique name."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	return dsn;
}

void
dsn_config_edit_properties (GtkWidget *dsn)
{
	DsnConfigPrivate *priv;
	GdaDataModel *model;
	GArray *selection;
	gchar *str;
	const GValue *cvalue;

	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);
	
	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (priv->dsn_list));
	if (!selection)
		return;

	model = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (priv->dsn_list));
	if (!GDA_IS_DATA_MODEL (model)) {
		g_array_free (selection, TRUE);
		return;
	}

	cvalue = gda_data_model_get_value_at (model, 0, g_array_index (selection, gint, 0), NULL);
	g_array_free (selection, TRUE);
	if (!cvalue) 
		return;

	str = gda_value_stringify ((GValue *) cvalue);
	dsn_properties_dialog (GTK_WINDOW (gtk_widget_get_toplevel (dsn)), str);

	g_free (str);
}

void
dsn_config_delete (GtkWidget *dsn)
{
	DsnConfigPrivate *priv;
	GtkTreeSelection *sel;
	GList *list, *sel_rows;
	GList *sel_dsn = NULL;
	GdaDataModel *model;

	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dsn_list));
	sel_rows = gtk_tree_selection_get_selected_rows (sel, NULL);
	model = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (priv->dsn_list));
	g_assert (GDA_IS_DATA_MODEL (model));

	/* make a list of DSN to remove */
	for (list = sel_rows; list; list = list->next) {
		gchar *dsn;
		gint *row;
		const GValue *cvalue;
		
		row = gtk_tree_path_get_indices ((GtkTreePath *) list->data);
		cvalue = gda_data_model_get_value_at (model, 0, *row, NULL);
		if (cvalue) {
			dsn = gda_value_stringify ((GValue *) cvalue);
			sel_dsn = g_list_prepend (sel_dsn, dsn);
		}
		gtk_tree_path_free ((GtkTreePath *) list->data);
	}
	g_list_free (sel_rows);

	/* actually remove the DSN listed */
	for (list = sel_dsn; list; list = list->next) {
		gchar *str, *dsn;
		GtkWidget *dialog;
		dsn = (gchar *) list->data;

		str = g_strdup_printf (_("Are you sure you want to remove the data source '%s'?"), dsn);
		dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (priv->dsn_list)),
							     GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
							     GTK_BUTTONS_YES_NO, 
							     "<b>%s:</b>\n\n%s", _("Data source removal confirmation"),
							     str);
		g_free (str);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);
		gtk_widget_show (dialog);
		
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) 
			gda_config_remove_dsn (dsn, NULL);
		gtk_widget_destroy (dialog);
		
		g_free (dsn);
	}
}
