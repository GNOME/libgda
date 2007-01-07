/* GNOME DB ODBC Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Michael Lausch <michael@lausch.at>
 *         Nick Gorham <nick@lurcher.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <stdlib.h>
#include <string.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-server-provider-extra.h>
#include "gda-odbc.h"
#include "gda-odbc-provider.h"


static void gda_odbc_provider_class_init (GdaOdbcProviderClass *klass);
static void gda_odbc_provider_init       (GdaOdbcProvider *provider,
					  GdaOdbcProviderClass *klass);
static void gda_odbc_provider_finalize   (GObject *object);


static const gchar *gda_odbc_provider_get_version (GdaServerProvider *provider);
static gboolean gda_odbc_provider_open_connection (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaQuarkList *params,
						   const gchar *username,
						   const gchar *password);

static gboolean gda_odbc_provider_close_connection (GdaServerProvider *provider,
						    GdaConnection *cnc);
static const gchar *gda_odbc_provider_get_server_version (GdaServerProvider *provider,
							  GdaConnection *cnc);
static const gchar *gda_odbc_provider_get_database (GdaServerProvider *provider,
						    GdaConnection *cnc);
static gboolean gda_odbc_provider_change_database (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   const gchar *name);
static GList *gda_odbc_provider_execute_command (GdaServerProvider *provider,
						 GdaConnection *cnc,
						 GdaCommand *cmd,
						 GdaParameterList *params);

static gboolean gda_odbc_provider_begin_transaction (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *name, GdaTransactionIsolation level,
						     GError **error);

static gboolean gda_odbc_provider_commit_transaction (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      const gchar *name, GError **error);

static gboolean gda_odbc_provider_rollback_transaction (GdaServerProvider *provider,
							GdaConnection *cnc,
							const gchar *name, GError **error);

static gboolean gda_odbc_provider_supports (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    GdaConnectionFeature feature);

static GdaDataModel *gda_odbc_provider_get_schema (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaConnectionSchema schema,
						   GdaParameterList *params);


static GObjectClass *parent_class = NULL;

/*
 * GdaOdbcProvider class implementation
 */

static void
gda_odbc_provider_class_init (GdaOdbcProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_odbc_provider_finalize;

	provider_class->get_version = gda_odbc_provider_get_version;
	provider_class->get_server_version = gda_odbc_provider_get_server_version;
	provider_class->get_info = NULL;
	provider_class->supports_feature = gda_odbc_provider_supports;
	provider_class->get_schema = gda_odbc_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_odbc_provider_open_connection;
	provider_class->close_connection = gda_odbc_provider_close_connection;
	provider_class->get_database = gda_odbc_provider_get_database;
	provider_class->change_database = gda_odbc_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_odbc_provider_execute_command;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_odbc_provider_begin_transaction;
	provider_class->commit_transaction = gda_odbc_provider_commit_transaction;
	provider_class->rollback_transaction = gda_odbc_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
}

static void
gda_odbc_provider_init (GdaOdbcProvider *provider, GdaOdbcProviderClass *klass)
{
}

static void
gda_odbc_provider_finalize (GObject *object)
{
	GdaOdbcProvider *provider = (GdaOdbcProvider *) object;

	g_return_if_fail (GDA_IS_ODBC_PROVIDER (provider));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_odbc_provider_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static GTypeInfo info = {
                        sizeof (GdaOdbcProviderClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_odbc_provider_class_init,
                        NULL, NULL,
                        sizeof (GdaOdbcProvider),
                        0,
                        (GInstanceInitFunc) gda_odbc_provider_init
                };
                type = g_type_register_static (PARENT_TYPE, "GdaOdbcProvider", &info, 0);
        }

        return type;
}

GdaServerProvider *
gda_odbc_provider_new (void)
{
	GdaServerProvider *provider;

	provider = g_object_new (gda_odbc_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* open_connection handler for the GdaOdbcProvider class */
static gboolean
gda_odbc_provider_open_connection (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaQuarkList *params,
				   const gchar *username,
				   const gchar *password)
{
	GdaOdbcConnectionData *priv_data;
	const gchar *odbc_string;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	odbc_string = gda_quark_list_find (params, "STRING");

	/* allocate needed structures & handles 
	 * the use of SQLAllocEnv will cause ODBC 2 operation
	 * we will see if thats ok, and maybe change to use ODBC 3 if
	 * needed 
	 */
	priv_data = g_new0 (GdaOdbcConnectionData, 1);
	rc = SQLAllocEnv (&priv_data->henv);
	if (!SQL_SUCCEEDED (rc)) {
		gda_connection_add_event_string (cnc,
					_("Unable to SQLAllocEnv()..."));
		g_free (priv_data);
		return FALSE;
	}

	rc = SQLAllocConnect (priv_data->henv, &priv_data->hdbc);
	if (!SQL_SUCCEEDED (rc)) {
		gda_odbc_emit_error (cnc, priv_data->henv,
				     SQL_NULL_HANDLE, SQL_NULL_HANDLE);
		SQLFreeEnv (priv_data->henv);
		g_free (priv_data);
		return FALSE;
	}

	/* set access mode */
	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		rc = SQLSetConnectOption (priv_data->hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_ONLY);

		if (!SQL_SUCCEEDED (rc)) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, SQL_NULL_HANDLE);
		}
	}

	/* open the connection */
	rc = SQLConnect (priv_data->hdbc,
			 (SQLCHAR *) odbc_string, SQL_NTS,
			 (SQLCHAR *) username, SQL_NTS,
			 (SQLCHAR *) password, SQL_NTS);
	if (!SQL_SUCCEEDED (rc)) {
		gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, SQL_NULL_HANDLE);
		SQLFreeConnect (priv_data->hdbc);
		SQLFreeEnv (priv_data->henv);
		g_free (priv_data);
		return FALSE;
	}

	/* get a working statement handle */
	rc = SQLAllocStmt (priv_data->hdbc, &priv_data->hstmt);
	if (!SQL_SUCCEEDED (rc)) {
		gda_odbc_emit_error (cnc, priv_data->henv,
						SQL_NULL_HANDLE, SQL_NULL_HANDLE);
		SQLDisconnect (priv_data->hdbc);
		SQLFreeConnect (priv_data->hdbc);
		SQLFreeEnv (priv_data->henv);
		g_free (priv_data);
		return FALSE;
	}

	/* get version info */

	rc = SQLGetInfo (priv_data->hdbc, SQL_DBMS_VER,
			priv_data->version, sizeof(priv_data->version), NULL);

	if (!SQL_SUCCEEDED(rc)) {
		strcpy(priv_data->version, "Unable to get version");
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE, priv_data);

	return TRUE;
}

/* close_connection handler for the GdaOdbcProvider class */
static gboolean
gda_odbc_provider_close_connection (GdaServerProvider *provider,
				    GdaConnection *cnc)
{
	GdaOdbcConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
        if (!priv_data)
                return FALSE;

	/* disconnect and free memory */
	SQLDisconnect (priv_data->hdbc);

	SQLFreeConnect (priv_data->hdbc);
        SQLFreeEnv (priv_data->henv);
	g_free (priv_data);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE, NULL);

	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql, GdaCommandOptions opts)
{
	GdaOdbcConnectionData *priv_data;
	GdaDataModelArray *recset;
	SQLSMALLINT ncols;
	SQLRETURN rc;
	SQLCHAR *sql_as_ansi;
	int i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
        if (!priv_data)
                return NULL;

	sql_as_ansi = (SQLCHAR *) g_locale_from_utf8 (sql, -1, NULL, NULL, NULL);
	rc = SQLExecDirect (priv_data->hstmt, sql_as_ansi, SQL_NTS);
	g_free (sql_as_ansi);
	if (SQL_SUCCEEDED(rc)) {
		do
		{
			recset = NULL;
			rc = SQLNumResultCols (priv_data->hstmt, &ncols);
			if (SQL_SUCCEEDED(rc) && ncols > 0) {
				/* result set */
				recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (ncols));
				for (i = 1; i <= ncols; i ++) {
					SQLCHAR name[256];
					SQLSMALLINT nlen;
					rc = SQLDescribeCol(priv_data->hstmt, i,
							name, sizeof(name), &nlen, NULL, 
							NULL, NULL, NULL);
					if (!SQL_SUCCEEDED(rc)) {
						if (opts & GDA_COMMAND_OPTION_IGNORE_ERRORS) {
							rc = SQLMoreResults(priv_data->hstmt);
							continue;
						}
						gda_odbc_emit_error (cnc, priv_data->henv,
								priv_data->hdbc, priv_data->hstmt);
						SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
						g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
						g_list_free (reclist);
						return NULL;
					}
					if (nlen > 0) {
						gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i - 1, name);
					}
					else
					{
						char nn[256];

						sprintf(nn, "expr%d", i);
						gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i - 1, nn);
					}
				}

				/* get rows */
				while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
					GList *value_list = NULL;
					SQLCHAR value[256];
					SQLINTEGER ind;
					GValue *tmpval;

					for (i = 1; i <= ncols; i ++) {
						rc = SQLGetData (priv_data->hstmt, i, SQL_C_CHAR,
								value, sizeof(value), &ind);

						if (SQL_SUCCEEDED (rc) && (ind >= 0)) {
							gchar *value_as_utf = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
							g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), 
									    value_as_utf);
							value_list = g_list_append (value_list, tmpval);
						}
						else {
							g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
							value_list = g_list_append (value_list, tmpval);
						}
					}

					gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

					g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
					g_list_free (value_list);
				}

				/* close result set */
				SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);
			}
			else if (SQL_SUCCEEDED(rc)) {
				/* no result set */
			}
			else {
				if (opts & GDA_COMMAND_OPTION_IGNORE_ERRORS) {
					rc = SQLMoreResults(priv_data->hstmt);
					continue;
				}
				gda_odbc_emit_error (cnc, priv_data->henv,
							priv_data->hdbc, priv_data->hstmt);
				SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
				return NULL;
			}
			reclist = g_list_append (reclist, recset);
			rc = SQLMoreResults(priv_data->hstmt);
		}
		while (SQL_SUCCEEDED(rc));
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
					priv_data->hdbc, priv_data->hstmt);
		return NULL;
	}

	return reclist;
}

/* execute_command handler for the GdaOdbcProvider class */
static GList *
gda_odbc_provider_execute_command (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaCommand *cmd,
				   GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaCommandOptions options;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	/* execute command */
	options = gda_command_get_options (cmd);
	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd),
						options);
		break;

	case GDA_COMMAND_TYPE_TABLE :
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = process_sql_commands (reclist, cnc, str, options);
		g_free (str);
		break;

	/* FIXME: do these types */
	case GDA_COMMAND_TYPE_PROCEDURE:
	case GDA_COMMAND_TYPE_SCHEMA:
		break;

	default :
		break;
	}

	return reclist;
}

static const gchar *
gda_odbc_provider_get_version (GdaServerProvider *provider)
{
	GdaOdbcProvider *pg_prv = (GdaOdbcProvider *) provider;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (pg_prv), NULL);

	return PACKAGE_VERSION;
}

static const gchar *
gda_odbc_provider_get_server_version (GdaServerProvider *provider,
							      GdaConnection *cnc)
{
	GdaOdbcProvider *pg_prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return NULL;
	}

	return priv_data -> version;
}

static const gchar *
gda_odbc_provider_get_database (GdaServerProvider *provider,
							GdaConnection *cnc)
{
	GdaOdbcProvider *pg_prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return NULL;
	}

	rc = SQLGetConnectOption(priv_data->hdbc, SQL_CURRENT_QUALIFIER, priv_data->db);

	if (SQL_SUCCEEDED(rc)) {
		return priv_data->db;
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
					priv_data->hdbc, NULL);
		return NULL;
	}
}

static gboolean 
gda_odbc_provider_change_database (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       const gchar *name)
{
	GdaOdbcProvider *prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return FALSE;
	}

	rc = SQLSetConnectOption(priv_data->hdbc, SQL_CURRENT_QUALIFIER, (unsigned long) name);

	if (SQL_SUCCEEDED(rc)) {
		return TRUE;
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
					priv_data->hdbc, NULL);
		return FALSE;
	}
}

static gboolean 
gda_odbc_provider_begin_transaction (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name, GdaTransactionIsolation level,
				     GError **error)
{
	GdaOdbcProvider *prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return FALSE;
	}

	rc = SQLSetConnectOption (priv_data->hdbc, SQL_AUTOCOMMIT, 
				  SQL_AUTOCOMMIT_OFF);

	if (SQL_SUCCEEDED(rc)) {
		return TRUE;
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
				     priv_data->hdbc, NULL);
		return FALSE;
	}
}

static gboolean 
gda_odbc_provider_commit_transaction (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	GdaOdbcProvider *prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return FALSE;
	}

	rc = SQLTransact (priv_data->henv, priv_data->hdbc, SQL_COMMIT);

	if (SQL_SUCCEEDED (rc)) {
		return TRUE;
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
				     priv_data->hdbc, NULL);
		return FALSE;
	}
}

static gboolean 
gda_odbc_provider_rollback_transaction (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name, GError **error)
{
	GdaOdbcProvider *prv = (GdaOdbcProvider *) provider;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid Odbc handle"));
		return FALSE;
	}

	rc = SQLTransact(priv_data->henv, priv_data->hdbc, SQL_ROLLBACK);

	if (SQL_SUCCEEDED(rc)) {
		return TRUE;
	}
	else {
		gda_odbc_emit_error (cnc, priv_data->henv,
					priv_data->hdbc, NULL);
		return FALSE;
	}
}

static gboolean 
gda_odbc_provider_supports (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaConnectionFeature feature)
{
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;
	SQLINTEGER ival;
	SQLSMALLINT sval;
	SQLCHAR yn[ 2 ];

	g_return_val_if_fail (GDA_IS_ODBC_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);
        if (!priv_data) 
                return FALSE;

	switch (feature) {
	case GDA_CONNECTION_FEATURE_INDEXES :
		rc = SQLGetInfo(priv_data -> hdbc, SQL_DDL_INDEX, 
				&ival, sizeof(ival), NULL);

		if (SQL_SUCCEEDED(rc)) {
			if (ival & SQL_DI_CREATE_INDEX) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
		break;

	case GDA_CONNECTION_FEATURE_NAMESPACES :
		rc = SQLGetInfo(priv_data -> hdbc, SQL_OWNER_USAGE, 
				&ival, sizeof(ival), NULL);

		if (SQL_SUCCEEDED(rc)) {
			if (ival) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
		break;

	case GDA_CONNECTION_FEATURE_PROCEDURES :
		rc = SQLGetInfo(priv_data -> hdbc, SQL_PROCEDURES, 
				yn, sizeof(yn), NULL);

		if (SQL_SUCCEEDED(rc)) {
			if (yn[ 0 ] == 'Y') {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
		break;

	case GDA_CONNECTION_FEATURE_SQL :
		return TRUE;

	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
		rc = SQLGetInfo(priv_data -> hdbc, SQL_TXN_CAPABLE, 
				&sval, sizeof(sval), NULL);

		if (SQL_SUCCEEDED(rc)) {
			if (sval != 0) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
		break;

	case GDA_CONNECTION_FEATURE_VIEWS :
		rc = SQLGetInfo(priv_data -> hdbc, SQL_CREATE_VIEW, 
				&ival, sizeof(ival), NULL);

		if (SQL_SUCCEEDED(rc)) {
			if (ival) {
				return TRUE;
			}
			else {
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
		break;

	/* we don't do these, but mergent crashes if we don't say this */
	case GDA_CONNECTION_FEATURE_SEQUENCES :
		return TRUE;
		break;

	default :
		return FALSE;
	}
}

static SQLRETURN 
get_databases_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset)
{
	SQLRETURN rc;
	GValue *tmpval;

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value [256];
		SQLINTEGER ind;

		/* name */
		rc = SQLGetData (priv_data->hstmt, 1, SQL_C_CHAR,
					value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	GdaParameter *par;
	SQLRETURN rc;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* any parameters */
	if (params && (par = gda_parameter_list_find_param (params, "name"))) {
		tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}
	else 
		tblname = NULL;
		
	/* generate the results */
	rc = SQLTables (priv_data->hstmt, "%", SQL_NTS,
			NULL, 0,
			NULL, 0,
			NULL, 0);

	if (SQL_SUCCEEDED (rc)){
		rc = get_databases_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_odbc_indexes (GdaConnection *cnc, GdaParameterList *params)
{
	GdaOdbcConnectionData *priv_data;
	GdaDataModelArray *recset;

	/* FIXME: make this do something useful */

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */

	/* FIXME: Find if this is correct, it was copied from the PG provider */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));

	return GDA_DATA_MODEL (recset);
}

static SQLRETURN 
get_columns_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset)
{
	SQLRETURN rc;
	GValue *tmpval;

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value [256];
		SQLINTEGER ind, ival;

		/* name */
		rc = SQLGetData (priv_data->hstmt, 4, SQL_C_CHAR,
					value, sizeof(value), &ind);

		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* type */	
		rc = SQLGetData (priv_data->hstmt, 6, SQL_C_CHAR,
				  value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* size */
		rc = SQLGetData (priv_data->hstmt, 7, SQL_C_LONG,
				  &ival, sizeof(ival), &ind);

		if (SQL_SUCCEEDED (rc) && (ind >= 0)) 
			g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), ival);
		else 
			g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 0);
		value_list = g_list_append (value_list, tmpval);

		/* scale */
		rc = SQLGetData (priv_data->hstmt, 9, SQL_C_LONG,
				  &ival, sizeof(ival), &ind);

		if (SQL_SUCCEEDED (rc) && (ind >= 0)) 
			g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), ival);
		else 
			g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 0);			
		value_list = g_list_append (value_list, tmpval);

		/* not nullable */
		rc = SQLGetData (priv_data->hstmt, 11, SQL_C_LONG,
				  &ival, sizeof(ival), &ind);

		if (SQL_SUCCEEDED (rc) && (ind >= 0))
			g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), ival != 0);
		else 
			g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		value_list = g_list_append (value_list, tmpval);

		/* FIXME: find just what these three fields should be and make it happen */

		/* primary key */
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		value_list = g_list_append (value_list, tmpval);

		/* unique index */
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		value_list = g_list_append (value_list, tmpval);

		/* references */
		value_list = g_list_append (value_list, gda_value_new_null ());

		/* default value */
		rc = SQLGetData (priv_data->hstmt, 13, SQL_C_CHAR,
				  value, sizeof(value), &ind);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_fields_metadata (GdaConnection *cnc, GdaParameterList *params)
{
	GdaOdbcConnectionData *priv_data;
	GdaDataModelArray *recset;
	GdaParameter *par;
	const gchar *tblname;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);
	
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_FIELDS);

	/* generate the results */
	rc = SQLColumns (priv_data->hstmt, NULL, 0,
			  NULL, 0,
			  (SQLCHAR*) tblname, SQL_NTS,
			  NULL, 0 );

	if (SQL_SUCCEEDED(rc)) {
		rc = get_columns_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
					     priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static SQLRETURN 
get_namespaces_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset)
{
	SQLRETURN rc;
	GValue *tmpval;

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value[256];
		SQLINTEGER ind;

		/* owner */
		rc = SQLGetData (priv_data->hstmt, 2, SQL_C_CHAR,
					value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_namespaces (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_NAMESPACES)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_NAMESPACES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* generate the results */
	rc = SQLTables (priv_data->hstmt, NULL, 0,
			 (SQLCHAR*) "%", SQL_NTS,
			 NULL, 0,
			 NULL, 0);

	if (SQL_SUCCEEDED(rc)) {
		rc = get_namespaces_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static SQLRETURN 
get_procedure_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset)
{
	SQLRETURN rc;
	GValue *tmpval;

	/* FIXME: try and get the number and types of parameters */

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value[256];
		SQLINTEGER ind;

		/* Procedure */
		rc = SQLGetData (priv_data->hstmt, 3, SQL_C_CHAR,
				 value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* id */
		value_list = g_list_append (value_list, gda_value_copy (tmpval));
	
		/* owner */
		rc = SQLGetData (priv_data->hstmt, 2, SQL_C_CHAR,
				  value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* Comments */
		rc = SQLGetData (priv_data->hstmt, 7, SQL_C_CHAR,
				  value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* Return type */
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
		value_list = g_list_append (value_list, tmpval);

		/* Nb args */
		g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 0);
		value_list = g_list_append (value_list, tmpval);

		/* Args types */
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
		value_list = g_list_append (value_list, tmpval);

		/* Definition */
		value_list = g_list_append (value_list, gda_value_new_null ());

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_procedures (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	GdaParameter *par;
	const gchar *procname;
	SQLRETURN rc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_PROCEDURES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* any parameters */
	if (params && (par = gda_parameter_list_find_param (params, "name"))) {
		procname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}
	else 
		procname = NULL;
		
	/* generate the results */
	rc = SQLProcedures (priv_data->hstmt, NULL, 0,
						NULL, 0,
						(SQLCHAR*) procname, SQL_NTS);

	if (SQL_SUCCEEDED(rc)) {
		rc = get_procedure_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static SQLRETURN 
get_tables_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset)
{
	SQLRETURN rc;
	GValue *tmpval;

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value[256];
		SQLINTEGER ind;

		/* name */
		rc = SQLGetData (priv_data->hstmt, 3, SQL_C_CHAR,
					value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);
	
		/* owner */
		rc = SQLGetData (priv_data->hstmt, 2, SQL_C_CHAR,
				  value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* remarks */
		rc = SQLGetData (priv_data->hstmt, 5, SQL_C_CHAR,
				  value, sizeof(value), &ind);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		value_list = g_list_append (value_list, gda_value_new_null ());

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	GdaParameter *par;
	SQLRETURN rc;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TABLES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* any parameters */
	if (params && (par = gda_parameter_list_find_param (params, "name"))) {
		tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}
	else
		tblname = NULL;
		
	/* generate the results */
	rc = SQLTables (priv_data->hstmt, NULL, 0,
						NULL, 0,
						(SQLCHAR*) tblname, SQL_NTS,
						(SQLCHAR*) "TABLE", SQL_NTS);

	if (SQL_SUCCEEDED(rc)) {
		rc = get_tables_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static SQLRETURN 
get_types_rs(GdaOdbcConnectionData *priv_data, GdaDataModelArray *recset, const gchar *typname)
{
	SQLRETURN rc;
	GValue *tmpval;

	while (SQL_SUCCEEDED((rc = SQLFetch (priv_data->hstmt)))) {
		GList *value_list = NULL;
		SQLCHAR value[256];
		SQLINTEGER ind, ival;

		/* name */
		rc = SQLGetData (priv_data->hstmt, 1, SQL_C_CHAR,
					value, sizeof(value), &ind);

		/* filter out unwanted types */
		if (typname && strcmp(typname, value))
			continue;

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), 
				    SQL_SUCCEEDED (rc) && (ind >= 0) ? value : "");
		value_list = g_list_append (value_list, tmpval);

		/* Owner */
		value_list = g_list_append (value_list, gda_value_new_null ());
		/* Comment */
		value_list = g_list_append (value_list, gda_value_new_null ());

		/* GDA Type */
		rc = SQLGetData (priv_data->hstmt, 2, SQL_C_LONG,
					&ival, sizeof(ival), &ind);

		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), 
				   SQL_SUCCEEDED (rc) && (ind >= 0) ? odbc_to_g_type (ival) : G_TYPE_INVALID);
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return rc;
}

static GdaDataModel *
get_odbc_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	GdaParameter *par;
	const gchar *typname;
	SQLRETURN rc;


	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* any parameters */
	if (params && (par = gda_parameter_list_find_param (params, "name"))) {
		typname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}
	else 
		typname = NULL;

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TYPES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* generate the results */
	rc = SQLGetTypeInfo (priv_data->hstmt, SQL_ALL_TYPES);

	if (SQL_SUCCEEDED(rc)) {
		rc = get_types_rs(priv_data, recset, typname);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_odbc_views (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;
	GdaParameter *par;
	SQLRETURN rc;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_VIEWS)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_VIEWS);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	/* any parameters */
	if (params && (par = gda_parameter_list_find_param (params, "name"))) {
		tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}
	else 
		tblname = NULL;

	/* generate the results */
	rc = SQLTables (priv_data->hstmt, NULL, 0,
			NULL, 0,
			(SQLCHAR*) tblname, SQL_NTS,
			(SQLCHAR*) "VIEW", SQL_NTS);

	if (SQL_SUCCEEDED(rc)) {
		rc = get_tables_rs(priv_data, recset);

		if (rc != SQL_NO_DATA) {
			gda_odbc_emit_error (cnc, priv_data->henv,
						priv_data->hdbc, priv_data->hstmt);
			SQLFreeStmt(priv_data->hstmt, SQL_CLOSE);
			return NULL;
		}
	}

	SQLFreeStmt (priv_data->hstmt, SQL_CLOSE);

	return GDA_DATA_MODEL (recset);
}

/* We shouldn't need this, but mergent doesn't seem to work otherwise */

static GdaDataModel *
get_odbc_sequence (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaOdbcConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_SEQUENCES)));
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_SEQUENCES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
gda_odbc_provider_get_schema (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaConnectionSchema schema,
						       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES:
		return get_odbc_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_INDEXES:
		return get_odbc_indexes (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS:
		return get_odbc_fields_metadata (cnc, params);
	case GDA_CONNECTION_SCHEMA_NAMESPACES:
		if (!gda_odbc_provider_supports (provider, cnc, GDA_CONNECTION_FEATURE_NAMESPACES))
			return NULL;
		return get_odbc_namespaces (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		return get_odbc_procedures (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES:
		return get_odbc_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES:
		return get_odbc_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_VIEWS:
		return get_odbc_views (cnc, params);
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
		return get_odbc_sequence (cnc, params);
	default:
		return NULL;
	}
}
