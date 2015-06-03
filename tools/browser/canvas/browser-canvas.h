/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __BROWSER_CANVAS__
#define __BROWSER_CANVAS__

#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "browser-canvas-decl.h"

G_BEGIN_DECLS

#define TYPE_BROWSER_CANVAS          (browser_canvas_get_type())
#define BROWSER_CANVAS(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_get_type(), BrowserCanvas)
#define BROWSER_CANVAS_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_get_type (), BrowserCanvasClass)
#define IS_BROWSER_CANVAS(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_get_type ())

typedef enum {
	BROWSER_CANVAS_LAYOUT_DEFAULT,
	BROWSER_CANVAS_LAYOUT_RADIAL
} BrowserCanvasLayoutAlgorithm;

/* struct for the object's data */
struct _BrowserCanvas
{
	GtkScrolledWindow   widget;

	/* pointer position when a context menu was last opened, or while moving around the canvas */
	gdouble             xmouse;
	gdouble             ymouse;

	/* private */
	BrowserCanvasPrivate  *priv;
};

/* struct for the object's class */
struct _BrowserCanvasClass
{
	GtkScrolledWindowClass parent_class;

	/* signals */
	void           (*item_selected) (BrowserCanvas *canvas, BrowserCanvasItem *item);

	/* virtual functions */
	void           (*clean_canvas_items)  (BrowserCanvas *canvas); /* clean any extra structure, not the individual items */
	GSList        *(*get_layout_items) (BrowserCanvas *canvas);

	GtkWidget     *(*build_context_menu)  (BrowserCanvas *canvas);
};

/* generic widget's functions */
GType              browser_canvas_get_type                (void) G_GNUC_CONST;
void               browser_canvas_declare_item            (BrowserCanvas *canvas, BrowserCanvasItem *item);

void               browser_canvas_set_zoom_factor         (BrowserCanvas *canvas, gdouble n);
gdouble            browser_canvas_get_zoom_factor         (BrowserCanvas *canvas);
gdouble            browser_canvas_fit_zoom_factor         (BrowserCanvas *canvas);
gboolean           browser_canvas_auto_layout_enabled     (BrowserCanvas *canvas);
void               browser_canvas_perform_auto_layout     (BrowserCanvas *canvas, gboolean animate,
							   BrowserCanvasLayoutAlgorithm algorithm);
void               browser_canvas_center                  (BrowserCanvas *canvas);
void               browser_canvas_scale_layout            (BrowserCanvas *canvas, gdouble scale);

gchar             *browser_canvas_serialize_items         (BrowserCanvas *canvas);

void               browser_canvas_item_toggle_select      (BrowserCanvas *canvas, BrowserCanvasItem *item);
void               browser_canvas_translate_item          (BrowserCanvas *canvas, BrowserCanvasItem *item,
							   gdouble dx, gdouble dy);

G_END_DECLS

#endif
