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

#if !defined(__gda_xql_dml_h__)
#  define __gda_xql_dml_h__

#include <glib.h>
#include <glib-object.h>
#include "gda-xql-item.h"

G_BEGIN_DECLS

#define GDA_TYPE_XQL_DML	(gda_xql_dml_get_type())
#define GDA_XQL_DML(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_dml_get_type(), GdaXqlDml)
#define GDA_XQL_DML_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gda_xql_dml_get_type(), GdaXqlDml const)
#define GDA_XQL_DML_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gda_xql_dml_get_type(), GdaXqlDmlClass)
#define GDA_IS_XQL_DML(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gda_xql_dml_get_type ())

#define GDA_XQL_DML_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gda_xql_dml_get_type(), GdaXqlDmlClass)

typedef struct _GdaXqlDml GdaXqlDml;
typedef struct _GdaXqlDmlClass GdaXqlDmlClass;
typedef struct _GdaXqlDmlPrivate GdaXqlDmlPrivate;

struct _GdaXqlDml {
	GdaXqlItem xqldml;
	/*< private >*/
	GdaXqlDmlPrivate *priv;
};
struct _GdaXqlDmlClass {
	GdaXqlItemClass parent_class;
	gchar * (* add_target_from_text) (GdaXqlDml *xqldml, gchar *name, GdaXqlItem *join);
	GdaXqlItem * (* add_field_from_text) (GdaXqlDml *xqldml, gchar *id, gchar *name, gchar *alias, gboolean group);
	GdaXqlItem * (* add_const_from_text) (GdaXqlDml *xqldml, gchar *value, gchar *type, gboolean null);
	void (* add_func) (GdaXqlDml *xqldml, GdaXqlItem *item);
	void (* add_query) (GdaXqlDml *xqldml, GdaXqlItem *item);
	void (* add_row_condition) (GdaXqlDml *xqldml, GdaXqlItem *cond, gchar *type);
	void (* add_group_condition) (GdaXqlDml *xqldml, GdaXqlItem *cond, gchar *type);
	void (* add_order) (GdaXqlDml *xqldml, gint column, gboolean asc);
	void (* add_set) (GdaXqlDml *xqldml, GdaXqlItem *item);
	void (* add_set_const) (GdaXqlDml *xqldml, gchar *field, gchar *value, gchar *type, gboolean null);
};

struct _GdaXqlDmlPrivate {
	GdaXqlItem *target;
	GdaXqlItem *valuelist;
	GdaXqlItem *where;
	GdaXqlItem *having;
	GdaXqlItem *group;
	GdaXqlItem *trailer;
	GdaXqlItem *dest;
	GdaXqlItem *source;
	GdaXqlItem *setlist;
};


GType	     gda_xql_dml_get_type  (void);
gchar       *gda_xql_dml_add_target_from_text (GdaXqlDml *xqldml,
					       gchar *name,
					       GdaXqlItem *join);
GdaXqlItem  *gda_xql_dml_add_field_from_text  (GdaXqlDml *xqldml,
					       gchar *id,
					       gchar *name,
					       gchar *alias,
					       gboolean group);
GdaXqlItem  *gda_xql_dml_add_const_from_text  (GdaXqlDml *xqldml,
					       gchar *value,
					       gchar *type,
					       gboolean null);
void 	gda_xql_dml_add_func            (GdaXqlDml *xqldml, GdaXqlItem *item);
void 	gda_xql_dml_add_query	        (GdaXqlDml *xqldml, GdaXqlItem *item);
void 	gda_xql_dml_add_row_condition	(GdaXqlDml *xqldml, GdaXqlItem *cond, gchar *type);
void 	gda_xql_dml_add_group_condition	(GdaXqlDml *xqldml, GdaXqlItem *cond, gchar *type);
void 	gda_xql_dml_add_order	        (GdaXqlDml *xqldml, gint column, gboolean asc);
void 	gda_xql_dml_add_set	        (GdaXqlDml *xqldml, GdaXqlItem *item);
void 	gda_xql_dml_add_set_const	(GdaXqlDml *xqldml, gchar *field, 
					 gchar * value, gchar * type, gboolean null);

G_END_DECLS

#endif
