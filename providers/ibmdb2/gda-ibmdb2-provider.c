/* GDA IBMDB2 Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db-org>
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
static gboolean gda_ibmdb2_provider_create_database (GdaServerProvider *provider,
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

static const gboolean gda_ibmdb2_provider_update_database (GdaIBMDB2ConnectionData *db2);


static gboolean
gda_ibmdb2_provider_open_connection (GdaServerProvider *provider,
                                     GdaConnection *cnc,
                                     GdaQuarkList *params,
                                     const gchar *username,
                                     const gchar *password)
{
	GdaError *error = NULL;
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *db2 = NULL;
	SQLUSMALLINT flag;
	gchar *db2_dsn = NULL;

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
	
/*
	if ((t_host == NULL) || (t_user == NULL) || (t_password == NULL)) {
		gda_connection_add_error_string (cnc, _("You must at least provide host, user and password to connect."));
		return FALSE;
	}
*/
	
	db2 = g_new0 (GdaIBMDB2ConnectionData, 1);
	g_return_val_if_fail (db2 != NULL, FALSE);
	db2->henv = SQL_NULL_HANDLE;
	db2->hdbc = SQL_NULL_HANDLE;
	
	db2->rc = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &db2->henv);
	if (db2->rc != SQL_SUCCESS) {
		if (db2->henv != SQL_NULL_HANDLE) {
			SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		}
		g_free (db2);
		db2 = NULL;
		
		gda_connection_add_error_string (cnc, _("Could not allocate environment handle."));
		return FALSE;
	}
	db2->rc = SQLAllocHandle (SQL_HANDLE_DBC, db2->henv, &db2->hdbc);
	if (db2->rc != SQL_SUCCESS) {
		if (db2->hdbc != SQL_NULL_HANDLE) {
			db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
		}
		db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		g_free (db2);
		db2 = NULL;
		
		gda_connection_add_error_string (cnc, _("Could not allocate environment handle."));
		return FALSE;
	}
	db2->rc = SQLSetConnectAttr(db2->hdbc, SQL_ATTR_AUTOCOMMIT,
	                            (void *) SQL_AUTOCOMMIT_OFF, SQL_NTS);
	if (db2->rc != SQL_SUCCESS) {
		db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		g_free (db2);
		db2 = NULL;

		gda_connection_add_error_string (cnc, _("Could not set autocommit to off.\n"));
		return FALSE;
	}
	db2->rc = SQLConnect (db2->hdbc,
	                      (SQLCHAR *) t_alias, SQL_NTS,
	                      (SQLCHAR *) t_user, SQL_NTS,
	                      (SQLCHAR *) t_password, SQL_NTS
	                     );
	if (db2->rc != SQL_SUCCESS) {
		db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		g_free (db2);
		db2 = NULL;

		gda_connection_add_error_string (cnc, _("Could not allocate environment handle.\n"));
		return FALSE;
	}

	db2->rc = SQLGetFunctions (db2->hdbc, SQL_API_SQLGETINFO, &flag);
	if (flag == SQL_TRUE) {
		db2->GetInfo_supported = TRUE;
	} else if (flag == SQL_FALSE) {
		db2->GetInfo_supported = FALSE;

		/* GetInfo is needed for obtaining database name 
		 * is needed for tables schema
		 */
		gda_connection_add_error_string (cnc, _("SQLGetInfo is unsupported. Hence IBM DB2 Provider will not work.\n"));

		db2->rc = SQLDisconnect (db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		g_free (db2);
		db2 = NULL;
		return FALSE;
	} else {
		db2->rc = SQLDisconnect (db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
		db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
		g_free (db2);
		db2 = NULL;
		gda_connection_add_error_string (cnc, _("SQLGetFunctions gave unexpected return. Connection terminated.\n"));
		return FALSE;
	}

	g_object_set_data (G_OBJECT  (cnc), OBJECT_DATA_IBMDB2_HANDLE, db2);
	
	return TRUE;
}

static gboolean
gda_ibmdb2_provider_close_connection (GdaServerProvider *provider,
                                      GdaConnection *cnc)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *db2 = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	db2 = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
	g_return_val_if_fail (db2 != NULL, FALSE);
	
	if (db2->database) {
		g_free (db2->database);
		db2->database = NULL;
	}
	/* FIXME: error handling */
	db2->rc = SQLDisconnect (db2->hdbc);
	db2->rc = SQLFreeHandle (SQL_HANDLE_DBC, db2->hdbc);
	db2->rc = SQLFreeHandle (SQL_HANDLE_ENV, db2->henv);
	
	g_free (db2);
	db2 = NULL;
	
	return TRUE;
}

static const gchar
*gda_ibmdb2_provider_get_database (GdaServerProvider *provider,
                                   GdaConnection *cnc)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;
	GdaIBMDB2ConnectionData *db2 = NULL;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	db2 = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);
	g_return_val_if_fail (db2 != NULL, NULL);
	g_return_val_if_fail (db2->GetInfo_supported, NULL);

	if (gda_ibmdb2_provider_update_database (db2)) {
		return (const gchar *) db2->database;
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
gda_ibmdb2_provider_create_database (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                     const gchar *name)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	return FALSE;
}

static gboolean
gda_ibmdb2_provider_drop_database (GdaServerProvider *provider,
                                   GdaConnection *cnc,
                                   const gchar *name)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return FALSE;
}

static GList
*gda_ibmdb2_provider_execute_command (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      GdaCommand *cmd,
                                      GdaParameterList *params)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	return NULL;
}

static gboolean
gda_ibmdb2_provider_begin_transaction (GdaServerProvider *provider,
                                       GdaConnection *cnc,
                                       GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return FALSE;
}

static gboolean
gda_ibmdb2_provider_commit_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return FALSE;
}

static gboolean
gda_ibmdb2_provider_rollback_transaction (GdaServerProvider *provider,
                                          GdaConnection *cnc,
                                          GdaTransaction *xaction)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return FALSE;
}

static gboolean
gda_ibmdb2_provider_supports (GdaServerProvider *provider,
                              GdaConnection *cnc,
                              GdaConnectionFeature feature)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), FALSE);

	return FALSE;
}

static GdaDataModel
*gda_ibmdb2_provider_get_schema (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                 GdaConnectionSchema schema,
                                 GdaParameterList *params)
{
	GdaIBMDB2Provider *db2_prov = (GdaIBMDB2Provider *) provider;

	g_return_val_if_fail (GDA_IS_IBMDB2_PROVIDER (db2_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	return NULL;
}

static const gboolean 
gda_ibmdb2_provider_update_database (GdaIBMDB2ConnectionData *db2)
{
	SQLRETURN   rc = SQL_SUCCESS;
	SQLCHAR     database[255];
	SQLSMALLINT name_len = 0;
	
	g_return_val_if_fail (db2 != NULL, FALSE);
	g_return_val_if_fail (db2->GetInfo_supported != FALSE, FALSE);

	rc = SQLGetInfo (db2->hdbc, SQL_DATABASE_NAME, database,
	                 sizeof(database), &name_len);
	if (rc != SQL_SUCCESS) {
		return FALSE;
	}
	if (db2->database) {
		g_free (db2->database);
	}
	db2->database = g_strndup (database, name_len);
	if (db2->database) {
		return TRUE;
	}

	return FALSE;
}



/* Initialization */
static void
gda_ibmdb2_provider_class_init (GdaIBMDB2ProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_ibmdb2_provider_finalize;
	provider_class->open_connection = gda_ibmdb2_provider_open_connection;
	provider_class->close_connection = gda_ibmdb2_provider_close_connection;
	provider_class->get_database = gda_ibmdb2_provider_get_database;
	provider_class->change_database = gda_ibmdb2_provider_change_database;
	provider_class->create_database = gda_ibmdb2_provider_create_database;
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
	GdaIBMDB2Provider *provider = (GdaIBMDB2Provider *) object;

	g_return_if_fail (GDA_IS_IBMDB2_PROVIDER (provider));

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
