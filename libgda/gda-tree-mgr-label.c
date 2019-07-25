/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-tree-mgr-label"

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-label.h"
#include "gda-tree-node.h"

typedef struct {
	gchar         *label; /* imposed upon construction */
} GdaTreeMgrLabelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaTreeMgrLabel, gda_tree_mgr_label, GDA_TYPE_TREE_MANAGER)

static void gda_tree_mgr_label_dispose    (GObject *object);
static void gda_tree_mgr_label_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_tree_mgr_label_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_label_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
						   gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/* properties */
enum {
        PROP_0,
	PROP_LABEL
};

/*
 * GdaTreeMgrLabel class implementation
 * @klass:
 */
static void
gda_tree_mgr_label_class_init (GdaTreeMgrLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_label_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_label_set_property;
        object_class->get_property = gda_tree_mgr_label_get_property;

        g_object_class_install_property (object_class, PROP_LABEL,
                                         g_param_spec_string ("label", NULL,
                                                              "Label for the node",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	object_class->dispose = gda_tree_mgr_label_dispose;
}

static void
gda_tree_mgr_label_init (GdaTreeMgrLabel *mgr) {}

static void
gda_tree_mgr_label_dispose (GObject *object)
{
	GdaTreeMgrLabel *mgr = (GdaTreeMgrLabel *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_LABEL (mgr));
	GdaTreeMgrLabelPrivate *priv = gda_tree_mgr_label_get_instance_private (mgr);

	g_free (priv->label);
	priv->label = NULL;

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_tree_mgr_label_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrLabel *mgr;

	mgr = GDA_TREE_MGR_LABEL (object);
	GdaTreeMgrLabelPrivate *priv = gda_tree_mgr_label_get_instance_private (mgr);
	switch (param_id) {
		case PROP_LABEL:
			priv->label = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_tree_mgr_label_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrLabel *mgr;

	mgr = GDA_TREE_MGR_LABEL (object);
	GdaTreeMgrLabelPrivate *priv = gda_tree_mgr_label_get_instance_private (mgr);
	switch (param_id) {
		case PROP_LABEL:
			g_value_set_string (value, priv->label);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_tree_mgr_label_new:
 * @label: a label string
 *
 * Creates a new #GdaTreeManager object which will add one tree node labelled @label
 *
 * Returns: (transfer full): a new #GdaTreeManager object
 * 
 * Since: 4.2
 */
GdaTreeManager*
gda_tree_mgr_label_new (const gchar *label)
{
	GdaTreeMgrLabel *mgr;
	mgr = (GdaTreeMgrLabel*) g_object_new (GDA_TYPE_TREE_MGR_LABEL,
					       "label", label, NULL);
	return (GdaTreeManager*) mgr;
}

void ref_elements (GObject *object, G_GNUC_UNUSED gpointer user_data)
{
  g_object_ref (object);
}

static GSList *
gda_tree_mgr_label_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				    G_GNUC_UNUSED gboolean *out_error, G_GNUC_UNUSED GError **error)
{
	if (children_nodes) {
		GSList *list = g_slist_copy ((GSList*) children_nodes);
		g_slist_foreach (list, (GFunc) ref_elements, NULL);
		return list;
	}

	GdaTreeMgrLabel *mgr = GDA_TREE_MGR_LABEL (manager);
	GdaTreeNode *snode;
	GdaTreeMgrLabelPrivate *priv = gda_tree_mgr_label_get_instance_private (mgr);

	snode = gda_tree_manager_create_node (manager, node, priv->label ? priv->label : _("No name"));
	return g_slist_prepend (NULL, snode);
}
