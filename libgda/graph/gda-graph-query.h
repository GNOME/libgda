/* gda-graph-query.h
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


#ifndef __GDA_GRAPH_QUERY_H_
#define __GDA_GRAPH_QUERY_H_

#include "gda-graph.h"

G_BEGIN_DECLS

#define GDA_TYPE_GRAPH_QUERY          (gda_graph_query_get_type())
#define GDA_GRAPH_QUERY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_graph_query_get_type(), GdaGraphQuery)
#define GDA_GRAPH_QUERY_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_graph_query_get_type (), GdaGraphQueryClass)
#define GDA_IS_GRAPH_QUERY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_graph_query_get_type ())

/* error reporting */
/*
extern GQuark gda_graph_query_error_quark (void);
#define GDA_GRAPH_QUERY_ERROR gda_graph_query_error_quark ()

typedef enum
{
	GDA_GRAPH_QUERY_GENERAL_ERROR
} GdaGraphQueryError;
*/

/* struct for the object's data */
struct _GdaGraphQuery
{
	GdaGraph               object;
	GdaGraphQueryPrivate  *priv;
};

/* struct for the object's class */
struct _GdaGraphQueryClass
{
	GdaGraphClass   parent_class;
};

GType            gda_graph_query_get_type            (void) G_GNUC_CONST;

GObject         *gda_graph_query_new                 (GdaQuery *query);
void             gda_graph_query_sync_targets        (GdaGraphQuery *graph);

G_END_DECLS

#endif
