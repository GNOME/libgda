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

#if !defined(__gda_xql_dual_h__)
#  define __gda_xql_dual_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-item.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_DUAL	(gda_xql_dual_get_type())
#define GDA_XQL_DUAL(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_dual_get_type(), GdaXqlDual)
#define GDA_XQL_DUAL_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_dual_get_type(), GdaXqlDual const)
#define GDA_XQL_DUAL_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_dual_get_type(), GdaXqlDualClass)
#define GDA_IS_XQL_DUAL(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_dual_get_type ())

#define GDA_XQL_DUAL_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_dual_get_type(), GdaXqlDualClass)

typedef struct _GdaXqlDual GdaXqlDual;
typedef struct _GdaXqlDualClass GdaXqlDualClass;
typedef struct _GdaXqlDualPrivate GdaXqlDualPrivate;

struct _GdaXqlDual {
	GdaXqlItem xqldual;
	/*< private >*/
	GdaXqlDualPrivate *priv;
};

struct _GdaXqlDualClass {
	GdaXqlItemClass parent_class;
};

GType	        gda_xql_dual_get_type	(void);
GdaXqlItem     *gda_xql_dual_new	(gchar *tag,
					 gchar *sqlfmt,
					 gchar *sqlop);
GdaXqlItem     *gda_xql_dual_new_with_data     (gchar *tag,
						gchar *sqlfmt,
						gchar *sqlop,
						GdaXqlItem *left,
						GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_get_left	(GdaXqlDual *self);
GdaXqlItem     *gda_xql_dual_get_right	(GdaXqlDual *self);
GdaXqlItem     *gda_xql_dual_new_eq	(void);
GdaXqlItem     *gda_xql_dual_new_eq_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_ne	(void);
GdaXqlItem     *gda_xql_dual_new_ne_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_lt	(void);
GdaXqlItem     *gda_xql_dual_new_lt_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_le	(void);
GdaXqlItem     *gda_xql_dual_new_le_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_gt	(void);
GdaXqlItem     *gda_xql_dual_new_gt_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_ge	(void);
GdaXqlItem     *gda_xql_dual_new_ge_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_like	(void);
GdaXqlItem     *gda_xql_dual_new_like_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_in	(void);
GdaXqlItem     *gda_xql_dual_new_in_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);
GdaXqlItem     *gda_xql_dual_new_set	(void);
GdaXqlItem     *gda_xql_dual_new_set_with_data	(GdaXqlItem *left,
						 GdaXqlItem *right);

G_END_DECLS

#endif
