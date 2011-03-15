/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
 *               1997-2000 GTK+ Team and others.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#include <gtk/gtk.h>

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
};

struct _WidgetEmbedderClass
{
	GtkContainerClass parent_class;
};

GType      widget_embedder_get_type  (void) G_GNUC_CONST;
GtkWidget* widget_embedder_new       (void);
void       widget_embedder_set_valid (WidgetEmbedder *bin, gboolean valid);

