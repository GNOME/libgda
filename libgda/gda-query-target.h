/* gda-query-target.h
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


#ifndef __GDA_QUERY_TARGET_H_
#define __GDA_QUERY_TARGET_H_

#include <libgda/gda-query-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_TARGET          (gda_query_target_get_type())
#define GDA_QUERY_TARGET(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_target_get_type(), GdaQueryTarget)
#define GDA_QUERY_TARGET_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_target_get_type (), GdaQueryTargetClass)
#define GDA_IS_QUERY_TARGET(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_target_get_type ())


/* error reporting */
extern GQuark gda_query_target_error_quark (void);
#define GDA_QUERY_TARGET_ERROR gda_query_target_error_quark ()

typedef enum
{
	GDA_QUERY_TARGET_XML_LOAD_ERROR,
	GDA_QUERY_TARGET_XML_SAVE_ERROR,
	GDA_QUERY_TARGET_META_DATA_UPDATE,
	GDA_QUERY_TARGET_FIELDS_ERROR
} GdaQueryTargetError;


/* struct for the object's data */
struct _GdaQueryTarget
{
	GdaQueryObject          object;
	GdaQueryTargetPrivate  *priv;
};

/* struct for the object's class */
struct _GdaQueryTargetClass
{
	GdaQueryObjectClass     parent_class;
};

GType           gda_query_target_get_type               (void);

GdaQueryTarget *gda_query_target_new                    (GdaQuery *query, const gchar *table);
GdaQueryTarget *gda_query_target_new_copy               (GdaQueryTarget *orig);

GdaQuery       *gda_query_target_get_query              (GdaQueryTarget *target);
GdaEntity      *gda_query_target_get_represented_entity (GdaQueryTarget *target);
const gchar    *gda_query_target_get_represented_table_name (GdaQueryTarget *target);

void            gda_query_target_set_alias              (GdaQueryTarget *target, const gchar *alias);
const gchar    *gda_query_target_get_alias              (GdaQueryTarget *target);
gchar          *gda_query_target_get_complete_name      (GdaQueryTarget *target);

G_END_DECLS

#endif
