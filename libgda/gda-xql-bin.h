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

#if !defined(__gda_xql_bin_h__)
#  define __gda_xql_bin_h__

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-xql-item.h>

G_BEGIN_DECLS

#define GDA_TYPE_XQL_BIN	(gda_xql_bin_get_type())
#define GDA_XQL_BIN(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_bin_get_type(), GdaXqlBin)
#define GDA_XQL_BIN_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_bin_get_type(), GdaXqlBin const)
#define GDA_XQL_BIN_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_bin_get_type(), GdaXqlBinClass)
#define GDA_IS_XQL_BIN(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_bin_get_type ())

#define GDA_XQL_BIN_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_bin_get_type(), GdaXqlBinClass)


typedef struct _GdaXqlBin GdaXqlBin;
typedef struct _GdaXqlBinClass GdaXqlBinClass;
typedef struct _GdaXqlBinPrivate GdaXqlBinPrivate;

struct _GdaXqlBin {
	GdaXqlItem xqlitem;

	GdaXqlBinPrivate *priv;
};

struct _GdaXqlBinClass {
	GdaXqlItemClass parent_class;
};

GType	     gda_xql_bin_get_type (void);
GdaXqlItem  *gda_xql_bin_new	  (gchar * tag,
				   gchar * sqlfmt,
				   gchar * sqlop);
GdaXqlItem  *gda_xql_bin_new_with_data (gchar * tag,
					gchar * sqlfmt,
					gchar * sqlop,
					GdaXqlItem * child);
GdaXqlItem  *gda_xql_bin_get_child     (GdaXqlBin * self);
void 	     gda_xql_bin_set_child     (GdaXqlBin * self,
					GdaXqlItem * item);
GdaXqlItem  *gda_xql_bin_new_union     (void);
GdaXqlItem  *gda_xql_bin_new_unionall  (void);
GdaXqlItem  *gda_xql_bin_new_intersect (void);
GdaXqlItem  *gda_xql_bin_new_minus     (void);
GdaXqlItem  *gda_xql_bin_new_where     (void);
GdaXqlItem  *gda_xql_bin_new_where_with_data  (GdaXqlItem * data);
GdaXqlItem  *gda_xql_bin_new_having    (void);
GdaXqlItem  *gda_xql_bin_new_having_with_data (GdaXqlItem * data);
GdaXqlItem  *gda_xql_bin_new_on	       (void);
GdaXqlItem  *gda_xql_bin_new_not       (void);
GdaXqlItem  *gda_xql_bin_new_not_with_data    (GdaXqlItem * data);
GdaXqlItem  *gda_xql_bin_new_exists    (void);
GdaXqlItem  *gda_xql_bin_new_null      (void);
GdaXqlItem  *gda_xql_bin_new_null_with_data   (GdaXqlItem * data);

G_END_DECLS

#endif
