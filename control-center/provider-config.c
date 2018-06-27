/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <gtk/gtk.h>
#include <libgda-ui/libgda-ui.h>
#include "provider-config.h"
#include "gdaui-bar.h"
#include "support.h"

#define PROVIDER_CONFIG_DATA "Provider_ConfigData"

typedef struct {
	GtkWidget *title;
	GtkWidget *provider_list;
	GtkWidget *prov_image;
	GtkWidget *prov_name;
	GtkWidget *prov_loc;
} ProviderConfigPrivate;

/*
 * Public functions
 */

static void
provider_selection_changed_cb (GtkTreeSelection *selection, ProviderConfigPrivate *priv)
{
	GdaDataModelIter *sel_iter;
	sel_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (priv->provider_list));
	GdkPixbuf *pix = NULL;
	const GValue *cvalue = NULL;
	if (sel_iter)
		cvalue = gda_data_model_iter_get_value_at (sel_iter, 0);
	if (cvalue) {
		GdaProviderInfo *pinfo;
		pinfo = gda_config_get_provider_info (g_value_get_string (cvalue));
		pix = support_create_pixbuf_for_provider (pinfo);

		gchar *tmp, *tmp1, *tmp2;
		tmp1 = g_markup_printf_escaped ("%s", pinfo->id);
		tmp2 = g_markup_printf_escaped ("%s", pinfo->description);
		tmp = g_strdup_printf ("<b><big>%s</big></b>\n%s", tmp1, tmp2);
		g_free (tmp1);
		g_free (tmp2);
		gtk_label_set_markup (GTK_LABEL (priv->prov_name), tmp);
		g_free (tmp);

		GString *string;
		string = g_string_new ("");

		GdaSet *set;
		set = pinfo->dsn_params;
		if (set) {
			tmp1 = g_markup_printf_escaped ("%s", _("Accepted connection parameters"));
			g_string_append_printf (string, "<b><u>%s</u></b>:\n", tmp1);
			g_free (tmp1);

			GSList *list;
			for (list = gda_set_get_holders (set); list; list = list->next) {
				GdaHolder *holder = GDA_HOLDER (list->data);
				tmp1 = g_markup_printf_escaped ("%s", gda_holder_get_id (holder));
				gchar *descr;
				g_object_get (holder, "description", &descr, NULL);
				tmp2 = descr ? g_markup_printf_escaped ("%s", descr) : NULL;
				g_free (descr);
				if (gda_holder_get_not_null (holder)) {
					if (tmp2)
						g_string_append_printf (string, "<b>%s</b>: %s\n", tmp1, tmp2);
					else
						g_string_append_printf (string, "<b>%s</b>:\n", tmp1);
				}
				else {
					if (tmp2)
						g_string_append_printf (string, "<b>%s</b> (%s): %s\n",
									tmp1, _("optional"), tmp2);
					else
						g_string_append_printf (string, "<b>%s</b> (%s):\n",
									tmp1, _("optional"));
				}
				g_free (tmp1);
				g_free (tmp2);
			}
		}

		set = pinfo->auth_params;
		if (set && gda_set_get_holders (set)) {
			tmp1 = g_markup_printf_escaped ("%s", _("Authentication parameters"));
			g_string_append_printf (string, "\n<b><u>%s</u></b>:\n", tmp1);
			g_free (tmp1);

			GSList *list;
			for (list = gda_set_get_holders (set); list; list = list->next) {
				GdaHolder *holder = GDA_HOLDER (list->data);
				tmp1 = g_markup_printf_escaped ("%s", gda_holder_get_id (holder));
				gchar *descr;
				g_object_get (holder, "description", &descr, NULL);
				tmp2 = descr ? g_markup_printf_escaped ("%s", descr) : NULL;
				g_free (descr);
				if (gda_holder_get_not_null (holder)) {
					if (tmp2)
						g_string_append_printf (string, "<b>%s</b>: %s\n", tmp1, tmp2);
					else
						g_string_append_printf (string, "<b>%s</b>\n", tmp1);
				}
				else {
					if (tmp2)
						g_string_append_printf (string, "<b>%s</b> (%s): %s\n",
									tmp1, _("optional"), tmp2);
					else
						g_string_append_printf (string, "<b>%s</b> (%s)\n",
									tmp1, _("optional"));
				}
				g_free (tmp1);
				g_free (tmp2);
			}
		}

		tmp1 = g_markup_printf_escaped ("%s", _("Shared object file"));
		tmp2 = g_markup_printf_escaped ("%s", pinfo->location);
		g_string_append_printf (string, "\n<b><u>%s</u></b>:\n%s", tmp1, tmp2);
		g_free (tmp1);
		g_free (tmp2);

		gtk_label_set_markup (GTK_LABEL (priv->prov_loc), string->str);
		g_string_free (string, TRUE);
	}
	if (pix) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->prov_image), pix);
		g_object_unref (pix);
	}
	else
		gtk_image_clear (GTK_IMAGE (priv->prov_image));
	gtk_widget_grab_focus (priv->provider_list);
}

GtkWidget *
provider_config_new (void)
{
	ProviderConfigPrivate *priv;
	GtkWidget *provider;
	GtkWidget *paned;
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
				 _("Providers are addons that actually implement the access "
				   "to each database using the means provided by each database vendor."));
	priv->title = gdaui_bar_new (title);
	g_free (title);

	gdaui_bar_set_icon_from_resource (GDAUI_BAR (priv->title), "/images/gdaui-generic.png");

	gtk_box_pack_start (GTK_BOX (provider), priv->title, FALSE, FALSE, 0);
	gtk_widget_show (priv->title);

	/* horizontal box for the provider list and its properties */
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position (GTK_PANED (paned), 200);
	gtk_box_pack_start (GTK_BOX (provider), paned, TRUE, TRUE, 0);
	gtk_widget_show (paned);

	/* create the provider list */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_paned_add1 (GTK_PANED (paned), sw);

	model = gda_config_list_providers ();
	priv->provider_list = gdaui_raw_grid_new (model);
	gdaui_data_proxy_column_set_editable (GDAUI_DATA_PROXY (priv->provider_list), 0, FALSE);
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->provider_list), -1, FALSE);
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->provider_list), 0, TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->provider_list), FALSE);
	g_object_set (G_OBJECT (priv->provider_list), "info-cell-visible", FALSE, NULL);

	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->provider_list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (provider_selection_changed_cb), priv);
	gtk_container_add (GTK_CONTAINER (sw), priv->provider_list);
	gtk_widget_show_all (sw);
	g_object_unref (model);

	/* properties */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_paned_add2 (GTK_PANED (paned), sw);

	GtkWidget *grid;
	grid = gtk_grid_new ();
	g_object_set (grid, "margin-top", 20, NULL);
	gtk_container_add (GTK_CONTAINER (sw), grid);
	gtk_widget_show (sw);

	priv->prov_image = gtk_image_new ();
	g_object_set (priv->prov_image,
		      "halign", GTK_ALIGN_END,
		      "valign", GTK_ALIGN_START,
		      "hexpand", TRUE,
		      "vexpand", FALSE, NULL);
	gtk_grid_attach (GTK_GRID (grid), priv->prov_image, 1, 0, 1, 1);

	priv->prov_name = gtk_label_new ("");
	g_object_set (priv->prov_name,
		      "xalign", 0.,
		      "yalign", 0.,
		      "halign", GTK_ALIGN_START,
		      "valign", GTK_ALIGN_START,
		      "hexpand", TRUE,
		      "margin-start", 20, NULL);
	gtk_label_set_line_wrap (GTK_LABEL (priv->prov_name), TRUE);
	gtk_label_set_selectable (GTK_LABEL (priv->prov_name), TRUE);
	gtk_grid_attach (GTK_GRID (grid), priv->prov_name, 0, 0, 1, 1);
	gtk_widget_set_size_request (priv->prov_name, -1, 96);

	priv->prov_loc = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (priv->prov_loc), TRUE);
	g_object_set (priv->prov_loc,
		      "xalign", 0.,
		      "halign", GTK_ALIGN_START,
		      "hexpand", TRUE,
		      "margin-start", 20, NULL);
	gtk_widget_set_hexpand (priv->prov_loc, TRUE);
	gtk_label_set_selectable (GTK_LABEL (priv->prov_loc), TRUE);
	gtk_grid_attach (GTK_GRID (grid), priv->prov_loc, 0, 1, 2, 1);
	
	gtk_widget_show_all (grid);

	gdaui_data_selector_select_row (GDAUI_DATA_SELECTOR (priv->provider_list), 0);
	return provider;
}
