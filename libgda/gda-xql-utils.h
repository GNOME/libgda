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
#if !defined(__gda_xql_utils_h__)
#  define __gda_xql_utils_h__

#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/documents.h>
#include <libxslt/xsltutils.h>

gchar    *gda_xql_gensym (gchar *sym);
gboolean  gda_xql_destroy_hash_pair (gchar *key, gpointer *value, GFreeFunc func);
gchar    *gda_xql_dom_to_xml        (xmlNode *node,  gboolean freedoc);
gchar    *gda_xql_dom_to_sql        (xmlNode * node, gboolean freedoc);
xmlNode  *gda_xql_new_node          (gchar *tag, xmlNode *parNode);
void      gda_xql_new_attr          (gchar * key, gchar * value, xmlNode * node);

#endif
