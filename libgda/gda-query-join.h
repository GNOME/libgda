/* gda-query-join.h
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


#ifndef __GDA_QUERY_JOIN_H_
#define __GDA_QUERY_JOIN_H_

#include <libgda/gda-query-object.h>
#include "gda-decl.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_JOIN          (gda_query_join_get_type())
#define GDA_QUERY_JOIN(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_join_get_type(), GdaQueryJoin)
#define GDA_QUERY_JOIN_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_join_get_type (), GdaQueryJoinClass)
#define GDA_IS_QUERY_JOIN(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_join_get_type ())

/* error reporting */
extern GQuark gda_query_join_error_quark (void);
#define GDA_QUERY_JOIN_ERROR gda_query_join_error_quark ()

/* different possible types for a query */
typedef enum {
        GDA_QUERY_JOIN_TYPE_INNER,
	GDA_QUERY_JOIN_TYPE_LEFT_OUTER,
	GDA_QUERY_JOIN_TYPE_RIGHT_OUTER,
	GDA_QUERY_JOIN_TYPE_FULL_OUTER,
        GDA_QUERY_JOIN_TYPE_CROSS,
        GDA_QUERY_JOIN_TYPE_LAST
} GdaQueryJoinType;

typedef enum
{
	GDA_QUERY_JOIN_XML_LOAD_ERROR,
	GDA_QUERY_JOIN_META_DATA_UPDATE,
	GDA_QUERY_JOIN_FIELDS_ERROR,
	GDA_QUERY_JOIN_SQL_ANALYSE_ERROR,
	GDA_QUERY_JOIN_PARSE_ERROR
} GdaQueryJoinError;


/* struct for the object's data */
struct _GdaQueryJoin
{
	GdaQueryObject             object;
	GdaQueryJoinPrivate       *priv;
};

/* struct for the object's class */
struct _GdaQueryJoinClass
{
	GdaQueryObjectClass        parent_class;

	/* signals */
	void   (*type_changed)         (GdaQueryJoin *join);
	void   (*condition_changed)    (GdaQueryJoin *join);
};

GType              gda_query_join_get_type                  (void);
GdaQueryJoin      *gda_query_join_new_with_targets          (GdaQuery *query, GdaQueryTarget *target_1, GdaQueryTarget *target_2);
GdaQueryJoin      *gda_query_join_new_with_xml_ids          (GdaQuery *query, const gchar *target_1_xml_id, const gchar *target_2_xml_id);
GdaQueryJoin      *gda_query_join_new_copy                  (GdaQueryJoin *orig, GHashTable *replacements);

void               gda_query_join_set_join_type             (GdaQueryJoin *join, GdaQueryJoinType type);
GdaQueryJoinType   gda_query_join_get_join_type             (GdaQueryJoin *join);
GdaQuery          *gda_query_join_get_query                 (GdaQueryJoin *join);

GdaQueryTarget    *gda_query_join_get_target_1              (GdaQueryJoin *join);
GdaQueryTarget    *gda_query_join_get_target_2              (GdaQueryJoin *join);
void               gda_query_join_swap_targets              (GdaQueryJoin *join);

gboolean           gda_query_join_set_condition             (GdaQueryJoin *join, GdaQueryCondition *cond);
GdaQueryCondition *gda_query_join_get_condition             (GdaQueryJoin *join);
gboolean           gda_query_join_set_condition_from_fkcons (GdaQueryJoin *join);
gboolean           gda_query_join_set_condition_from_sql    (GdaQueryJoin *join, const gchar *cond, GError **error);

const gchar       *gda_query_join_render_type               (GdaQueryJoin *join);

G_END_DECLS

#endif
