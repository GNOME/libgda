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

static GdaDataModel *gda_freetds_get_databases (GdaConnection *cnc,
                                                GdaParameterList *params);

static GList* gda_freetds_provider_process_sql_commands(GList         *reclist,
                                                        GdaConnection *cnc,
                                                        const gchar   *sql);


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
	
	
	tds_cnc = g_new0 (GdaFreeTDSConnectionData, 1);
	g_return_val_if_fail(tds_cnc != NULL, FALSE);

	tds_cnc->login = tds_alloc_login();
	if (! tds_cnc->login) {
		g_free(tds_cnc);
		tds_cnc = NULL;
		return FALSE;
	}
	
	// FreeTDS SIGSEGV on NULL pointers
	if ((t_user == NULL) || (t_host == NULL) || (t_password == NULL)) {
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
		g_free(tds_cnc);
		tds_cnc = NULL;
		error = gda_freetds_make_error("Connection aborted. You must provide at least a host, username and password using DSN 'QUERY=;USER=;PASSWORD='.");
		gda_connection_add_error(cnc, error);

		return FALSE;
	}
				
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
	
	if ((tds_majver != NULL) & (tds_minver != NULL))
		tds_set_version(tds_cnc->login,
		                (short) atoi (tds_majver),
		                (short) atoi (tds_minver)
		               );

	tds_cnc->socket = tds_connect(tds_cnc->login, NULL);
	if (! tds_cnc->socket) {
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
		g_free(tds_cnc);
		tds_cnc = NULL;
		error = gda_freetds_make_error("Establishing connection failed.");
		gda_connection_add_error(cnc, error);
		return FALSE;
	}

	tds_cnc->config = tds_get_config(tds_cnc->socket, tds_cnc->login);
	if (! tds_cnc->config) {
		tds_free_socket(tds_cnc->socket);
		tds_cnc->socket = NULL;
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
		g_free(tds_cnc);
		tds_cnc = NULL;
		error = gda_freetds_make_error("Failed getting connection info.");
		gda_connection_add_error(cnc, error);
		return FALSE;
	}

	tds_cnc->rc = TDS_SUCCEED;
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE, tds_cnc);

	return TRUE;
}

static gboolean
gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                       GdaConnection *cnc)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaFreeTDSProvider *tds_provider = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail(GDA_IS_FREETDS_PROVIDER (tds_provider), FALSE);
	g_return_val_if_fail(GDA_IS_CONNECTION (cnc), FALSE);

	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	if (! tds_cnc)
		return FALSE;

	if (tds_cnc->socket)
		tds_free_socket(tds_cnc->socket);
	tds_cnc->socket = NULL;
	if (tds_cnc->login)
		tds_free_login(tds_cnc->login);
	tds_cnc->login = NULL;
	if (tds_cnc->config)
		tds_free_config(tds_cnc->config);
	tds_cnc->config = NULL;

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
	tds_cnc->config = tds_get_config(tds_cnc->socket, tds_cnc->login);
	
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
		default:
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
		default:
	}
	
	return FALSE;
}

static GdaDataModel
*gda_freetds_provider_get_schema (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                  GdaConnectionSchema schema,
                                  GdaParameterList *params)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
		case GDA_CONNECTION_SCHEMA_DATABASES:
			return gda_freetds_get_databases (cnc, params);	
		default:
	}
	
	return NULL;
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


static GList* gda_freetds_provider_process_sql_commands(GList         *reclist,
                                                        GdaConnection *cnc,
                                                        const gchar   *sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	gchar **arr;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);

//	arr = gda_freetds_split_commandlist(sql);
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;
		while (arr[n]) {
			GdaFreeTDSRecordset *recset;
			tds_cnc->rc = tds_submit_query(tds_cnc->socket, arr[n]);

			// if (tds_cnc->rc == TDS_SUCEED) {
			recset = gda_freetds_recordset_new(cnc, TRUE);
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
