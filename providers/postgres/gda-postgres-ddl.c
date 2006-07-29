/* GNOME DB Postgres Provider
 * Copyright (C) 2006 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
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

	/* CREATE TABLE */
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

	/* DROP TABLE */
	string = g_string_new ("DROP TABLE ");
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

	/* CREATE INDEX */
	string = g_string_new ("CREATE INDEX ");
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

	/* DROP INDEX */
	string = g_string_new ("DROP INDEX ");
	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

