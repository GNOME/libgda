/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Gerhard Dieringer <gdieringer@compuserve.com>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_xql_column_h__)
#  define __gda_xql_column_h__

#include <gda-xml-atom-item.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define GDA_TYPE_XQL_COLUMN            (gda_xql_column_get_type ())
#define GDA_XQL_COLUMN(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XQL_COLUMN, GdaXqlColumn)
#define GDA_XQL_COLUMN_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XQL_COLUMN, GdaXqlColumnClass)
#define GDA_IS_XQL_COLUMN(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XMLATOM)
#define GDA_IS_XQL_COLUMN_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XQL_COLUMN))

typedef struct _GdaXqlColumn      GdaXqlColumn;
typedef struct _GdaXqlColumnClass GdaXqlColumnClass;

typedef struct _GdaXqlColumn {
	GdaXmlAtomItem item;
};

typedef struct _GdaXqlColumnClass {
	GdaXmlAtomItemClass parent_class;
};

GtkType     gda_xql_column_get_type      (void);
GdaXmlItem* gda_xql_column_new           (void);
GdaXmlItem* gda_xql_column_new_with_data (gint num, gboolean asc);

#if defined(__cplusplus)
}
#endif

#endif
