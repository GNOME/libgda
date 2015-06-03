/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-xml.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrXmlPriv {
	xmlNodePtr   root;
	gchar      **attributes;
};

static void gda_tree_mgr_xml_class_init (GdaTreeMgrXmlClass *klass);
static void gda_tree_mgr_xml_init       (GdaTreeMgrXml *tmgr1, GdaTreeMgrXmlClass *klass);
static void gda_tree_mgr_xml_dispose    (GObject *object);

/* virtual methods */
static GSList *gda_tree_mgr_xml_update_children (GdaTreeManager *manager, GdaTreeNode *node, 
						 const GSList *children_nodes, gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * GdaTreeMgrXml class implementation
 * @klass:
 */
static void
gda_tree_mgr_xml_class_init (GdaTreeMgrXmlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_xml_update_children;

	object_class->dispose = gda_tree_mgr_xml_dispose;
}

static void
gda_tree_mgr_xml_init (GdaTreeMgrXml *mgr, G_GNUC_UNUSED GdaTreeMgrXmlClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_XML (mgr));
	mgr->priv = g_new0 (GdaTreeMgrXmlPriv, 1);
}

static void
gda_tree_mgr_xml_dispose (GObject *object)
{
	GdaTreeMgrXml *mgr = (GdaTreeMgrXml *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_XML (mgr));

	if (mgr->priv) {
		if (mgr->priv->attributes)
			g_strfreev (mgr->priv->attributes);
		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_xml_get_type:
 *
 * Returns: the GType
 */
GType
gda_tree_mgr_xml_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrXmlClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_xml_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrXml),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_xml_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrXml", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

/**
 * gda_tree_mgr_xml_new:
 * rootnode: the #xmlNodePtr root
 * xml_attributes: (allow-none): a string containing the XML attributes which will appear in each node, separated by the | character, or %NULL to list all attributes
 *
 * Creates a new #GdaTreeManager object which will add one tree node for each
 * subnodes of an XmlNodePtr
 *
 * Returns: (transfer full): a new #GdaTreeManager object 
 */
GdaTreeManager*
gda_tree_mgr_xml_new (xmlNodePtr rootnode, const gchar *xml_attributes)
{
	GdaTreeMgrXml *mgr;
	mgr = (GdaTreeMgrXml*) g_object_new (GDA_TYPE_TREE_MGR_XML, NULL);
	mgr->priv->root = rootnode;
	if (xml_attributes)
		mgr->priv->attributes = g_strsplit (xml_attributes, "|", 0);
	return (GdaTreeManager*) mgr;
}

static GSList *
gda_tree_mgr_xml_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				  G_GNUC_UNUSED const GSList *children_nodes,
				  gboolean *out_error, GError **error)
{
	GdaTreeMgrXml *mgr = GDA_TREE_MGR_XML (manager);
	GSList *list = NULL;
	xmlNodePtr xnode, child;
	const GValue *cvalue;
	
	cvalue = gda_tree_node_get_node_attribute (node, "xmlnode");
	if (cvalue)
		xnode = (xmlNodePtr) g_value_get_pointer (cvalue);
	else
		xnode = mgr->priv->root;
	for (child = xnode->children; child; child = child->next) {
		GString *string = NULL;
		if (mgr->priv->attributes) {
			gint i;
			for (i = 0; ; i++) {
				xmlChar *prop;
				if (! mgr->priv->attributes[i])
					break;
				prop = xmlGetProp (child, BAD_CAST mgr->priv->attributes[i]);
				if (!prop)
					continue;
				if (!string)
					string = g_string_new ("");
				else
					g_string_append_c (string, ' ');
				g_string_append_printf (string, "[%s=%s]", mgr->priv->attributes[i],
							(gchar*) prop);
				xmlFree (prop);
			}
		}
		else {
			/* use all attributes */
			xmlAttrPtr attr;
			
			for (attr = child->properties; attr; attr = attr->next) {
				xmlChar *prop;
				prop = xmlGetProp (child, attr->name);
				if (!prop)
					continue;
				if (!string)
					string = g_string_new ("");
				else
					g_string_append_c (string, ' ');
				g_string_append_printf (string, "[%s=%s]", (gchar*) attr->name, (gchar*) prop);
				xmlFree (prop);
			}
		}
		if (string) {
			GValue *xvalue;
			GdaTreeNode* snode;
			snode = gda_tree_manager_create_node (manager, node, string->str);
			g_string_free (string, TRUE);
			g_value_set_pointer ((xvalue = gda_value_new (G_TYPE_POINTER)), child);
			gda_tree_node_set_node_attribute (snode, "xmlnode", xvalue, NULL);
			gda_value_free (xvalue);
			list = g_slist_prepend (list, snode);
		}
	}

	return list;
}
