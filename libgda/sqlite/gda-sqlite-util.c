/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include "gda-sqlite-util.h"
#include <stdlib.h>
#include <string.h>
#include "gda-sqlite.h"
#include <libgda/gda-connection-private.h>
#undef GDA_DISABLE_DEPRECATED
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include "gda-sqlite-recordset.h"

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.code" /* this one is dynamically generated */

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
	if (!cdata->types_hash) {
		gint i;
		GType type, *array;
		GHashTable *hash;
		cdata->types_hash = g_hash_table_new (nocase_str_hash, nocase_str_equal);
		hash = cdata->types_hash;
#define NB_DECLARED_G_TYPES 14
		cdata->types_array = g_new (GType, NB_DECLARED_G_TYPES);
		array = cdata->types_array;

		type = G_TYPE_INT;
		i = 0;
		array [i] = type;
		g_hash_table_insert (hash, "integer", array + i);
		g_hash_table_insert (hash, "int", array + i);

		type = G_TYPE_UINT;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "unsigned integer", array + i);
		g_hash_table_insert (hash, "unsigned int", array + i);
		g_hash_table_insert (hash, "uint", array + i);

		type = G_TYPE_BOOLEAN;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "boolean", array + i);

		type = G_TYPE_DATE;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "date", array + i);

		type = GDA_TYPE_TIME;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "time", array + i);

		type = G_TYPE_DATE_TIME;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "timestamp", array + i);

		type = G_TYPE_DOUBLE;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "real", array + i);

		type = G_TYPE_STRING;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "text", array + i);
		g_hash_table_insert (hash, "string", array + i);
		g_hash_table_insert (hash, "varchar", array + i);

		type = GDA_TYPE_BINARY;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "binary", array + i);

		type = GDA_TYPE_BLOB;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "blob", array + i);

		type = G_TYPE_INT64;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "int64", array + i);
		g_hash_table_insert (hash, "bigint", array + i);

		type = G_TYPE_UINT64;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "uint64", array + i);

		type = GDA_TYPE_SHORT;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "short", array + i);

		type = GDA_TYPE_USHORT;
		i++;
		array [i] = type;
		g_hash_table_insert (hash, "ushort", array + i);
		g_hash_table_insert (hash, "unsigned short", array + i);
		g_assert (i < NB_DECLARED_G_TYPES);
	}
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
		return GDA_TYPE_BLOB;
	case SQLITE_NULL:
		return GDA_TYPE_NULL;
	default:
		g_warning ("Unknown SQLite internal data type %d", sqlite_type);
		return G_TYPE_STRING;
	}
}



GdaSqlReservedKeywordsFunc
_gda_sqlite_get_reserved_keyword_func (void)
{
#ifdef GDA_DEBUG
  test_keywords ();
#endif
  return is_keyword;
}

static gchar *
identifier_add_quotes (const gchar *str)
{
        gchar *retval, *rptr;
        const gchar *sptr;
        gint len;

        if (!str)
                return NULL;

        len = strlen (str);
        retval = g_new (gchar, 2*len + 3);
        *retval = '"';
        for (rptr = retval+1, sptr = str; *sptr; sptr++, rptr++) {
                if (*sptr == '"') {
                        *rptr = '"';
                        rptr++;
                        *rptr = *sptr;
                }
                else
                        *rptr = *sptr;
        }
        *rptr = '"';
        rptr++;
        *rptr = 0;
        return retval;
}

static gboolean
_sql_identifier_needs_quotes (const gchar *str)
{
	const gchar *ptr;

	g_return_val_if_fail (str, FALSE);
	for (ptr = str; *ptr; ptr++) {
		/* quote if 1st char is a number */
		if ((*ptr <= '9') && (*ptr >= '0')) {
			if (ptr == str)
				return TRUE;
			continue;
		}
		if (((*ptr >= 'A') && (*ptr <= 'Z')) ||
		    ((*ptr >= 'a') && (*ptr <= 'z')))
			continue;

		if ((*ptr != '$') && (*ptr != '_') && (*ptr != '#'))
			return TRUE;
	}
	return FALSE;
}

/* Returns: @str */
static gchar *
sqlite_remove_quotes (gchar *str)
{
        glong total;
        gchar *ptr;
        glong offset = 0;
	char delim;
	
	if (!str)
		return NULL;
	delim = *str;
	if ((delim != '[') && (delim != '"') && (delim != '\'') && (delim != '`'))
		return str;

        total = strlen (str);
        if ((str[total-1] == delim) || ((delim == '[') && (str[total-1] == ']'))) {
		/* string is correctly terminated */
		memmove (str, str+1, total-2);
		total -=2;
	}
	else {
		/* string is _not_ correctly terminated */
		memmove (str, str+1, total-1);
		total -=1;
	}
        str[total] = 0;

	if ((delim == '"') || (delim == '\'')) {
		ptr = (gchar *) str;
		while (offset < total) {
			/* we accept the "''" as a synonym of "\'" */
			if (*ptr == delim) {
				if (*(ptr+1) == delim) {
					memmove (ptr+1, ptr+2, total - offset);
					offset += 2;
				}
				else {
					*str = 0;
					return str;
				}
			}
			else if (*ptr == '"') {
				if (*(ptr+1) == '"') {
					memmove (ptr+1, ptr+2, total - offset);
					offset += 2;
				}
				else {
					*str = 0;
					return str;
				}
			}
			else if (*ptr == '\\') {
				if (*(ptr+1) == '\\') {
					memmove (ptr+1, ptr+2, total - offset);
					offset += 2;
				}
				else {
					if (*(ptr+1) == delim) {
						*ptr = delim;
						memmove (ptr+1, ptr+2, total - offset);
						offset += 2;
					}
					else {
						*str = 0;
						return str;
					}
				}
			}
			else
				offset ++;
			
			ptr++;
		}
	}

        return str;
}

gchar *
_gda_sqlite_identifier_quote (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc,
			     const gchar *id,
			     gboolean for_meta_store, gboolean force_quotes)
{
        GdaSqlReservedKeywordsFunc kwfunc;
        kwfunc = _gda_sqlite_get_reserved_keyword_func ();

	if (for_meta_store) {
		gchar *tmp, *ptr;
		tmp = sqlite_remove_quotes (g_strdup (id));
		if (kwfunc (tmp)) {
			ptr = gda_sql_identifier_force_quotes (tmp);
			g_free (tmp);
			return ptr;
		}
		else {
			/* if only alphanum => don't quote */
			for (ptr = tmp; *ptr; ptr++) {
				if ((*ptr >= 'A') && (*ptr <= 'Z'))
					*ptr += 'a' - 'A';
				if (((*ptr >= 'a') && (*ptr <= 'z')) ||
				    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
				    (*ptr >= '_'))
					continue;
				else {
					ptr = gda_sql_identifier_force_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			return tmp;
		}
	}
	else {
		if (*id == '"') {
			/* there are already some quotes */
			return g_strdup (id);
		}
		else if ((*id == '[') || (*id == '`')) {
			/* there are already some quotes */
			gchar *tmp, *ptr;
			tmp = sqlite_remove_quotes (g_strdup (id));
			ptr = gda_sql_identifier_force_quotes (tmp);
			g_free (tmp);
			return ptr;
		}
		if (kwfunc (id) || _sql_identifier_needs_quotes (id) || force_quotes)
			return identifier_add_quotes (id);

		/* nothing to do */
		return g_strdup (id);
	}
}

gboolean
_gda_sqlite_check_transaction_started (GdaConnection *cnc, gboolean *out_started, GError **error)
{
	GdaTransactionStatus *trans;

        trans = gda_connection_get_transaction_status (cnc);
        if (!trans) {
		if (!gda_connection_begin_transaction (cnc, NULL,
						       GDA_TRANSACTION_ISOLATION_UNKNOWN, error))
			return FALSE;
		else
			*out_started = TRUE;
	}
	return TRUE;
}
