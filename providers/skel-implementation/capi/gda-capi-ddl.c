/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gda-capi-ddl.h"

gchar *
gda_capi_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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
	gchar *tmp;

	/* CREATE TABLE */
	string = g_string_new ("CREATE TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/TABLE_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);
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
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
				tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
										  "/FIELDS_A/@COLUMN_NAME/%d", error, i);
				if (!tmp) {
					g_string_free (string, TRUE);
					return NULL;
				}
				pkfields = g_slist_append (pkfields, tmp);
				nbpkfields++;
			}
		}

		/* manually defined fields */
		first = TRUE;
		for (i = 0; i < nrows; i++) {
			hasfields = TRUE;
			if (first) 
				first = FALSE;
			else
				g_string_append (string, ", ");
				
			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/FIELDS_A/@COLUMN_NAME/%d", error, i);
			if (!tmp) {
				g_string_free (string, TRUE);
				return NULL;
			}

			g_string_append (string, tmp);
			g_free (tmp);
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
		GSList *list;

		g_string_append (string, ", PRIMARY KEY (");
		for (list = pkfields; list; list = list->next) {
			if (list != pkfields)
				g_string_append (string, ", ");
			g_string_append (string, (gchar*) list->data);
		}
		g_string_append_c (string, ')');
	}
	g_slist_free_full (pkfields, (GDestroyNotify) g_free);

	g_string_append (string, ")");

	if (!hasfields) {
		allok = FALSE;
		g_set_error (error, GDA_SERVER_OPERATION_ERROR,
                             GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("Table to create must have at least one row"));
	}

	return g_string_free (string, FALSE);
}
