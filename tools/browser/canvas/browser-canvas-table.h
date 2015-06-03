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

#ifndef __BROWSER_CANVAS_TABLE__
#define __BROWSER_CANVAS_TABLE__

#include "browser-canvas-item.h"
#include "browser-canvas-decl.h"
#include <libgda/gda-meta-struct.h>

G_BEGIN_DECLS

/*
 * 
 * "Drag item" GooCanvas item: a BrowserCanvasItem item which is used to represent
 * an element being dragged, and destroys itself when the mouse button is released
 *
 */

#define TYPE_BROWSER_CANVAS_TABLE          (browser_canvas_table_get_type())
#define BROWSER_CANVAS_TABLE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_table_get_type(), BrowserCanvasTable)
#define BROWSER_CANVAS_TABLE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_table_get_type (), BrowserCanvasTableClass)
#define IS_BROWSER_CANVAS_TABLE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_table_get_type ())


/* struct for the object's data */
struct _BrowserCanvasTable
{
	BrowserCanvasItem           object;

	BrowserCanvasTablePrivate *priv;
};

/* struct for the object's class */
struct _BrowserCanvasTableClass
{
	BrowserCanvasItemClass   parent_class;
};

/* generic widget's functions */
GType                browser_canvas_table_get_type        (void) G_GNUC_CONST;
GooCanvasItem       *browser_canvas_table_new             (GooCanvasItem *parent, 
							   GdaMetaStruct *mstruct, GdaMetaTable *table, 
							   gdouble x, gdouble y, ...);
BrowserCanvasColumn *browser_canvas_table_get_column_item (BrowserCanvasTable *ce, GdaMetaTableColumn *column);
gdouble              browser_canvas_table_get_column_ypos (BrowserCanvasTable *ce, GdaMetaTableColumn *column);
void                 browser_canvas_table_get_anchor_bounds (BrowserCanvasTable *ce, GooCanvasBounds *bounds);

G_END_DECLS

#endif
