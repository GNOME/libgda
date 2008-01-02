/* gda-query-field-field.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_QUERY_FIELD_FIELD_H_
#define __GDA_QUERY_FIELD_FIELD_H_

#ifndef GDA_DISABLE_DEPRECATED

#include "gda-decl.h"
#include "gda-query-field.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD_FIELD          (gda_query_field_field_get_type())
#define GDA_QUERY_FIELD_FIELD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_field_get_type(), GdaQueryFieldField)
#define GDA_QUERY_FIELD_FIELD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_field_get_type (), GdaQueryFieldFieldClass)
#define GDA_IS_QUERY_FIELD_FIELD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_field_get_type ())


/* error reporting */
extern GQuark gda_query_field_field_error_quark (void);
#define GDA_QUERY_FIELD_FIELD_ERROR gda_query_field_field_error_quark ()

typedef enum
{
	GDA_QUERY_FIELD_FIELD_XML_LOAD_ERROR,
	GDA_QUERY_FIELD_FIELD_RENDER_ERROR
} GdaQueryFieldFieldError;


/* struct for the object's data */
struct _GdaQueryFieldField
{
	GdaQueryField              object;
	GdaQueryFieldFieldPrivate       *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldFieldClass
{
	GdaQueryFieldClass                  parent_class;
};

GType             gda_query_field_field_get_type            (void) G_GNUC_CONST;
GdaQueryField    *gda_query_field_field_new                 (GdaQuery *query, const gchar *field);

gchar            *gda_query_field_field_get_ref_field_name  (GdaQueryFieldField *field);
GdaEntityField   *gda_query_field_field_get_ref_field       (GdaQueryFieldField *field);
GdaQueryTarget   *gda_query_field_field_get_target          (GdaQueryFieldField *field);

G_END_DECLS

#endif /* GDA_DISABLE_DEPRECATED */

#endif
