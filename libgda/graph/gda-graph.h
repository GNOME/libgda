/* gda-graph.h
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


#ifndef __GDA_GRAPH_H_
#define __GDA_GRAPH_H_

#include <libgda/gda-decl.h>
#include <libgda/gda-object.h>
#include <libgda/gda-enums.h>

G_BEGIN_DECLS

#define GDA_TYPE_GRAPH          (gda_graph_get_type())
#define GDA_GRAPH(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_graph_get_type(), GdaGraph)
#define GDA_GRAPH_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_graph_get_type (), GdaGraphClass)
#define GDA_IS_GRAPH(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_graph_get_type ())

/* error reporting */
extern GQuark gda_graph_error_quark (void);
#define GDA_GRAPH_ERROR gda_graph_error_quark ()

typedef enum
{
	GDA_GRAPH_XML_LOAD_ERROR
} GdaGraphError;

/* struct for the object's data */
struct _GdaGraph
{
	GdaObject         object;
	GdaGraphPrivate  *priv;
};

/* struct for the object's class */
struct _GdaGraphClass
{
	GdaObjectClass   parent_class;
	
	/* signals */
	void        (*item_added)   (GdaGraph *graph, GdaGraphItem *item);
	void        (*item_dropped) (GdaGraph *graph, GdaGraphItem *item);
	void        (*item_moved)   (GdaGraph *graph, GdaGraphItem *item);
};

GType            gda_graph_get_type            (void) G_GNUC_CONST;

GObject         *gda_graph_new                 (GdaDict *dict, GdaGraphType type);
GdaGraphType     gda_graph_get_graph_type      (GdaGraph *graph);

void             gda_graph_add_item            (GdaGraph *graph, GdaGraphItem *item);
void             gda_graph_del_item            (GdaGraph *graph, GdaGraphItem *item);
GdaGraphItem    *gda_graph_get_item_from_obj   (GdaGraph *graph, GdaObject *ref_obj, gboolean create_if_needed);
GSList          *gda_graph_get_items           (GdaGraph *graph);

G_END_DECLS

#endif
