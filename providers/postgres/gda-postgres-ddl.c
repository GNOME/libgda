/*
 * Copyright (C) 2006 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 - 2012 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/libgda.h>
#include "gda-postgres-ddl.h"
#include "gda-postgres.h"

gchar *
gda_postgres_render_CREATE_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("CREATE DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
  if (!value)
    return NULL;

	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) 
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
		
		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			g_string_append (string, " ENCODING ");
			g_string_append (string, str);
			g_free (str);
		}
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/TABLESPACE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " TABLESPACE ");
		g_string_append (string, g_value_get_string (value));
	}

	sql = g_string_free (string, FALSE);

	return sql;	
}

gchar *
gda_postgres_render_DROP_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	gchar *sql = NULL;
  const GValue *value = NULL;

	string = g_string_new ("DROP DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string(value))
		g_string_append (string, g_value_get_string (value));

	sql = g_string_free (string, FALSE);

	return sql;	
}

/*FIXME: Add error messages  */
gchar *
gda_postgres_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	const GValue *valuex;
	const GValue *value1 = NULL;
	gboolean hasfields = FALSE;
	gint nrows;
	gint i;
	gboolean first;
	GSList *pkfields = NULL; /* list of GValue* composing the pkey */
	gint nbpkfields = 0;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");
	g_string_append (string, "TABLE ");
	valuex = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
	if (valuex && G_VALUE_HOLDS (valuex, G_TYPE_BOOLEAN) && g_value_get_boolean (valuex))
		g_string_append (string, "IF NOT EXISTS ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
							  "/TABLE_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);
	g_string_append (string, " (");
		
	/* FIELDS */
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
			nbpkfields ++;
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

		value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_AUTOINC/%d", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			g_string_append (string, "serial");
		else {
			value1 = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
			g_string_append (string, g_value_get_string (value1));
		}

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
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && value1 != NULL) {
			const gchar *str = g_value_get_string (value);
			if (str && *str) {
				g_string_append (string, " DEFAULT ");
        const gchar* valtmp = g_value_get_string (value1);
        if (!g_ascii_strcasecmp (valtmp,"string") ||
            !g_ascii_strcasecmp (valtmp,"varchar")) {
          g_string_append_c (string,'\'');
          g_string_append (string, str);
          g_string_append_c (string,'\'');
        }
        else
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
			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/TABLE_PARENTS_S/%d/TABLE_PARENT_TABLE",
									  error, i);
			if (!tmp) {
				g_string_free (string, TRUE);
				return NULL;
			}

			hasfields = TRUE;
			if (first) 
				first = FALSE;
			else
				g_string_append (string, ", ");

			g_string_append (string, "LIKE ");
			g_string_append (string, tmp);
			value = gda_server_operation_get_value_at (op, 
								   "/TABLE_PARENTS_S/%d/TABLE_PARENT_COPY_DEFAULTS", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && 
			    g_value_get_boolean (value))
				g_string_append (string, " INCLUDING DEFAULTS");
			g_free (tmp);
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

	/* foreign keys */
	first = TRUE;
	node = gda_server_operation_get_node_info (op, "/FKEY_S");
	if (node) {
		nrows = gda_server_operation_get_sequence_size (op, "/FKEY_S");
		for (i = 0; i < nrows; i++) {
			gint nbfields = 0;
			gint j;

			g_string_append (string, ", FOREIGN KEY (");
			node = gda_server_operation_get_node_info (op, "/FKEY_S/%d/FKEY_FIELDS_A", i);
			if (!node || ((nbfields = gda_data_model_get_n_rows (node->model)) == 0)) {
				g_string_free (string, TRUE);
				g_set_error (error, GDA_SERVER_OPERATION_ERROR,
					     GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
					     "%s",
					     _("No field specified in foreign key constraint"));
				return NULL;
			}
			else {
				for (j = 0; j < nbfields; j++) {
					if (j != 0)
						g_string_append (string, ", ");
					tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
											  "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
											  error, i, j);
					if (tmp) {
						g_string_append (string, tmp);
						g_free (tmp);
					}
					else {
						g_string_free (string, TRUE);
						return NULL;
					}
				}
			}
			g_string_append (string, ") REFERENCES ");

			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/FKEY_S/%d/FKEY_REF_TABLE", error, i);
			if (tmp) {
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else {
				g_string_free (string, TRUE);
				return NULL;
			}

			g_string_append (string, " (");
			for (j = 0; j < nbfields; j++) {
				if (j != 0)
					g_string_append (string, ", ");
				tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
										  "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
										  error, i, j);
				if (tmp) {
					g_string_append (string, tmp);
					g_free (tmp);
				}
				else {
					g_string_free (string, TRUE);
					return NULL;
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

	g_string_append (string, ")");

	/* INHERITS */
	first = TRUE;
	nrows = gda_server_operation_get_sequence_size (op, "/TABLE_PARENTS_S");
	for (i = 0; i < nrows; i++) {
		value = gda_server_operation_get_value_at (op, "/TABLE_PARENTS_S/%d/TABLE_PARENT_COPY", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/TABLE_PARENTS_S/%d/TABLE_PARENT_TABLE",
									  error, i);
			if (tmp) {
				hasfields = TRUE;
				if (first) {
					g_string_append (string, " INHERITS ");
					first = FALSE;
				}
				else
					g_string_append (string, ", ");
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else {
				g_string_free (string, TRUE);
				return NULL;
			}
		}
	}

	if (!hasfields) {
		g_string_free (string, TRUE);
		g_set_error (error, GDA_SERVER_OPERATION_ERROR,
                             GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("Table to create must have at least one row"));
		return NULL;
	}

	sql = g_string_free (string, FALSE);

#ifdef GDA_DEBUG
	g_print ("Renderer SQL for PostgreSQL: %s\n", sql);
#endif
	return sql;
}

gchar *
gda_postgres_render_DROP_TABLE   (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP TABLE IF EXISTS ");

  value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/TABLE_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/TABLE_DESC_P/TABLE_NEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, " RENAME TO ");
	g_string_append (string, tmp);
	g_free (tmp);

	sql = g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_postgres_render_ADD_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("ALTER TABLE ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/TABLE_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF EXISTS ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/TABLE_ONLY");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "ONLY ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	g_string_append (string, " ADD COLUMN ");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DEF_P/COLUMN_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

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

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_DROP_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);
	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DESC_P/COLUMN_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, " DROP COLUMN ");
	g_string_append (string, tmp);
	g_free (tmp);

	value = gda_server_operation_get_value_at (op, "/COLUMN_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		const gchar *str = g_value_get_string (value);
		if (str && *str) {
			g_string_append_c (string, ' ');
			g_string_append (string, str);
		}
	}

	sql = g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_postgres_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	GdaServerOperationNode *node;
	gint nrows, i;
	gchar *tmp;

	string = g_string_new ("CREATE ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_TYPE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, g_value_get_string (value));
		g_string_append_c (string, ' ');
	}

	g_string_append (string, "INDEX ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/INDEX_DEF_P/INDEX_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	g_string_append (string, " ON ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/INDEX_DEF_P/INDEX_ON_TABLE", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

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
		tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
								  "/INDEX_FIELDS_S/%d/INDEX_FIELD", error, i);
		if (tmp) {
			if (i != 0)
				g_string_append (string, ", ");
			g_string_append (string, tmp);
			g_free (tmp);

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
		else {
			g_string_free (string, TRUE);
			return NULL;
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

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_DROP_INDEX   (GdaServerProvider *provider, GdaConnection *cnc, 
				  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("DROP INDEX ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/INDEX_DESC_P/INDEX_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_CREATE_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
				 GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("CREATE ");

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_OR_REPLACE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "OR REPLACE ");

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");

	g_string_append (string, "VIEW ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/VIEW_DEF_P/VIEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);
	
	GdaServerOperationNode *node;

	node = gda_server_operation_get_node_info (op, "/FIELDS_A");
	if (node) {
		gint i, nrows;
		GString *cols = NULL;

		nrows = gda_data_model_get_n_rows (node->model);
		for (i = 0; i < nrows; i++) {
			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/FIELDS_A/@COLUMN_NAME/%d",
									  error, i);
			if (!tmp) {
				g_string_free (string, TRUE);
				return NULL;
			}
			if (cols)
				g_string_append (cols, ", ");
			g_string_append (cols, tmp);
			g_string_append_c (cols, ' ');
			g_free (tmp);
		}
		if (cols) {
			g_string_append_c (cols, ')');
			g_string_append (string, cols->str);
			g_string_free (cols, TRUE);
		}
	}

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_DEF");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, " AS ");
	g_string_append (string, g_value_get_string (value));

	sql = g_string_free (string, FALSE);

	return sql;
}
	
gchar *
gda_postgres_render_DROP_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("DROP VIEW");

	value = gda_server_operation_get_value_at (op, "/VIEW_DESC_P/VIEW_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
							  "/VIEW_DESC_P/VIEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append_c (string, ' ');
	g_string_append (string, tmp);
	g_free (tmp);

	value = gda_server_operation_get_value_at (op, "/VIEW_DESC_P/REFERENCED_ACTION");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append_c (string, ' ');
	g_string_append (string, g_value_get_string (value));

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_CREATE_USER (GdaServerProvider *provider, GdaConnection *cnc, 
				 GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;
	gboolean with = FALSE, first, use_role = TRUE;
	gint nrows, i;
	PostgresConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}
	if (cdata && (cdata->reuseable->version_float < 8.1))
		use_role = FALSE;

	if (use_role)
		string = g_string_new ("CREATE ROLE ");
	else
		string = g_string_new ("CREATE USER ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/USER_DEF_P/USER_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/PASSWORD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) &&
	    g_value_get_string (value) && (*g_value_get_string (value))) {
		GdaDataHandler *dh;
		const GValue *value2;

		g_string_append (string, " WITH");
		with = TRUE;

		value2 = gda_server_operation_get_value_at (op, "/USER_DEF_P/PASSWORD_ENCRYPTED");
		if (value2 && G_VALUE_HOLDS (value2, G_TYPE_BOOLEAN) && g_value_get_boolean (value2))
			g_string_append (string, " ENCRYPTED");

		g_string_append (string, " PASSWORD ");
		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		if (!dh)
			dh = gda_data_handler_get_default (G_TYPE_STRING);
		else
			g_object_ref (dh);

		if (!dh) {
		    g_set_error (error,
				 GDA_SERVER_OPERATION_ERROR,
				 GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
				 "%s: %s",
				 G_STRLOC,
				 _ ("Dataholder type is unknown."));
		    return NULL;
		}

		tmp = gda_data_handler_get_sql_from_value (dh, value);
		g_object_unref (dh);
		g_string_append (string, tmp);
		g_free (tmp);
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/UID");
	if (value && G_VALUE_HOLDS (value, G_TYPE_UINT)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append_printf (string, "SYSID %u", g_value_get_uint (value));
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_SUPERUSER");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " SUPERUSER");
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_CREATEDB");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " CREATEDB");
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_CREATEROLE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " CREATEROLE");
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_CREATEUSER");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " CREATEUSER");
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_INHERIT");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " INHERIT");
	}
	else {
		if (!with) {
			g_string_append (string, " WITH");
			with = TRUE;
		}
		g_string_append (string, " NOINHERIT");
	}

	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CAP_LOGIN");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
		g_string_append (string, " LOGIN");
		value = gda_server_operation_get_value_at (op, "/USER_DEF_P/CNX_LIMIT");
		if (value && G_VALUE_HOLDS (value, G_TYPE_INT))
			g_string_append_printf (string, " CONNECTION LIMIT %d",
						g_value_get_int (value));
	}

	
	nrows = gda_server_operation_get_sequence_size (op, "/GROUPS_S");
	for (first  = TRUE, i = 0; i < nrows; i++) {
		gchar *name;
		if (use_role)
			name = gda_connection_operation_get_sql_identifier_at (cnc, op, "/GROUPS_S/%d/ROLE",
									   error, i);
		else
			name = gda_connection_operation_get_sql_identifier_at (cnc, op, "/GROUPS_S/%d/USER",
									   error, i);

		if (name) {
			if (first) {
				first = FALSE;
				if (use_role)
					g_string_append (string, " IN ROLE ");
				else
					g_string_append (string, " IN GROUP ");
			}
			else
				g_string_append (string, ", ");

			g_string_append (string, name);
			g_free (name);
		}
		else {
			g_string_free (string, TRUE);
			return NULL;
		}
	}

	nrows = gda_server_operation_get_sequence_size (op, "/ROLES_S");
	for (first  = TRUE, i = 0; i < nrows; i++) {
		gchar *name;
		name = gda_connection_operation_get_sql_identifier_at (cnc, op, "/ROLES_S/%d/ROLE", error, i);
		if (name) {
			if (first) {
				first = FALSE;
				g_string_append (string, " ROLE ");
			}
			else
				g_string_append (string, ", ");

			g_string_append (string, name);
			g_free (name);
		}
		else {
			g_string_free (string, TRUE);
			return NULL;
		}
	}

	nrows = gda_server_operation_get_sequence_size (op, "/ADMINS_S");
	for (first  = TRUE, i = 0; i < nrows; i++) {
		gchar *name;
		name = gda_connection_operation_get_sql_identifier_at (cnc, op, "/ADMINS_S/%d/ROLE", error, i);
		if (name) {
			if (first) {
				first = FALSE;
				g_string_append (string, " ADMIN ");
			}
			else
				g_string_append (string, ", ");

			g_string_append (string, name);
			g_free (name);
		}
		else {
			g_string_free (string, TRUE);
			return NULL;
		}
	}
	
	value = gda_server_operation_get_value_at (op, "/USER_DEF_P/VALIDITY");
	if (value && G_VALUE_HOLDS (value, G_TYPE_DATE_TIME)) {
		if (value) {
			GdaDataHandler *dh;
			if (!with) {
				g_string_append (string, " WITH");
				with = TRUE;
			}
			dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_DATE_TIME);
			if (!dh)
				dh = gda_data_handler_get_default (G_TYPE_DATE_TIME);
			else
				g_object_ref (dh);
		
			if (!dh) {
				g_set_error (error,
					     GDA_SERVER_OPERATION_ERROR,
					     GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
					     "%s: %s",
					     G_STRLOC,
					     _ ("Dataholder type is unknown. Report this as a bug."));
				return NULL;
			}
			g_string_append (string, " VALID UNTIL ");
			tmp = gda_data_handler_get_sql_from_value (dh, value);
			g_object_unref (dh);
			g_string_append (string, tmp);
			g_free (tmp);
		}
	}

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_postgres_render_DROP_USER (GdaServerProvider *provider, GdaConnection *cnc,
			       GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;
	gboolean use_role = TRUE;
	PostgresConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}
	if (cdata && (cdata->reuseable->version_float < 8.1))
		use_role = FALSE;

	if (use_role)
		string = g_string_new ("DROP ROLE ");
	else
		string = g_string_new ("DROP USER ");

	value = gda_server_operation_get_value_at (op, "/USER_DESC_P/USER_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
							  "/USER_DESC_P/USER_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append_c (string, ' ');
	g_string_append (string, tmp);
	g_free (tmp);

	sql = g_string_free (string, FALSE);

	return sql;
}
