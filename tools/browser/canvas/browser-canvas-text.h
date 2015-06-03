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

#ifndef __BROWSER_CANVAS_TEXT__
#define __BROWSER_CANVAS_TEXT__

#include "browser-canvas-item.h"

G_BEGIN_DECLS

/*
 * 
 * "Drag item" GooCanvas item: a BrowserCanvasItem item which is used to represent
 * an element being dragged, and destroys itself when the mouse button is released
 *
 */

#define TYPE_BROWSER_CANVAS_TEXT          (browser_canvas_text_get_type())
#define BROWSER_CANVAS_TEXT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_text_get_type(), BrowserCanvasText)
#define BROWSER_CANVAS_TEXT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_text_get_type (), BrowserCanvasTextClass)
#define IS_BROWSER_CANVAS_TEXT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_text_get_type ())


typedef struct _BrowserCanvasText        BrowserCanvasText;
typedef struct _BrowserCanvasTextClass   BrowserCanvasTextClass;
typedef struct _BrowserCanvasTextPrivate BrowserCanvasTextPrivate;


/* struct for the object's data */
struct _BrowserCanvasText
{
	BrowserCanvasItem           object;

	BrowserCanvasTextPrivate *priv;
};

/* struct for the object's class */
struct _BrowserCanvasTextClass
{
	BrowserCanvasItemClass   parent_class;
};

/* generic widget's functions */
GType          browser_canvas_text_get_type      (void) G_GNUC_CONST;
GooCanvasItem* browser_canvas_text_new           (GooCanvasItem *parent,
						   const gchar *txt,     
						   gdouble x,
						   gdouble y,
						   ...);
void           browser_canvas_text_set_highlight (BrowserCanvasText *ct, gboolean highlight);

G_END_DECLS

#endif
