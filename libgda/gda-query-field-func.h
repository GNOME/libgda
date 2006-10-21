/* gda-query-field-func.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_QUERY_FIELD_FUNC_H_
#define __GDA_QUERY_FIELD_FUNC_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"
#include "gda-query-field.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD_FUNC          (gda_query_field_func_get_type())
#define GDA_QUERY_FIELD_FUNC(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_func_get_type(), GdaQueryFieldFunc)
#define GDA_QUERY_FIELD_FUNC_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_func_get_type (), GdaQueryFieldFuncClass)
#define GDA_IS_QUERY_FIELD_FUNC(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_func_get_type ())


/* error reporting */
extern GQuark gda_query_field_func_error_quark (void);
#define GDA_QUERY_FIELD_FUNC_ERROR gda_query_field_func_error_quark ()

typedef enum
{
	GDA_QUERY_FIELD_FUNC_XML_LOAD_ERROR,
	GDA_QUERY_FIELD_FUNC_RENDER_ERROR
} GdaQueryFieldFuncError;


/* struct for the object's data */
struct _GdaQueryFieldFunc
{
	GdaQueryField               object;
	GdaQueryFieldFuncPrivate   *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldFuncClass
{
	GdaQueryFieldClass          parent_class;
};

GType                  gda_query_field_func_get_type          (void);
GdaQueryField         *gda_query_field_func_new               (GdaQuery *query, const gchar *func_name);

GdaDictFunction       *gda_query_field_func_get_ref_func      (GdaQueryFieldFunc *func);
const gchar           *gda_query_field_func_get_ref_func_name (GdaQueryFieldFunc *func);
gboolean               gda_query_field_func_set_args          (GdaQueryFieldFunc *func, GSList *args);
GSList                *gda_query_field_func_get_args          (GdaQueryFieldFunc *func);

G_END_DECLS

#endif
