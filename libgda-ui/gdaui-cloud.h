/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDAUI_CLOUD__
#define __GDAUI_CLOUD__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_CLOUD          (gdaui_cloud_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiCloud, gdaui_cloud, GDAUI, CLOUD, GtkBox)
/* struct for the object's class */
struct _GdauiCloudClass
{
	GtkBoxClass        parent_class;
	void            (* activate) (GdauiCloud *cloud, gint row);
	gpointer           padding[12];
};

/**
 * SECTION:gdaui-cloud
 * @short_description: Cloud widget
 * @title: GdauiCloud
 * @stability: Stable
 * @Image: vi-cloud.png
 * @see_also:
 *
 * The #GdauiCloud widget displays a string for each row in a #GdaDataModel for which the size
 * is variable (determined either by some data in the data model, or by a function provided by
 * the programmer).
 *
 * Depending on the selection mode of the widget, each string can be selected by the user and
 * the "selection-changed" signal is emitted.
 */

GtkWidget        *gdaui_cloud_new                  (GdaDataModel *model, gint label_column, gint weight_column);
void              gdaui_cloud_set_selection_mode   (GdauiCloud *cloud, GtkSelectionMode mode);

void              gdaui_cloud_filter               (GdauiCloud *cloud, const gchar *filter);
GtkWidget        *gdaui_cloud_create_filter_widget (GdauiCloud *cloud);

typedef gdouble (*GdauiCloudWeightFunc)            (GdaDataModel *model, gint row, gpointer data);
void              gdaui_cloud_set_weight_func      (GdauiCloud *cloud, GdauiCloudWeightFunc func, gpointer data);

G_END_DECLS

#endif



