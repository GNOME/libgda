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
 * gda_xml_util_gensym
 */
gchar *
gda_xml_util_gensym (const gchar * sym)
{
	static gint count = 0;

	return g_strdup_printf ("%s%d", sym, ++count);
}

/**
 * gda_xml_util_dom_to_xml
 */
gchar *
gda_xml_util_dom_to_xml (xmlNodePtr node, gboolean freedoc)
{
	xmlDocPtr doc;
	gchar *buffer;
	gint size;

	g_return_val_if_fail (node != NULL, NULL);

	doc = node->doc;
	xmlDocDumpMemory (doc, (xmlChar **) & buffer, &size);
	if (freedoc)
		xmlFreeDoc (doc);

	return buffer;
}

/**
 * gda_xml_util_dom_to_sql
 */
gchar *
gda_xml_util_dom_to_sql (xmlNodePtr node, gboolean freedoc)
{
	/* FIXME: see gda-xml-query/utils.c (xml_query_dom_to_sql) */
}

/**
 * gda_xml_util_new_node
 */
xmlNodePtr
gda_xml_util_new_node (const gchar * tag, xmlNodePtr parent_node)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	xmlParserInputBuffer *input;

	if (parent_node == NULL) {
		doc = xmlNewDoc ("1.0");
#if 0
		doc->extSubset =
			xmlIOParseDTD (NULL, input, XML_CHAR_ENCODING_NONE);
#endif

		node = xmlNewDocNode (doc, NULL, (xmlChar *) tag, NULL);
		xmlDocSetRootElement (doc, node);
	}
	else
		node = xmlNewChild (parent_node, NULL, (xmlChar *) tag, NULL);

	return node;
}

/**
 * gda_xml_util_new_attr
 */
void
gda_xml_util_new_attr (gchar * key, gchar * value, xmlNodePtr node)
{
	xmlAttr *attr;
	xmlDocPtr doc;

	g_return_if_fail (node != NULL);

	doc = node->doc;
	attr = xmlSetProp (node, key, value);

	if (xmlIsID (doc, node, attr))
		xmlAddID (NULL, doc, value, attr);
	else if (xmlIsRef (doc, node, attr))
		xmlAddRef (NULL, doc, value, attr);
}
