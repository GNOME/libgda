/* XmlQuery: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 *
 * This library is free software; you can redistribute it and/or
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
#ifndef _UTILS_H_
#define _UTILS_H_

#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/documents.h>
#include <libxslt/xsltutils.h>


extern gchar * xml_query_gensym (gchar * sym);

extern gboolean
xml_query_destroy_hash_pair (gchar * key, gpointer * value, GFreeFunc func);

extern gchar *xml_query_dom_to_xml (xmlNode * node, gboolean freedoc);

extern gchar *xml_query_dom_to_sql (xmlNode * node, gboolean freedoc);

xmlNode *xml_query_new_node (gchar * tag, xmlNode * parNode);


void xml_query_new_attr (gchar * key, gchar * value, xmlNode * node);


#endif /* _UTILS_H_ */
