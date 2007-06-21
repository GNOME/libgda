/* gda-graphviz.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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


#ifndef __GDA_GRAPHVIZ_H_
#define __GDA_GRAPHVIZ_H_

#include <libgda/gda-object.h>

G_BEGIN_DECLS

typedef struct _GdaGraphviz GdaGraphviz;
typedef struct _GdaGraphvizClass GdaGraphvizClass;
typedef struct _GdaGraphvizPrivate GdaGraphvizPrivate;

#define GDA_TYPE_GRAPHVIZ          (gda_graphviz_get_type())
#define GDA_GRAPHVIZ(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_graphviz_get_type(), GdaGraphviz)
#define GDA_GRAPHVIZ_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_graphviz_get_type (), GdaGraphvizClass)
#define GDA_IS_GRAPHVIZ(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_graphviz_get_type ())

/* error reporting */
/*
extern GQuark gda_graphviz_error_quark (void);
#define GDA_GRAPHVIZ_ERROR gda_graphviz_error_quark ()

typedef enum
{
        GDA_GRAPHVIZ_SAVE_ERROR
} GdaGraphvizError;
*/


/* struct for the object's data */
struct _GdaGraphviz
{
	GdaObject                 object;
	GdaGraphvizPrivate       *priv;
};

/* struct for the object's class */
struct _GdaGraphvizClass
{
	GdaObjectClass              parent_class;
};

GType           gda_graphviz_get_type         (void) G_GNUC_CONST;
GObject        *gda_graphviz_new              (GdaDict *dict);

void            gda_graphviz_add_to_graph     (GdaGraphviz *graph, GObject *obj);
gboolean        gda_graphviz_save_file        (GdaGraphviz *graph, const gchar *filename, GError **error);
G_END_DECLS

#endif
