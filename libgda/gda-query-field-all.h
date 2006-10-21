/* gda-query-field-all.h
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


#ifndef __GDA_QUERY_FIELD_ALL_H_
#define __GDA_QUERY_FIELD_ALL_H_

#include "gda-decl.h"
#include "gda-query-field.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD_ALL          (gda_query_field_all_get_type())
#define GDA_QUERY_FIELD_ALL(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_all_get_type(), GdaQueryFieldAll)
#define GDA_QUERY_FIELD_ALL_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_all_get_type (), GdaQueryFieldAllClass)
#define GDA_IS_QUERY_FIELD_ALL(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_all_get_type ())


/* error reporting */
extern GQuark gda_query_field_all_error_quark (void);
#define GDA_QUERY_FIELD_ALL_ERROR gda_query_field_all_error_quark ()

typedef enum
{
	GDA_QUERY_FIELD_ALL_XML_LOAD_ERROR,
	GDA_QUERY_FIELD_ALL_RENDER_ERROR
} GdaQueryFieldAllError;


/* struct for the object's data */
struct _GdaQueryFieldAll
{
	GdaQueryField              object;
	GdaQueryFieldAllPrivate   *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldAllClass
{
	GdaQueryFieldClass         parent_class;
};

GType           gda_query_field_all_get_type             (void);
GdaQueryField  *gda_query_field_all_new                  (GdaQuery *query, const gchar *target);

GdaQueryTarget *gda_query_field_all_get_target           (GdaQueryFieldAll *field);

G_END_DECLS

#endif
