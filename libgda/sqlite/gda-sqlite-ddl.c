/* GNOME DB Sqlite Provider
 * Copyright (C) 2006 - 2008 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Bas Driessen <bas.driessen@xobas.com>
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

#include "gda-sqlite-ddl.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>


gchar *
_gda_sqlite_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gboolean allok = TRUE;
	gboolean hasfields = FALSE;
	gint nrows;
	gint i;
	gboolean first;
	GSList *pkfields = NULL; /* list of GValue* composing the pkey */
	gint nbpkfields = 0;
	gchar *sql = NULL;
	gchar *conflict_algo = NULL;

	/* CREATE TABLE */
	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");
	g_string_append (string, "TABLE ");

	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");
		
	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));
	g_string_append (string, " (");
		
	/* FIELDS */
	if (allok) {
		GdaServerOperationNode *node;

		node = gda_server_operation_get_node_info (op, "/FIELDS_A");
		g_assert (node);

		/* finding if there is a composed primary key */
		nrows = gda_data_model_get_n_rows (node->model);
		for (i = 0; i < nrows; i++) {
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_PKEY/%d", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
				pkfields = g_slist_append (pkfields,
							   (GValue *) gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_NAME/%d", i));
		}
		nbpkfields = g_slist_length (pkfields);

		/* manually defined fields */
		first = TRUE;
		for (i = 0; i < nrows; i++) {
			gboolean pkautoinc = FALSE;
			hasfields = TRUE;
			if (first) 
				first = FALSE;
			else
				g_string_append (string, ", ");
				
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_NAME/%d", i);
			g_string_append (string, g_value_get_string (value));
			g_string_append_c (string, ' ');
				
			if (nbpkfields == 1) {
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_AUTOINC/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
					const gchar *tmp;
					value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
					tmp = g_value_get_string (value);
					if (!g_ascii_strcasecmp (tmp, "gint") ||
					    !g_ascii_strcasecmp (tmp, "int")) {
						g_string_append (string, "INTEGER PRIMARY KEY AUTOINCREMENT");
						pkautoinc = TRUE;
					}
				}
			}

			if (!pkautoinc) {
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
				g_string_append (string, g_value_get_string (value));
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_SIZE/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_UINT)) {
					g_string_append_printf (string, "(%d", g_value_get_uint (value));
					
					value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_SCALE/%d", i);
					if (value && G_VALUE_HOLDS (value, G_TYPE_UINT))
						g_string_append_printf (string, ",%d)", g_value_get_uint (value));
					else
						g_string_append (string, ")");
				}
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_DEFAULT/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
					const gchar *str = g_value_get_string (value);
					if (str && *str) {
						g_string_append (string, " DEFAULT ");
						g_string_append (string, str);
					}
				}
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_NNUL/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
					g_string_append (string, " NOT NULL");
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_UNIQUE/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
					g_string_append (string, " UNIQUE");

				if (nbpkfields == 1) {
					value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_PKEY/%d", i);
					if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
						g_string_append (string, " PRIMARY KEY");
						
						value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_CONFLICT/%d", i);
						if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
							const gchar *str = g_value_get_string (value);
							if (str && *str) {
								g_string_append (string, " ON CONFLICT ");
								g_string_append (string, str);
							}
							
						} 
						value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_AUTOINC/%d", i);
						if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
							g_string_append (string, " AUTOINCREMENT");
						}
					}
					
				}
				else {
					if (!conflict_algo) {
						value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_CONFLICT/%d", i);
						if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
							const gchar *str = g_value_get_string (value);
							if (str && *str) 
								conflict_algo = g_strdup (str);
						} 
					}
				}
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_CHECK/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
					const gchar *str = g_value_get_string (value);
					if (str && *str) {
						g_string_append (string, " CHECK (");
						g_string_append (string, str);
						g_string_append_c (string, ')');
					}
				}
				
				value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_COLLATE/%d", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
					const gchar *str = g_value_get_string (value);
					if (str && *str) {
						g_string_append (string, " COLLATE ");
						g_string_append (string, str);
					}
				}
			}
		}
	}

	/* composed primary key */
	if (nbpkfields > 1) {
		GSList *list = pkfields;

		g_string_append (string, ", PRIMARY KEY (");
		while (list) {
			if (list != pkfields)
				g_string_append (string, ", ");
			g_string_append (string, g_value_get_string ((GValue*) list->data));
			list = list->next;
		}
		g_string_append_c (string, ')');
		
		if (conflict_algo) {
			g_string_append (string, " ON CONFLICT ");
			g_string_append (string, conflict_algo);
		}
	}

	g_free (conflict_algo);
	g_string_append (string, ")");

	if (!hasfields) {
		allok = FALSE;
		g_set_error (error, 0, 0, "%s", _("Table to create must have at least one row"));
	}
	g_slist_free (pkfields);


	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_DROP_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	/* DROP TABLE */
	string = g_string_new ("DROP TABLE");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append_c (string, ' ');
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	/* DROP TABLE */
	string = g_string_new ("ALTER TABLE ");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NEW_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, " RENAME TO ");
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_ADD_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
			      GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	/* DROP TABLE */
	string = g_string_new ("ALTER TABLE ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	g_string_append (string, " ADD COLUMN ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_TYPE");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append_c (string, ' ');
	g_string_append (string, g_value_get_string (value));
				
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_SIZE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_UINT)) {
		g_string_append_printf (string, "(%d", g_value_get_uint (value));

		value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_SCALE");
		if (value && G_VALUE_HOLDS (value, G_TYPE_UINT))
			g_string_append_printf (string, ",%d)", g_value_get_uint (value));
		else
			g_string_append (string, ")");
	}

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_DEFAULT");
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_DEFAULT");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		const gchar *str = g_value_get_string (value);
		if (str && *str) {
			g_string_append (string, " DEFAULT ");
			g_string_append (string, str);
		}
	}
				
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_NNUL");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " NOT NULL");
				
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_CHECK");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		const gchar *str = g_value_get_string (value);
		if (str && *str) {
			g_string_append (string, " CHECK (");
			g_string_append (string, str);
			g_string_append_c (string, ')');
		}
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}


gchar *
_gda_sqlite_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	GdaServerOperationNode *node;
	gint nrows, i;

	/* CREATE INDEX */
	string = g_string_new ("CREATE ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_TYPE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, g_value_get_string (value));
		g_string_append_c (string, ' ');
	}

	g_string_append (string, "INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF NOT EXISTS ");
	

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	g_string_append (string, " ON ");
	
	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_ON_TABLE");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));


	/* fields or expressions the index is on */
	g_string_append (string, " (");
	node = gda_server_operation_get_node_info (op, "/INDEX_FIELDS_S");
	g_assert (node);
	nrows = gda_server_operation_get_sequence_size (op, "/INDEX_FIELDS_S");
	for (i = 0; i < nrows; i++) {
		value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_FIELD", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
			if (i != 0)
				g_string_append (string, ", ");
			g_string_append (string, g_value_get_string (value));
			
			value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_COLLATE", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
				const gchar *str = g_value_get_string (value);
				if (str && *str) {
					g_string_append (string, " COLLATE ");
					g_string_append (string, str);
				}
			}


			value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_SORT_ORDER", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
				const gchar *str = g_value_get_string (value);
				if (str && *str) {
					g_string_append_c (string, ' ');
					g_string_append (string, str);
				}
			}
		}
	}

	g_string_append (string, ")");

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_DROP_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	/* DROP INDEX */
	string = g_string_new ("DROP INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF EXISTS ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_CREATE_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gboolean allok = TRUE;
	gchar *sql = NULL;

	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");

	g_string_append (string, "VIEW ");

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");

		
	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));
	
	if (allok) {
		value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_DEF");
		g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
		g_string_append (string, " AS ");
		g_string_append (string, g_value_get_string (value));
	}

	if (allok) {
		sql = string->str;
		g_string_free (string, FALSE);
	}
	else {
		sql = NULL;
		g_string_free (string, TRUE);
	}

	return sql;
}
	
gchar *
_gda_sqlite_render_DROP_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP VIEW");

	value = gda_server_operation_get_value_at (op, "/VIEW_DESC_P/VIEW_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	value = gda_server_operation_get_value_at (op, "/VIEW_DESC_P/VIEW_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append_c (string, ' ');
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}
