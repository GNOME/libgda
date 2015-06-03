/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <gtk/gtk.h>

#define WIDGET_OVERLAY_TYPE              (widget_overlay_get_type ())
#define WIDGET_OVERLAY(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), WIDGET_OVERLAY_TYPE, WidgetOverlay))
#define WIDGET_OVERLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WIDGET_OVERLAY_TYPE, WidgetOverlayClass))
#define IS_WIDGET_OVERLAY(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WIDGET_OVERLAY_TYPE))
#define IS_WIDGET_OVERLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WIDGET_OVERLAY_TYPE))
#define WIDGET_OVERLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WIDGET_OVERLAY_TYPE, WidgetOverlayClass))

typedef struct _WidgetOverlay WidgetOverlay;
typedef struct _WidgetOverlayClass WidgetOverlayClass;
typedef struct _WidgetOverlayPrivate WidgetOverlayPrivate;

struct _WidgetOverlay
{
	GtkContainer container;
	WidgetOverlayPrivate *priv;
};

struct _WidgetOverlayClass
{
	GtkContainerClass parent_class;
};

typedef enum
{
	WIDGET_OVERLAY_ALIGN_FILL,
	WIDGET_OVERLAY_ALIGN_START,
	WIDGET_OVERLAY_ALIGN_END,
	WIDGET_OVERLAY_ALIGN_CENTER
} WidgetOverlayAlign;

typedef enum {
	WIDGET_OVERLAY_CHILD_VALIGN, /* WidgetOverlayAlign */
	WIDGET_OVERLAY_CHILD_HALIGN, /* WidgetOverlayAlign */
	WIDGET_OVERLAY_CHILD_ALPHA,  /* gdouble */
	WIDGET_OVERLAY_CHILD_HAS_EVENTS, /* gboolean */
	WIDGET_OVERLAY_CHILD_SCALE,  /* gdouble */
	WIDGET_OVERLAY_CHILD_TOOLTIP, /* gboolean */
} WidgetOverlayChildProperty;

GType      widget_overlay_get_type        (void) G_GNUC_CONST;
GtkWidget* widget_overlay_new             (void);
void       widget_overlay_set_child_props (WidgetOverlay *ovl, GtkWidget *child, ...);
