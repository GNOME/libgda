/* GDA IBMDB2 Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db-org>
 *	   Sergey N. Belinsky <sergey_be@mail.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <sqlcli1.h>
#include "gda-ibmdb2.h"
#include "gda-ibmdb2-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_IBMDB2_HANDLE "GDA_IBMDB2_IBMDB2Handle"

/*
 * Private declarations and functions
 */

static GObjectClass *parent_class = NULL;

static void gda_ibmdb2_provider_class_init (GdaIBMDB2ProviderClass *klass);
static void gda_ibmdb2_provider_init       (GdaIBMDB2Provider      *provider,
                                            GdaIBMDB2ProviderClass *klass);
static void gda_ibmdb2_provider_finalize   (GObject                 *object);

static gboolean gda_ibmdb2_provider_open_connection (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     GdaQuarkList *params,
                                                     const gchar *username,
                                                     const gchar *password);
static gboolean gda_ibmdb2_provider_close_connection (GdaServerProvider *provider,
                                                      GdaConnection *cnc);
static const gchar *gda_ibmdb2_provider_get_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc);
static gboolean gda_ibmdb2_provider_change_database (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     const gchar *name);
static gboolean gda_ibmdb2_provider_create_database_cnc (GdaServerProvider *provider,
                                                         GdaConnection *cnc,
                                                         const gchar *name);
static gboolean gda_ibmdb2_provider_drop_database (GdaServerProvider *provider,
                                                   GdaConnection *cnc,
                                                   const gchar *name);
static GList *gda_ibmdb2_provider_execute_command (GdaServerProvider *provider,
                                                   GdaConnection *cnc,
                                                   GdaCommand *cmd,
                                                   GdaParameterList *params);
static gboolean gda_ibmdb2_provider_begin_transaction (GdaServerProvider *provider,
                                                       GdaConnection *cnc,
                                                       GdaTransaction *xaction);
static gboolean gda_ibmdb2_provider_commit_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        GdaTransaction *xaction);
static gboolean gda_ibmdb2_provider_rollback_transaction (GdaServerProvider *provider,
                                                          GdaConnection *cnc,
                                                          GdaTransaction *xaction);
static gboolean gda_ibmdb2_provider_supports (GdaServerProvider *provider,
                                              GdaConnection *cnc,
                                              GdaConnectionFeature feature);
static GdaDataModel *gda_ibmdb2_provider_get_schema (GdaServerProvider *provider,
                                                     GdaConnection *cnc,
                                                     GdaConnectionSchema schema,
                                                     GdaParameterList *params);

static const gboolean gda_ibmdb2_provider_update_database (GdaIBMDB2ConnectionData *conn_data);

static const gchar *gda_ibmdb2_provider_get_version (GdaServerProvider *provider);
static const gchar *gda_ibmdb2_provider_get_server_version (GdaServerProvider *provider,
                                                            GdaConnection *cnc);
static gboolean gda_ibmdb2_provider_single_command (GdaIBMDB2ConnectionData *conn_data,
						    GdaConnection *cnc,
						    const gchar *command);
									    
							   
static gboolean
gda_ibmdb2_provider_open_connection (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     GdaQuarkList *params,
                                     const gchar *username,
                                     const gchar *password)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;
	SQLUSMALLINT flag;
	
	SQLCHAR buffer[255];
	SQLSMALLINT outlen;
	
	const gchar *t_database = NULL;
	const gchar *t_alias = NULL;
	const gchar *t_user = NULL;
	const gchar *t_password = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	t_database = gda_quark_list_find (params, "DATABASE");
	t_alias = gda_quark_list_find (params, "ALIAS");
	t_user = gda_quark_list_find (params, "USER");
	t_password = gda_quark_list_find (params, "PASSWORD");

	if (username)
		t_user = username;
	if (password)
		t_password = password;

	if (t_user == NULL)
		t_user = "";
	if (t_password == NULL)
		t_password = "";
	if (t_alias == NULL)
		t_alias = "sample";
	
/*	if ((t_host == NULL) || (t_user == NULL) || (t_password == NULL)) {
		gda_connection_add_error_string (cnc, _("You must at least provide host, user and password to connect."));
		return FALSE;
	}*/
	
	conn_data = g_new0 (GdaIBMDB2ConnectionData, 1);
	g_return_val_if_fail (conn_data != NULL, FALSE);
	conn_data->henv = SQL_NULL_HANDLE;
	conn_data->hdbc = SQL_NULL_HANDLE;
	conn_data->hstmt = SQL_NULL_HANDLE;
	
	conn_data->rc = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &conn_data->henv);
	if (conn_data->rc != SQL_SUCCESS) {
		if (conn_data->henv != SQL_NULL_HANDLE) {
			SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		}
		g_free (conn_data);
		conn_data = NULL;
		
		gda_connection_add_error_string (cnc, _("Could not allocate environment handle.\n"));
		return FALSE;
	}
	conn_data->rc = SQLAllocHandle (SQL_HANDLE_DBC, conn_data->henv, &conn_data->hdbc);
	if (conn_data->rc != SQL_SUCCESS) {
		if (conn_data->hdbc != SQL_NULL_HANDLE) {
			conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
		}
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		g_free (conn_data);
		conn_data = NULL;
		
		gda_ibmdb2_emit_error (cnc, conn_data->henv, SQL_NULL_HANDLE, SQL_NULL_HANDLE);
		
		return FALSE;
	}
	/* IBM DB2 use autocommit for all DML commands by default.
	   See gda_ibmdb2_provider_begin_transaction and gda_ibmdb2_provider_begin_transaction
	 
	conn_data->rc = SQLSetConnectAttr(conn_data->hdbc, SQL_ATTR_AUTOCOMMIT,
	                            (void *) SQL_AUTOCOMMIT_OFF, SQL_NTS);
	if (conn_data->rc != SQL_SUCCESS) {
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		g_free (conn_data);
		conn_data = NULL;

		gda_connection_add_error_string (cnc, _("Could not set autocommit to off.\n"));
		return FALSE;
	}
	*/
	conn_data->rc = SQLConnect (conn_data->hdbc,
	                      (SQLCHAR *) t_alias, SQL_NTS,
	                      (SQLCHAR *) t_user, SQL_NTS,
	                      (SQLCHAR *) t_password, SQL_NTS);
	if (conn_data->rc != SQL_SUCCESS) {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		g_free (conn_data);
		conn_data = NULL;
		
		return FALSE;
	}

	conn_data->rc = SQLGetFunctions (conn_data->hdbc, SQL_API_SQLGETINFO, &flag);
	if (flag == SQL_TRUE) {
		conn_data->GetInfo_supported = TRUE;

		/* Set Version info */
		conn_data->rc = SQLGetInfo(conn_data->hdbc, SQL_DBMS_VER, buffer, 255, &outlen);
		conn_data->server_version = g_strndup (buffer, outlen);
		
	} else if (flag == SQL_FALSE) {
		conn_data->GetInfo_supported = FALSE;

		/* GetInfo is needed for obtaining database name */
		/* is needed for tables schema */
		gda_connection_add_error_string (cnc, _("SQLGetInfo is unsupported. Hence IBM DB2 Provider will not work.\n"));

		conn_data->rc = SQLDisconnect (conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		g_free (conn_data);
		conn_data = NULL;
		return FALSE;
	} else {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		conn_data->rc = SQLDisconnect (conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
		conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
		g_free (conn_data);
		conn_data = NULL;
				return FALSE;
	}

	g_object_set_data (G_OBJECT  (cnc), OBJECT_DATA_IBMDB2_HANDLE, conn_data);
	
	return TRUE;
}

static gboolean
gda_ibmdb2_provider_close_connection (GdaServerProvider *provider,
                                      GdaConnection *cnc)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;
	gboolean retval = TRUE;
	
	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);

	if (!conn_data)
	       return FALSE;
	
	if (conn_data->hstmt != SQL_NULL_HANDLE) {
		conn_data->rc = SQLFreeHandle(SQL_HANDLE_STMT, conn_data->hstmt);
		if (conn_data->rc != SQL_SUCCESS) {
			gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, conn_data->hstmt);
			retval = FALSE;
		}
		conn_data->hstmt = SQL_NULL_HANDLE;
	}		
		
	conn_data->rc = SQLDisconnect (conn_data->hdbc);
        if (conn_data->rc != SQL_SUCCESS)
        {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		retval = FALSE;
        }
	
	conn_data->rc = SQLFreeHandle (SQL_HANDLE_DBC, conn_data->hdbc);
	if (conn_data->rc != SQL_SUCCESS)
        {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		retval = FALSE;
        }
	conn_data->hdbc = SQL_NULL_HANDLE;
	
	conn_data->rc = SQLFreeHandle (SQL_HANDLE_ENV, conn_data->henv);
	if (conn_data->rc != SQL_SUCCESS)
        {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, SQL_NULL_HANDLE, SQL_NULL_HANDLE);
		retval = FALSE;
        } 
	conn_data->henv = SQL_NULL_HANDLE;
	
	g_free (conn_data->database);
	conn_data->database = NULL;
	
	g_free(conn_data->server_version);
	conn_data->server_version = NULL;
	
	g_free (conn_data);
	conn_data = NULL;
	
	return retval;
}

static const gchar
*gda_ibmdb2_provider_get_database (GdaServerProvider *provider,
                                   GdaConnection *cnc)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);

	g_return_val_if_fail (conn_data != NULL, NULL);
	g_return_val_if_fail (conn_data->GetInfo_supported, NULL);

	if (gda_ibmdb2_provider_update_database (conn_data)) {
		return (const gchar *) conn_data->database;
	}
	
	return NULL;
}

static gboolean
gda_ibmdb2_provider_change_database (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     const gchar *name)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return FALSE;
}

static gboolean
gda_ibmdb2_provider_single_command (GdaIBMDB2ConnectionData *conn_data,
                                    GdaConnection *cnc,
				    const gchar *command)
{
	SQLHANDLE hstmt = SQL_NULL_HANDLE;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
        g_return_val_if_fail (conn_data != NULL, FALSE);
	g_return_val_if_fail (command != NULL, FALSE);

	conn_data->rc = SQLAllocHandle (SQL_HANDLE_STMT, conn_data->hdbc, &hstmt);
	if (conn_data->rc != SQL_SUCCESS)
	{
                gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
                return FALSE;
	}

	conn_data->rc = SQLExecDirect(hstmt, (SQLCHAR*)command, SQL_NTS);	

	if (conn_data->rc != SQL_SUCCESS)
	{
                gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
		return FALSE;
	}

	conn_data->rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt );

	if (conn_data->rc != SQL_SUCCESS)
	{
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
                return FALSE;
	}
	return TRUE;
}


static gboolean
gda_ibmdb2_provider_create_database_cnc (GdaServerProvider *provider,                                                    
				         GdaConnection *cnc,
                                         const gchar *name)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;
	gchar *sql;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
        conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        g_return_val_if_fail (conn_data != NULL, FALSE);
	
	sql = g_strdup_printf("CREATE DATABASE %s", name);
	retval = gda_ibmdb2_provider_single_command (conn_data, cnc, sql);	
	g_free(sql);
	
	return retval;
}



static gboolean
gda_ibmdb2_provider_drop_database (GdaServerProvider *provider,
                                   GdaConnection *cnc,
                                   const gchar *name)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;
	gchar *sql;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
        conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        g_return_val_if_fail (conn_data != NULL, FALSE);
	
	sql = g_strdup_printf("DROP DATABASE %s", name);
	retval = gda_ibmdb2_provider_single_command (conn_data, cnc, sql);	
	g_free(sql);
	
	return retval;
}

static const gboolean 
gda_ibmdb2_provider_update_database (GdaIBMDB2ConnectionData *conn_data)
{
	SQLRETURN   rc = SQL_SUCCESS;
	SQLCHAR     database[255];
	SQLSMALLINT name_len = 0;
	
	g_return_val_if_fail (conn_data != NULL, FALSE);
	g_return_val_if_fail (conn_data->GetInfo_supported != FALSE, FALSE);

	rc = SQLGetInfo (conn_data->hdbc, SQL_DATABASE_NAME, database,
	                 sizeof(database), &name_len);
	if (rc != SQL_SUCCESS) {
		return FALSE;
	}
	if (conn_data->database) {
		g_free (conn_data->database);
	}
	conn_data->database = g_strndup (database, name_len);
	if (conn_data->database) {
		return TRUE;
	}

	return FALSE;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql, GdaCommandOptions options)
{
	GdaIBMDB2ConnectionData *conn_data = NULL;
        gchar **arr;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        if (!conn_data) {
		gda_connection_add_error_string (cnc, _("Invalid IBM DB2 handle"));
		return NULL;
	}
	
	arr = g_strsplit (sql, ";", 0);
	
        if (arr) {
 		gint n = 0;     
		while (arr[n]) {
			GdaDataModel *recset;
		
			g_assert(conn_data->hstmt == SQL_NULL_HANDLE);
			
			conn_data->rc = SQLAllocHandle (SQL_HANDLE_STMT, conn_data->hdbc, &conn_data->hstmt);
			if (conn_data->rc != SQL_SUCCESS) {
				gda_ibmdb2_emit_error(cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
				conn_data->hstmt = SQL_NULL_HANDLE;
				
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
				
				if (options == GDA_COMMAND_OPTION_STOP_ON_ERRORS) {
					g_strfreev(arr);
					return NULL;
				}
			}
					
			conn_data->rc = SQLExecDirect(conn_data->hstmt,(SQLCHAR*)arr[n], SQL_NTS);
			if (conn_data->rc != SQL_SUCCESS) {
				gda_ibmdb2_emit_error(cnc, conn_data->henv, conn_data->hdbc, conn_data->hstmt);
				
				conn_data->rc = SQLFreeHandle(SQL_HANDLE_STMT, conn_data->hstmt);
				if (conn_data->rc != SQL_SUCCESS) {
					gda_ibmdb2_emit_error(cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
				}
				conn_data->hstmt = SQL_NULL_HANDLE;
				
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);

				if (options == GDA_COMMAND_OPTION_STOP_ON_ERRORS) {
					g_strfreev(arr);
					return NULL;
				}
			}

			recset = gda_ibmdb2_recordset_new (cnc, conn_data->hstmt);

                    	if (GDA_IS_IBMDB2_RECORDSET (recset)) {
				gda_data_model_set_command_text (recset, arr[n]);
				gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
                                reclist = g_list_append (reclist, recset);
                    	} else {
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
			}
			
			conn_data->rc = SQLFreeHandle(SQL_HANDLE_STMT, conn_data->hstmt);
			if (conn_data->rc != SQL_SUCCESS) {
				gda_ibmdb2_emit_error(cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
				
				if (options == GDA_COMMAND_OPTION_STOP_ON_ERRORS) {
					g_strfreev(arr);
					return NULL;
				}
			}
			conn_data->hstmt = SQL_NULL_HANDLE;
			
			n++;
		}
	}
	g_strfreev(arr);
	
	return reclist;
}
				
static GList
*gda_ibmdb2_provider_execute_command (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      GdaCommand *cmd,
                                      GdaParameterList *params)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaCommandOptions options;
	GList  *reclist = NULL;
	gchar *str;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

        options = gda_command_get_options (cmd);
        switch (gda_command_get_command_type (cmd)) {
            case GDA_COMMAND_TYPE_SQL:
                reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd), options);
                break;
	    case GDA_COMMAND_TYPE_TABLE:
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
                reclist = process_sql_commands (reclist, cnc, str, options);
                if (reclist && GDA_IS_DATA_MODEL (reclist->data)) {
			gda_data_model_set_command_text (GDA_DATA_MODEL (reclist->data),
                                                         gda_command_get_text (cmd));
                        gda_data_model_set_command_type (GDA_DATA_MODEL (reclist->data),
                                                         GDA_COMMAND_TYPE_TABLE);
		}
		g_free(str);
		break;
            case GDA_COMMAND_TYPE_XML:
	    case GDA_COMMAND_TYPE_PROCEDURE:
	    case GDA_COMMAND_TYPE_SCHEMA:
	    case GDA_COMMAND_TYPE_INVALID:
		return NULL;
		break;
	}
	return reclist;
}

static gboolean
gda_ibmdb2_provider_begin_transaction (GdaServerProvider *provider,
                                       GdaConnection *cnc,
                                       GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        if (!conn_data) {
		gda_connection_add_error_string (cnc, _("Invalid IBM DB2 handle"));
		return FALSE;
	}

	conn_data->rc = SQLSetConnectAttr (conn_data->hdbc,
					   SQL_ATTR_AUTOCOMMIT,
				           (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 
				           SQL_NTS);
	if (conn_data->rc != SQL_SUCCESS)
	{
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
                return FALSE;
	}
								      
	return TRUE;
}

static gboolean
gda_ibmdb2_provider_commit_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        g_return_val_if_fail (conn_data != NULL, FALSE);
	
    	conn_data->rc = SQLTransact (conn_data->henv, conn_data->hdbc, SQL_COMMIT);
    
	if (conn_data->rc != SQL_SUCCESS)
	{
                gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		return FALSE;
	}

	conn_data->rc = SQLSetConnectAttr (conn_data->hdbc,
					   SQL_ATTR_AUTOCOMMIT,
				           (SQLPOINTER)SQL_AUTOCOMMIT_ON, 
				           SQL_NTS);
	if (conn_data->rc != SQL_SUCCESS)
	{
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
                return FALSE;
	}
	return TRUE;
}

static gboolean
gda_ibmdb2_provider_rollback_transaction (GdaServerProvider *provider,
                                          GdaConnection *cnc,
                                          GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *conn_data = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        g_return_val_if_fail (conn_data != NULL, FALSE);
	
    	conn_data->rc = SQLTransact (conn_data->henv, conn_data->hdbc, SQL_ROLLBACK);
    
	if (conn_data->rc != SQL_SUCCESS)
	{
                gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		return FALSE;
	}

	conn_data->rc = SQLSetConnectAttr (conn_data->hdbc,
					   SQL_ATTR_AUTOCOMMIT,
				           (SQLPOINTER)SQL_AUTOCOMMIT_ON, 
				           SQL_NTS);
	if (conn_data->rc != SQL_SUCCESS)
	{
                gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, SQL_NULL_HANDLE);
		return FALSE;
	}

	return TRUE;

}

static const gchar *
gda_ibmdb2_provider_get_version (GdaServerProvider *provider)
{
    GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	
    g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
		
    return PACKAGE_VERSION;
}
				
static const gchar *
gda_ibmdb2_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
    GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
    GdaIBMDB2ConnectionData *conn_data;

    g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
    g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

    conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
    if (!conn_data) {
            gda_connection_add_error_string (cnc, _("Invalid IBM DB2 handle"));
            return NULL;
    }
    return conn_data->server_version;
}


static gboolean
gda_ibmdb2_provider_supports (GdaServerProvider *provider,
                              GdaConnection *cnc,
                              GdaConnectionFeature feature)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
        switch (feature) {
	    case GDA_CONNECTION_FEATURE_SQL:
	    case GDA_CONNECTION_FEATURE_TRANSACTIONS:
	    case GDA_CONNECTION_FEATURE_AGGREGATES:
	    case GDA_CONNECTION_FEATURE_INDEXES:
	    case GDA_CONNECTION_FEATURE_PROCEDURES:
	    case GDA_CONNECTION_FEATURE_TRIGGERS:
	    case GDA_CONNECTION_FEATURE_VIEWS:
	    case GDA_CONNECTION_FEATURE_USERS:
		return TRUE;
	    case GDA_CONNECTION_FEATURE_NAMESPACES:
		return FALSE;
	    default:
		return FALSE;
	}
	return FALSE;
}

					
static GdaDataModel
*get_db2_aggregates(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        reclist = process_sql_commands (NULL, cnc,
					"SELECT FUNCNAME,FUNCSCHEMA FROM SYSCAT.FUNCTIONS ORDER BY FUNCNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Function"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
	
	return recset;
}

static GdaDataModel
*get_db2_databases(GdaConnection *cnc, GdaParameterList *params)
{
	GdaIBMDB2ConnectionData *conn_data = NULL;
	SQLCHAR alias[SQL_MAX_DSN_LENGTH + 1];
	SQLCHAR comment[255];
	SQLSMALLINT alias_len, comment_len;
	gint i;
	GdaDataModelArray *recset;
	GList *value_list;	
	gchar *str;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
        if (!conn_data) {
		gda_connection_add_error_string (cnc, _("Invalid IBM DB2 handle"));
		return NULL;
	}

	conn_data->rc = SQLDataSources (conn_data->henv,
				        SQL_FETCH_NEXT,
				        alias,
				        SQL_MAX_DSN_LENGTH + 1,
				        &alias_len,
				        comment,
				        255,
				       &comment_len);
	
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Database"));

	for (i = 0; conn_data->rc != SQL_NO_DATA_FOUND; i++)
	{
	    
		str = g_strndup(alias, alias_len);
	    
	    	value_list = g_list_append (NULL, gda_value_new_string (str));
	    	gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);
	   
	    	g_free(str);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	    	
		conn_data->rc = SQLDataSources (conn_data->henv,
					        SQL_FETCH_NEXT,
					        alias,
				                SQL_MAX_DSN_LENGTH + 1,
				                &alias_len,
				                comment,
				                255,
				                &comment_len);
	}

	
	return GDA_DATA_MODEL (recset);
}

static GdaDataModel
*get_db2_fields(GdaConnection *cnc, GdaParameterList *params)
{
	return NULL;
}

static GdaDataModel
*get_db2_indexes(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        reclist = process_sql_commands (NULL, cnc,
					"SELECT INDNAME,INDSCHEMA,TABNAME FROM SYSCAT.INDEXES "
					"WHERE (TABSCHEMA != 'SYSIBM') ORDER BY INDNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Index"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
        gda_data_model_set_column_title (recset, 2, _("Table"));
	
	return recset;
}

static GdaDataModel
*get_db2_procedures(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* May be use SQLTables? */
        reclist = process_sql_commands (NULL, cnc,
					"SELECT PROCNAME,PROCSCHEMA,LANGUAGE,REMARKS "
					"FROM SYSCAT.PROCEDURES "
					"WHERE (DEFINER != 'SYSIBM') ORDER BY PROCNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Procedure"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
	gda_data_model_set_column_title (recset, 2, _("Language"));
        gda_data_model_set_column_title (recset, 3, _("Description"));
	
	return recset;
}


static GdaDataModel
*get_db2_tables(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* May be use SQLTables? */
        reclist = process_sql_commands (NULL, cnc,
					"SELECT TABNAME,TABSCHEMA,REMARKS,'' FROM SYSCAT.TABLES "
					"WHERE (TYPE='T' AND DEFINER != 'SYSIBM') ORDER BY TABNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Table"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
        gda_data_model_set_column_title (recset, 2, _("Description"));
	gda_data_model_set_column_title (recset, 3, _("SQL Definition"));
	
	return recset;
}

static GdaDataModel
*get_db2_triggers(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        reclist = process_sql_commands (NULL, cnc,
					"SELECT TRIGNAME,TRIGSCHEMA,REMARKS,TEXT FROM SYSCAT.TRIGGERS "
					"WHERE (DEFINER != 'SYSIBM') ORDER BY TRIGNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Triggers"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
        gda_data_model_set_column_title (recset, 2, _("Description"));
	gda_data_model_set_column_title (recset, 3, _("SQL Definition"));
	
	return recset;
}

static GdaDataModel
*get_db2_types(GdaConnection *cnc, GdaParameterList *params)
{
        GdaDataModel *recset;
	GList *reclist;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc,
					"SELECT TYPENAME,DEFINER,REMARKS,'' FROM SYSCAT.DATATYPES "
					"ORDER BY TYPENAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist) {
		return NULL;
	}
	
        recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
        gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
        gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
        gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));

        return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_db2_users (GdaConnection *cnc, GdaParameterList *params)
{
        GList *reclist;
        GdaDataModel *recset;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        reclist = process_sql_commands (NULL, cnc, 
					"SELECT DISTINCT GRANTEE FROM SYSCAT.TABAUTH WHERE GRANTEETYPE = 'U' "
					"UNION SELECT DISTINCT GRANTEE FROM SYSCAT.INDEXAUTH WHERE GRANTEETYPE = 'U' "
					"UNION SELECT DISTINCT GRANTEE FROM SYSCAT.SCHEMAAUTH WHERE GRANTEETYPE = 'U' "
					"UNION SELECT DISTINCT GRANTEE FROM SYSCAT.DBAUTH WHERE GRANTEETYPE = 'U'",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
        if (!reclist)
                return NULL;

        recset = GDA_DATA_MODEL (reclist->data);
        g_list_free (reclist);

        gda_data_model_set_column_title (recset, 0, _("Users"));

        return recset;
}

static GdaDataModel
*get_db2_views(GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GList *reclist;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        reclist = process_sql_commands (NULL, cnc,
					"SELECT VIEWNAME,VIEWSCHEMA,'',TEXT FROM SYSCAT.VIEWS "
					"WHERE (DEFINER != 'SYSIBM') ORDER BY VIEWNAME",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
	{
		return NULL;
	}

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	
        gda_data_model_set_column_title (recset, 0, _("Table"));
        gda_data_model_set_column_title (recset, 1, _("Owner"));
        gda_data_model_set_column_title (recset, 2, _("Description"));
	gda_data_model_set_column_title (recset, 3, _("SQL Definition"));
	
	return recset;
}


static GdaDataModel
*gda_ibmdb2_provider_get_schema (GdaServerProvider *provider,
				 GdaConnection *cnc,
                                 GdaConnectionSchema schema,
                                 GdaParameterList *params)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	
	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        switch (schema) {
        case GDA_CONNECTION_SCHEMA_AGGREGATES :
                return get_db2_aggregates (cnc, params);
        case GDA_CONNECTION_SCHEMA_DATABASES :
                return get_db2_databases (cnc, params);
        case GDA_CONNECTION_SCHEMA_FIELDS :
                return get_db2_fields (cnc, params);
	case GDA_CONNECTION_SCHEMA_INDEXES :
		return get_db2_indexes (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		return get_db2_procedures (cnc, params);
        case GDA_CONNECTION_SCHEMA_TABLES :
                return get_db2_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TRIGGERS :
		return get_db2_triggers (cnc, params);
        case GDA_CONNECTION_SCHEMA_TYPES :
                return get_db2_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_USERS :
                return get_db2_users (cnc, params);
	case GDA_CONNECTION_SCHEMA_VIEWS :
		return get_db2_views (cnc, params);
	default:
		return NULL;
        }

	return NULL;
}


/* Initialization */
static void
gda_ibmdb2_provider_class_init (GdaIBMDB2ProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_ibmdb2_provider_finalize;
	provider_class->get_version = gda_ibmdb2_provider_get_version;
	provider_class->open_connection = gda_ibmdb2_provider_open_connection;
	provider_class->close_connection = gda_ibmdb2_provider_close_connection;
	provider_class->get_server_version = gda_ibmdb2_provider_get_server_version;
	provider_class->get_database = gda_ibmdb2_provider_get_database;
	provider_class->change_database = gda_ibmdb2_provider_change_database;
	provider_class->create_database_cnc = gda_ibmdb2_provider_create_database_cnc;
	provider_class->drop_database = gda_ibmdb2_provider_drop_database;
	provider_class->execute_command = gda_ibmdb2_provider_execute_command;
	provider_class->begin_transaction = gda_ibmdb2_provider_begin_transaction;
	provider_class->commit_transaction = gda_ibmdb2_provider_commit_transaction;
	provider_class->rollback_transaction = gda_ibmdb2_provider_rollback_transaction;
	provider_class->supports = gda_ibmdb2_provider_supports;
	provider_class->get_schema = gda_ibmdb2_provider_get_schema;
}

static void
gda_ibmdb2_provider_init (GdaIBMDB2Provider *provider,
                          GdaIBMDB2ProviderClass *klass)
{
}

static void
gda_ibmdb2_provider_finalize (GObject *object)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) object;
	
	g_return_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov));
 	
	/* chain to parent class */
	parent_class->finalize (object);
}


/*
 * Public functions                                                       
 */

GType
gda_ibmdb2_provider_get_type (void)
{
        static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaIBMDB2ProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_ibmdb2_provider_class_init,
			NULL, NULL,
			sizeof (GdaIBMDB2Provider),
			0,
			(GInstanceInitFunc) gda_ibmdb2_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaIBMDB2Provider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_ibmdb2_provider_new (void)
{
	GdaIBMDB2Provider *provider;

	provider = g_object_new (gda_ibmdb2_provider_get_type (), NULL);
	
	return GDA_SERVER_PROVIDER (provider);
}
