/* GDA FreeTDS Provider
 * Copyright (C) 2002 - 2006 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#endif

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-parameter-list.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include "gda-freetds.h"
#include "gda-freetds-defs.h"

#include <libgda/sql-delimiter/gda-sql-delimiter.h>

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_FREETDS_HANDLE "GDA_FreeTDS_FreeTDSHandle"

/*
 * Private declarations and functions
 */

static GObjectClass *parent_class = NULL;

static void gda_freetds_provider_class_init (GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_init       (GdaFreeTDSProvider      *provider,
                                             GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_finalize   (GObject *object);

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
static GList *gda_freetds_provider_execute_command (GdaServerProvider *provider,
                                                    GdaConnection *cnc,
                                                    GdaCommand *cmd,
                                                    GdaParameterList *params);
static gboolean gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        const gchar *name, GdaTransactionIsolation *level,
							GError **error);
static gboolean gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                                         GdaConnection *cnc,
                                                         const gchar *name, GError **error);
static gboolean gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                                           GdaConnection *cnc,
                                                           const gchar *name, GError **error);
static const gchar *gda_freetds_provider_get_server_version (GdaServerProvider *provider,
                                                             GdaConnection *cnc);
static const gchar *gda_freetds_provider_get_version (GdaServerProvider *provider);
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

static gchar *gda_freetds_get_stringresult_of_query (GdaConnection *cnc, 
                                                     const gchar *sql,
                                                     const gint col,
                                                     const gint row);

#ifdef HAVE_FREETDS_VER0_5X
  static gboolean tds_cbs_initialized = FALSE;
  extern int (*g_tds_msg_handler)();
  extern int (*g_tds_err_handler)();
#endif

static void gda_freetds_free_connection_data (GdaFreeTDSConnectionData *tds_cnc);

static gboolean gda_freetds_execute_cmd (GdaConnection *cnc, const gchar *sql);

static int gda_freetds_provider_tds_handle_message (void *aStruct,
						    void *bStruct,
                                                    const gboolean is_err_msg);
#if defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_60)
  static int gda_freetds_provider_tds_handle_info_msg (TDSCONTEXT *,
                                                       TDSSOCKET *,
                                                       _TDSMSGINFO *);
  static int gda_freetds_provider_tds_handle_err_msg (TDSCONTEXT *,
                                                      TDSSOCKET *,
                                                      _TDSMSGINFO *);
#else
  static int gda_freetds_provider_tds_handle_info_msg (void *aStruct);
  static int gda_freetds_provider_tds_handle_err_msg (void *aStruct);
#endif

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

	GdaConnectionEvent *error = NULL;
	GdaFreeTDSProvider *tds_provider = (GdaFreeTDSProvider *) provider;
	GdaFreeTDSConnectionData *tds_cnc = NULL;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	t_database = gda_quark_list_find (params, "DATABASE");
	t_host = gda_quark_list_find (params, "HOST");
	t_hostaddr = gda_quark_list_find (params, "HOSTADDR");
	t_options = gda_quark_list_find (params, "OPTIONS");
	t_user = gda_quark_list_find (params, "USER");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_port = gda_quark_list_find (params, "PORT");
	
	/* These shall override environment variables or previous settings */
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
	
	/* sanity check */
	/* FreeTDS SIGSEGV on NULL pointers */
	if ((t_user == NULL) || (t_host == NULL) || (t_password == NULL)) {
		error = gda_freetds_make_error(NULL, _("Connection aborted. You must provide at least a host, username and password using DSN 'USER=;USER=;PASSWORD='."));
		gda_connection_add_event(cnc, error);

		return FALSE;
	}
	
	tds_cnc = g_new0 (GdaFreeTDSConnectionData, 1);
	g_return_val_if_fail(tds_cnc != NULL, FALSE);
	tds_cnc->msg_arr = g_ptr_array_new ();

	if (tds_cnc->msg_arr == NULL) {
		gda_freetds_free_connection_data (tds_cnc);
		tds_cnc = NULL;
		gda_connection_add_event_string (cnc, _("Error creating message container."));
		return FALSE;
	}

	tds_cnc->err_arr = g_ptr_array_new ();
	if (tds_cnc->err_arr == NULL) {
		gda_freetds_free_connection_data (tds_cnc);
		gda_connection_add_event_string (cnc, _("Error creating error container."));
		return FALSE;
	}

	/* allocate login */
	tds_cnc->login = tds_alloc_login();
	if (! tds_cnc->login) {
		gda_freetds_free_connection_data (tds_cnc);
		return FALSE;
	}
	
	/* set tds version */
	if ((tds_majver != NULL) & (tds_minver != NULL))
		tds_set_version(tds_cnc->login,
		                (short) atoi (tds_majver),
		                (short) atoi (tds_minver)
		               );

	/* apply connection settings */
	tds_set_user(tds_cnc->login, (char *) t_user);
	tds_set_passwd(tds_cnc->login, (char *) t_password);
	tds_set_app(tds_cnc->login, "libgda");
	
	if (t_hostaddr)
		tds_set_host(tds_cnc->login, (char *) t_hostaddr);
	
	tds_set_library(tds_cnc->login, "TDS-Library");
	if (t_host)
		tds_set_server(tds_cnc->login, (char *) t_host);

	if (t_port)
		tds_set_port(tds_cnc->login, atoi(t_port));
	
	tds_set_client_charset(tds_cnc->login, "iso_1");
	tds_set_language(tds_cnc->login, "us_english");
	tds_set_packet(tds_cnc->login, 512);

	/* Version 0.60 api uses context additionaly */
#if defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_60)
	tds_cnc->ctx = tds_alloc_context();
	if (! tds_cnc->ctx) {
		gda_log_error (_("Allocating tds context failed."));
		gda_freetds_free_connection_data (tds_cnc);
		error = gda_freetds_make_error(NULL, _("Allocating tds context failed."));
		gda_connection_add_event(cnc, error);
		return FALSE;
	}
	/* Initialize callbacks which are now in context struct */
	tds_cnc->ctx->msg_handler = gda_freetds_provider_tds_handle_info_msg;
	tds_cnc->ctx->err_handler = gda_freetds_provider_tds_handle_err_msg;
#endif

	/* establish connection; change in 0.6x api */
#if defined(HAVE_FREETDS_VER0_60)
	tds_cnc->tds = tds_connect(tds_cnc->login, tds_cnc->ctx, NULL);
#elif defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
	tds_cnc->tds = tds_alloc_socket(tds_cnc->ctx, 512);
	if (! tds_cnc->tds) {
		gda_log_error (_("Allocating tds socket failed."));
		gda_freetds_free_connection_data (tds_cnc);
		gda_connection_add_event_string (cnc, _("Allocating tds socket failed."));
		return FALSE;
	}
	tds_set_parent (tds_cnc->tds, NULL);
	tds_cnc->config = tds_read_config_info (NULL, tds_cnc->login, tds_cnc->ctx->locale);
	if (tds_connect (tds_cnc->tds, tds_cnc->config) != TDS_SUCCEED) {
		gda_log_error (_("Establishing connection failed."));
		//gda_freetds_free_connection_data (tds_cnc);
		gda_connection_add_event_string (cnc, _("Establishing connection failed."));
		return FALSE;
	}
#else
	tds_cnc->tds = tds_connect(tds_cnc->login, NULL);
#endif
	if (! tds_cnc->tds) {
		gda_log_error (_("Establishing connection failed."));
		gda_freetds_free_connection_data (tds_cnc);
		error = gda_freetds_make_error(NULL, _("Establishing connection failed."));
		gda_connection_add_event(cnc, error);
		return FALSE;
	}

	/* try to receive connection info for sanity check */
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
	/* do nothing */
#elif defined(HAVE_FREETDS_VER0_60)
	tds_cnc->config = tds_get_config(tds_cnc->tds, tds_cnc->login, tds_cnc->ctx->locale);
#else
	tds_cnc->config = tds_get_config(tds_cnc->tds, tds_cnc->login);
#endif
	if (! tds_cnc->config) {
		gda_log_error (_("Failed getting connection info."));	
		error = gda_freetds_make_error (tds_cnc->tds, _("Failed getting connection info."));
		gda_connection_add_event (cnc, error);

		gda_freetds_free_connection_data (tds_cnc);
		return FALSE;
	}

	/* Pass cnc pointer to tds (user data ptr) for reuse in callbacks */
	tds_set_parent (tds_cnc->tds, (void *) cnc);
	/* set cnc object handle; */
	/* thus make calls to e.g. gda_freetds_execute_cmd possible */
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE, tds_cnc);
	
	if ((t_database != NULL)
	    && (gda_freetds_provider_change_database(provider,
	                                             cnc, t_database
	                                            ) != TRUE)) {
		g_object_set_data (G_OBJECT (cnc),
		                   OBJECT_DATA_FREETDS_HANDLE,
				   NULL);
		gda_freetds_free_connection_data (tds_cnc);
		return FALSE;
	}

	/* fixme: these have been removed in debian sid
	 * 
	 * tds_cnc->is_sybase = TDS_IS_SYBASE (tds_cnc->tds);
	 * tds_cnc->srv_ver = tds_cnc->tds->product_version;
	 */
	
	tds_cnc->rc = TDS_SUCCEED;

	return TRUE;
}

static void
gda_freetds_free_connection_data (GdaFreeTDSConnectionData *tds_cnc)
{
	GdaFreeTDSMessage *msg = NULL;

	g_return_if_fail (tds_cnc != NULL);

	if (tds_cnc->server_id) {
		g_free (tds_cnc->server_id);
		tds_cnc->server_id = NULL;
	}
	if (tds_cnc->database) {
		g_free (tds_cnc->database);
		tds_cnc->database = NULL;
	}
	if (tds_cnc->config) {
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
		tds_free_connection (tds_cnc->config);
#else
		tds_free_config(tds_cnc->config);
#endif
		tds_cnc->config = NULL;
	}
	if (tds_cnc->tds) {
		/* clear tds user data pointer */
		tds_set_parent (tds_cnc->tds, NULL);
		tds_free_socket (tds_cnc->tds);
		tds_cnc->tds = NULL;
	}
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_60)
	if (tds_cnc->ctx) {
		/* Clear callback handler */
		tds_cnc->ctx->msg_handler = NULL;
		tds_cnc->ctx->err_handler = NULL;
		tds_free_context (tds_cnc->ctx);
		tds_cnc->ctx = NULL;
	}
#endif
	if (tds_cnc->login) {
		tds_free_login(tds_cnc->login);
		tds_cnc->login = NULL;
	}

	if (tds_cnc->err_arr) {
		while (tds_cnc->err_arr->len > 0) {
			msg = (GdaFreeTDSMessage *) g_ptr_array_index (tds_cnc->err_arr, 0);
			if (msg != NULL) {
				gda_freetds_message_free (msg);
			}

			g_ptr_array_remove_index (tds_cnc->err_arr, 0);
		}
			
		g_ptr_array_free (tds_cnc->err_arr, TRUE);
		tds_cnc->err_arr = NULL;
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
	
	tds_cnc->is_sybase = FALSE;
	tds_cnc->srv_ver = 0;
		
	g_free(tds_cnc);
	tds_cnc = NULL;
}

static gboolean
gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                       GdaConnection *cnc)
{
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaFreeTDSProvider *tds_provider = (GdaFreeTDSProvider *) provider;
	
	g_return_val_if_fail(GDA_IS_FREETDS_PROVIDER (tds_provider), FALSE);
	g_return_val_if_fail(GDA_IS_CONNECTION (cnc), FALSE);

	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	if (! tds_cnc)
		return FALSE;

	gda_freetds_free_connection_data (tds_cnc);

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
	g_return_val_if_fail (tds_cnc->tds != NULL, NULL);

	if (tds_cnc->database) {
		g_free (tds_cnc->database);
	}
	tds_cnc->database = gda_freetds_get_stringresult_of_query (cnc,
	                       TDS_QUERY_CURRENT_DATABASE, 0, 0);

	return (const gchar*) tds_cnc->database;
}

static gboolean
gda_freetds_provider_change_database (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      const gchar *name)
{
	gchar *sql_cmd = NULL;
	gboolean ret = FALSE;
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	sql_cmd = g_strdup_printf("USE %s", name);
	ret = gda_freetds_execute_cmd(cnc, sql_cmd);
	g_free(sql_cmd);
	
	return ret;
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
			if (reclist && GDA_IS_DATA_MODEL (reclist->data)) 
				g_object_set (G_OBJECT (reclist->data), 
					      "command_text", gda_command_get_text (cmd),
					      "command_type", GDA_COMMAND_TYPE_TABLE, NULL);

			g_free(query);
			query = NULL;
			break;
		case GDA_COMMAND_TYPE_XML:
		case GDA_COMMAND_TYPE_PROCEDURE:
		case GDA_COMMAND_TYPE_SCHEMA:
		case GDA_COMMAND_TYPE_INVALID:
			return reclist;
			break;
	}

	return reclist;
}

static gboolean
gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        const gchar *name, GdaTransactionIsolation *level,
					GError **error)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	return FALSE;
}

static gboolean
gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                         GdaConnection *cnc,
                                         const gchar *name, GError **error)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	return FALSE;
}

static gboolean
gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                           GdaConnection *cnc,
                                           const gchar *name, GError **error)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

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
	case GDA_CONNECTION_FEATURE_PROCEDURES:
	case GDA_CONNECTION_FEATURE_SQL:
	case GDA_CONNECTION_FEATURE_USERS:
	case GDA_CONNECTION_FEATURE_VIEWS:
		return TRUE;
		
	case GDA_CONNECTION_FEATURE_AGGREGATES:
	case GDA_CONNECTION_FEATURE_INDEXES:
	case GDA_CONNECTION_FEATURE_INHERITANCE:
	case GDA_CONNECTION_FEATURE_SEQUENCES:
	case GDA_CONNECTION_FEATURE_TRANSACTIONS:
	case GDA_CONNECTION_FEATURE_TRIGGERS:
	case GDA_CONNECTION_FEATURE_XML_QUERIES:
	default:
			return FALSE;
	}
}

static const gchar
*gda_freetds_provider_get_server_version (GdaServerProvider *provider,
                                          GdaConnection *cnc)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaDataModel *model = NULL;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);

	if (!tds_cnc->server_id) {
		model = gda_freetds_execute_query (cnc, TDS_QUERY_SERVER_VERSION);
		if (model) {
			if ((gda_data_model_get_n_columns (model) == 1)
			    && (gda_data_model_get_n_rows (model) == 1)) {
				GValue *value;

				value = (GValue *) gda_data_model_get_value_at
				                             (model, 0, 0);
				tds_cnc->server_id = gda_value_stringify ((GValue *) value);
			}
			g_object_unref (model);
		}
	}

	return (const gchar *) tds_cnc->server_id;
}

static const gchar
*gda_freetds_provider_get_version (GdaServerProvider *provider)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;

	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);

	return PACKAGE_VERSION;
}

static GdaDataModel *
gda_freetds_provider_get_types (GdaConnection    *cnc,
				GdaParameterList *params)
{
	GdaDataModel *model = NULL;
	_TDSCOLINFO col;
	GType g_type;
	GValue     *value = NULL;
	gint i = 1;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	model = gda_freetds_execute_query (cnc, TDS_SCHEMA_TYPES);
	TDS_FIXMODEL_SCHEMA_TYPES (model);

	memset (&col, 0, sizeof (col));
	
	if (model) {
		for (i = 0; i < gda_data_model_get_n_rows (model); i++) {
			GdaRow *row = (GdaRow *) gda_data_model_row_get_row (GDA_DATA_MODEL_ROW (model), i, NULL);
			
			/* first fix g_type */
			if (row) {
				value = gda_row_get_value (row, 2);
				if (G_VALUE_TYPE (value) == G_TYPE_INT)
					col.column_size = g_value_get_int (value);
				else
					col.column_size = 0;
				
				value = gda_row_get_value (row, 3);
				if (G_VALUE_TYPE (value) != G_TYPE_CHAR)
					col.column_type = SYBVARIANT;
				else 
					col.column_type = g_value_get_char (value);
				
				g_type = gda_freetds_get_value_type (&col);
				
				/* col 3: type */
				g_value_set_ulong (value, g_type);
				
				/* col 2: comment */
				value = gda_row_get_value (row, 2);
				g_value_set_string (value, "");
			}
		}
	}

	return model;
}

static GdaDataModel *
gda_freetds_provider_get_schema (GdaServerProvider *provider,
				  GdaConnection *cnc,
                                  GdaConnectionSchema schema,
                                  GdaParameterList *params)
{
	GdaFreeTDSProvider *tds_prov = (GdaFreeTDSProvider *) provider;
	GdaDataModel *recset = NULL;
	
	g_return_val_if_fail (GDA_IS_FREETDS_PROVIDER (tds_prov), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES:
		return gda_freetds_get_databases (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_FIELDS:
		return gda_freetds_get_fields (cnc, params);
		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		recset = gda_freetds_execute_query (cnc, TDS_SCHEMA_PROCEDURES);
		TDS_FIXMODEL_SCHEMA_PROCEDURES (recset)
			
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_TABLES:
		recset = gda_freetds_execute_query (cnc, TDS_SCHEMA_TABLES);
		TDS_FIXMODEL_SCHEMA_TABLES (recset)
			
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_TYPES:
		recset = gda_freetds_provider_get_types (cnc, params);

		return recset;
		break;
	case GDA_CONNECTION_SCHEMA_USERS:
		recset = gda_freetds_execute_query (cnc, TDS_SCHEMA_USERS);
		TDS_FIXMODEL_SCHEMA_USERS (recset)
				
			return recset;
		break;
	case GDA_CONNECTION_SCHEMA_VIEWS:
		recset = gda_freetds_execute_query (cnc, TDS_SCHEMA_VIEWS);
		TDS_FIXMODEL_SCHEMA_VIEWS (recset)

			return recset;
		break;

	case GDA_CONNECTION_SCHEMA_AGGREGATES:
	case GDA_CONNECTION_SCHEMA_INDEXES:
	case GDA_CONNECTION_SCHEMA_PARENT_TABLES:
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
	case GDA_CONNECTION_SCHEMA_TRIGGERS:
	default:
		return NULL;
		break;
	}
	
	return NULL;
}

static gboolean
gda_freetds_execute_cmd (GdaConnection *cnc, const gchar *sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaConnectionEvent *error;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (sql != NULL, FALSE);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);

	g_return_val_if_fail (tds_cnc != NULL, FALSE);
	g_return_val_if_fail (tds_cnc->tds != NULL, FALSE);

	tds_cnc->rc = tds_submit_query (tds_cnc->tds, (char *) sql);
	if (tds_cnc->rc != TDS_SUCCEED) {
		gda_log_error (_("Query did not succeed in execute_cmd()."));
		error = gda_freetds_make_error (tds_cnc->tds, _("Query did not succeed in execute_cmd()."));
		gda_connection_add_event (cnc, error);
		return FALSE;
	}

	/* there should not be any result tokens */
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
	while ((tds_cnc->rc = tds_process_result_tokens (tds_cnc->tds, &tds_cnc->result_type, NULL))
#else
	while ((tds_cnc->rc = tds_process_result_tokens (tds_cnc->tds)) 
#endif
	       == TDS_SUCCEED) {
		if (tds_cnc->tds->res_info->rows_exist) {
			gda_log_error (_("Unexpected result tokens in execute_cmd()."));
			error = gda_freetds_make_error (tds_cnc->tds,
			                                _("Unexpected result tokens in execute_cmd()."));
			gda_connection_add_event (cnc, error);
			return FALSE;
		}
	}
	
	if ((tds_cnc->rc != TDS_FAIL) && (tds_cnc->rc != TDS_NO_MORE_RESULTS)) {
		error = gda_freetds_make_error (tds_cnc->tds,
		                                _("Unexpected return in execute_cmd()."));
		gda_log_error (_("Unexpected return in execute_cmd()."));
		gda_connection_add_event (cnc, error);
		return FALSE;
	}

	return TRUE;
}

static GdaDataModel *
gda_freetds_execute_query (GdaConnection *cnc, const gchar* sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaConnectionEvent *error;
	GdaDataModel *recset;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	g_return_val_if_fail (tds_cnc->tds != NULL, NULL);

	tds_cnc->rc = tds_submit_query(tds_cnc->tds, (gchar *) sql);

	if (tds_cnc->rc != TDS_SUCCEED) {
		error = gda_freetds_make_error (tds_cnc->tds, NULL);
		gda_connection_add_event (cnc, error);
		return NULL;
	}
	recset = gda_freetds_recordset_new (cnc, TRUE);
	if (GDA_IS_FREETDS_RECORDSET (recset)) 
		g_object_set (G_OBJECT (recset), 
			      "command_text", sql,
			      "command_type", GDA_COMMAND_TYPE_SQL, NULL);

	return recset;
}

/* make sure to g_free() result after use */
static gchar *
gda_freetds_get_stringresult_of_query (GdaConnection *cnc, 
                                       const gchar *sql,
                                       const gint col,
                                       const gint row)
{
	GdaDataModel    *model = NULL;
	GValue        *value = NULL;
	gchar           *ret = NULL;

	/* GDA_IS_CONNECTION (cnc) validation in function call */
	model = gda_freetds_execute_query (cnc, sql);
	
	if (model) {
		value = (GValue *) gda_data_model_get_value_at (model,
		                                                  col, row);

		if ((value != NULL)
		    && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)
		    && (G_VALUE_TYPE (value) != G_TYPE_INVALID)) {
			ret = (gchar *) gda_value_stringify (value);
		}
		
		g_object_unref (model);
	}

	return ret;
}

static GdaDataModel *
gda_freetds_get_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaFreeTDSRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = gda_freetds_provider_process_sql_commands (NULL, cnc, TDS_SCHEMA_DATABASES);
	if (!reclist)
		return NULL;

	recset = GDA_FREETDS_RECORDSET (reclist->data);
	g_list_free (reclist);
	TDS_FIXMODEL_SCHEMA_DATABASES(recset)

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

	parameter = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (parameter != NULL, NULL);

	table = g_value_get_string ((GValue *) gda_parameter_get_value (parameter));
	g_return_val_if_fail (table != NULL, NULL);

	query = g_strdup_printf (TDS_SCHEMA_FIELDS, table);
	recset = gda_freetds_execute_query (cnc, query);
	g_free (query);
	query = NULL;

	if (GDA_IS_FREETDS_RECORDSET (recset)) {
		TDS_FIXMODEL_SCHEMA_FIELDS (recset)
	}

	return recset;
}

static GList* gda_freetds_provider_process_sql_commands(GList         *reclist,
                                                        GdaConnection *cnc,
                                                        const gchar   *sql)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaConnectionEvent *error;
	gchar **arr;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	g_return_val_if_fail (tds_cnc->tds != NULL, NULL);

/*	arr = gda_freetds_split_commandlist(sql); */
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		gint n = 0;
		while (arr[n]) {
			GdaDataModel *recset;
			tds_cnc->rc = tds_submit_query(tds_cnc->tds, arr[n]);

			if (tds_cnc->rc != TDS_SUCCEED) {
				error = gda_freetds_make_error(tds_cnc->tds,
						               NULL);
				gda_connection_add_event (cnc, error);
			}
			recset = gda_freetds_recordset_new (cnc, TRUE);
			if (GDA_IS_FREETDS_RECORDSET (recset))
				g_object_set (G_OBJECT (recset), 
					      "command_text", arr[n],
					      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
				reclist = g_list_append (reclist, recset);

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

	provider_class->get_version = gda_freetds_provider_get_version;
	provider_class->get_server_version = gda_freetds_provider_get_server_version;
	provider_class->get_info = NULL;
	provider_class->supports_feature = gda_freetds_provider_supports;
	provider_class->get_schema = gda_freetds_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_freetds_provider_open_connection;
	provider_class->close_connection = gda_freetds_provider_close_connection;
	provider_class->get_database = gda_freetds_provider_get_database;
	provider_class->change_database = gda_freetds_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_freetds_provider_execute_command;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_freetds_provider_begin_transaction;
	provider_class->commit_transaction = gda_freetds_provider_commit_transaction;
	provider_class->rollback_transaction = gda_freetds_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
	
	provider_class->create_blob = NULL;
	provider_class->fetch_blob = NULL;	
	

#ifdef HAVE_FREETDS_VER0_5X
	if (tds_cbs_initialized == FALSE) {
		tds_cbs_initialized = TRUE;

		g_tds_msg_handler = gda_freetds_provider_tds_handle_info_msg;
		g_tds_err_handler = gda_freetds_provider_tds_handle_err_msg;
	}
#endif
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

#ifdef HAVE_FREETDS_VER0_5X
	tds_cbs_initialized = FALSE;
	g_tds_msg_handler = NULL;
	g_tds_err_handler = NULL;
#endif

	/* chain to parent class */
	parent_class->finalize (object);
}

/* FIXME: rewrite this function to suit new callback parameters
 *        (0.6x code)
 */
static int gda_freetds_provider_tds_handle_message (void *aStruct,
						    void *bStruct,
                                                    const gboolean is_err_msg)
{
	TDSSOCKET *tds = (TDSSOCKET *) aStruct;
	_TDSMSGINFO *msg_info = (_TDSMSGINFO *) bStruct;
	GdaConnection *cnc = NULL;
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaConnectionEvent *error = NULL;
	gchar *msg = NULL;

	g_return_val_if_fail (tds != NULL, TDS_SUCCEED);
	g_return_val_if_fail (msg_info != NULL, TDS_SUCCEED);
	cnc = (GdaConnection *) tds_get_parent (tds);
	
	/* what if we cannot determine where the message belongs to? */
	g_return_val_if_fail (((GDA_IS_CONNECTION (cnc)) || (cnc == NULL)),
	                      TDS_SUCCEED);

	msg = g_strdup_printf(_("Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n"),
	                      msg_info->msg_number,
	                      msg_info->msg_level,
	                      msg_info->msg_state,
	                      (msg_info->server ? msg_info->server : ""),
	                      msg_info->line_number,
	                      msg_info->message ? msg_info->message : "");

	/* if errormessage, proceed */
	if (is_err_msg == TRUE) {
		if (cnc != NULL) {
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (error, msg);
			gda_connection_event_set_code (error, msg_info->msg_number);
			gda_connection_event_set_source (error, "gda-freetds");
			if (msg_info->sql_state != NULL) {
				gda_connection_event_set_sqlstate (error,
				                        msg_info->sql_state);
			} else {
				gda_connection_event_set_sqlstate (error, _("Not available"));
			}

			gda_connection_add_event (cnc, error);
		} else {
			gda_log_error (msg);
		}
	} else {
		gda_log_message (msg);
	}
	
	if (msg) {
		g_free(msg);
		msg = NULL;
	}

	return TDS_SUCCEED;
}

#if defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_60)
/* FIXME: rewrite tds_handle_message as well/use new parameters here */
static int
gda_freetds_provider_tds_handle_info_msg (TDSCONTEXT *ctx, TDSSOCKET *tds,
                                          _TDSMSGINFO *msg)
{
	return gda_freetds_provider_tds_handle_message ((void *) tds,
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
							(void *) msg,
#else
							(void *) tds->msg_info,
#endif
							FALSE);
}
#else
static int
gda_freetds_provider_tds_handle_info_msg (void *aStruct)
{
	return gda_freetds_provider_tds_handle_message (aStruct, FALSE);
}
#endif

#if defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_60)
/* FIXME: rewrite tds_handle_message as well/use new parameters here */
static int
gda_freetds_provider_tds_handle_err_msg (TDSCONTEXT *ctx, TDSSOCKET *tds,
                                         _TDSMSGINFO *msg)
{
	return gda_freetds_provider_tds_handle_message ((void *) tds,
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63)
							(void *) msg,
#else
							(void *) tds->msg_info,
#endif
							TRUE);
}
#else
static int
gda_freetds_provider_tds_handle_err_msg (void *aStruct)
{
	return gda_freetds_provider_tds_handle_message (aStruct, TRUE);
}
#endif

/*
 * Public functions                                                       
 */

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

/*
	if (! gda_log_is_enabled())
		gda_log_enable();
*/
	
	return GDA_SERVER_PROVIDER (provider);
}
