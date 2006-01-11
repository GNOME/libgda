/* gda-query-field-agg.h
 *
 * Copyright (C) 2005 Vivien Malerba
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


#ifndef __GDA_QUERY_FIELD_AGG_H_
#define __GDA_QUERY_FIELD_AGG_H_

#include "gda-decl.h"
#include "gda-query-field.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_FIELD_AGG          (gda_query_field_agg_get_type())
#define GDA_QUERY_FIELD_AGG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_field_agg_get_type(), GdaQueryFieldAgg)
#define GDA_QUERY_FIELD_AGG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_field_agg_get_type (), GdaQueryFieldAggClass)
#define GDA_IS_QUERY_FIELD_AGG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_field_agg_get_type ())


/* error reporting */
extern GQuark gda_query_field_agg_error_quark (void);
#define GDA_QUERY_FIELD_AGG_ERROR gda_query_field_agg_error_quark ()

enum
{
	GDA_QUERY_FIELD_AGG_XML_LOAD_ERROR,
	GDA_QUERY_FIELD_AGG_RENDER_ERROR
};


/* struct for the object's data */
struct _GdaQueryFieldAgg
{
	GdaQueryField                  object;
	GdaQueryFieldAggPrivate       *priv;
};

/* struct for the object's class */
struct _GdaQueryFieldAggClass
{
	GdaQueryFieldClass             parent_class;
};

GType                   gda_query_field_agg_get_type         (void);
GObject                *gda_query_field_agg_new              (GdaQuery *query, const gchar *agg_name);

GdaDictAggregate       *gda_query_field_agg_get_ref_agg      (GdaQueryFieldAgg *agg);
gboolean                gda_query_field_agg_set_arg          (GdaQueryFieldAgg *agg, GdaQueryField *arg);
GdaQueryField          *gda_query_field_agg_get_arg          (GdaQueryFieldAgg *agg);

G_END_DECLS

#endif
