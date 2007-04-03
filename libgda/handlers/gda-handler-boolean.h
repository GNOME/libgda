/* gda-handler-boolean.h
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

#ifndef __GDA_HANDLER_BOOLEAN__
#define __GDA_HANDLER_BOOLEAN__

#include <libgda/gda-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_HANDLER_BOOLEAN          (gda_handler_boolean_get_type())
#define GDA_HANDLER_BOOLEAN(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_handler_boolean_get_type(), GdaHandlerBoolean)
#define GDA_HANDLER_BOOLEAN_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_handler_boolean_get_type (), GdaHandlerBooleanClass)
#define GDA_IS_HANDLER_BOOLEAN(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_handler_boolean_get_type ())


typedef struct _GdaHandlerBoolean      GdaHandlerBoolean;
typedef struct _GdaHandlerBooleanClass GdaHandlerBooleanClass;
typedef struct _GdaHandlerBooleanPriv  GdaHandlerBooleanPriv;


/* struct for the object's data */
struct _GdaHandlerBoolean
{
	GdaObject                object;

	GdaHandlerBooleanPriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerBooleanClass
{
	GdaObjectClass            parent_class;
};


GType           gda_handler_boolean_get_type      (void);
GdaDataHandler *gda_handler_boolean_new           (void);

G_END_DECLS

#endif
