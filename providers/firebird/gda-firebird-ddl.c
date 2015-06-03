/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include "gda-firebird-ddl.h"

gchar *
gda_firebird_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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
	string = g_string_new ("CREATE TABLE ");

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

	g_string_append (string, ")");

	if (!hasfields) {
		allok = FALSE;
		g_set_error (error, GDA_SERVER_OPERATION_ERROR,
			     GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("Table to create must have at least one row"));
	}
	g_slist_free (pkfields);

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}
