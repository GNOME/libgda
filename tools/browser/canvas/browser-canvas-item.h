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

#ifndef __BROWSER_CANVAS_ITEM__
#define __BROWSER_CANVAS_ITEM__

#include <goocanvas.h>
#include "browser-canvas-decl.h"
#include <libxml/tree.h>

G_BEGIN_DECLS

#define TYPE_BROWSER_CANVAS_ITEM          (browser_canvas_item_get_type())
#define BROWSER_CANVAS_ITEM(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_item_get_type(), BrowserCanvasItem)
#define BROWSER_CANVAS_ITEM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_item_get_type (), BrowserCanvasItemClass)
#define IS_BROWSER_CANVAS_ITEM(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_item_get_type ())

/* struct for the object's data */
struct _BrowserCanvasItem
{
	GooCanvasGroup         object;

	BrowserCanvasItemPrivate *priv;
};

/* struct for the object's class */
struct _BrowserCanvasItemClass
{
	GooCanvasGroupClass    parent_class;

	/* signals */
	void (*moved)        (BrowserCanvasItem *citem);
	void (*moving)       (BrowserCanvasItem *citem);

	/* virtual functions */
	void (*extra_event)  (BrowserCanvasItem *citem, GdkEventType event_type);
	void (*get_edge_nodes)(BrowserCanvasItem *citem, BrowserCanvasItem **from, BrowserCanvasItem **to);
	void (*drag_data_get) (BrowserCanvasItem *citem, GdkDragContext *drag_context,
			       GtkSelectionData *data, guint info, guint time);
	void (*set_selected)  (BrowserCanvasItem *citem, gboolean selected);

	/* serialization and de-serialization virtual methods (don't need to be implemented) */
	xmlNodePtr (*serialize) (BrowserCanvasItem *citem);
};

GType              browser_canvas_item_get_type       (void) G_GNUC_CONST;

void               browser_canvas_item_get_edge_nodes (BrowserCanvasItem *item, 
						       BrowserCanvasItem **from, BrowserCanvasItem **to);
void               browser_canvas_item_translate      (BrowserCanvasItem *item, gdouble dx, gdouble dy);
BrowserCanvas     *browser_canvas_item_get_canvas     (BrowserCanvasItem *item);

G_END_DECLS

#endif
