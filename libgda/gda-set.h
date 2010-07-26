/* gda-set.h
 *
 * Copyright (C) 2003 - 2009 Vivien Malerba
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

struct _GdaSetNode {
	GdaHolder    *holder;        /* Can't be NULL */
	GdaDataModel *source_model;  /* may be NULL */
	gint          source_column; /* unused if @source_model is NULL */

	/*< private >*/
	/* Padding for future expansion */
	gpointer      _gda_reserved1;
	gpointer      _gda_reserved2;
};

struct _GdaSetGroup {
	GSList       *nodes;       /* list of GdaSetNode, at least one entry */
	GdaSetSource *nodes_source; /* if NULL, then @nodes contains exactly one entry */

	/*< private >*/
	/* Padding for future expansion */
	gpointer      _gda_reserved1;
	gpointer      _gda_reserved2;
};

struct _GdaSetSource {
	GdaDataModel   *data_model;   /* Can't be NULL */
	GSList         *nodes;        /* list of #GdaSetNode for which source_model == @data_model */

	/*< private >*/
	/* Padding for future expansion */
	gpointer        _gda_reserved1;
	gpointer        _gda_reserved2;
	gpointer        _gda_reserved3;
	gpointer        _gda_reserved4;
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
	void                  (*holder_attr_changed)   (GdaSet *set, GdaHolder *holder, 
							const gchar *attr_name, const GValue *attr_value);
	void                  (*public_data_changed)   (GdaSet *set);

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         gda_set_get_type                 (void) G_GNUC_CONST;
GdaSet       *gda_set_new                      (GSList *holders);
GdaSet       *gda_set_copy                     (GdaSet *set);
GdaSet       *gda_set_new_inline               (gint nb, ...);

GdaSet       *gda_set_new_from_spec_string     (const gchar *xml_spec, GError **error);
GdaSet       *gda_set_new_from_spec_node       (xmlNodePtr xml_spec, GError **error);

gboolean      gda_set_set_holder_value         (GdaSet *set, GError **error, const gchar *holder_id, ...);
const GValue *gda_set_get_holder_value         (GdaSet *set, const gchar *holder_id);
GdaHolder    *gda_set_get_holder               (GdaSet *set, const gchar *holder_id);
gboolean      gda_set_add_holder               (GdaSet *set, GdaHolder *holder);
void          gda_set_remove_holder            (GdaSet *set, GdaHolder *holder);
void          gda_set_merge_with_set           (GdaSet *set, GdaSet *set_to_merge);
gboolean      gda_set_is_valid                 (GdaSet *set, GError **error);

/* public data lookup functions */
GdaSetNode   *gda_set_get_node                 (GdaSet *set, GdaHolder *holder);
GdaSetSource *gda_set_get_source_for_model     (GdaSet *set, GdaDataModel *model);
GdaSetSource *gda_set_get_source               (GdaSet *set, GdaHolder *holder);
GdaSetGroup  *gda_set_get_group                (GdaSet *set, GdaHolder *holder);

/* private */
gboolean      _gda_set_validate                (GdaSet *set, GError **error);


G_END_DECLS

#endif
