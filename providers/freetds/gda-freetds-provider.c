/* GDA FreeTDS Provider
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

#include <config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <glib.h>
#include <stdlib.h>
#include <tds.h>
#include "gda-freetds.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_FREETDS_HANDLE "GDA_FreeTDS_FreeTDSHandle"

/////////////////////////////////////////////////////////////////////////////
// Private declarations and functions
/////////////////////////////////////////////////////////////////////////////

static GObjectClass *parent_class = NULL;

static void gda_freetds_provider_class_init (GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_init       (GdaFreeTDSProvider      *provider,
                                             GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_finalize   (GObject                 *object);

static gboolean gda_freetds_provider_open_connection (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      GdaQuarkList *params,
                                                      const gchar *username,
                                                      const gchar *password);
static gboolean gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                                       GdaConnection *cnc);
static const gchar *gda_freetds_provider_get_database (GdaServerProvider *provider,
                                                       GdaConnection *cnc);
static gboolean gda_freetds_provider_change_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      const gchar *name);
static gboolean gda_freetds_provider_create_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      const gchar *name);
static gboolean gda_freetds_provider_drop_database (GdaServerProvider *provider,
                                                    GdaConnection *cnc,
                                                    const gchar *name);
static GList *gda_freetds_provider_execute_command (GdaServerProvider *provider,
                                                    GdaConnection *cnc,
                                                    GdaCommand *cmd,
                                                    GdaParameterList *params);
static gboolean gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        GdaTransaction *xaction);
static gboolean gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                                         GdaConnection *cnc,
                                                         GdaTransaction *xaction);
static gboolean gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                                           GdaConnection *cnc,
                                                           GdaTransaction *xaction);
static gboolean gda_freetds_provider_supports (GdaServerProvider *provider,
                                               GdaConnection *cnc,
                                               GdaConnectionFeature feature);
static GdaDataModel *gda_freetds_provider_get_schema (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      GdaConnectionSchema schema,
                                                      GdaParameterList *params);
static GdaDataModel * gda_freetds_execute_query (GdaConnection *cnc,
                                                 const gchar* sql);
static GdaDataModel *gda_freetds_get_databases (GdaConnection *cnc,
                                                GdaParameterList *params);
static GdaDataModel *gda_freetds_get_fields (GdaConnection *cnc,
                                             GdaParameterList *params);

static GList* gda_freetds_provider_process_sql_commands(GList         *reclist,
                                                        GdaConnection *cnc,
                                                        const gchar   *sql);

static gboolean tds_cbs_initialized = FALSE;
extern int (*g_tds_msg_handler)();
extern int (*g_tds_err_handler)();

static int gda_freetds_provider_tds_handle_message (void *aStruct,
                                                    const gboolean is_err_msg);
static int gda_freetds_provider_tds_handle_info_msg (void *aStruct);
static int gda_freetds_provider_tds_handle_err_msg (void *aStruct);

static gboolean
gda_freetds_provider_open_connection (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      GdaQuarkList *params,
                                      const gchar *username,
                                      const gchar *password)
{
	const gchar *t_database = NULL;
	const gchar *t_host = NULL;
	const gchar *t_hostaddr = NULL;
	const gchar *t_options = NULL;
	const gchar *t_password = NULL;
	const gchar *t_port = NULL;
	const gchar *t_user = NULL;
	
	const gchar *tds_majver = NULL;
	const gchar *tds_minver = NULL;
	const gchar *tds_sybase = NULL;
	const gchar *tds_freetdsconf = NULL;
	const gchar *tds_host = NULL;
	const gchar *tds_query = NULL;
	const gchar *tds_port = NULL;
	const gchar *tds_dump = NULL;
	const gchar *tds_dumpconfig = NULL;

	GdaError *error = NULL;
	GdaFreeTDSProvider *tds_provider = (GdaFreeTDSProvider *) provider;
	GdaFreeTDSConnectionData *tds_cnc = NULL;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	t_database = gda_quark_list_find (params, "DATABASE");
	t_host = gda_quark_list_find (params, "HOST");
	t_hostaddr = gda_quark_list_find (params, "HOSTADDR");
	t_options = gda_quark_list_find (params, "OPTIONS");
	t_user = gda_quark_list_find (params, "USERNAME");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_port = gda_quark_list_find (params, "PORT");
	
	// These shall override environment variables or previous settings
	tds_majver = gda_quark_list_find (params, "TDS_MAJVER");
	tds_minver = gda_quark_list_find (params, "TDS_MINVER");
	
	tds_sybase = gda_quark_list_find (params, "SYBASE");
	tds_freetdsconf = gda_quark_list_find (params, "TDS_FREETDSCONF");
	tds_host = gda_quark_list_find (params, "TDS_HOST");
	tds_query = gda_quark_list_find (params, "TDS_QUERY");
	tds_port = gda_quark_list_find (params, "TDS_PORT");
	tds_dump = gda_quark_list_find (params, "TDS_DUMP");
	tds_dumpconfig = gda_quark_list_find (params, "TDS_DUMPCONFIG");

	if (username)
		t_user = username;
	if ((password != NULL) & (t_password == NULL))
		t_password = password;
	if (tds_query)
		t_host = tds_query;
	if (tds_host)
		t_hostaddr = tds_host;
	if (tds_port)
		t_port = tds_port;
	
	// sanity check
	// FreeTDS SIGSEGV on NULL pointers
	if ((t_user == NULL) || (t_host == NULL) || (t_password == NULL)) {
		error = gda_freetds_make_error(NULL, _("Connection aborted. You must provide at least a host, username and password using DSN 'QUERY=;USER=;PASSWORD='."));
		gda_connection_add_error(cnc, error);

		return FALSE;
	}
	
	tds_cnc = g_new0 (GdaFreeTDSConnectionData, 1);
	g_return_val_if_fail(tds_cnc != NULL, FALSE);
	tds_cnc->msg_arr = g_ptr_array_new ();

	if (tds_cnc->msg_arr == NULL) {
		g_free (tds_cnc);
		tds_cnc = NULL;
		gda_connection_add_error_string (cnc, _("Error creating message container."));
		return FALSE;
	}

	// allocate login
	tds_cnc->login = tds_alloc_login();
	if (! tds_cnc->login) {
		g_free(tds_cnc);
		tds_cnc = NULL;
		return FALSE;
	}
	
	// set tds version
	if ((tds_majver != NULL) & (tds_minver != NULL))
		tds_set_version(tds_cnc->login,
		                (short) atoi (tds_majver),
		                (short) atoi (tds_minver)
		               );

	// apply connection settings
	tds_set_user(tds_cnc->login, (char *) t_user);
	tds_set_passwd(tds_cnc->login, (char *) t_password);
	tds_set_app(tds_cnc->login, "libgda");
	if (t_hostaddr)
		tds_set_host(tds_cnc->login, (char *) t_hostaddr);
	tds_set_library(tds_cnc->login, "TDS-Library");
	if (t_host)
		tds_set_server(tds_cnc->login, (char *) t_host);
	tds_set_charset(tds_cnc->login, "iso-1");
	tds_set_language(tds_cnc->login, "us_english");
	tds_set_packet(tds_cnc->login, 512);
	
	// establish connection
	tds_cnc->tds = tds_connect(tds_cnc->login, NULL);
	if (! tds_cnc->tds) {
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
		g_free(tds_cnc);
		tds_cnc = NULL;
		error = gda_freetds_make_error(NULL, _("Establishing connection failed."));
		gda_connection_add_error(cnc, error);
		return FALSE;
	}

	// try to receive connection info for sanity check
	tds_cnc->config = tds_get_config(tds_cnc->tds, tds_cnc->login);
	if (! tds_cnc->config) {
		error = gda_freetds_make_error (tds_cnc->tds, _("Failed getting connection info."));
		gda_connection_add_error (cnc, error);

		tds_free_socket(tds_cnc->tds);
		tds_cnc->tds = NULL;
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
		g_free(tds_cnc);
		tds_cnc = NULL;
		return FALSE;
	}

	tds_cnc->rc = TDS_SUCCEED;
	tds_set_parent (tds_cnc->tds, (void *) cnc);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE, tds_cnc);

	return TRUE;
}

static gboolean
gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                       GdaConnection *cnc)
{
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaFreeTDSProvider *tds_provider = (GdaFreeTDSProvider *) provider;
	GdaFreeTDSMessage *msg = NULL;

	g_return_val_if_fail(GDA_IS_FREETDS_PROVIDER (tds_provider), FALSE);
	g_return_val_if_fail(GDA_IS_CONNECTION (cnc), FALSE);

	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	if (! tds_cnc)
		return FALSE;

	if (tds_cnc->config) {
		tds_free_config(tds_cnc->config);
		tds_cnc->config = NULL;
	}
	if (tds_cnc->tds) {
		tds_set_parent (tds_cnc->tds, NULL);
		tds_free_socket (tds_cnc->tds);
		tds_cnc->tds = NULL;
	}
	if (tds_cnc->login) {
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
	}

	if (tds_cnc->msg_arr) {
		while (tds_cnc->msg_arr->len > 0) {
			msg = (GdaFreeTDSMessage *) g_ptr_array_index (tds_cnc->msg_arr, 0);
			if (msg != NULL) {
				gda_freetds_message_free (msg);
			}

			g_ptr_array_remove_index (tds_cnc->msg_arr, 0);
		}
		g_ptr_array_free (tds_cnc->msg_arr, TRUE);
		tds_cnc->msg_arr = NULL;
	}
	
	g_free(tds_cnc);
	tds_cnc = NULL;
	
	return TRUE;
}

static const gchar
*gda_freetds_provider_get_database (GdaServerProvider *provider,
                                    GdaConnection *cnc)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	GdaFreeTDSConnectionData *tds_cnc;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	
	if (tds_cnc->config) {
		tds_free_config(tds_cnc->config);
		tds_cnc->config = NULL;
	}
	tds_cnc->config = tds_get_config(tds_cnc->tds, tds_cnc->login);
	
	return tds_cnc->config->database;
}

static gboolean
gda_freetds_provider_change_database (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      const gchar *name)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_create_database (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                      const gchar *name)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_drop_database (GdaServerProvider *provider,
                                    GdaConnection *cnc,
                                    const gchar *name)
{
	return FALSE;
}

static GList
*gda_freetds_provider_execute_command (GdaServerProvider *provider,
                                       GdaConnection *cnc,
                                       GdaCommand *cmd,
                                       GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *query = NULL;
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	switch (gda_command_get_command_type (cmd)) {
		case GDA_COMMAND_TYPE_SQL:
			reclist = gda_freetds_provider_process_sql_commands (reclist, cnc, gda_command_get_text (cmd));
			break;
		case GDA_COMMAND_TYPE_TABLE:
			query = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
			reclist = gda_freetds_provider_process_sql_commands (reclist, cnc, query);
			if (reclist && GDA_IS_DATA_MODEL (reclist->data)) {
				gda_data_model_set_command_text (GDA_DATA_MODEL (reclist->data),
				                                 gda_command_get_text (cmd));
				gda_data_model_set_command_type (GDA_DATA_MODEL (reclist->data),
				                                 GDA_COMMAND_TYPE_TABLE);
			}
			g_free(query);
			query = NULL;
			break;
	}

	return reclist;
}

static gboolean
gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                         GdaConnection *cnc,
                                         GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                           GdaConnection *cnc,
                                           GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_supports (GdaServerProvider *provider,
                               GdaConnection *cnc,
                               GdaConnectionFeature feature)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), FALSE);

	switch (feature) {
		case GDA_CONNECTION_FEATURE_SQL:
			return TRUE;
	}
	
	return FALSE;
}

static GdaDataModel
*gda_freetds_provider_get_schema (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                  GdaConnectionSchema schema,
                                  GdaParameterList *params)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	gchar        *query = NULL;
	GdaDataModel *recset = NULL;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
		case GDA_CONNECTION_SCHEMA_DATABASES:
			return gda_freetds_get_databases (cnc, params);
		case GDA_CONNECTION_SCHEMA_FIELDS:
			return gda_freetds_get_fields (cnc, params);
		case GDA_CONNECTION_SCHEMA_TABLES:
			query = g_strdup ("SELECT name "
			                  "FROM sysobjects "
			                    "WHERE (type = 'U') AND "
			                          "(name NOT LIKE 'spt_%') AND "
						  "(name != 'syblicenseslog') "
			                  "ORDER BY name");

			recset =gda_freetds_execute_query (cnc, query);
			g_free (query);
			query = NULL;
			gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
			                                 0, _("Tables"));
			return recset;
		case GDA_CONNECTION_SCHEMA_TYPES:
			recset = gda_freetds_execute_query (cnc, "SELECT name FROM systypes ORDER BY name");
			gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
			                                 0, _("Types"));
			return recset;
	}
	
	return NULL;
}

static GdaDataModel *
gda_freetds_execute_query (GdaConnection *cnc, const gchar* sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaError *error;
	GdaDataModel *recset;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	g_return_val_if_fail (tds_cnc->tds != NULL, NULL);

	tds_cnc->rc = tds_submit_query(tds_cnc->tds, (gchar *) sql);

	if (tds_cnc->rc != TDS_SUCCEED) {
		error = gda_freetds_make_error (tds_cnc->tds, NULL);
		gda_connection_add_error (cnc, error);
		return NULL;
	}
	recset = gda_freetds_recordset_new (cnc, TRUE);
	if (GDA_IS_FREETDS_RECORDSET (recset)) {
		gda_data_model_set_command_text (recset, sql);
		gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
	}

	return recset;
}

static GdaDataModel *
gda_freetds_get_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaFreeTDSRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = gda_freetds_provider_process_sql_commands (NULL, cnc, "SELECT name FROM sysdatabases");
	if (!reclist)
		return NULL;

	recset = GDA_FREETDS_RECORDSET (reclist->data);
	g_list_free (reclist);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
gda_freetds_get_fields (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	GdaParameter *parameter;
	gchar *query;
	const gchar *table;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	parameter = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (parameter != NULL, NULL);

	table = gda_value_get_string ((GdaValue *) gda_parameter_get_value (parameter));
	g_return_val_if_fail (table != NULL, NULL);

	query = g_strdup_printf ("SELECT c.name, "
	                         "       t.name, "
	                         "       c.length, "
	                         "       c.prec, "
	                         "       c.scale, "
	                         "       (CASE WHEN ((c.status & 0x80) = 0x80) THEN 1 ELSE 0 END) AS \"identity\", "
				 "       (CASE WHEN ((c.status & 0x08) = 0x08) THEN 1 ELSE 0 END) AS nullable, "
	                         "       c.domain, "
	                         "       c.printfmt "
	                         "  FROM syscolumns c, systypes t "
	                         "    WHERE (c.id = OBJECT_ID('%s')) "
	                         "      AND (c.usertype = t.usertype) "
	                         "  ORDER BY c.colid ASC", table);
	recset = gda_freetds_execute_query (cnc, query);
	g_free (query);
	query = NULL;

	if (GDA_IS_FREETDS_RECORDSET (recset)) {
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  0,
		                                 _("Field Name"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  1,
		                                 _("Data Type"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  2,
		                                 _("Size"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  3,
		                                 _("Precision"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  4,
		                                 _("Scale"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  5,
		                                 _("Identity"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  6,
		                                 _("Nullable"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  7,
		                                 _("Domain"));
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset),  8,
		                                 _("Printfmt"));
	}

	return recset;
}

static GList* gda_freetds_provider_process_sql_commands(GList         *reclist,
                                                        GdaConnection *cnc,
                                                        const gchar   *sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaError *error;
	gchar **arr;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	g_return_val_if_fail (tds_cnc->tds != NULL, NULL);

//	arr = gda_freetds_split_commandlist(sql);
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;
		while (arr[n]) {
			GdaDataModel *recset;
			tds_cnc->rc = tds_submit_query(tds_cnc->tds, arr[n]);

			if (tds_cnc->rc != TDS_SUCCEED) {
				error = gda_freetds_make_error(tds_cnc->tds,
						               NULL);
				gda_connection_add_error (cnc, error);
			}
			recset = gda_freetds_recordset_new (cnc, TRUE);
			if (GDA_IS_FREETDS_RECORDSET (recset)) {
				gda_data_model_set_command_text (recset, arr[n]);
				gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
				reclist = g_list_append (reclist, recset);
                        }

                        n++;

		}
		g_strfreev(arr);
	}

	return reclist;
}


/* Initialization */
static void
gda_freetds_provider_class_init (GdaFreeTDSProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_freetds_provider_finalize;
	provider_class->open_connection = gda_freetds_provider_open_connection;
	provider_class->close_connection = gda_freetds_provider_close_connection;
	provider_class->get_database = gda_freetds_provider_get_database;
	provider_class->change_database = gda_freetds_provider_change_database;
	provider_class->create_database = gda_freetds_provider_create_database;
	provider_class->drop_database = gda_freetds_provider_drop_database;
	provider_class->execute_command = gda_freetds_provider_execute_command;
	provider_class->begin_transaction = gda_freetds_provider_begin_transaction;
	provider_class->commit_transaction = gda_freetds_provider_commit_transaction;
	provider_class->rollback_transaction = gda_freetds_provider_rollback_transaction;
	provider_class->supports = gda_freetds_provider_supports;
	provider_class->get_schema = gda_freetds_provider_get_schema;

	if (tds_cbs_initialized == FALSE) {
		tds_cbs_initialized = TRUE;

		g_tds_msg_handler = gda_freetds_provider_tds_handle_info_msg;
		g_tds_err_handler = gda_freetds_provider_tds_handle_err_msg;
	}
}

static void
gda_freetds_provider_init (GdaFreeTDSProvider *provider,
                           GdaFreeTDSProviderClass *klass)
{
}

static void
gda_freetds_provider_finalize (GObject *object)
{
	GdaFreeTDSProvider *provider = (GdaFreeTDSProvider *) object;

	g_return_if_fail (GDA_IS_FREETDS_PROVIDER (provider));

	/* chain to parent class */
	parent_class->finalize (object);
}

static int gda_freetds_provider_tds_handle_message (void *aStruct,
                                                    const gboolean is_err_msg)
{
	TDSSOCKET *tds = (TDSSOCKET *) aStruct;
	GdaConnection *cnc = NULL;
	GdaError *error = NULL;
	gchar *msg = NULL;

	g_return_val_if_fail (tds != NULL, TDS_SUCCEED);
	g_return_val_if_fail (tds->msg_info != NULL, TDS_SUCCEED);
	cnc = (GdaConnection *) tds_get_parent (tds);
	
	// what if we cannot determine where the message belongs to?
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), TDS_SUCCEED);

	msg = g_strdup_printf(_("Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n"),
	                      tds->msg_info->msg_number,
	                      tds->msg_info->msg_level,
	                      tds->msg_info->msg_state,
	                      (tds->msg_info->server ? tds->msg_info->server : ""),
	                      tds->msg_info->line_number,
	                      tds->msg_info->message ? tds->msg_info->message : "");

	// if errormessage, proceed
	if (is_err_msg == TRUE) {
		error = gda_error_new ();
		gda_error_set_description (error, msg);
		gda_error_set_number (error, tds->msg_info->msg_number);
		gda_error_set_source (error, "gda-freetds");
		if (tds->msg_info->sql_state != NULL) {
			gda_error_set_sqlstate (error,
			                        tds->msg_info->sql_state);
		} else {
			gda_error_set_sqlstate (error, _("Not available"));
		}

		gda_connection_add_error (cnc, error);
	}
	
	if (msg) {
		g_free(msg);
		msg = NULL;
	}

	return TDS_SUCCEED;
}

static int gda_freetds_provider_tds_handle_info_msg (void *aStruct)
{
	return gda_freetds_provider_tds_handle_message (aStruct, FALSE);
}

static int gda_freetds_provider_tds_handle_err_msg (void *aStruct)
{
	return gda_freetds_provider_tds_handle_message (aStruct, TRUE);
}


/////////////////////////////////////////////////////////////////////////////
// Public functions                                                       
/////////////////////////////////////////////////////////////////////////////

GType
gda_freetds_provider_get_type (void)
{
        static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaFreeTDSProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_freetds_provider_class_init,
			NULL, NULL,
			sizeof (GdaFreeTDSProvider),
			0,
			(GInstanceInitFunc) gda_freetds_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFreeTDSProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_freetds_provider_new (void)
{
	GdaFreeTDSProvider *provider;

	provider = g_object_new (gda_freetds_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}
