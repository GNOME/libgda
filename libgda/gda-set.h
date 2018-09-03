/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */


#ifndef __GDA_SET_H_
#define __GDA_SET_H_

#include "gda-value.h"
#include <libxml/tree.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gda_set_error_quark (void);
#define GDA_SET_ERROR gda_set_error_quark ()

typedef enum
{
	GDA_SET_XML_SPEC_ERROR,
	GDA_SET_HOLDER_NOT_FOUND_ERROR,
	GDA_SET_INVALID_ERROR,
	GDA_SET_READ_ONLY_ERROR,
	GDA_SET_IMPLEMENTATION_ERROR
} GdaSetError;

typedef struct _GdaSetNode GdaSetNode;
typedef struct _GdaSetGroup GdaSetGroup;
typedef struct _GdaSetSource GdaSetSource;

#define GDA_TYPE_SET_NODE (gda_set_node_get_type ())
#define GDA_SET_NODE(x) ((GdaSetNode *)(x))
GType         gda_set_node_get_type          (void) G_GNUC_CONST;
GdaSetNode   *gda_set_node_new               (GdaHolder *holder);
void          gda_set_node_free              (GdaSetNode *node);
GdaSetNode   *gda_set_node_copy              (GdaSetNode *node);
GdaHolder    *gda_set_node_get_holder        (GdaSetNode *node);
void          gda_set_node_set_holder        (GdaSetNode *node, GdaHolder *holder);
GdaDataModel *gda_set_node_get_data_model    (GdaSetNode *node);
void          gda_set_node_set_data_model    (GdaSetNode *node, GdaDataModel *model);
gint          gda_set_node_get_source_column (GdaSetNode *node);
void          gda_set_node_set_source_column (GdaSetNode *node, gint column);

#define GDA_TYPE_SET_GROUP (gda_set_group_get_type ())
#define GDA_SET_GROUP(x) ((GdaSetGroup *)(x))
GType         gda_set_group_get_type        (void) G_GNUC_CONST;
GdaSetGroup  *gda_set_group_new             (GdaSetNode *node);
void          gda_set_group_free            (GdaSetGroup *sg);
GdaSetGroup  *gda_set_group_copy            (GdaSetGroup *sg);
void          gda_set_group_add_node        (GdaSetGroup *sg, GdaSetNode *node);
GdaSetNode   *gda_set_group_get_node        (GdaSetGroup *sg);
GSList       *gda_set_group_get_nodes       (GdaSetGroup *sg);
gint          gda_set_group_get_n_nodes     (GdaSetGroup *sg);
void          gda_set_group_set_source      (GdaSetGroup *sg, GdaSetSource *source);
GdaSetSource *gda_set_group_get_source      (GdaSetGroup *sg);

#define GDA_TYPE_SET_SOURCE (gda_set_source_get_type ())
#define GDA_SET_SOURCE(x) ((GdaSetSource *)(x))
GType         gda_set_source_get_type       (void) G_GNUC_CONST;
GdaSetSource *gda_set_source_new            (GdaDataModel *model);
void          gda_set_source_free           (GdaSetSource *s);
GdaSetSource *gda_set_source_copy           (GdaSetSource *s);
void          gda_set_source_add_node       (GdaSetSource *s, GdaSetNode *node);
GSList       *gda_set_source_get_nodes      (GdaSetSource *s);
gint          gda_set_source_get_n_nodes    (GdaSetSource *s);
GdaDataModel *gda_set_source_get_data_model (GdaSetSource *s);
void          gda_set_source_set_data_model (GdaSetSource *s, GdaDataModel *model);

/* struct for the object's data */

#define GDA_TYPE_SET          (gda_set_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaSet, gda_set, GDA, SET, GObject)

/* struct for the object's class */
struct _GdaSetClass
{
	GObjectClass            parent_class;

	GError               *(*validate_holder_change)(GdaSet *set, GdaHolder *holder, const GValue *new_value);
	GError               *(*validate_set)          (GdaSet *set);
	void                  (*holder_changed)        (GdaSet *set, GdaHolder *holder);
	void                  (*holder_attr_changed)   (GdaSet *set, GdaHolder *holder, 
							const gchar *attr_name, const GValue *attr_value);
	void                  (*public_data_changed)   (GdaSet *set);
	void                  (*holder_type_set)       (GdaSet *set, GdaHolder *holder);
	void                  (*source_model_changed)  (GdaSet *set, GdaSetSource *source);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-set
 * @short_description: Container for several values
 * @title: GdaSet
 * @stability: Stable
 * @see_also: #GdaHolder
 *
 * The #GdaSet object is a container for several values (as #GdaHolder objects). Each #GdaSet object also
 * maintains some publicly accessible information about the #GdaHolder objects, through the #GdaSetNode, #GdaSetSource and
 * #GdaSetGroup structures (see gda_set_get_node(), gda_set_get_source() and gda_set_get_group()).
 *
 * It is possible to control the values a #GdaHolder can have in the #GdaSet by connecting to the 
 * <link linkend="GdaSet-before-holder-change">"before-holder-change"</link> signal.
 */

GdaSet       *gda_set_new                      (GSList *holders);
GdaSet       *gda_set_copy                     (GdaSet *set);
GdaSet       *gda_set_new_inline               (gint nb, ...);

GdaSet       *gda_set_new_from_spec_string     (const gchar *xml_spec, GError **error);
GdaSet       *gda_set_new_from_spec_node       (xmlNodePtr xml_spec, GError **error);

gboolean      gda_set_set_holder_value         (GdaSet *set, GError **error, const gchar *holder_id, ...);
const GValue *gda_set_get_holder_value         (GdaSet *set, const gchar *holder_id);
GSList       *gda_set_get_holders              (GdaSet *set);
GdaHolder    *gda_set_get_holder               (GdaSet *set, const gchar *holder_id);
GdaHolder    *gda_set_get_nth_holder           (GdaSet *set, gint pos);
gboolean      gda_set_add_holder               (GdaSet *set, GdaHolder *holder);
void          gda_set_remove_holder            (GdaSet *set, GdaHolder *holder);
void          gda_set_merge_with_set           (GdaSet *set, GdaSet *set_to_merge);
gboolean      gda_set_is_valid                 (GdaSet *set, GError **error);

void          gda_set_replace_source_model     (GdaSet *set, GdaSetSource *source,
						GdaDataModel *model);

/* public data lookup functions */
GSList       *gda_set_get_nodes                (GdaSet *set);
GdaSetNode   *gda_set_get_node                 (GdaSet *set, GdaHolder *holder);
GSList       *gda_set_get_sources              (GdaSet *set);
GdaSetSource *gda_set_get_source_for_model     (GdaSet *set, GdaDataModel *model);
GdaSetSource *gda_set_get_source               (GdaSet *set, GdaHolder *holder);
GSList       *gda_set_get_groups               (GdaSet *set);
GdaSetGroup  *gda_set_get_group                (GdaSet *set, GdaHolder *holder);

/* private */
gboolean      _gda_set_validate                (GdaSet *set, GError **error);
GdaSet *      gda_set_new_read_only            (GSList *holders);




G_END_DECLS

#endif
