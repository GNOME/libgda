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

#if !defined(__gda_xql_dml_h__)
#  define __gda_xql_dml_h__

#include <gda-xml-item.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GDA_TYPE_XQL_DML            (gda_xql_dml_get_type ())
#define GDA_XQL_DML(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_XQL_DML, GdaXqlDml)
#define GDA_XQL_DML_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_XQL_DML, GdaXqlDmlClass)
#define GDA_IS_XQL_DML(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_XMLATOM)
#define GDA_IS_XQL_DML_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_XQL_DML))

typedef struct _GdaXqlDml        GdaXqlDml;
typedef struct _GdaXqlDmlClass   GdaXqlDmlClass;
typedef struct _GdaXqlDmlPrivate GdaXqlDmlPrivate;

typedef struct _GdaXqlDml {
	GdaXmlItem item;
	GdaXqlDmlPrivate *priv;
};

typedef struct _GdaXqlDmlClass {
	GdaXmlItemClass parent_class;

	/* virtual methods */
	gchar *      (* add_target_from_text) (GdaXqlDml *dml, const gchar *name, GdaXmlItem *join);
	GdaXmlItem * (* add_field_from_text) (GdaXqlDml *dml, const gchar *id,
					      const gchar *name, const gchar *alias,
					      gboolean group);
	GdaXmlItem * (* add_const_from_text) (GdaXqlDml *dml, const gchar *value,
					      const gchar *type, gboolean null);
	void         (* add_func) (GdaXqlDml *dml, GdaXmlItem *item);
	void         (* add_query) (GdaXqlDml *dml, GdaXmlItem *item);
	void         (* add_row_condition) (GdaXqlDml *dml, GdaXmlItem *cond, const gchar *type);
	void         (* add_group_condition) (GdaXqlDml *dml, GdaXmlItem *cond, const gchar *type);
	void         (* add_order) (GdaXqlDml *dml, gint column, gboolean asc);
	void         (* add_set) (GdaXqlDml *dml, GdaXmlItem *item);
	void         (* add_set_const) (GdaXqlDml *dml, const gchar *filed,
					const gchar *value, const gchar *type,
					gboolean null);
};

GtkType     gda_xql_dml_get_type (void);

gchar      *gda_xql_dml_add_target_from_text (GdaXqlDml *dml,
					      const gchar *name,
					      GdaXmlItem *join);
GdaXmlItem *gda_xql_dml_add_field_from_text (GdaXqlDml *dml, const gchar *id,
					     const gchar *name, const gchar *alias,
					     gboolean group);
GdaXmlItem *gda_xql_dml_add_const_from_text (GdaXqlDml *dml, const gchar *value,
					     const gchar *type, gboolean null);
void        gda_xql_dml_add_func (GdaXqlDml *dml, GdaXmlItem *item);
void        gda_xql_dml_add_query (GdaXqlDml *dml, GdaXmlItem *item);
void        gda_xql_dml_add_row_condition (GdaXqlDml *dml, GdaXmlItem *cond, const gchar *type);
void        gda_xql_dml_add_group_condition (GdaXqlDml *dml, GdaXmlItem *cond, const gchar *type);
void        gda_xql_dml_add_order (GdaXqlDml *dml, gint column, gboolean asc);
void        gda_xql_dml_add_set (GdaXqlDml *dml, GdaXmlItem *item);
void        gda_xql_dml_add_set_const (GdaXqlDml *dml, const gchar *field,
				       const gchar *value, const gchar *type,
				       gboolean null);

#ifdef __cplusplus
}
#endif

#endif
