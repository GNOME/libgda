/* gda-query-condition.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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


#ifndef __GDA_QUERY_CONDITION_H_
#define __GDA_QUERY_CONDITION_H_

#include <libgda/gda-query-object.h>
#include "gda-decl.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_QUERY_CONDITION          (gda_query_condition_get_type())
#define GDA_QUERY_CONDITION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_condition_get_type(), GdaQueryCondition)
#define GDA_QUERY_CONDITION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_condition_get_type (), GdaQueryConditionClass)
#define GDA_IS_QUERY_CONDITION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_condition_get_type ())

/* error reporting */
extern GQuark gda_query_condition_error_quark (void);
#define GDA_QUERY_CONDITION_ERROR gda_query_condition_error_quark ()

/* different possible types for a query */
typedef enum {
        GDA_QUERY_CONDITION_NODE_AND,
        GDA_QUERY_CONDITION_NODE_OR,
        GDA_QUERY_CONDITION_NODE_NOT,
	GDA_QUERY_CONDITION_LEAF_EQUAL,
        GDA_QUERY_CONDITION_LEAF_DIFF,
        GDA_QUERY_CONDITION_LEAF_SUP,
        GDA_QUERY_CONDITION_LEAF_SUPEQUAL,
        GDA_QUERY_CONDITION_LEAF_INF,
        GDA_QUERY_CONDITION_LEAF_INFEQUAL,
        GDA_QUERY_CONDITION_LEAF_LIKE,
	GDA_QUERY_CONDITION_LEAF_SIMILAR,
        GDA_QUERY_CONDITION_LEAF_REGEX,
        GDA_QUERY_CONDITION_LEAF_REGEX_NOCASE,
        GDA_QUERY_CONDITION_LEAF_NOT_REGEX,
        GDA_QUERY_CONDITION_LEAF_NOT_REGEX_NOCASE,
        GDA_QUERY_CONDITION_LEAF_IN,
        GDA_QUERY_CONDITION_LEAF_BETWEEN,
        GDA_QUERY_CONDITION_TYPE_UNKNOWN
} GdaQueryConditionType;

typedef enum {
	GDA_QUERY_CONDITION_OP_LEFT   = 0,
	GDA_QUERY_CONDITION_OP_RIGHT  = 1,
	GDA_QUERY_CONDITION_OP_RIGHT2 = 2
} GdaQueryConditionOperator;

typedef enum
{
	GDA_QUERY_CONDITION_XML_LOAD_ERROR,
	GDA_QUERY_CONDITION_RENDERER_ERROR,
	GDA_QUERY_CONDITION_PARENT_ERROR
} GdaQueryConditionError;


/* struct for the object's data */
struct _GdaQueryCondition
{
	GdaQueryObject             object;
	GdaQueryConditionPrivate  *priv;
};

/* struct for the object's class */
struct _GdaQueryConditionClass
{
	GdaQueryObjectClass        parent_class;
};

GType                 gda_query_condition_get_type                (void);
GdaQueryCondition    *gda_query_condition_new                     (GdaQuery *query, GdaQueryConditionType type);
GdaQueryCondition    *gda_query_condition_new_copy                (GdaQueryCondition *orig, GHashTable *replacements);
GdaQueryCondition    *gda_query_condition_new_from_sql            (GdaQuery *query, const gchar *sql_cond, 
								   GSList **targets, GError **error);

void                  gda_query_condition_set_cond_type           (GdaQueryCondition *condition, GdaQueryConditionType type);
GdaQueryConditionType gda_query_condition_get_cond_type           (GdaQueryCondition *condition);

GSList               *gda_query_condition_get_children            (GdaQueryCondition *condition);
GdaQueryCondition    *gda_query_condition_get_parent              (GdaQueryCondition *condition);
GdaQueryCondition    *gda_query_condition_get_child_by_xml_id     (GdaQueryCondition *condition, const gchar *xml_id);
gboolean              gda_query_condition_is_ancestor             (GdaQueryCondition *condition, GdaQueryCondition *ancestor);
gboolean              gda_query_condition_is_leaf                 (GdaQueryCondition *condition);

/* Node conditions */    
gboolean              gda_query_condition_node_add_child          (GdaQueryCondition *condition, 
								   GdaQueryCondition *child, GError **error);
void                  gda_query_condition_node_del_child          (GdaQueryCondition *condition, GdaQueryCondition *child);


/* Leaf conditions */
void                  gda_query_condition_leaf_set_operator      (GdaQueryCondition *condition, 
								  GdaQueryConditionOperator op, GdaQueryField *field);
GdaQueryField        *gda_query_condition_leaf_get_operator      (GdaQueryCondition *condition, GdaQueryConditionOperator op);

GSList               *gda_query_condition_get_ref_objects_all    (GdaQueryCondition *cond);

gboolean              gda_query_condition_represents_join        (GdaQueryCondition *condition,
								  GdaQueryTarget **target1, GdaQueryTarget **target2,
								  gboolean *is_equi_join);
gboolean              gda_query_condition_represents_join_strict (GdaQueryCondition *condition,
								  GdaQueryTarget **target1, GdaQueryTarget **target2);
GSList               *gda_query_condition_get_main_conditions    (GdaQueryCondition *condition);
G_END_DECLS

#endif
