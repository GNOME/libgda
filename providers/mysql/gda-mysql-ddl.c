/*
 * Copyright (C) 2006 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
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

#include "gda-mysql-ddl.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>
#include <libgda/sql-parser/gda-sql-parser.h>

gchar *
gda_mysql_render_CREATE_DB (GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, 
			    GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	const gchar *tmp = NULL;

	string = g_string_new ("CREATE DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		tmp = g_value_get_string (value);
		if (tmp)
		  g_string_append (string, tmp);
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		tmp = g_value_get_string (value);
		if (tmp)
		  g_string_append (string, tmp);
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_CSET");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		tmp = g_value_get_string (value);
		if (tmp) {
		    g_string_append (string, " CHARACTER SET ");
		    g_string_append (string, tmp);
		}
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_COLLATION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
	    tmp = g_value_get_string (value);
	    if (tmp) {
		g_string_append (string, " COLLATION ");
		g_string_append (string, tmp);
	    }
	}

	return g_string_free (string, FALSE);
}

gchar *
gda_mysql_render_DROP_DB (GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, 
			  GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	const gchar *tmp;

	string = g_string_new ("DROP DATABASE IF EXISTS ");

	value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");

	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
	  tmp = g_value_get_string (value);
	  g_string_append (string, tmp);
	}

	return g_string_free (string, FALSE);
}


gchar *
gda_mysql_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	const GValue *value1;
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

	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");
		
	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/TABLE_DEF_P/TABLE_NAME", error);
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
									  "/FIELDS_A/@COLUMN_NAME/%d",
									  error, i);
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

		value1 = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
		g_string_append (string, g_value_get_string (value1));
				
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
        const gchar* valtmp = g_value_get_string (value1);
        if (!g_ascii_strcasecmp (valtmp,"string") ||
            !g_ascii_strcasecmp (valtmp,"gchararray")) {
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
			
		value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_AUTOINC/%d", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			g_string_append (string, " AUTO_INCREMENT");

		value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_UNIQUE/%d", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			g_string_append (string, " UNIQUE");
				
		if (nbpkfields == 1) {
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_PKEY/%d", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
				g_string_append (string, " PRIMARY KEY");
		}

		value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_COMMENT/%d", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
			GdaDataHandler *dh;
			gchar *str;

			dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
			str = gda_data_handler_get_sql_from_value (dh, value);
			if (str) {
				if (*str) {
					g_string_append (string, " COMMENT ");
					g_string_append (string, str);
				}
				g_free (str);
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
	}

	/* composed primary key */
	if (nbpkfields > 1) {
		GSList *list;

		g_string_append (string, ", PRIMARY KEY (");
		for (list = pkfields; list; list = list->next) {
			if (list != pkfields)
				g_string_append (string, ", ");
			g_string_append (string, (gchar *) list->data);
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
			gint nbfields = 0, j;

			g_string_append (string, ", FOREIGN KEY (");
			node = gda_server_operation_get_node_info (op, "/FKEY_S/%d/FKEY_FIELDS_A", i);
			if (!node || ((nbfields = gda_data_model_get_n_rows (node->model)) == 0)) {
				g_string_free (string, TRUE);
				g_set_error (error, GDA_SERVER_OPERATION_ERROR,
					     GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
					     "%s", _("No field specified in foreign key constraint"));
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
		}
	}

	g_string_append (string, ")");

	if (!hasfields) {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR,
                             GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("Table to create must have at least one row"));
		g_string_free (string, TRUE);
		return NULL;
	}

	/* other options */
	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_ENGINE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value))
		g_string_append (string, " ENGINE = ");
	g_string_append (string, g_value_get_string (value));
		
	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_AUTOINC_VALUE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_INT)) 
		g_string_append_printf (string, " AUTO_INCREMENT = %d", g_value_get_int (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_AVG_ROW_LENTGH");
	if (value && G_VALUE_HOLDS (value, G_TYPE_INT)) 
		g_string_append_printf (string, " AVG_ROW_LENGTH = %d", g_value_get_int (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_CSET");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, " CHARACTER SET ");
		g_string_append (string, g_value_get_string (value));
		value = gda_server_operation_get_value_at_path (op, "/TABLE_OPTIONS_P/TABLE_COLLATION");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) &&
	    g_value_get_string (value) && *g_value_get_string (value)) {
			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/TABLE_OPTIONS_P/TABLE_COLLATION", error);
			if (!tmp) {
				g_string_free (string, TRUE);
				return NULL;
			}
			g_string_append (string, " COLLATE ");
			g_string_append (string, tmp);
			g_free (tmp);
		}
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_CHECKSUM");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) 
		g_string_append (string, " CHECKSUM = 1");

	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_COMMENT");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		GdaDataHandler *dh;
		gchar *str;

		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			g_string_append (string, " COMMENT = ");
			g_string_append (string, str);
			g_free (str);
		}
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_MAX_ROWS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_INT)) 
		g_string_append_printf (string, " MAX_ROWS = %d", g_value_get_int (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_MIN_ROWS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_INT)) 
		g_string_append_printf (string, " MIN_ROWS = %d", g_value_get_int (value));

	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_PACK_KEYS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, " PACK_KEYS = ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_PASSWORD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		GdaDataHandler *dh;
		gchar *str;

		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			g_string_append (string, " PASSWORD = ");
			g_string_append (string, str);
			g_free (str);
		}
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_DELAY_KEY_WRITE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) 
		g_string_append (string, " DELAY_KEY_WRITE = 1");

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_ROW_FORMAT");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, " ROW_FORMAT = ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_UNION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, " UNION = ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_INSERT_METHOD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		g_string_append (string, " INSERT_METHOD = ");
		g_string_append (string, g_value_get_string (value));
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_DATA_DIR");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		GdaDataHandler *dh;
		gchar *str;

		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			g_string_append (string, " DATA_DIRECTORY = ");
			g_string_append (string, str);
			g_free (str);
		}
	}

	value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_INDEX_DIR");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
	    g_value_get_string (value) && *g_value_get_string (value)) {
		GdaDataHandler *dh;
		gchar *str;

		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			g_string_append (string, " INDEX_DIRECTORY = ");
			g_string_append (string, str);
			g_free (str);
		}
	}
	sql = g_string_free (string, FALSE);
	
	return sql;
}

gchar *
gda_mysql_render_DROP_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("DROP");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " TEMPORARY");

	g_string_append (string, " TABLE");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/TABLE_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append_c (string, ' ');
	g_string_append (string, tmp);
	g_free (tmp);

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/REFERENCED_ACTION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		g_string_append_c (string, ' ');
		g_string_append (string, g_value_get_string (value));
	}

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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
gda_mysql_render_COMMENT_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
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

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_COMMENT");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, " COMMENT '");
	g_string_append (string, g_value_get_string (value));
	g_string_append (string, "'");

	sql = g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_mysql_render_ADD_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	g_string_append (string, " ADD COLUMN ");

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
	
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_AUTOINC");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " AUTO_INCREMENT");
	
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_UNIQUE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " UNIQUE");
	
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_PKEY");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " PRIMARY KEY");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_COMMENT");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		GdaDataHandler *dh;
		gchar *str;
		
		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_TYPE_STRING);
		str = gda_data_handler_get_sql_from_value (dh, value);
		if (str) {
			if (*str) {
				g_string_append (string, " COMMENT ");
				g_string_append (string, str);
			}
			g_free (str);
		}
	}
	
	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_CHECK");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
		const gchar *str = g_value_get_string (value);
		if (str && *str) {
			g_string_append (string, " CHECK (");
			g_string_append (string, str);
			g_string_append_c (string, ')');
		}
	}

	value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_FIRST");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " FIRST");
	else {
		value = gda_server_operation_get_value_at (op, "/COLUMN_DEF_P/COLUMN_AFTER");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING)) {
			const gchar *str = g_value_get_string (value);
			if (str && *str) {
				g_string_append (string, " AFTER ");
				g_string_append (string, str);
			}
		}
	}
		
	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_DROP_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
			      GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
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

	sql = g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_mysql_render_COMMENT_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
				 GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;
	gchar *table_name;
	gchar *column_name;

	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	table_name = tmp;
	g_string_append (string, tmp);
	g_free (tmp);

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DESC_P/COLUMN_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	column_name = tmp;
	g_string_append (string, " CHANGE COLUMN ");
	g_string_append (string, tmp);
	g_string_append (string, " ");
	g_string_append (string, tmp);
	g_string_append (string, " ");

	GString *tmp_string = g_string_new ("SELECT column_type FROM "
					    "information_schema.columns "
					    "WHERE table_name = ");
	g_string_append (tmp_string, table_name);
	g_string_append (tmp_string, " AND column_name = ");
	g_string_append (tmp_string, column_name);

	g_free (table_name);
	g_free (column_name);

	GdaSqlParser *parser = gda_connection_create_parser (cnc);
	if (parser == NULL)        /* @cnc does not provide its own parser. Use default one */
		parser = gda_sql_parser_new ();
  	sql = g_string_free (tmp_string, FALSE);
        GdaStatement *statement = gda_sql_parser_parse_string (parser,
							       sql,
							       NULL, NULL);
	GdaDataModel *model;
	GError *gerror = NULL;
	model = gda_connection_statement_execute_select (cnc, statement,
							 NULL, &gerror);
	g_object_unref (G_OBJECT(statement));

	g_assert (model != NULL && gda_data_model_get_n_rows (model) == 1);

	const GValue *tmp_value;
	tmp_value = gda_data_model_get_value_at (model, 0, 0, error);

	gchar *str;
	g_assert (tmp_value && (str = gda_value_stringify (tmp_value)));

	g_string_append (string, str);
	g_free (str);

	g_object_unref (G_OBJECT(model));

	g_string_append (string, " COMMENT");
	g_string_append (string, " '");

	value = gda_server_operation_get_value_at (op, "/COLUMN_DESC_P/COLUMN_COMMENT");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));
	g_string_append (string, "'");

	sql = g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_mysql_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
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

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_METHOD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " USING ");
		g_string_append (string, g_value_get_string (value));
	}

	g_string_append (string, " ON ");
	
	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/INDEX_DEF_P/INDEX_ON_TABLE", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	/* fields or expressions the index is on */
	g_string_append (string, " (");
	node = gda_server_operation_get_node_info (op, "/INDEX_FIELDS_S");
	g_assert (node);
	nrows = gda_server_operation_get_sequence_size (op, "/INDEX_FIELDS_S");
	for (i = 0; i < nrows; i++) {
		tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
								  "/INDEX_FIELDS_S/%d/INDEX_FIELD", error, i);
		if (!tmp) {
			g_string_free (string, TRUE);
			return NULL;
		}

		if (i != 0)
			g_string_append (string, ", ");
		g_string_append (string, tmp);
		g_free (tmp);
			
		value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_LENGTH", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_INT) && (g_value_get_int (value) > 0))
			g_string_append_printf (string, " (%d)", g_value_get_int (value));

		value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_SORT_ORDER", i);
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
			g_string_append_c (string, ' ');
			g_string_append (string, g_value_get_string (value));
		}
	}

	g_string_append (string, ")");

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_DROP_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
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

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/INDEX_DESC_P/INDEX_ON_TABLE", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, " ON ");
	g_string_append (string, tmp);
	g_free (tmp);

	sql = g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_CREATE_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
			      GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gboolean allok = TRUE;
	gchar *sql = NULL;
	GdaServerOperationNode *node;
	gchar *tmp;

	string = g_string_new ("CREATE ");

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_OR_REPLACE");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "OR REPLACE ");

	g_string_append (string, "VIEW ");
	
	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/VIEW_DEF_P/VIEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}

	g_string_append (string, tmp);
	g_free (tmp);

	node = gda_server_operation_get_node_info (op, "/FIELDS_A");
	if (node) {
		gint nrows;
		gint i;
		/* finding if there is a composed primary key */
		nrows = gda_data_model_get_n_rows (node->model);
		for (i = 0; (i < nrows) && allok; i++) {
			if (i == 0)
				g_string_append (string, " (");

			tmp = gda_connection_operation_get_sql_identifier_at (cnc, op,
									  "/FIELDS_A/@COLUMN_NAME/%d", error, i);
			if (!tmp) {
				g_string_free (string, TRUE);
				return NULL;
			}

			if (i != 0) 
				g_string_append (string, ", ");
				
			g_string_append (string, tmp);
			g_string_append_c (string, ' ');
			g_free (tmp);
		}
		if (i > 0)
			g_string_append (string, ")");
	}
	
	if (allok) {
		value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_DEF");
		g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
		g_string_append (string, " AS ");
		g_string_append (string, g_value_get_string (value));
	}

	if (allok) {
		sql = g_string_free (string, FALSE);
	}
	else {
		sql = NULL;
		g_string_free (string, TRUE);
	}

	return sql;
}
	
gchar *
gda_mysql_render_DROP_VIEW (GdaServerProvider *provider, GdaConnection *cnc, 
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

	tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/VIEW_DESC_P/VIEW_NAME", error);
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
