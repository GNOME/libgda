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

/* 
 * copied from SQLite's sources to determine column affinity 
 */
#define SQLITE_AFF_TEXT     'a'
#define SQLITE_AFF_NONE     'b'
#define SQLITE_AFF_NUMERIC  'c'
#define SQLITE_AFF_INTEGER  'd'
#define SQLITE_AFF_REAL     'e'
static char get_affinity (const gchar *type)
{
	guint32 h = 0;
	char aff = SQLITE_AFF_NUMERIC;
	const unsigned char *ptr = (unsigned char *) type;
	
	while( *ptr ){
		h = (h<<8) + g_ascii_tolower(*ptr);
		ptr++;
		if( h==(('c'<<24)+('h'<<16)+('a'<<8)+'r') ){             /* CHAR */
			aff = SQLITE_AFF_TEXT;
		}else if( h==(('c'<<24)+('l'<<16)+('o'<<8)+'b') ){       /* CLOB */
			aff = SQLITE_AFF_TEXT;
		}else if( h==(('t'<<24)+('e'<<16)+('x'<<8)+'t') ){       /* TEXT */
			aff = SQLITE_AFF_TEXT;
		}else if( h==(('b'<<24)+('l'<<16)+('o'<<8)+'b')          /* BLOB */
			  && (aff==SQLITE_AFF_NUMERIC || aff==SQLITE_AFF_REAL) ){
			aff = SQLITE_AFF_NONE;
		}else if( h==(('r'<<24)+('e'<<16)+('a'<<8)+'l')          /* REAL */
			  && aff==SQLITE_AFF_NUMERIC ){
			aff = SQLITE_AFF_REAL;
		}else if( h==(('f'<<24)+('l'<<16)+('o'<<8)+'a')          /* FLOA */
			  && aff==SQLITE_AFF_NUMERIC ){
			aff = SQLITE_AFF_REAL;
		}else if( h==(('d'<<24)+('o'<<16)+('u'<<8)+'b')          /* DOUB */
			  && aff==SQLITE_AFF_NUMERIC ){
			aff = SQLITE_AFF_REAL;
		}else if( (h&0x00FFFFFF)==(('i'<<16)+('n'<<8)+'t') ){    /* INT */
			aff = SQLITE_AFF_INTEGER;
			break;
		}
	}
	
	return aff;
}

void
_gda_sqlite_update_types_hash (SqliteConnectionData *scnc)
{
	GHashTable *types;
	gint status;
	sqlite3_stmt *tables_stmt = NULL;

	types = scnc->types;
	if (!types) {
		types = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL); /* key= type name, value= gda type */
		scnc->types = types;
	}
	
	g_hash_table_insert (types, g_strdup ("integer"), GINT_TO_POINTER (G_TYPE_INT));
	g_hash_table_insert (types, g_strdup ("boolean"), GINT_TO_POINTER (G_TYPE_BOOLEAN));
	g_hash_table_insert (types, g_strdup ("date"), GINT_TO_POINTER (G_TYPE_DATE));
	g_hash_table_insert (types, g_strdup ("time"), GINT_TO_POINTER (GDA_TYPE_TIME));
	g_hash_table_insert (types, g_strdup ("timestamp"), GINT_TO_POINTER (GDA_TYPE_TIMESTAMP));
	g_hash_table_insert (types, g_strdup ("real"), GINT_TO_POINTER (G_TYPE_DOUBLE));
	g_hash_table_insert (types, g_strdup ("text"), GINT_TO_POINTER (G_TYPE_STRING));
	g_hash_table_insert (types, g_strdup ("blob"), GINT_TO_POINTER (GDA_TYPE_BINARY));

	/* HACK: force SQLite to reparse the schema and thus discover new tables if necessary */
	status = sqlite3_prepare_v2 (scnc->connection, "SELECT 1 FROM sqlite_master LIMIT 1", -1, &tables_stmt, NULL);
	if (status == SQLITE_OK)
		sqlite3_step (tables_stmt);
	if (tables_stmt)
		sqlite3_finalize (tables_stmt);

	/* build a list of tables */
	status = sqlite3_prepare_v2 (scnc->connection, "SELECT name "
				     " FROM (SELECT * FROM sqlite_master UNION ALL "
				     "       SELECT * FROM sqlite_temp_master) "
				     " WHERE name not like 'sqlite_%%'", -1, &tables_stmt, NULL);
	if ((status != SQLITE_OK) || !tables_stmt)
		return;
	for (status = sqlite3_step (tables_stmt); status == SQLITE_ROW; status = sqlite3_step (tables_stmt)) {
		gchar *sql;
		sqlite3_stmt *fields_stmt;
		gint fields_status;

		sql = g_strdup_printf ("PRAGMA table_info('%s');", sqlite3_column_text (tables_stmt, 0));
		fields_status = sqlite3_prepare_v2 (scnc->connection, sql, -1, &fields_stmt, NULL);
		g_free (sql);
		if ((fields_status != SQLITE_OK) || !fields_stmt)
			break;
		
		for (fields_status = sqlite3_step (fields_stmt); fields_status == SQLITE_ROW; 
		     fields_status = sqlite3_step (fields_stmt)) {
			const gchar *typname = sqlite3_column_text (fields_stmt, 2);
			if (typname && !g_hash_table_lookup (types, typname)) {
				GType type;
				switch (get_affinity (typname)) {
				case SQLITE_AFF_INTEGER:
					type = G_TYPE_INT;
					break;
				case SQLITE_AFF_REAL:
					type = G_TYPE_DOUBLE;
					break;
				default:
					type = G_TYPE_STRING;
					break;
				}
				g_hash_table_insert (types, g_strdup (typname), GINT_TO_POINTER (type));
			}
		}
		sqlite3_finalize (fields_stmt);
	}
	sqlite3_finalize (tables_stmt);
}
