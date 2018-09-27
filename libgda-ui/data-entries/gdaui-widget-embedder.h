/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#define GDAUI_TYPE_WIDGET_EMBEDDER              (gdaui_widget_embedder_get_type ())
G_DECLARE_DERIVABLE_TYPE (GdauiWidgetEmbedder, gdaui_widget_embedder, GDAUI, WIDGET_EMBEDDER, GtkContainer)
struct _GdauiWidgetEmbedderClass
{
	GtkContainerClass parent_class;
};

GtkWidget* gdaui_widget_embedder_new        (void);
void       gdaui_widget_embedder_set_valid  (GdauiWidgetEmbedder *bin, gboolean valid);
void       gdaui_widget_embedder_set_ucolor (GdauiWidgetEmbedder *bin, gdouble red, gdouble green,
				       gdouble blue, gdouble alpha);

