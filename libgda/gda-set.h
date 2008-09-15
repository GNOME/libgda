/* gda-set.h
 *
 * Copyright (C) 2003 - 2008 Vivien Malerba
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


#ifndef __GDA_SET_H_
#define __GDA_SET_H_

#include "gda-value.h"
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_SET          (gda_set_get_type())
#define GDA_SET(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_set_get_type(), GdaSet)
#define GDA_SET_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_set_get_type (), GdaSetClass)
#define GDA_IS_SET(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_set_get_type ())


/* error reporting */
extern GQuark gda_set_error_quark (void);
#define GDA_SET_ERROR gda_set_error_quark ()

typedef enum
{
	GDA_SET_XML_SPEC_ERROR,
	GDA_SET_HOLDER_NOT_FOUND_ERROR,
	GDA_SET_INVALID_ERROR
} GdaSetError;

typedef enum {
	GDA_SET_HOLDER_READ_ONLY = 1 << 0, /* holder should not be affected by user modifications */
	GDA_SET_HOLDER_HIDE      = 1 << 1  /* holder should not be shown to the user */
} GdaSetHint;


/**
 * GdaSetNode:
 *
 * For each #GdaHolder object in the #GdaSet object, there is a
 * #GdaSetNode structure which sums up all the information for
 * each GdaHolder.
 */
struct _GdaSetNode {
	GdaHolder    *holder;         /* Can't be NULL */
	GdaDataModel *source_model;  /* may be NULL if @holder does not have any source */
	gint          source_column; /* unused is @source_model is NULL */
	GdaSetHint    hint;
};

/**
 * GdaSetGroup:
 *
 * The #GdaSetGroup is another view of the #GdaHolder objects
 * contained in the #GdaSet: there is one such structure
 * for each _independant_ parameter (parameters which have the same source data model
 * all appear in the same #GdaSetGroup structure).
 */
struct _GdaSetGroup {
	GSList       *nodes;       /* list of GdaSetNode, at least one entry */
	GdaSetSource *nodes_source; /* if NULL, then @nodes contains exactly one entry */
};

/**
 * GdaSetSource:
 *
 * There is a #GdaSetSource structure for each #GdaDataModel which
 * is a source for least one #GdaHolder in the #GdaSet object.
 */
struct _GdaSetSource {
	GdaDataModel   *data_model;   /* Can't be NULL */
	GSList         *nodes;        /* list of #GdaSetNode for which source_model == @data_model */

	/* displayed columns in 'data_model' */
        gint            shown_n_cols;
        gint           *shown_cols_index;

        /* columns used as a reference (corresponding to PK values) in 'data_model' */
        gint            ref_n_cols;
        gint           *ref_cols_index;
};

#define GDA_SET_NODE(x) ((GdaSetNode *)(x))
#define GDA_SET_SOURCE(x) ((GdaSetSource *)(x))
#define GDA_SET_GROUP(x) ((GdaSetGroup *)(x))

/* struct for the object's data */
struct _GdaSet
{
	GObject         object;
	GdaSetPrivate  *priv;

	/* public READ ONLY data */
	GSList         *holders;   /* list of GdaHolder objects */
	GSList         *nodes_list;   /* list of GdaSetNode */
        GSList         *sources_list; /* list of GdaSetSource */
	GSList         *groups_list;  /* list of GdaSetGroup */
};

/* struct for the object's class */
struct _GdaSetClass
{
	GObjectClass            parent_class;

	GError               *(*validate_holder_change)(GdaSet *set, GdaHolder *holder, const GValue *new_value);
	GError               *(*validate_set)          (GdaSet *set);
	void                  (*holder_changed)        (GdaSet *set, GdaHolder *holder);
	void                  (*holder_plugin_changed) (GdaSet *set, GdaHolder *holder);
	void                  (*holder_attr_changed)   (GdaSet *set, GdaHolder *holder);
	void                  (*public_data_changed)   (GdaSet *set);
};

GType         gda_set_get_type                 (void) G_GNUC_CONST;
GdaSet       *gda_set_new                      (GSList *holders);
GdaSet       *gda_set_copy                     (GdaSet *set);
GdaSet       *gda_set_new_inline               (gint nb, ...);

GdaSet       *gda_set_new_from_spec_string     (const gchar *xml_spec, GError **error);
GdaSet       *gda_set_new_from_spec_node       (xmlNodePtr xml_spec, GError **error);
gchar        *gda_set_get_spec                 (GdaSet *set);

gboolean      gda_set_set_holder_value         (GdaSet *set, GError **error, const gchar *holder_id, ...);
const GValue *gda_set_get_holder_value         (GdaSet *set, const gchar *holder_id);
GdaHolder    *gda_set_get_holder               (GdaSet *set, const gchar *holder_id);
gboolean      gda_set_add_holder               (GdaSet *set, GdaHolder *holder);
void          gda_set_remove_holder            (GdaSet *set, GdaHolder *holder);
void          gda_set_merge_with_set           (GdaSet *set, GdaSet *set_to_merge);
gboolean      gda_set_is_valid                 (GdaSet *set, GError **error);

/* public data lookup functions */
GdaSetNode   *gda_set_get_node                 (GdaSet *set, GdaHolder *param);
GdaSetSource *gda_set_get_source_for_model     (GdaSet *set, GdaDataModel *model);
GdaSetSource *gda_set_get_source               (GdaSet *set, GdaHolder *param);
GdaSetGroup  *gda_set_get_group                (GdaSet *set, GdaHolder *param);

G_END_DECLS

#endif
