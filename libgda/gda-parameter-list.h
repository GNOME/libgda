/* gda-parameter-list.h
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


#ifndef __GDA_PARAMETER_LIST_H_
#define __GDA_PARAMETER_LIST_H_

#include <libgda/gda-object.h>
#include "gda-value.h"
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_PARAMETER_LIST          (gda_parameter_list_get_type())
#define GDA_PARAMETER_LIST(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_parameter_list_get_type(), GdaParameterList)
#define GDA_PARAMETER_LIST_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_parameter_list_get_type (), GdaParameterListClass)
#define GDA_IS_PARAMETER_LIST(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_parameter_list_get_type ())


/* error reporting */
extern GQuark gda_parameter_list_error_quark (void);
#define GDA_PARAMETER_LIST_ERROR gda_parameter_list_error_quark ()

enum
{
	GDA_PARAMETER_LIST_NO_NODE_ERROR,
	GDA_PARAMETER_LIST_NODE_OUTDATED_ERROR,
	GDA_PARAMETER_LIST_VALUE_PROV_OBJ_TYPE_ERROR,
	GDA_PARAMETER_LIST_VALUE_PROV_DATA_TYPE_ERROR,
	GDA_PARAMETER_LIST_VALUE_PROV_INVISIBLE_ERROR,
	GDA_PARAMETER_LIST_VALUE_PROV_QUERY_TYPE_ERROR,
	GDA_PARAMETER_LIST_DEPENDENCY_NOT_IN_PARAMLIST_ERROR,
	GDA_PARAMETER_LIST_DEPENDENCY_POSITION_ERROR,
	GDA_PARAMETER_LIST_XML_SPEC_ERROR
};

typedef enum {
	GDA_PARAMETER_LIST_PARAM_READ_ONLY = 1 << 0, /* param should not be affected by user modifications */
	GDA_PARAMETER_LIST_PARAM_HIDE      = 1 << 1  /* param should not be shown to the user */
} GdaParameterListParamHint;


/**
 * For each #GdaParameter object in the #GdaParameterList object, there is a #GdaParameterListNode structure
 * which sums up all the information for each parameter
 */
struct _GdaParameterListNode {
	GdaParameter   *param;         /* Can't be NULL */
	GdaDataModel   *source_model;  /* may be NULL if @param is free-fill */
	gint            source_column; /* unused is @source_model is NULL */
	guint           hint;
};

/**
 * The #GdaParameterListGroup is another view of the parameters list contained in the #GdaParameterList object:
 * there is one such structure for each _independant_ parameter (parameters which are constrained by the same
 * data model all appear in the same #GdaParameterListGroup structure)
 */
struct _GdaParameterListGroup {
	GSList                 *nodes;        /* list of GdaParameterListNode, at least one entry */
	GdaParameterListSource *nodes_source; /* if NULL, then @nodes contains exactly one entry */
};

/**
 * There is a #GdaParameterListSource structure for each #GdaDataModel which constrains at least on parameter
 * in the #GdaParameterList object
 */
struct _GdaParameterListSource {
	GdaDataModel   *data_model;   /* Can't be NULL */
	GSList         *nodes;        /* list of #GdaParameterListNode for which source_model == @data_model */

	/* displayed columns in 'data_model' */
        gint            shown_n_cols;
        gint           *shown_cols_index;

        /* columns used as a reference (corresponding to PK values) in 'data_model' */
        gint            ref_n_cols;
        gint           *ref_cols_index;
};
#define GDA_PARAMETER_LIST_NODE(x) ((GdaParameterListNode *)x)
#define GDA_PARAMETER_LIST_SOURCE(x) ((GdaParameterListSource *)x)
#define GDA_PARAMETER_LIST_GROUP(x) ((GdaParameterListGroup *)x)

/* struct for the object's data */
struct _GdaParameterList
{
	GdaObject                 object;
	GSList                   *parameters;   /* list of GdaParameter objects */

	GSList                   *nodes_list;   /* list of GdaParameterListNode */
        GSList                   *sources_list; /* list of GdaParameterListSource */
	GSList                   *groups_list;  /* list of GdaParameterListGroup */

	GdaParameterListPrivate  *priv;
};

/* struct for the object's class */
struct _GdaParameterListClass
{
	GdaObjectClass          parent_class;

	void                  (*param_changed) (GdaParameterList *paramlist, GdaParameter *param);
	void                  (*public_data_changed) (GdaParameterList *paramlist);
};

GType                   gda_parameter_list_get_type                 (void);
GdaParameterList       *gda_parameter_list_new                      (GSList *params);
GdaParameterList       *gda_parameter_list_new_inline               (GdaDict *dict, ...);

GdaParameterList       *gda_parameter_list_new_from_spec_string     (GdaDict *dict, const gchar *xml_spec, GError **error);
GdaParameterList       *gda_parameter_list_new_from_spec_node       (GdaDict *dict, xmlNodePtr xml_spec, GError **error);
gchar                  *gda_parameter_list_get_spec                 (GdaParameterList *paramlist);

guint                   gda_parameter_list_get_length               (GdaParameterList *plist);


void                    gda_parameter_list_add_param                (GdaParameterList *paramlist, GdaParameter *param);
GdaParameter           *gda_parameter_list_add_param_from_string    (GdaParameterList *paramlist, const gchar *name,
								     GType type, const gchar *str);
GdaParameter           *gda_parameter_list_add_param_from_value     (GdaParameterList *paramlist, const gchar *name,
								     GValue *value);
void                    gda_parameter_list_merge                    (GdaParameterList *paramlist, 
								     GdaParameterList *paramlist_to_merge);
gboolean                gda_parameter_list_is_coherent              (GdaParameterList *paramlist, GError **error);
gboolean                gda_parameter_list_is_valid                 (GdaParameterList *paramlist);

GdaParameter           *gda_parameter_list_find_param               (GdaParameterList *paramlist, 
								     const gchar *param_name);
GdaParameter           *gda_parameter_list_find_param_for_user      (GdaParameterList *paramlist, 
								     GdaObject *user);
GdaParameterListNode   *gda_parameter_list_find_node_for_param      (GdaParameterList *paramlist, 
								     GdaParameter *param);
GdaParameterListSource *gda_parameter_list_find_source              (GdaParameterList *paramlist, 
								     GdaDataModel *model);
GdaParameterListSource *gda_parameter_list_find_source_for_param    (GdaParameterList *paramlist, 
								     GdaParameter *param);
GdaParameterListGroup  *gda_parameter_list_find_group_for_param     (GdaParameterList *paramlist, 
								     GdaParameter *param);

void                    gda_parameter_list_set_param_default_value  (GdaParameterList *paramlist, 
								     GdaParameter *param, const GValue *value);
void                    gda_parameter_list_set_param_default_alias  (GdaParameterList *paramlist, 
								     GdaParameter *param, GdaParameter *alias);
const GValue           *gda_parameter_list_get_param_default_value  (GdaParameterList *paramlist, GdaParameter *param);

G_END_DECLS

#endif
