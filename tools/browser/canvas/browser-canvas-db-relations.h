/* browser-canvas-db-relations.h
 *
 * Copyright (C) 2004 - 2009 Vivien Malerba
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

#ifndef __BROWSER_CANVAS_DB_RELATIONS__
#define __BROWSER_CANVAS_DB_RELATIONS__

#include "browser-canvas.h"

G_BEGIN_DECLS

#define TYPE_BROWSER_CANVAS_DB_RELATIONS          (browser_canvas_db_relations_get_type())
#define BROWSER_CANVAS_DB_RELATIONS(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_canvas_db_relations_get_type(), BrowserCanvasDbRelations)
#define BROWSER_CANVAS_DB_RELATIONS_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_canvas_db_relations_get_type (), BrowserCanvasDbRelationsClass)
#define IS_BROWSER_CANVAS_DB_RELATIONS(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_canvas_db_relations_get_type ())


typedef struct _BrowserCanvasDbRelations        BrowserCanvasDbRelations;
typedef struct _BrowserCanvasDbRelationsClass   BrowserCanvasDbRelationsClass;
typedef struct _BrowserCanvasDbRelationsPrivate BrowserCanvasDbRelationsPrivate;


/* struct for the object's data */
struct _BrowserCanvasDbRelations
{
	BrowserCanvas                       widget;

	BrowserCanvasDbRelationsPrivate    *priv;
};

/* struct for the object's class */
struct _BrowserCanvasDbRelationsClass
{
	BrowserCanvasClass                  parent_class;
};

/* generic widget's functions */
GType            browser_canvas_db_relations_get_type        (void) G_GNUC_CONST;

GtkWidget       *browser_canvas_db_relations_new             (GdaMetaStruct *mstruct);

BrowserCanvasTable *browser_canvas_db_relations_get_table_item  (BrowserCanvasDbRelations *canvas, GdaMetaTable *table);
BrowserCanvasTable *browser_canvas_db_relations_add_table  (BrowserCanvasDbRelations *canvas, 
							    const GValue *table_catalog,
							    const GValue *table_schema,
							    const GValue *table_name);
void                browser_canvas_db_relations_select_table (BrowserCanvasDbRelations *canvas,
							      BrowserCanvasTable *table);

G_END_DECLS

#endif
