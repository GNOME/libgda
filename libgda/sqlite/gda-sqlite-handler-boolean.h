/* gda-sqlite-handler-boolean.h
 *
 * Copyright (C) 2010 Vivien Malerba
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

#ifndef __GDA_SQLITE_HANDLER_BOOLEAN__
#define __GDA_SQLITE_HANDLER_BOOLEAN__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_HANDLER_BOOLEAN          (_gda_sqlite_handler_boolean_get_type())
#define GDA_SQLITE_HANDLER_BOOLEAN(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, _gda_sqlite_handler_boolean_get_type(), GdaSqliteHandlerBoolean)
#define GDA_SQLITE_HANDLER_BOOLEAN_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, _gda_sqlite_handler_boolean_get_type (), GdaSqliteHandlerBooleanClass)
#define GDA_IS_SQLITE_HANDLER_BOOLEAN(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, _gda_sqlite_handler_boolean_get_type ())

typedef struct _GdaSqliteHandlerBoolean      GdaSqliteHandlerBoolean;
typedef struct _GdaSqliteHandlerBooleanClass GdaSqliteHandlerBooleanClass;
typedef struct _GdaSqliteHandlerBooleanPriv  GdaSqliteHandlerBooleanPriv;

/* struct for the object's data */
struct _GdaSqliteHandlerBoolean
{
	GObject                 object;
	GdaSqliteHandlerBooleanPriv  *priv;
};

/* struct for the object's class */
struct _GdaSqliteHandlerBooleanClass
{
	GObjectClass           parent_class;
};


GType           _gda_sqlite_handler_boolean_get_type      (void) G_GNUC_CONST;
GdaDataHandler *_gda_sqlite_handler_boolean_new           (void);

G_END_DECLS

#endif
