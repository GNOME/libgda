/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_MYSQL_HANDLER_BOOLEAN__
#define __GDA_MYSQL_HANDLER_BOOLEAN__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_MYSQL_HANDLER_BOOLEAN          (gda_mysql_handler_boolean_get_type())
#define GDA_MYSQL_HANDLER_BOOLEAN(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_mysql_handler_boolean_get_type(), GdaMysqlHandlerBoolean)
#define GDA_MYSQL_HANDLER_BOOLEAN_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_mysql_handler_boolean_get_type (), GdaMysqlHandlerBooleanClass)
#define GDA_IS_MYSQL_HANDLER_BOOLEAN(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_mysql_handler_boolean_get_type ())

typedef struct _GdaMysqlHandlerBoolean      GdaMysqlHandlerBoolean;
typedef struct _GdaMysqlHandlerBooleanClass GdaMysqlHandlerBooleanClass;
typedef struct _GdaMysqlHandlerBooleanPriv  GdaMysqlHandlerBooleanPriv;

/* struct for the object's data */
struct _GdaMysqlHandlerBoolean
{
	GObject                 object;
	GdaMysqlHandlerBooleanPriv  *priv;
};

/* struct for the object's class */
struct _GdaMysqlHandlerBooleanClass
{
	GObjectClass           parent_class;
};


GType           gda_mysql_handler_boolean_get_type      (void) G_GNUC_CONST;
GdaDataHandler *gda_mysql_handler_boolean_new           (void);

G_END_DECLS

#endif
