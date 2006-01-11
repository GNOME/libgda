/* gda-query-field.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/*
 * This object is the qfield object for most of Mergeant's objects, it provides
 * basic facilities:
 * - a reference to the GdaDict object
 * - a unique id which is used to XML storing procedures
 * - some attributes such as name, description and owner of the object (only used
 *   for DBMS object which are derived from this class.
 */


#ifndef __GDA_QUERY_FIELD_H_
#define __GDA_QUERY_FIELD_H_

#include "gda-decl.h"
#include <libxml/tree.h>
#include <libgda/gda-query-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD          (gda_query_field_get_type())
#define GDA_QUERY_FIELD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_get_type(), GdaQueryField)
#define GDA_QUERY_FIELD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_get_type (), GdaQueryFieldClass)
#define GDA_IS_QUERY_FIELD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_get_type ())


/* error reporting */
extern GQuark gda_query_field_error_quark (void);
#define GDA_QUERY_FIELD_ERROR gda_query_field_error_quark ()

enum
{
	GDA_QUERY_FIELD_XML_LOAD_ERROR
};


/* struct for the object's data */
struct _GdaQueryField
{
	GdaQueryObject              object;
	GdaQueryFieldPrivate       *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldClass
{
	GdaQueryObjectClass         parent_class;

	/* pure virtual functions */
	GObject          *(*copy)           (GdaQueryField *orig);
	GSList           *(*get_params)     (GdaQueryField *qfield);
	gboolean          (*is_equal)       (GdaQueryField *qfield1, GdaQueryField *qfield2);
	gboolean          (*is_list)        (GdaQueryField *qfield);
};

GType             gda_query_field_get_type        (void);
GdaQueryField    *gda_query_field_new_from_xml    (GdaQuery *query, xmlNodePtr node, GError **error);
GdaQueryField    *gda_query_field_new_copy        (GdaQueryField *orig);
GdaQueryField    *gda_query_field_new_from_sql    (GdaQuery *query, const gchar *sqlfield, GError **error);

GdaDictType      *gda_query_field_get_dict_type   (GdaQueryField *qfield);
GSList           *gda_query_field_get_parameters  (GdaQueryField *qfield);
void              gda_query_field_set_alias       (GdaQueryField *qfield, const gchar *alias);
const gchar      *gda_query_field_get_alias       (GdaQueryField *qfield);

void              gda_query_field_set_visible     (GdaQueryField *qfield, gboolean visible);
gboolean          gda_query_field_is_visible      (GdaQueryField *qfield);

void              gda_query_field_set_internal    (GdaQueryField *qfield, gboolean internal);
gboolean          gda_query_field_is_internal     (GdaQueryField *qfield);

gboolean          gda_query_field_is_equal        (GdaQueryField *qfield1, GdaQueryField *qfield2);

gboolean          gda_query_field_is_list         (GdaQueryField *qfield);

G_END_DECLS

#endif
