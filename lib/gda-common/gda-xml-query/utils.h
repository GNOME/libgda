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
#include <tree.h>
#include <parser.h>

extern gchar
*gensym(gchar *sym);

extern gboolean
destroy_hash_pair(gchar *key, gpointer *value, GFreeFunc func);

extern gchar *
dom_to_xml(xmlNode *node, gboolean freedoc);

#endif /* _UTILS_H_ */
