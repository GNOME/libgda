/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
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

#if GTK_CHECK_VERSION(2,20,0)
#define WIDGET_EMBEDDER_TYPE              (widget_embedder_get_type ())
#define WIDGET_EMBEDDER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), WIDGET_EMBEDDER_TYPE, WidgetEmbedder))
#define WIDGET_EMBEDDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WIDGET_EMBEDDER_TYPE, WidgetEmbedderClass))
#define IS_WIDGET_EMBEDDER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WIDGET_EMBEDDER_TYPE))
#define IS_WIDGET_EMBEDDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WIDGET_EMBEDDER_TYPE))
#define WIDGET_EMBEDDER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WIDGET_EMBEDDER_TYPE, WidgetEmbedderClass))

typedef struct _WidgetEmbedder   WidgetEmbedder;
typedef struct _WidgetEmbedderClass  WidgetEmbedderClass;

struct _WidgetEmbedder
{
	GtkContainer container;

	GtkWidget *child;
	GdkWindow *offscreen_window;
	gboolean   valid;

	/* unknown colors */
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble alpha;
};

struct _WidgetEmbedderClass
{
	GtkContainerClass parent_class;
};

GType      widget_embedder_get_type   (void) G_GNUC_CONST;
GtkWidget* widget_embedder_new        (void);
void       widget_embedder_set_valid  (WidgetEmbedder *bin, gboolean valid);
void       widget_embedder_set_ucolor (WidgetEmbedder *bin, gdouble red, gdouble green,
				       gdouble blue, gdouble alpha);

#endif
