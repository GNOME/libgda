/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation
 *
 * AUTHORS:
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Carlos Perello Marin <carlos@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-sqlite.h"
#include <libgda/gda-connection-private.h>
#include "gda-sqlite-recordset.h"

static guint
nocase_str_hash (gconstpointer v)
{
	guint ret;
	gchar *up = g_ascii_strup ((gchar *) v, -1);
	ret = g_str_hash ((gconstpointer) up);
	g_free (up);
	return ret;
}

static gboolean
nocase_str_equal (gconstpointer v1, gconstpointer v2)
{
	return g_ascii_strcasecmp ((gchar *) v1, (gchar *) v2) == 0 ? TRUE : FALSE;
}

void
_gda_sqlite_compute_types_hash (SqliteConnectionData *cdata)
{
	GHashTable *types;
	types = cdata->types;
	if (!types) {
		types = g_hash_table_new_full (nocase_str_hash, nocase_str_equal, g_free, NULL); 
		/* key= type name, value= gda type */
		cdata->types = types;
	}
	
	g_hash_table_insert (types, g_strdup ("integer"), GINT_TO_POINTER (G_TYPE_INT));
	g_hash_table_insert (types, g_strdup ("int"), GINT_TO_POINTER (G_TYPE_INT));
	g_hash_table_insert (types, g_strdup ("boolean"), GINT_TO_POINTER (G_TYPE_BOOLEAN));
	g_hash_table_insert (types, g_strdup ("date"), GINT_TO_POINTER (G_TYPE_DATE));
	g_hash_table_insert (types, g_strdup ("time"), GINT_TO_POINTER (GDA_TYPE_TIME));
	g_hash_table_insert (types, g_strdup ("timestamp"), GINT_TO_POINTER (GDA_TYPE_TIMESTAMP));
	g_hash_table_insert (types, g_strdup ("real"), GINT_TO_POINTER (G_TYPE_DOUBLE));
	g_hash_table_insert (types, g_strdup ("text"), GINT_TO_POINTER (G_TYPE_STRING));
	g_hash_table_insert (types, g_strdup ("string"), GINT_TO_POINTER (G_TYPE_STRING));
	g_hash_table_insert (types, g_strdup ("blob"), GINT_TO_POINTER (GDA_TYPE_BINARY));
}

GType
_gda_sqlite_compute_g_type (int sqlite_type)
{
	switch (sqlite_type) {
	case SQLITE_INTEGER:
		return G_TYPE_INT;
	case SQLITE_FLOAT:
		return G_TYPE_DOUBLE;
	case 0:
	case SQLITE_TEXT:
		return G_TYPE_STRING;
	case SQLITE_BLOB:
		return GDA_TYPE_BINARY;
	case SQLITE_NULL:
		return GDA_TYPE_NULL;
	default:
		g_warning ("Unknown SQLite internal data type %d", sqlite_type);
		return G_TYPE_STRING;
	}
}

