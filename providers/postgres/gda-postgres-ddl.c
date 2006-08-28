/* GNOME DB Postgres Provider
 * Copyright (C) 2006 The GNOME Foundation
 *
 * AUTHORS:
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

#include "gda-postgres-ddl.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>

gchar *
gda_postgres_render_CREATE_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("CREATE DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/OWNER");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " OWNER ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/TEMPLATE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " TEMPLATE ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_CSET");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		GdaDataHandler *dh;
		gchar *str;
		
		dh = gda_server_provider_get_data_handler_gda (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);

		g_string_append (string, " ENCODING ");
		g_string_append (string, str);
		g_free (str);
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/TABLESPACE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " TABLESPACE ");
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;	
}

gchar *
gda_postgres_render_DROP_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;	
}


gchar *
gda_postgres_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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

	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");
	g_string_append (string, "TABLE ");
		
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
			hasfields = TRUE;
			if (first) 
				first = FALSE;
			else
				g_string_append (string, ", ");
				
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_NAME/%d", i);
			g_string_append (string, g_value_get_string (value));
			g_string_append_c (string, ' ');
				
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
			g_string_append (string, g_value_get_string (value));
				
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
				if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
					g_string_append (string, " PRIMARY KEY");
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
		}

		/* LIKE inheritance */
		nrows = gda_server_operation_get_sequence_size (op, "/TABLE_PARENTS_S");
		for (i = 0; i < nrows; i++) {
			value = gda_server_operation_get_value_at (op, "/TABLE_PARENTS_S/%d/TABLE_PARENT_COPY", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && !g_value_get_boolean (value)) {
				value = gda_server_operation_get_value_at (op, "/TABLE_PARENTS_S/%d/TABLE_PARENT_TABLE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
					const gchar *str = g_value_get_string (value);
					if (str && *str) {
						hasfields = TRUE;
						if (first) 
							first = FALSE;
						else
							g_string_append (string, ", ");

						g_string_append (string, "LIKE ");
						g_string_append (string, str);
						value = gda_server_operation_get_value_at (op, 
											   "/TABLE_PARENTS_S/%d/TABLE_PARENT_COPY_DEFAULTS", i);
						if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && 
						    g_value_get_boolean (value))
							g_string_append (string, " INCLUDING DEFAULTS");
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
	}

	/* foreign keys */
	if (allok) {
		GdaServerOperationNode *node;

		first = TRUE;
		node = gda_server_operation_get_node_info (op, "/FKEY_S");
		if (node) {
			nrows = gda_server_operation_get_sequence_size (op, "/FKEY_S");
			for (i = 0; i < nrows; i++) {
				gint nbfields, j;

				g_string_append (string, ", FOREIGN KEY (");
				node = gda_server_operation_get_node_info (op, "/FKEY_S/%d/FKEY_FIELDS_A", i);
				if (!node || ((nbfields = gda_data_model_get_n_rows (node->model)) == 0)) {
					allok = FALSE;
					g_set_error (error, 0, 0, _("No field specified in foreign key constraint"));
				}
				else {
					for (j = 0; j < nbfields; j++) {
						if (j != 0)
							g_string_append (string, ", ");
						value = gda_server_operation_get_value_at (op, 
											   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d", i, j);
						if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
							g_string_append (string, g_value_get_string (value));
						else {
							allok = FALSE;
							g_set_error (error, 0, 0, 
								     _("Empty field specified in foreign key constraint"));
						}
					}
				}
				g_string_append (string, ") REFERENCES ");
				value = gda_server_operation_get_value_at (op, "/FKEY_S/%d/FKEY_REF_TABLE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
					g_string_append (string, g_value_get_string (value));
				else {
					allok = FALSE;
					g_set_error (error, 0, 0, _("No referenced table specified in foreign key constraint"));
				}

				g_string_append (string, " (");
				for (j = 0; j < nbfields; j++) {
					if (j != 0)
						g_string_append (string, ", ");
					value = gda_server_operation_get_value_at (op, 
										   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d", i, j);
					if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
						g_string_append (string, g_value_get_string (value));
					else {
						allok = FALSE;
						g_set_error (error, 0, 0, 
							     _("Empty referenced field specified in foreign key constraint"));
					}
				}
				g_string_append_c (string, ')');
				value = gda_server_operation_get_value_at (op, "/FKEY_S/%d/FKEY_MATCH_TYPE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
					g_string_append_printf (string, " %s", g_value_get_string (value));
				value = gda_server_operation_get_value_at (op, "/FKEY_S/%d/FKEY_ONUPDATE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
					g_string_append_printf (string, " ON UPDATE %s", g_value_get_string (value));
				value = gda_server_operation_get_value_at (op, "/FKEY_S/%d/FKEY_ONDELETE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
					g_string_append_printf (string, " ON DELETE %s", g_value_get_string (value));
				value = gda_server_operation_get_value_at (op, "/FKEY_S/%d/FKEY_DEFERRABLE", i);
				if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
					g_string_append_printf (string, " %s", g_value_get_string (value));
			}
		}
	}

	g_string_append (string, ")");

	/* INHERITS */
	first = TRUE;
	nrows = gda_server_operation_get_sequence_size (op, "/TABLE_PARENTS_S");
	for (i = 0; i < nrows; i++) {
		value = gda_server_operation_get_value_at (op, "/TABLE_PARENTS_S/%d/TABLE_PARENT_COPY", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
			value = gda_server_operation_get_value_at (op, "/TABLE_PARENTS_S/%d/TABLE_PARENT_TABLE", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
				const gchar *str = g_value_get_string (value);
				if (str && *str) {
					hasfields = TRUE;
					if (first) {
						g_string_append (string, " INHERITS ");
						first = FALSE;
					}
					else
						g_string_append (string, ", ");
					g_string_append (string, str);
				}
			}
		}
	}

	if (!hasfields) {
		allok = FALSE;
		g_set_error (error, 0, 0, _("Table to create must have at least one row"));
	}

	if (allok) {
		value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_WITH_OIDS");
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			g_string_append (string, " WITH OIDS");
	}

	g_slist_free (pkfields);


	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_DROP_TABLE   (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP TABLE ");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

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
gda_postgres_render_ADD_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("ALTER TABLE ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/TABLE_ONLY");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "ONLY ");

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
				
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_UNIQUE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " UNIQUE");
	
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_PKEY");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " PRIMARY KEY");
				
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
gda_postgres_render_DROP_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("ALTER TABLE ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/COLUMN_DESC_P/COLUMN_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, " DROP COLUMN ");
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/COLUMN_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		const gchar *str = g_value_get_string (value);
		if (str && *str) {
			g_string_append_c (string, ' ');
			g_string_append (string, str);
		}
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_postgres_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	GdaServerOperationNode *node;
	gint nrows, i;

	string = g_string_new ("CREATE ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_TYPE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, g_value_get_string (value));
		g_string_append_c (string, ' ');
	}

	g_string_append (string, "INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	g_string_append (string, " ON ");
	
	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_ON_TABLE");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_METHOD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " USING ");
		g_string_append (string, g_value_get_string (value));
	}

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
		}
	}

	g_string_append (string, ")");

	/* options */
	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_TABLESPACE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " TABLESPACE ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_PREDICATE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " WHERE ");
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_DROP_INDEX   (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

