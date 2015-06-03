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

#ifndef __BROWSER_CANVAS_FKEY__
#define __BROWSER_CANVAS_FKEY__

#include "browser-canvas-item.h"

G_BEGIN_DECLS

/*
 * 
 * "Drag item" GooCanvas item: a BrowserCanvasItem item which is used to represent
 * an element being dragged, and destroys itself when the mouse button is released
 *
 */

#define TYPE_BROWSER_CANVAS_FKEY          (browser_canvas_fkey_get_type())
#define BROWSER_CANVAS_FKEY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_fkey_get_type(), BrowserCanvasFkey)
#define BROWSER_CANVAS_FKEY_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_fkey_get_type (), BrowserCanvasFkeyClass)
#define IS_BROWSER_CANVAS_FKEY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_fkey_get_type ())


typedef struct _BrowserCanvasFkey        BrowserCanvasFkey;
typedef struct _BrowserCanvasFkeyClass   BrowserCanvasFkeyClass;
typedef struct _BrowserCanvasFkeyPrivate BrowserCanvasFkeyPrivate;


/* struct for the object's data */
struct _BrowserCanvasFkey
{
	BrowserCanvasItem         object;
	BrowserCanvasFkeyPrivate *priv;
};

/* struct for the object's class */
struct _BrowserCanvasFkeyClass
{
	BrowserCanvasItemClass    parent_class;
};

GType          browser_canvas_fkey_get_type       (void) G_GNUC_CONST;
GooCanvasItem *browser_canvas_fkey_new            (GooCanvasItem *parent,
						   GdaMetaStruct *mstruct, GdaMetaTableForeignKey *fk, ...);

G_END_DECLS

#endif
