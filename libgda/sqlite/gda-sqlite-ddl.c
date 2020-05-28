/*
 * Copyright (C) 2006 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
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

#include "gda-sqlite-ddl.h"
#include "gda-server-provider.h"
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-handler.h>


gchar *
_gda_sqlite_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				 GdaServerOperation *op, GError **error)
{
	g_return_val_if_fail (provider, NULL);
	g_return_val_if_fail (cnc, NULL);
	g_return_val_if_fail (op, NULL);
	g_return_val_if_fail (gda_server_operation_get_op_type (op) == GDA_SERVER_OPERATION_CREATE_TABLE, NULL);

	GString *string;
	const GValue *value;
	const GValue *value1;
	gboolean allok = TRUE;
	gboolean hasfields = FALSE;
	gint nrows;
	gint i;
	gboolean first;
	GSList *pkfields = NULL; /* list of GValue* composing the pkey */
	gint nbpkfields = 0;
	gchar *sql = NULL;
	gchar *conflict_algo = NULL;
	gchar *tmp;

	/* CREATE TABLE */
	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");
	g_string_append (string, "TABLE ");

	value = gda_server_operation_get_value_at (op, "/TABLE_DEF_P/TABLE_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");
		
	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/TABLE_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		if (error != NULL) {
			if (*error == NULL) {
				g_warning (_("Internal error, creating table in SQLite provider"));
			}
		}
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);
	g_string_append (string, " (");
		
  GdaServerOperationNode *node;
	/* FIELDS */
	if (allok) {

		node = gda_server_operation_get_node_info (op, "/FIELDS_A");
		if (node == NULL) {
			g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("No fields are defined for CREATE TABLE operation"));
			return NULL;
		}

		/* finding if there is a composed primary key */
		nrows = gda_data_model_get_n_rows (node->model);
		for (i = 0; i < nrows; i++) {
			value = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_PKEY/%d", i);
			if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value)) {
				tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op,
										  "/FIELDS_A/@COLUMN_NAME/%d", error,
										  i);
				if (!tmp) {
					g_string_free (string, TRUE);
					g_assert (*error != NULL);
					return NULL;
				}
				pkfields = g_slist_append (pkfields, tmp);
				nbpkfields++;
			}
		}

		/* manually defined fields */
		first = TRUE;
		for (i = 0; i < nrows; i++) {
			gboolean pkautoinc = FALSE;
			hasfields = TRUE;
			if (first) 
				first = FALSE;
			else
				g_string_append (string, ", ");
				
			tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op,
									  "/FIELDS_A/@COLUMN_NAME/%d", error, i);
			if (!tmp) {
				g_string_free (string, TRUE);
				g_assert (*error != NULL);
				return NULL;
			}
			g_string_append (string, tmp);
			g_free (tmp);
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
				value1 = gda_server_operation_get_value_at (op, "/FIELDS_A/@COLUMN_TYPE/%d", i);
				GType gtype = g_type_from_name (g_value_get_string (value1));
        if (gtype == G_TYPE_INVALID) {
          g_string_append (string, g_value_get_string (value1));
        } else {
          g_string_append (string,
                           gda_server_provider_get_default_dbms_type (provider,
                                                                      cnc, gtype));
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
            else {
              g_string_append (string, str);
            }
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
		GSList *list;

		g_string_append (string, ", PRIMARY KEY (");
		for (list = pkfields; list; list = list->next) {
			if (list != pkfields)
				g_string_append (string, ", ");
			g_string_append (string, (gchar *) list->data);
		}
		g_string_append_c (string, ')');
		
		if (conflict_algo) {
			g_string_append (string, " ON CONFLICT ");
			g_string_append (string, conflict_algo);
		}
	}
	g_slist_free_full (pkfields, (GDestroyNotify) g_free);

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

  /* CONSTRAINT can be inserted without specifying the keyword CONSTRAINT for sqlite.
   * Here, we just insert what we have saved in the GdaServerOperation object. Since
   * we are moving towards new implementation of how DDL operations should be handled,
   * we don't need to put more effort here. This is just a temporary working solution
   */
  node = gda_server_operation_get_node_info (op, "/TABLE_CONSTRAINTS_S");

  if (node)
    {
      nrows = gda_server_operation_get_sequence_size (op, "/TABLE_CONSTRAINTS_S");

      for (i = 0; i < nrows; i++)
        {
          g_string_append (string, ", ");

          const GValue *tval = gda_server_operation_get_value_at (op, "/TABLE_CONSTRAINTS_S/%d/CONSTRAINT_STRING", i);

          g_string_append (string, g_value_get_string (tval));
        }
    }

  g_free (conflict_algo);
  g_string_append (string, ")");

  if (!hasfields) {
    allok = FALSE;
		g_set_error (error, GDA_SERVER_OPERATION_ERROR,
			     GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
			     "%s", _("Table to create must have at least one field"));
		g_string_free (string, TRUE);
		return NULL;
	}

	sql = string->str;
	g_string_free (string, FALSE);
#ifdef GDA_DEBUG
	g_print ("Renderer SQL for SQLite: %s\n", sql);
#endif
	return sql;
}

gchar *
_gda_sqlite_render_DROP_TABLE (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	/* DROP TABLE */
	string = g_string_new ("DROP TABLE");

	value = gda_server_operation_get_value_at (op, "/TABLE_DESC_P/TABLE_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, " IF EXISTS");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/TABLE_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append_c (string, ' ');
	g_string_append (string, tmp);
	g_free (tmp);

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_RENAME_TABLE (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
				 GdaServerOperation *op, GError **error)
{
	GString *string;
	gchar *sql = NULL;
	gchar *tmp;

	/* DROP TABLE */
	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/TABLE_DESC_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/TABLE_DESC_P/TABLE_NEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, " RENAME TO ");
	g_string_append (string, tmp);
	g_free (tmp);

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_ADD_COLUMN (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
			      GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	/* DROP TABLE */
	string = g_string_new ("ALTER TABLE ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/COLUMN_DEF_P/TABLE_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);

	g_string_append (string, " ADD COLUMN ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/COLUMN_DEF_P/COLUMN_NAME", error);
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
_gda_sqlite_render_CREATE_INDEX (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	GdaServerOperationNode *node;
	gint nrows, i;
	gchar *tmp;

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
	

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/INDEX_DEF_P/INDEX_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);

	g_string_append (string, " ON ");
	
	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/INDEX_DEF_P/INDEX_ON_TABLE", error);
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
		tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op,
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

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_DROP_INDEX (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
			     GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gchar *sql = NULL;
	gchar *tmp;

	/* DROP INDEX */
	string = g_string_new ("DROP INDEX ");

	value = gda_server_operation_get_value_at (op, "/INDEX_DESC_P/INDEX_IFEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF EXISTS ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/INDEX_DESC_P/INDEX_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_CREATE_VIEW (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
			       GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	GString *string;
	const GValue *value;
	gboolean allok = TRUE;
	gchar *sql = NULL;
	gchar *tmp;

	string = g_string_new ("CREATE ");
	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_TEMP");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "TEMP ");

	g_string_append (string, "VIEW ");

	value = gda_server_operation_get_value_at (op, "/VIEW_DEF_P/VIEW_IFNOTEXISTS");
	if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
		g_string_append (string, "IF NOT EXISTS ");

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/VIEW_DEF_P/VIEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append (string, tmp);
	g_free (tmp);
	
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
_gda_sqlite_render_DROP_VIEW (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc, 
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

	tmp = gda_connection_operation_get_sql_identifier_at (cnc,  op, "/VIEW_DESC_P/VIEW_NAME", error);
	if (!tmp) {
		g_string_free (string, TRUE);
		return NULL;
	}
	g_string_append_c (string, ' ');
	g_string_append (string, tmp);
	g_free (tmp);

	sql = string->str;
	g_string_free (string, FALSE);

	return sql;
}

gchar *
_gda_sqlite_render_RENAME_COLUMN  (GdaServerProvider *provider,
                                   GdaConnection *cnc,
                                   GdaServerOperation *op,
                                   GError **error)
{
  gchar *sql = NULL;

  GString *string;
  gchar *tmp;

  /* ALTER TABLE */
  string = g_string_new ("ALTER TABLE ");

  tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DEF_P/TABLE_NAME", error);

  if (!tmp) {
    g_string_free (string, TRUE);
    return NULL;
  }

  g_string_append (string, tmp);
  g_free (tmp);

  g_string_append (string, " RENAME COLUMN ");

  tmp = gda_connection_operation_get_sql_identifier_at(cnc, op, "/COLUMN_DEF_P/COLUMN_NAME", error);
  if (!tmp) {
    g_string_free(string, TRUE);
    return NULL;
  }

  g_string_append (string, tmp);
  g_free (tmp);

  g_string_append (string, " TO ");

  tmp = gda_connection_operation_get_sql_identifier_at (cnc, op, "/COLUMN_DEF_P/COLUMN_NAME_NEW", error);

  if (!tmp) {
    g_string_free (string, TRUE);
    return NULL;
  }

  g_string_append (string, tmp);
  g_free (tmp);

  sql = string->str;
  g_string_free (string, FALSE);

#ifdef GDA_DEBUG
	g_print ("Renderer SQL for SQLite: %s\n", sql);
#endif

  return sql;
}
