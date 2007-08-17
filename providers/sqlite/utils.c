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
#ifndef HAVE_SQLITE
#include "sqliteInt.h"
#endif
#include <libgda/gda-connection-private.h>
#include <libgda/gda-parameter-list.h>
#include "gda-sqlite-recordset.h"

void
gda_sqlite_update_types_hash (SQLITEcnc *scnc)
{
	GHashTable *types;

	types = scnc->types;
	if (!types) {
		types = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL); /* key= type name, value= gda type */
		scnc->types = types;
	}

	g_hash_table_insert (types, g_strdup ("integer"), GINT_TO_POINTER (G_TYPE_INT));
	g_hash_table_insert (types, g_strdup ("real"), GINT_TO_POINTER (G_TYPE_DOUBLE));
	g_hash_table_insert (types, g_strdup ("text"), GINT_TO_POINTER (G_TYPE_STRING));
	g_hash_table_insert (types, g_strdup ("blob"), GINT_TO_POINTER (GDA_TYPE_BINARY));

#ifdef HAVE_SQLITE
	SQLITEresult *sres;
	gint status;
	int rc;
        gboolean end = FALSE;

	sres = g_new0 (SQLITEresult, 1);
	status = sqlite3_prepare_v2 (scnc->connection, "PRAGMA table_types_list;", -1, &(sres->stmt), NULL);
	if (status != SQLITE_OK) 
		return;

	while (!end) {
                rc = sqlite3_step (sres->stmt);
                switch (rc) {
                case  SQLITE_ROW: {
			const gchar *typname, *aff;
			typname = sqlite3_column_text (sres->stmt, 2);
			aff = sqlite3_column_text (sres->stmt, 3);
			if (typname && !g_hash_table_lookup (types, typname)) {
				GType type = G_TYPE_STRING;
				if (aff) {
					if (*aff == 'i')
						type = G_TYPE_INT;
					else if (*aff == 'r')
						type = G_TYPE_DOUBLE;
				}
				g_hash_table_insert (types, g_strdup (typname), GINT_TO_POINTER (type));
			}
			break;
		}
		case SQLITE_DONE:
                        end = TRUE;
                        break;
		default:
			break;
		}
	}
	gda_sqlite_free_result (sres);
#else
	if (SQLITE_VERSION_NUMBER >= 3000000) {
		Db *db;
		gint i;


		for (i = OMIT_TEMPDB; i < scnc->connection->nDb; i++) {
			Hash *tab_hash;
			HashElem *tab_elem;
			Table *table;

			db = &(scnc->connection->aDb[i]);
			tab_hash = &(db->pSchema->tblHash);
			for (tab_elem = sqliteHashFirst (tab_hash); tab_elem ; tab_elem = sqliteHashNext (tab_elem)) {
				gint j;
				
				table = sqliteHashData (tab_elem);
				/*g_print ("table: %s\n", table->zName);*/
				for (j = 0; j < table->nCol; j++) {
					Column *column = &(table->aCol[j]);
					
					if (column->zType && !g_hash_table_lookup (types, column->zType)) {
						GType type;
						switch (column->affinity) {
						case SQLITE_AFF_INTEGER:
							type = G_TYPE_INT;
							break;
						case SQLITE_AFF_NUMERIC:
							/* We don't want numerical affinity to be set to
							 * GDA_TYPE_NUMERIC because it does not work for dates. */
							/* type = GDA_TYPE_NUMERIC; */
							/* break; */
						case SQLITE_AFF_TEXT:
						case SQLITE_AFF_NONE:
						default:
							type = G_TYPE_STRING;
							break;
						}
						g_hash_table_insert (types, g_strdup (column->zType), 
								     GINT_TO_POINTER (type));
					}
				}
			}
		}
	}
#endif
}

void
gda_sqlite_result_take_sql (SQLITEresult *sres, gchar *sql)
{
	gchar *ptr;
	g_strchug (sql);
	sres->sql = sql;
}

void
gda_sqlite_free_result (SQLITEresult *sres)
{
	if (!sres)
		return;

	g_free (sres->sql);
	if (sres->stmt)
		sqlite3_finalize (sres->stmt);
	if (sres->types)
		g_free (sres->types);
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
