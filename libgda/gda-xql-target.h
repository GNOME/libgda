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

#if !defined(__gda_xql_target_h__)
#  define __gda_xql_target_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-bin.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_TARGET	(gda_xql_target_get_type())
#define GDA_XQL_TARGET(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_target_get_type(), GdaXqlTarget)
#define GDA_XQL_TARGET_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_target_get_type(), GdaXqlTarget const)
#define GDA_XQL_TARGET_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_target_get_type(), GdaXqlTargetClass)
#define GDA_IS_XQL_TARGET(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_target_get_type ())

#define GDA_XQL_TARGET_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_target_get_type(), GdaXqlTargetClass)

typedef struct _GdaXqlTarget GdaXqlTarget;
typedef struct _GdaXqlTargetClass GdaXqlTargetClass;

struct _GdaXqlTarget {
	GdaXqlBin xqlbin;
};

struct _GdaXqlTargetClass {
	GdaXqlBinClass parent_class;
};


GType	    gda_xql_target_get_type	 (void);
GdaXqlItem *gda_xql_target_new	         (void);
GdaXqlItem *gda_xql_target_new_with_data (gchar * id,
					  gchar * name,
					  GdaXqlItem * join);

G_END_DECLS

#endif
