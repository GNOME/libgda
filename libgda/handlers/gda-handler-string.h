/* gda-handler-string.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GDA_HANDLER_STRING__
#define __GDA_HANDLER_STRING__

#include <libgda/gda-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_HANDLER_STRING          (gda_handler_string_get_type())
#define GDA_HANDLER_STRING(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_handler_string_get_type(), GdaHandlerString)
#define GDA_HANDLER_STRING_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_handler_string_get_type (), GdaHandlerStringClass)
#define GDA_IS_HANDLER_STRING(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_handler_string_get_type ())


typedef struct _GdaHandlerString      GdaHandlerString;
typedef struct _GdaHandlerStringClass GdaHandlerStringClass;
typedef struct _GdaHandlerStringPriv  GdaHandlerStringPriv;


/* struct for the object's data */
struct _GdaHandlerString
{
	GdaObject                object;

	GdaHandlerStringPriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerStringClass
{
	GdaObjectClass            parent_class;
};


GType           gda_handler_string_get_type      (void);
GdaDataHandler *gda_handler_string_new           (void);

G_END_DECLS

#endif
