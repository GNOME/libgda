/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-label.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrLabelPriv {
	gchar         *label; /* imposed upon construction */
};

static void gda_tree_mgr_label_class_init (GdaTreeMgrLabelClass *klass);
static void gda_tree_mgr_label_init       (GdaTreeMgrLabel *tmgr1, GdaTreeMgrLabelClass *klass);
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

	parent_class = g_type_class_peek_parent (klass);

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
gda_tree_mgr_label_init (GdaTreeMgrLabel *mgr, GdaTreeMgrLabelClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_LABEL (mgr));
	mgr->priv = g_new0 (GdaTreeMgrLabelPriv, 1);
}

static void
gda_tree_mgr_label_dispose (GObject *object)
{
	GdaTreeMgrLabel *mgr = (GdaTreeMgrLabel *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_LABEL (mgr));

	if (mgr->priv) {
		g_free (mgr->priv->label);
		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_label_get_type
 *
 * Since: 4.2
 */
GType
gda_tree_mgr_label_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrLabelClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_label_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrLabel),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_label_init
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrLabel", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_mgr_label_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
        GdaTreeMgrLabel *mgr;

        mgr = GDA_TREE_MGR_LABEL (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_LABEL:
			mgr->priv->label = g_value_dup_string (value);
			break;
                }
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
        if (mgr->priv) {
                switch (param_id) {
		case PROP_LABEL:
			g_value_set_string (value, mgr->priv->label);
			break;
                }
        }
}

/**
 * gda_tree_mgr_label_new
 * @label: a label string
 *
 * Creates a new #GdaTreeManager object which will add one tree node labelled @label
 *
 * Returns: a new #GdaTreeManager object
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

static GSList *
gda_tree_mgr_label_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				    gboolean *out_error, GError **error)
{
	if (children_nodes) {
		GSList *list = g_slist_copy ((GSList*) children_nodes);
		g_slist_foreach (list, (GFunc) g_object_ref, NULL);
		return list;
	}

	GdaTreeMgrLabel *mgr = GDA_TREE_MGR_LABEL (manager);
	GdaTreeNode *snode;

	snode = gda_tree_manager_create_node (manager, node, mgr->priv->label ? mgr->priv->label : _("No name"));
	return g_slist_prepend (NULL, snode);
}
