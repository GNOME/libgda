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

#if !defined(__gda_xql_const_h__)
#  define __gda_xql_const_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-atom.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_CONST	(gda_xql_const_get_type())
#define GDA_XQL_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_const_get_type(), GdaXqlConst)
#define GDA_XQL_CONST_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_const_get_type(), GdaXqlConst const)
#define GDA_XQL_CONST_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_const_get_type(), GdaXqlConstClass)
#define GDA_IS_XQL_CONST(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_const_get_type ())

#define GDA_XQL_CONST_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_const_get_type(), GdaXqlConstClass)

typedef struct _GdaXqlConst GdaXqlConst;
typedef struct _GdaXqlConstClass GdaXqlConstClass;

struct _GdaXqlConst {
	GdaXqlAtom xqlatom;
};

struct _GdaXqlConstClass {
	GdaXqlAtomClass parent_class;
};

GType	      gda_xql_const_get_type	(void);
GdaXqlItem   *gda_xql_const_new	        (void);
GdaXqlItem   *gda_xql_const_new_with_data (gchar * value,
					   gchar * alias,
					   gchar * type,
					   gchar * null);

G_END_DECLS

#endif
