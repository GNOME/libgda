/* browser-canvas-utility.h
 * Copyright (C) 2007 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <goocanvas.h>
#include "browser-canvas-table.h"

G_BEGIN_DECLS

enum {
        CANVAS_SHAPE_EXT_JOIN_OUTER_1  = 1 << 0,
        CANVAS_SHAPE_EXT_JOIN_OUTER_2  = 1 << 1
};

typedef struct {
	gchar         *id;
	GooCanvasItem *item;
	gboolean       _used;
	gboolean       is_new;
} BrowserCanvasCanvasShape;

GSList *browser_canvas_util_compute_anchor_shapes  (GooCanvasItem *parent, GSList *shapes,
						  BrowserCanvasTable *fk_ent, BrowserCanvasTable *ref_pk_ent,
						  guint nb_anchors, guint ext, gboolean with_handle);
GSList *browser_canvas_util_compute_connect_shapes (GooCanvasItem *parent, GSList *shapes,
						  BrowserCanvasTable *ent1, GdaMetaTableColumn *field1,
						  BrowserCanvasTable *ent2, GdaMetaTableColumn *field2, 
						  guint nb_connect, guint ext);

void    browser_canvas_canvas_shapes_dump (GSList *list);
void    browser_canvas_canvas_shapes_remove_all (GSList *list);
GSList *browser_canvas_canvas_shapes_remove_obsolete_shapes (GSList *list);

#define BROWSER_CANVAS_CANVAS_SHAPE(x) ((BrowserCanvasCanvasShape *)(x))

G_END_DECLS
