/* GDA common library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "gda-xml-util.h"

/**
 * gda_xml_util_get_children
 * @parent: The parent node
 *
 * Return a #xmlNodePtr containing all the children nodes for
 * the given XML node
 */
xmlNodePtr
gda_xml_util_get_children (xmlNodePtr parent)
{
	g_return_val_if_fail (parent != NULL, NULL);
	return parent->xmlChildrenNode;
}

/**
 * gda_xml_util_get_root_children
 */
xmlNodePtr
gda_xml_util_get_root_children (xmlDocPtr doc)
{
	return gda_xml_util_get_children (xmlDocGetRootElement (doc));
}

/**
 * gda_xml_util_get_child_by_name
 */
xmlNodePtr
gda_xml_util_get_child_by_name (xmlNodePtr parent, const gchar *name)
{
	xmlNodePtr child;

	g_return_val_if_fail (parent != NULL, NULL);

	for (child = gda_xml_util_get_children (parent);
	     child != NULL;
	     child = child->next) {
		if (strcmp (child->name, name) == 0)
			return child;
	}

	return NULL;
}

/**
 * gda_xml_util_remove_node
 */
void
gda_xml_util_remove_node (xmlNodePtr node)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (node->doc != NULL);
	g_return_if_fail (node->parent != NULL);
        g_return_if_fail (node->doc->xmlRootNode != node);

	if (node->prev == NULL) {
                g_assert (node->parent->xmlChildrenNode == node);
                node->parent->xmlChildrenNode = node->next;
        }
	else {
                g_assert (node->parent->xmlChildrenNode != node);
                node->prev->next = node->next;
        }

        if (node->next == NULL) {
                g_assert (node->parent->last == node);
                node->parent->last = node->prev;
        }
	else {
                g_assert (node->parent->last != node);
                node->next->prev = node->prev;
        }

        node->doc = NULL;
        node->parent = NULL;
        node->next = NULL;
        node->prev = NULL;
}
