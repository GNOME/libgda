/* gda-query-field-value.h
 *
 * Copyright (C) 2003 - 2007 Vivien Malerba
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


#ifndef __GDA_QUERY_FIELD_VALUE_H_
#define __GDA_QUERY_FIELD_VALUE_H_

#include "gda-query-field.h"
#include "gda-value.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD_VALUE          (gda_query_field_value_get_type())
#define GDA_QUERY_FIELD_VALUE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_value_get_type(), GdaQueryFieldValue)
#define GDA_QUERY_FIELD_VALUE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_value_get_type (), GdaQueryFieldValueClass)
#define GDA_IS_QUERY_FIELD_VALUE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_value_get_type ())


/* error reporting */
extern GQuark gda_query_field_value_error_quark (void);
#define GDA_QUERY_FIELD_VALUE_ERROR gda_query_field_value_error_quark ()

typedef enum
{
	GDA_QUERY_FIELD_VALUE_XML_LOAD_ERROR,
	GDA_QUERY_FIELD_VALUE_RENDER_ERROR,
	GDA_QUERY_FIELD_VALUE_PARAM_ERROR,
	GDA_QUERY_FIELD_VALUE_DEFAULT_PARAM_ERROR
} GdaQueryFieldValueError;


/* struct for the object's data */
struct _GdaQueryFieldValue
{
	GdaQueryField              object;
	GdaQueryFieldValuePrivate     *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldValueClass
{
	GdaQueryFieldClass                  parent_class;
};

GType             gda_query_field_value_get_type            (void);
GdaQueryField    *gda_query_field_value_new                 (GdaQuery *query, GType type);

void              gda_query_field_value_set_value           (GdaQueryFieldValue *field, const GValue *val);
const GValue     *gda_query_field_value_get_value           (GdaQueryFieldValue *field);
void              gda_query_field_value_set_default_value   (GdaQueryFieldValue *field, const GValue *default_val);
const GValue     *gda_query_field_value_get_default_value   (GdaQueryFieldValue *field);

void              gda_query_field_value_set_is_parameter    (GdaQueryFieldValue *field, gboolean is_param);
gboolean          gda_query_field_value_get_is_parameter    (GdaQueryFieldValue *field);
gint              gda_query_field_value_get_parameter_index (GdaQueryFieldValue *field);
void              gda_query_field_value_set_not_null        (GdaQueryFieldValue *field, gboolean not_null);
gboolean          gda_query_field_value_get_not_null        (GdaQueryFieldValue *field);
gboolean          gda_query_field_value_is_value_null       (GdaQueryFieldValue *field, GdaParameterList *context);

gboolean          gda_query_field_value_restrict            (GdaQueryFieldValue *field, 
							     GdaDataModel *model, gint col, GError **error);

G_END_DECLS

#endif
