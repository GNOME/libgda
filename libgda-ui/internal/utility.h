/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <libgda/gda-set.h>
#include <libgda-ui/gdaui-data-entry.h>
#include <libgda-ui/gdaui-data-store.h>
#include <libgda-ui/gdaui-set.h>
#include <libgda-ui/gdaui-data-proxy.h>

/*
 *
 * Data Entry utilities
 *
 */
GtkWidget              *_gdaui_utility_entry_build_actions_menu      (GObject *obj_data, guint attrs, GCallback function);
GdkRGBA               **_gdaui_utility_entry_build_info_colors_array_a (void);
gchar                  *_gdaui_utility_markup_title                  (const gchar *title, gboolean optional);

/*
 * Computes attributes and values list from the attributes and values stored in @store and
 * corresponding to the columns for the parameters in @group (as described by @model_iter), 
 * at row pointed by @tree_iter
 */
guint            _gdaui_utility_proxy_compute_attributes_for_group (GdauiSetGroup *group, 
								    GdauiDataStore *store, GdaDataModelIter *model_iter, 
								    GtkTreeIter *tree_iter, 
								    gboolean *to_be_deleted);
GList           *_gdaui_utility_proxy_compute_values_for_group     (GdauiSetGroup *group, 
								    GdauiDataStore *store, GdaDataModelIter *model_iter, 
								    GtkTreeIter *tree_iter, gboolean model_values);

/*
 * Some dialogs used by GnomeDbDataProxy widgets
 */
gboolean         _gdaui_utility_display_error_with_keep_or_discard_choice (GdauiDataProxy *form, GError *filled_error);
void             _gdaui_utility_display_error                             (GdauiDataProxy *form, gboolean can_discard, GError *filled_error);
void             _gdaui_utility_show_error (GtkWindow *parent, const gchar *format, ...);

/*
 * Misc
 */
gboolean         _gdaui_utility_iter_differ (GdaDataModelIter *iter1, GdaDataModelIter *iter2);

/*
 * Handle selection and right click on treeviews
 */
void             _gdaui_setup_right_click_selection_on_treeview (GtkTreeView *tview);
