/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <gtk/gtk.h>
#include <libgda-ui/libgda-ui.h>
#include "provider-config.h"
#include "cc-gray-bar.h"

#define PROVIDER_CONFIG_DATA "Provider_ConfigData"

typedef struct {
	GtkWidget *title;
	GtkWidget *provider_list;
} ProviderConfigPrivate;

/*
 * Public functions
 */

GtkWidget *
provider_config_new (void)
{
	ProviderConfigPrivate *priv;
	GtkWidget *provider;
	GtkWidget *box;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *sw;
	gchar *title;
	GdaDataModel *model;

	priv = g_new0 (ProviderConfigPrivate, 1);
	provider = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (provider);
        gtk_container_set_border_width (GTK_CONTAINER (provider), 6);
	g_object_set_data_full (G_OBJECT (provider), PROVIDER_CONFIG_DATA, priv,
				(GDestroyNotify) g_free);

	/* title */
	title = g_strdup_printf ("<b>%s</b>\n%s", _("Providers"),
				 _("Installed providers"));
	priv->title = cc_gray_bar_new (title);
	g_free (title);

	gchar *path;
	path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gdaui-generic.png", NULL);
	cc_gray_bar_set_icon_from_file (CC_GRAY_BAR (priv->title), path);
	g_free (path);

	gtk_box_pack_start (GTK_BOX (provider), priv->title, FALSE, FALSE, 0);
	gtk_widget_show (priv->title);

	/* create the provider list */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (provider), sw, TRUE, TRUE, 0);

	model = gda_config_list_providers ();
	priv->provider_list = gdaui_raw_grid_new (model);
	g_object_unref (model);
	gdaui_data_proxy_column_set_editable (GDAUI_DATA_PROXY (priv->provider_list), 0, FALSE);
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->provider_list), 2, FALSE);
	g_object_set (G_OBJECT (priv->provider_list), "info-cell-visible", FALSE, NULL);
	gtk_container_add (GTK_CONTAINER (sw), priv->provider_list);
	
	gtk_widget_show_all (sw);

	/* add tip */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (box);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_box_pack_start (GTK_BOX (provider), box, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

	label = gtk_label_new (_("Providers are addons that actually implement the access "
				 "to each database using the means provided by each database vendor."));
	gtk_widget_show (label);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, FALSE, 0);

	return provider;
}
