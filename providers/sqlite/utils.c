/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-sqlite.h"
#include "sqliteInt.h"

void
gda_sqlite_update_types_hash (SQLITEcnc *scnc)
{
	GHashTable *types = scnc->types;

	if (!types) {
		types = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL); /* key= type name, value= gda type */
		scnc->types = types;
	}

	if (SQLITE_VERSION_NUMBER >= 3000000) {
		Db *db;
		gint i;

		g_hash_table_insert (types, g_strdup ("integer"), GINT_TO_POINTER (GDA_VALUE_TYPE_INTEGER));
		g_hash_table_insert (types, g_strdup ("real"), GINT_TO_POINTER (GDA_VALUE_TYPE_DOUBLE));
		g_hash_table_insert (types, g_strdup ("string"), GINT_TO_POINTER (GDA_VALUE_TYPE_STRING));
		g_hash_table_insert (types, g_strdup ("blob"), GINT_TO_POINTER (GDA_VALUE_TYPE_BLOB));

		for (i = OMIT_TEMPDB; i < scnc->connection->nDb; i++) {
			Hash *tab_hash;
			HashElem *tab_elem;
			Table *table;

			db = &(scnc->connection->aDb[i]);
			tab_hash = &(db->tblHash);
			for (tab_elem = sqliteHashFirst (tab_hash); tab_elem ; tab_elem = sqliteHashNext (tab_elem)) {
				GList *rowlist = NULL;
				GdaValue *value;
				gint j;
				
				table = sqliteHashData (tab_elem);
				/*g_print ("table: %s\n", table->zName);*/
				for (j = 0; j < table->nCol; j++) {
					Column *column = &(table->aCol[j]);
					
					if (column->zType && !g_hash_table_lookup (types, column->zType)) {
						GdaValueType type;
						switch (column->affinity) {
						case SQLITE_AFF_INTEGER:
							type = GDA_VALUE_TYPE_INTEGER;
							break;
						case SQLITE_AFF_NUMERIC:
							type = GDA_VALUE_TYPE_NUMERIC;
							break;
						case SQLITE_AFF_TEXT:
						case SQLITE_AFF_NONE:
						default:
							type = GDA_VALUE_TYPE_STRING;
							break;
						}
						g_hash_table_insert (types, g_strdup (column->zType), 
								     GINT_TO_POINTER (type));
					}
				}
			}
		}
	}
}

void
gda_sqlite_free_result (SQLITEresult *sres)
{
	if (!sres)
		return;

	if (sres->stmt)
		sqlite3_finalize (sres->stmt);
	if (sres->types)
		g_free (sres->types);
	if (sres->sqlite_types)
		g_free (sres->sqlite_types);
	if (sres->cols_size)
		g_free (sres->cols_size);
	g_free (sres);	
}

void
gda_sqlite_free_cnc (SQLITEcnc *scnc)
{
	if (!scnc)
		return;

	sqlite3_close (scnc->connection);
	g_free (scnc->file);
	if (scnc->types)
		g_hash_table_destroy (scnc->types);
	g_free (scnc);
}
