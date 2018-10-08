/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "mgr-ldap-entries.h"
#include <libgda/gda-tree-node.h>
#include "../ui-support.h"

struct _MgrLdapEntriesPriv {
	TConnection *tcnc;
	gchar             *dn;
};

static void mgr_ldap_entries_class_init (MgrLdapEntriesClass *klass);
static void mgr_ldap_entries_init       (MgrLdapEntries *tmgr1, MgrLdapEntriesClass *klass);
static void mgr_ldap_entries_dispose    (GObject *object);

/* virtual methods */
static GSList *mgr_ldap_entries_update_children (GdaTreeManager *manager, GdaTreeNode *node,
						      const GSList *children_nodes,
						      gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * MgrLdapEntries class implementation
 * @klass:
 */
static void
mgr_ldap_entries_class_init (MgrLdapEntriesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = mgr_ldap_entries_update_children;

	/* Properties */
	object_class->dispose = mgr_ldap_entries_dispose;
}

static void
mgr_ldap_entries_init (MgrLdapEntries *mgr, G_GNUC_UNUSED MgrLdapEntriesClass *klass)
{
	g_return_if_fail (MGR_IS_LDAP_ENTRIES (mgr));
	mgr->priv = g_new0 (MgrLdapEntriesPriv, 1);
}

static void
mgr_ldap_entries_dispose (GObject *object)
{
	MgrLdapEntries *mgr = (MgrLdapEntries *) object;

	g_return_if_fail (MGR_IS_LDAP_ENTRIES (mgr));

	if (mgr->priv) {
		if (mgr->priv->tcnc)
			g_object_unref (mgr->priv->tcnc);
		g_free (mgr->priv->dn);
		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * browser_tree_mgr_select_get_type:
 *
 * Returns: the GType
 */
GType
mgr_ldap_entries_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (MgrLdapEntriesClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) mgr_ldap_entries_class_init,
                        NULL,
                        NULL,
                        sizeof (MgrLdapEntries),
                        0,
                        (GInstanceInitFunc) mgr_ldap_entries_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "MgrLdapEntries", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

/**
 * mgr_ldap_entries_new:
 * @cnc: a #TConnection object
 * @dn: (nullable): a schema name or %NULL
 *
 * Creates a new #BrowserTreeManager object which will list the children of the LDAP entry which Distinguished name
 * is @dn. If @dn is %NULL, then the tree manager will look in the tree itself for an attribute named "dn" and
 * use it.
 *
 * Returns: (transfer full): a new #BrowserTreeManager object
 */
GdaTreeManager*
mgr_ldap_entries_new (TConnection *tcnc, const gchar *dn)
{
	MgrLdapEntries *mgr;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	mgr = (MgrLdapEntries*) g_object_new (MGR_TYPE_LDAP_ENTRIES, NULL);

	mgr->priv->tcnc = g_object_ref (tcnc);
	if (dn)
		mgr->priv->dn = g_strdup (dn);

	return (GdaTreeManager*) mgr;
}

static gint
lentry_array_sort_func (gconstpointer a, gconstpointer b)
{
        GdaLdapEntry *e1, *e2;
        e1 = *((GdaLdapEntry**) a);
        e2 = *((GdaLdapEntry**) b);
	GdaLdapAttribute *cna1, *cna2;
	const gchar *str1 = NULL, *str2 = NULL;

	cna1 = g_hash_table_lookup (e1->attributes_hash, "cn");
	cna2 = g_hash_table_lookup (e2->attributes_hash, "cn");
	if (cna1 && (cna1->nb_values > 0) && (G_VALUE_TYPE (cna1->values[0]) == G_TYPE_STRING))
		str1 = g_value_get_string (cna1->values[0]);
	else
		str1 = e1->dn ? e1->dn : "";
	if (cna2 && (cna2->nb_values > 0) && (G_VALUE_TYPE (cna2->values[0]) == G_TYPE_STRING))
		str2 = g_value_get_string (cna2->values[0]);
	else
		str2 = e2->dn ? e2->dn : "";

	return strcmp (str2, str1);
}

static GSList *
mgr_ldap_entries_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				  G_GNUC_UNUSED const GSList *children_nodes, gboolean *out_error,
				  GError **error)
{
	MgrLdapEntries *mgr = MGR_LDAP_ENTRIES (manager);
	gchar *real_dn = NULL;

	g_return_val_if_fail (mgr->priv->tcnc, NULL);

	if (mgr->priv->dn)
		real_dn = g_strdup (mgr->priv->dn);
	else if (node) {
		/* looking for a dn in @node's attributes */
		const GValue *cvalue;
		cvalue = gda_tree_node_fetch_attribute (node, "dn");
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
			real_dn = g_value_dup_string (cvalue);
	}

	GdaLdapEntry **entries;
	gchar *attrs[] = {"objectClass", "cn", NULL};
	entries = t_connection_ldap_get_entry_children (mgr->priv->tcnc, real_dn, attrs, error);
	
	if (entries) {
		guint i;
		GSList *list = NULL;
		GArray *sorted_array;
		sorted_array = g_array_new (FALSE, FALSE, sizeof (GdaLdapEntry*));
		for (i = 0; entries [i]; i++) {
			GdaLdapEntry *lentry;
			lentry = entries [i];
			g_array_prepend_val (sorted_array, lentry);
		}
		g_free (entries);

		g_array_sort (sorted_array, (GCompareFunc) lentry_array_sort_func);

		for (i = 0; i < sorted_array->len; i++) {
			GdaTreeNode* snode;
			GValue *dnv;
			GdaLdapEntry *lentry;

			lentry = g_array_index (sorted_array, GdaLdapEntry*, i);
			snode = gda_tree_manager_create_node (manager, node, lentry->dn);

			/* full DN */
			g_value_set_string ((dnv = gda_value_new (G_TYPE_STRING)), lentry->dn);
			gda_tree_node_set_node_attribute (snode, "dn", dnv, NULL);
			gda_value_free (dnv);

			/* RDN */
                        gchar **array;
                        array = gda_ldap_dn_split (lentry->dn, FALSE);
                        if (array) {
                                g_value_set_string ((dnv = gda_value_new (G_TYPE_STRING)), array [0]);
                                gda_tree_node_set_node_attribute (snode, "rdn", dnv, NULL);
				gda_value_free (dnv);
                                g_strfreev (array);
                        }

			/* CN */
			GdaLdapAttribute *attr;
			attr = g_hash_table_lookup (lentry->attributes_hash, "cn");
			if (attr && (attr->nb_values >= 1)) {
				const GValue *cvalue;
				cvalue = attr->values [0];
				if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
					gda_tree_node_set_node_attribute (snode, "cn", cvalue, NULL);
			}

			/* icon */
			GdkPixbuf *pixbuf;
			attr = g_hash_table_lookup (lentry->attributes_hash, "objectClass");
			pixbuf = ui_connection_ldap_icon_for_class (attr);

			dnv = gda_value_new (G_TYPE_OBJECT);
			g_value_set_object (dnv, pixbuf);
			gda_tree_node_set_node_attribute (snode, "icon", dnv, NULL);
			gda_value_free (dnv);

			if (gda_tree_manager_get_managers (manager)) {
				g_value_set_boolean ((dnv = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
				gda_tree_node_set_node_attribute (snode,
								  GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN,
								  dnv, NULL);
				gda_value_free (dnv);
			}

			list = g_slist_prepend (list, snode);
			gda_ldap_entry_free (lentry);
		}
		g_array_free (sorted_array, TRUE);

		if (node)
			gda_tree_node_set_node_attribute (node,
							  GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN,
							  NULL, NULL);

		return list;
	}
	else
		return NULL;
}
