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

#if !defined(__gda_xql_field_h__)
#  define __gda_xql_field_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-atom.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_FIELD	(gda_xql_field_get_type())
#define GDA_XQL_FIELD(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_field_get_type(), GdaXqlField)
#define GDA_XQL_FIELD_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_field_get_type(), GdaXqlField const)
#define GDA_XQL_FIELD_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_field_get_type(), GdaXqlFieldClass)
#define GDA_IS_XQL_FIELD(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_field_get_type ())

#define GDA_XQL_FIELD_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_field_get_type(), GdaXqlFieldClass)

typedef struct _GdaXqlField GdaXqlField;
typedef struct _GdaXqlFieldClass GdaXqlFieldClass;

struct _GdaXqlField {
	GdaXqlAtom xqlatom;
};


struct _GdaXqlFieldClass {
	GdaXqlAtomClass parent_class;
};


GType	gda_xql_field_get_type	(void) G_GNUC_CONST;
GdaXqlItem * 	gda_xql_field_new	(void);
GdaXqlItem * 	gda_xql_field_new_with_data	(gchar * source,
					gchar * name,
					gchar * alias);

G_BEGIN_DECLS

#endif
