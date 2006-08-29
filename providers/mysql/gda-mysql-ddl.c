/* GNOME DB Mysql Provider
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

#include "gda-mysql-ddl.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>

gchar *
gda_mysql_render_CREATE_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			    GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gboolean first = TRUE;

	string = g_string_new ("CREATE DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_CSET");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " CHARECTER SET ");
		g_string_append (string, g_value_get_string (value));
		first = FALSE;
	}

	value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_COLLATION");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		if (first)
			first = FALSE;
		else
			g_string_append (string, ", ");
		g_string_append (string, " COLLATION ");
		g_string_append (string, g_value_get_string (value));
	}

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;	
}

gchar *
gda_mysql_render_DROP_DB (GdaServerProvider *provider, GdaConnection *cnc, 
			  GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP DATABASE ");

	value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF EXISTS ");

	value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;	
}


gchar *
gda_mysql_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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
				
				dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
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
			}
		}
	}

	g_string_append (string, ")");

	if (!hasfields) {
		allok = FALSE;
		g_set_error (error, 0, 0, _("Table to create must have at least one row"));
	}

	/* other options */
	if (allok) {
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

			value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_COLLATION");
			if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
			    g_value_get_string (value) && *g_value_get_string (value)) {
				g_string_append (string, " COLLATE ");
				g_string_append (string, g_value_get_string (value));
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

			dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
			str = gda_data_handler_get_sql_from_value (dh, value);
			g_string_append (string, " COMMENT = ");
			g_string_append (string, str);
			g_free (str);
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

			dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
			str = gda_data_handler_get_sql_from_value (dh, value);
			g_string_append (string, " PASSWORD = ");
			g_string_append (string, str);
			g_free (str);
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

			dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
			str = gda_data_handler_get_sql_from_value (dh, value);
			g_string_append (string, " DATA_DIRECTORY = ");
			g_string_append (string, str);
			g_free (str);
		}

		value = gda_server_operation_get_value_at (op, "/TABLE_OPTIONS_P/TABLE_INDEX_DIR");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && 
		    g_value_get_string (value) && *g_value_get_string (value)) {
			GdaDataHandler *dh;
			gchar *str;

			dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
			str = gda_data_handler_get_sql_from_value (dh, value);
			g_string_append (string, " INDEX_DIRECTORY = ");
			g_string_append (string, str);
			g_free (str);
		}
	}

	g_slist_free (pkfields);


	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_DROP_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " TEMPORARY");

	g_string_append (string, " TABLE");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append_c (string, ' ');
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
gda_mysql_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
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
gda_mysql_render_ADD_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

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
		
		dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_TYPE_STRING);
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
		
	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_DROP_COLUMN (GdaServerProvider *provider, GdaConnection *cnc, 
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

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}


gchar *
gda_mysql_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
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

	value = gda_server_operation_get_value_at (op, "/INDEX_DEF_P/INDEX_METHOD");
	if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
		g_string_append (string, " USING ");
		g_string_append (string, g_value_get_string (value));
	}

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
			
			value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_LENGTH", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_INT) && (g_value_get_int (value) > 0))
				g_string_append_printf (string, " (%d)", g_value_get_int (value));

			value = gda_server_operation_get_value_at (op, "/INDEX_FIELDS_S/%d/INDEX_SORT_ORDER", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value)) {
				g_string_append_c (string, ' ');
				g_string_append (string, g_value_get_string (value));
			}
		}
	}

	g_string_append (string, ")");

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
gda_mysql_render_DROP_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;

	string = g_string_new ("DROP INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_NAME");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, g_value_get_string (value));

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_ON_TABLE");
	g_assert (value && G_VALUE_HOLDS (value, G_TYPE_STRING));
	g_string_append (string, " ON ");
	g_string_append (string, g_value_get_string (value));

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

