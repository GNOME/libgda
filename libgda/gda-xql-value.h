/* GdaXql: the xml query object
 * Copyright (C) 2000-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Gerhard Dieringer <gdieringer@compuserve.com>
 * 	Cleber Rodrigues <cleber@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_xql_value_h__)
#  define __gda_xql_value_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-bin.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_VALUE	(gda_xql_value_get_type())
#define GDA_XQL_VALUE(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_value_get_type(), GdaXqlValue)
#define GDA_XQL_VALUE_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_value_get_type(), GdaXqlValue const)
#define GDA_XQL_VALUE_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_value_get_type(), GdaXqlValueClass)
#define GDA_IS_XQL_VALUE(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_value_get_type ())

#define GDA_XQL_VALUE_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_value_get_type(), GdaXqlValueClass)

typedef struct _GdaXqlValue GdaXqlValue;
typedef struct _GdaXqlValueClass GdaXqlValueClass;

struct _GdaXqlValue {
	GdaXqlBin xqlbin;
};

struct _GdaXqlValueClass {
	GdaXqlBinClass parent_class;
};

GType	    gda_xql_value_get_type	(void);
GdaXqlItem *gda_xql_value_new	        (void);
GdaXqlItem *gda_xql_value_new_with_data	(gchar *id);

G_END_DECLS

#endif
