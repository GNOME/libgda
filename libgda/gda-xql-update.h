/* GdaXql: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 * deGOBified by Cleber Rodrigues (cleber@gnome-db.org)
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

#if !defined(__gda_xql_update_h__)
#  define __gda_xql_update_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-dml.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_UPDATE	(gda_xql_update_get_type())
#define GDA_XQL_UPDATE(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_update_get_type(), GdaXqlUpdate)
#define GDA_XQL_UPDATE_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_update_get_type(), GdaXqlUpdate const)
#define GDA_XQL_UPDATE_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_update_get_type(), GdaXqlUpdateClass)
#define GDA_IS_XQL_UPDATE(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_update_get_type ())

#define GDA_XQL_UPDATE_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_update_get_type(), GdaXqlUpdateClass)

typedef struct _GdaXqlUpdate GdaXqlUpdate;
typedef struct _GdaXqlUpdateClass GdaXqlUpdateClass;

struct _GdaXqlUpdate {
	GdaXqlDml *xqldml;
};


struct _GdaXqlUpdateClass {
	GdaXqlDmlClass parent_class;
};


GType	    gda_xql_update_get_type  (void);
GdaXqlItem *gda_xql_update_new	     (void);

G_END_DECLS

#endif
