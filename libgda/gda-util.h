/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#if !defined(__gda_util_h__)
#  define __gda_util_h__

#include <glib/ghash.h>
#include <glib/glist.h>
#include <libgda/gda-parameter.h>
#include <libgda/gda-row.h>

G_BEGIN_DECLS

const gchar *gda_type_to_string (GdaValueType type);
GdaValueType gda_type_from_string (const gchar *str);

GList       *gda_string_hash_to_list (GHashTable *hash_table);

/*
 * SQL parsing utilities
 */

gchar *gda_sql_replace_placeholders (const gchar *sql, GdaParameterList *params);

/*
 * File management utility functions
 */

gchar    *gda_file_load (const gchar *filename);
gboolean  gda_file_save (const gchar *filename, const gchar *buffer, gint len);

G_END_DECLS

#endif
