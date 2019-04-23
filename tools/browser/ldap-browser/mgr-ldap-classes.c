/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "mgr-ldap-classes.h"
#include "gda-tree-node.h"
#include <providers/ldap/gda-ldap-connection.h>
#include "../ui-support.h"

struct _MgrLdapClassesPriv {
	TConnection *tcnc;
	gchar *class;
	gboolean flat;
};

static void mgr_ldap_classes_class_init (MgrLdapClassesClass *klass);
static void mgr_ldap_classes_init       (MgrLdapClasses *tmgr1, MgrLdapClassesClass *klass);
static void mgr_ldap_classes_dispose    (GObject *object);

/* virtual methods */
static GSList *mgr_ldap_classes_update_children (GdaTreeManager *manager, GdaTreeNode *node,
						   const GSList *children_nodes,
						   gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * MgrLdapClasses class implementation
 * @klass:
 */
static void
mgr_ldap_classes_class_init (MgrLdapClassesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = mgr_ldap_classes_update_children;

	object_class->dispose = mgr_ldap_classes_dispose;
}

static void
mgr_ldap_classes_init (MgrLdapClasses *mgr, G_GNUC_UNUSED MgrLdapClassesClass *klass)
{
	g_return_if_fail (MGR_IS_LDAP_CLASSES (mgr));
	mgr->priv = g_new0 (MgrLdapClassesPriv, 1);
}

static void
mgr_ldap_classes_dispose (GObject *object)
{
	MgrLdapClasses *mgr = (MgrLdapClasses *) object;

	g_return_if_fail (MGR_IS_LDAP_CLASSES (mgr));

	if (mgr->priv) {
		if (mgr->priv->tcnc)
			g_object_unref (mgr->priv->tcnc);
		g_free (mgr->priv->class);
		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * mgr_ldap_classes_get_type:
 *
 * Returns: the GType
 */
GType
mgr_ldap_classes_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (MgrLdapClassesClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) mgr_ldap_classes_class_init,
                        NULL,
                        NULL,
                        sizeof (MgrLdapClasses),
                        0,
                        (GInstanceInitFunc) mgr_ldap_classes_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "_MgrLdapClasses", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

/*
 * mgr_ldap_classes_new:
 * @cnc: a #GdaConnection object
 * @flat: %TRUE if listing all the classes, if %TRUE, then @classname is ignored.
 * @classname: (nullable): an LDAP class or %NULL for the "top" class
 *
 * Creates a new #GdaTreeManager object which will list the children classes
 *
 * Returns: (transfer full): a new #GdaTreeManager object
 */
GdaTreeManager*
mgr_ldap_classes_new (TConnection *tcnc, gboolean flat, const gchar *classname)
{
	MgrLdapClasses *mgr;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	mgr = (MgrLdapClasses*) g_object_new (MGR_TYPE_LDAP_CLASSES, NULL);

	mgr->priv->tcnc = g_object_ref (tcnc);
	mgr->priv->flat = flat;
	if (!flat) {
		if (classname)
			mgr->priv->class = g_strdup (classname);
	}
		
	return (GdaTreeManager*) mgr;
}

static GSList *
mgr_ldap_classes_update_children_flat (MgrLdapClasses *mgr, GdaTreeNode *node,
					 gboolean *out_error,
					 GError **error);
static GSList *
mgr_ldap_classes_update_children_nonflat (MgrLdapClasses *mgr, GdaTreeNode *node,
					    gboolean *out_error,
					    GError **error);

static GSList *
mgr_ldap_classes_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				    G_GNUC_UNUSED const GSList *children_nodes, gboolean *out_error,
				    GError **error)
{
	MgrLdapClasses *mgr = MGR_LDAP_CLASSES (manager);

	if (mgr->priv->flat)
		return mgr_ldap_classes_update_children_flat (mgr, node, out_error, error);
	else
		return mgr_ldap_classes_update_children_nonflat (mgr, node, out_error, error);

}

static gint
class_sort_func (GdaLdapClass *lcl1, GdaLdapClass *lcl2)
{
	if (lcl1->kind == lcl2->kind)
		return g_ascii_strcasecmp (lcl1->names[0], lcl2->names[0]);
	else
		return lcl1->kind - lcl2->kind;
}

static GSList *
mgr_ldap_classes_update_children_nonflat (MgrLdapClasses *mgr, GdaTreeNode *node,
					    gboolean *out_error,
					    GError **error)
{
	gchar *real_class = NULL;
	if (!mgr->priv->tcnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No LDAP connection specified"));
		if (out_error)
			*out_error = TRUE;
		goto onerror;
	}

	const GValue *cvalue;
	cvalue = gda_tree_node_fetch_attribute (node, "kind");
	if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_BOOLEAN) && g_value_get_boolean (cvalue))
		return NULL;

	if (node) {
		/* looking for a dn in @node's attributes */
		cvalue = gda_tree_node_fetch_attribute (node, "class");
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
			real_class = g_value_dup_string (cvalue);
	}
	if (!real_class)
		real_class = g_strdup (mgr->priv->class);

	GSList *classes_list;
	gboolean list_to_free = FALSE;
	if (real_class) {
		GdaLdapClass *lcl;
		lcl = t_connection_get_class_info (mgr->priv->tcnc, real_class);
		if (!lcl)
			goto onerror;
		classes_list = (GSList*) lcl->children;
	}
	else {
		/* sort by kind */
		classes_list = g_slist_copy ((GSList*) t_connection_get_top_classes (mgr->priv->tcnc));
		classes_list = g_slist_sort (classes_list, (GCompareFunc) class_sort_func);
		list_to_free = TRUE;
	}

	GSList *list = NULL;
	const GSList *child;
	for (child = classes_list; child; child = child->next) {
		GdaLdapClass *sub;
		GdaTreeNode* snode;
		GValue *value;
		GdkPixbuf *pixbuf;

		sub = (GdaLdapClass*) child->data;

		snode = gda_tree_manager_create_node ((GdaTreeManager*) mgr, node, sub->names[0]);

		/* class name */
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), sub->names[0]);
		gda_tree_node_set_node_attribute (snode, "class", value, NULL);
		gda_value_free (value);

		/* icon */
		pixbuf = ui_connection_ldap_icon_for_class_kind (sub->kind);
		value = gda_value_new (G_TYPE_OBJECT);
		g_value_set_object (value, pixbuf);
		gda_tree_node_set_node_attribute (snode, "icon", value, NULL);
		gda_value_free (value);		

		list = g_slist_prepend (list, snode);
	}
	if (list_to_free)
		g_slist_free (classes_list);
	return g_slist_reverse (list);

 onerror:
	g_free (real_class);
	if (out_error)
		*out_error = TRUE;
	return NULL;
}

static void
classes_foreach_func (GdaLdapClass *lcl, GSList **list)
{
	if (!g_slist_find (*list, lcl))
		*list = g_slist_insert_sorted (*list, lcl, (GCompareFunc) class_sort_func);
	g_slist_foreach (lcl->children, (GFunc) classes_foreach_func, list);
}

static GSList *
mgr_ldap_classes_update_children_flat (MgrLdapClasses *mgr, GdaTreeNode *node,
					 gboolean *out_error,
					 GError **error)
{
	if (!mgr->priv->tcnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No LDAP connection specified"));
		if (out_error)
			*out_error = TRUE;
		goto onerror;
	}

	const GSList *top_classes_list;
	GSList *list = NULL, *classes_list = NULL;
	top_classes_list = t_connection_get_top_classes (mgr->priv->tcnc);
	for (list = (GSList*) top_classes_list; list; list = list->next) {
		GdaLdapClass *lcl;
		lcl = (GdaLdapClass*) list->data;
		classes_list = g_slist_insert_sorted (classes_list, lcl, (GCompareFunc) class_sort_func);
		g_slist_foreach (lcl->children, (GFunc) classes_foreach_func, &classes_list);
	}

	GSList *child;
	GdaLdapClassKind kind = 0;
	for (list = NULL, child = classes_list; child; child = child->next) {
		GdaLdapClass *sub;
		GdaTreeNode* snode;
		GValue *value;
		GdkPixbuf *pixbuf;

		sub = (GdaLdapClass*) child->data;

		if (kind != sub->kind) {
			/* add extra node as separator */
			const gchar *tmp;
			tmp = ui_connection_ldap_class_kind_to_string (sub->kind);
			snode = gda_tree_manager_create_node ((GdaTreeManager*) mgr, node, tmp);
			list = g_slist_prepend (list, snode);
			kind = sub->kind;

			/* marker */
			g_value_set_boolean ((value = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
			gda_tree_node_set_node_attribute (snode, "kind", value, NULL);
			gda_value_free (value);

			/* icon */
			pixbuf = ui_connection_ldap_icon_for_class_kind (sub->kind);
			value = gda_value_new (G_TYPE_OBJECT);
			g_value_set_object (value, pixbuf);
			gda_tree_node_set_node_attribute (snode, "icon", value, NULL);
			gda_value_free (value);
		}
		snode = gda_tree_manager_create_node ((GdaTreeManager*) mgr, node, sub->names[0]);

		/* class name */
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), sub->names[0]);
		gda_tree_node_set_node_attribute (snode, "class", value, NULL);
		gda_value_free (value);
		
		/* icon */
		pixbuf = ui_connection_ldap_icon_for_class_kind (sub->kind);
		value = gda_value_new (G_TYPE_OBJECT);
		g_value_set_object (value, pixbuf);
		gda_tree_node_set_node_attribute (snode, "icon", value, NULL);
		gda_value_free (value);	

		list = g_slist_prepend (list, snode);
	}
	g_slist_free (classes_list);
	return g_slist_reverse (list);

 onerror:
	if (out_error)
		*out_error = TRUE;
	return NULL;
}
