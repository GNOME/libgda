/* XmlQuery: the xml query object
 * Copyright (C) 2000-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Gerhard Dieringer <gdieringer@compuserve.com>
 * 	Cleber Rodrigues <cleber@gnome-db.org>
 *
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-xql-query.h"
#include "gda-xql-utils.h"

#define GDA_XQL_DOM_TO_SQL_STYLESHEET "gda_xql_sql_to_dom_stylesheet.xsl"
#define GDA_XQL_DTD "gda_xql.dtd"

void xmlMemFree (void *ptr);

gchar *
gda_xql_gensym (gchar *sym)
{
	static gint count = 0;

	return g_strdup_printf ("%s%d", sym, ++count);
}

gboolean
gda_xql_destroy_hash_pair (gchar *key, gpointer *value, GFreeFunc func)
{
	g_free (key);
	if ((func != NULL) && (value != NULL))
		func (value);
	return TRUE;
}

gchar *
gda_xql_dom_to_xml (xmlNode *node, gboolean freedoc)
{
	xmlDoc *doc;
	gchar *buffer;
	gint size;

	doc = node->doc;
	xmlDocDumpMemory (doc, (xmlChar **) & buffer, &size);
	if (freedoc)
		xmlFreeDoc (doc);

	return buffer;
}

gchar *
gda_xql_dom_to_sql (xmlNode *node, gboolean freedoc)
{
	xmlOutputBuffer *outbuf;
	xsltStylesheet *xsl;
	xmlDoc *doc, *res;
	gchar *buffer;

	xmlSubstituteEntitiesDefault (1);
	xmlLoadExtDtdDefaultValue = 0;
	xmlDoValidityCheckingDefaultValue = 0;
	xsl = xsltParseStylesheetFile ((const xmlChar *) GDA_XQL_DOM_TO_SQL_STYLESHEET);
	if (xsl != NULL) {
		if (xsl->indent == 1)
			xmlIndentTreeOutput = 1;
		else
			xmlIndentTreeOutput = 0;
	}

	xmlLoadExtDtdDefaultValue = 1;
	xmlDoValidityCheckingDefaultValue = 1;

	doc = node->doc;

	res = xsltApplyStylesheet (xsl, doc, NULL);

	outbuf = xmlAllocOutputBuffer (NULL);
	xsltSaveResultTo (outbuf, res, xsl);

	xsltFreeStylesheet (xsl);
	xmlFreeDoc (res);
	if (freedoc)
		xmlFreeDoc (doc);

	buffer = g_strdup (outbuf->buffer->content);
	xmlOutputBufferClose (outbuf);

	return buffer;
}

xmlNode *
gda_xql_new_node (gchar *tag, xmlNode *parent)
{
	xmlNode *node;
	xmlDoc *doc;
	xmlParserInputBuffer *input;

	if (parent == NULL) {
		doc = xmlNewDoc ("1.0");

		input = xmlParserInputBufferCreateFilename (GDA_XQL_DTD,
							    XML_CHAR_ENCODING_NONE);
		doc->extSubset =
			xmlIOParseDTD (NULL, input, XML_CHAR_ENCODING_NONE);

		node = xmlNewDocNode (doc, NULL, (xmlChar *) tag, NULL);
		xmlDocSetRootElement (doc, node);
	}
	else {
		node = xmlNewChild (parent, NULL, (xmlChar *) tag, NULL);
	}

	return node;
}

void
gda_xql_new_attr (gchar *key, gchar *value, xmlNode *node)
{
	xmlAttr *attr;
	xmlDoc *doc;

	doc = node->doc;
	attr = xmlSetProp (node, key, value);

	if (xmlIsID (doc, node, attr))
		xmlAddID (NULL, doc, value, attr);
	else if (xmlIsRef (doc, node, attr))
		xmlAddRef (NULL, doc, value, attr);

}
