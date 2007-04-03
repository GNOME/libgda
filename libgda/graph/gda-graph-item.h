/* gda-graph-item.h
 *
 * Copyright (C) 2004 - 2005 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GDA_GRAPH_ITEM_H_
#define __GDA_GRAPH_ITEM_H_

#include <libgda/gda-object.h>
#include <libgda/gda-decl.h>

/* Implements the GdaXmlStorage interface */

G_BEGIN_DECLS

#define GDA_TYPE_GRAPH_ITEM          (gda_graph_item_get_type())
#define GDA_GRAPH_ITEM(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_graph_item_get_type(), GdaGraphItem)
#define GDA_GRAPH_ITEM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_graph_item_get_type (), GdaGraphItemClass)
#define GDA_IS_GRAPH_ITEM(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_graph_item_get_type ())

/* error reporting */
extern GQuark gda_graph_item_error_quark (void);
#define GDA_GRAPH_ITEM_ERROR gda_graph_item_error_quark ()

typedef enum
{
	GDA_GRAPH_ITEM_XML_LOAD_ERROR
} GdaGraphItemError;


/* struct for the object's data */
struct _GdaGraphItem
{
	GdaObject               object;
	GdaGraphItemPrivate  *priv;
};

/* struct for the object's class */
struct _GdaGraphItemClass
{
	GdaObjectClass   parent_class;
	
	/* signals */
	void        (*moved) (GdaGraphItem *item);
};

GType            gda_graph_item_get_type            (void);

GObject         *gda_graph_item_new                 (GdaDict *dict, GdaObject *ref_obj);
GdaObject       *gda_graph_item_get_ref_object      (GdaGraphItem *item);
void             gda_graph_item_set_position        (GdaGraphItem *item, gdouble x, gdouble y);
void             gda_graph_item_get_position        (GdaGraphItem *item, gdouble *x, gdouble *y);

G_END_DECLS

#endif
