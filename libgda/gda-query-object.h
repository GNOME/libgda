/* gda-query-object.h
 *
 * Copyright (C) 2005 Vivien Malerba
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

#ifndef __GDA_QUERY_OBJECT_H_
#define __GDA_QUERY_OBJECT_H_

#include <glib-object.h>
#include <libgda/gda-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_OBJECT          (gda_query_object_get_type())
#define GDA_QUERY_OBJECT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_object_get_type(), GdaQueryObject)
#define GDA_QUERY_OBJECT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_object_get_type (), GdaQueryObjectClass)
#define GDA_IS_QUERY_OBJECT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_object_get_type ())

typedef struct _GdaQueryObject GdaQueryObject;
typedef struct _GdaQueryObjectClass GdaQueryObjectClass;
typedef struct _GdaQueryObjectPrivate GdaQueryObjectPrivate;

/* struct for the object's data */
struct _GdaQueryObject
{
	GdaObject               object;
	GdaQueryObjectPrivate  *priv;
};

/* struct for the object's class */
struct _GdaQueryObjectClass
{
	GdaObjectClass          parent_class;

	/* signals */
	void                  (*numid_changed)(GdaQueryObject *object);

	/* pure virtual functions */
	void                  (*set_int_id)  (GdaQueryObject *object, guint id);
};

GType        gda_query_object_get_type        (void) G_GNUC_CONST;
void         gda_query_object_set_int_id      (GdaQueryObject *qobj, guint id);
guint        gda_query_object_get_int_id      (GdaQueryObject *qobj);

G_END_DECLS

#endif
