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

#if !defined(__gda_xml_util_h__)
#  define __gda_xml_util_h__

#include <glib.h>
#include <tree.h>
#include <parser.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define    gda_xml_util_get_root_element(_doc_) xmlDocGetRootElement ((_doc_))

xmlNodePtr gda_xml_util_get_children (xmlNodePtr parent);
xmlNodePtr gda_xml_util_get_root_children (xmlDocPtr doc);
xmlNodePtr gda_xml_util_get_child_by_name (xmlNodePtr parent,
					   const gchar *name);
void       gda_xml_util_remove_node (xmlNodePtr node);

#if defined(__cplusplus)
}
#endif

#endif
