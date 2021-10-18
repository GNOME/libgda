/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2019 Daniel Espinosa <malerba@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-impl.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-web.h"
#include "gda-web-provider.h"
#include "gda-web-recordset.h"
#include "gda-web-ddl.h"
#include "gda-web-meta.h"
#include "gda-web-util.h"
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_web_provider_class_init (GdaWebProviderClass *klass);
static void gda_web_provider_init       (GdaWebProvider *provider);

G_DEFINE_TYPE(GdaWebProvider, gda_web_provider, GDA_TYPE_SERVER_PROVIDER)

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_web_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							     GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_web_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_web_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_web_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_web_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaServerOperationType type,
							      GdaSet *options, GError **error);
static gchar              *gda_web_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaServerOperation *op, GError **error);

static gboolean            gda_web_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_web_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_web_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GError **error);
static gboolean            gda_web_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								  const gchar *name, GError **error);
static gboolean            gda_web_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							   const gchar *name, GError **error);
static gboolean            gda_web_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GError **error);
static gboolean            gda_web_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							      const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_web_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_web_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaConnectionFeature feature);

static GdaWorker          *gda_web_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);

static const gchar        *gda_web_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_web_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
							      GType g_type, const gchar *dbms_type);

static const gchar*        gda_web_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								   GType type);
/* statements */
static GdaSqlParser        *gda_web_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_web_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								GdaStatement *stmt, GdaSet *params, 
								GdaStatementSqlFlag flags,
								GSList **params_used, GError **error);
static gboolean             gda_web_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								GdaStatement *stmt, GError **error);
static GObject             *gda_web_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								GdaStatement *stmt, GdaSet *params,
								GdaStatementModelUsage model_usage, 
								GType *col_types, GdaSet **last_inserted_row, GError **error);
static GdaSqlStatement     *gda_web_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
								GdaStatement *stmt, GdaSet *params, GError **error);

/* Quoting */
static gchar               *gda_web_provider_identifier_quote  (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *id,
								gboolean meta_store_convention, gboolean force_quotes);


/*
 * GdaWebProvider class implementation
 */
GdaServerProviderBase web_base_functions = {
	gda_web_provider_get_name,
	gda_web_provider_get_version,
	gda_web_provider_get_server_version,
	gda_web_provider_supports_feature,
	gda_web_provider_create_worker,
	NULL,
	gda_web_provider_create_parser,
	gda_web_provider_get_data_handler,
	gda_web_provider_get_default_dbms_type,
	gda_web_provider_supports_operation,
	gda_web_provider_create_operation,
	gda_web_provider_render_operation,
	gda_web_provider_statement_to_sql,
	gda_web_provider_identifier_quote,
	gda_web_provider_statement_rewrite,
	gda_web_provider_open_connection,
	NULL,
	gda_web_provider_close_connection,
	NULL,
	NULL,
	gda_web_provider_perform_operation,
	gda_web_provider_begin_transaction,
	gda_web_provider_commit_transaction,
	gda_web_provider_rollback_transaction,
	gda_web_provider_add_savepoint,
	gda_web_provider_rollback_savepoint,
	gda_web_provider_delete_savepoint,
	gda_web_provider_statement_prepare,
	gda_web_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta web_meta_functions = {
	_gda_web_meta__info,
	_gda_web_meta__btypes,
	_gda_web_meta__udt,
	_gda_web_meta_udt,
	_gda_web_meta__udt_cols,
	_gda_web_meta_udt_cols,
	_gda_web_meta__enums,
	_gda_web_meta_enums,
	_gda_web_meta__domains,
	_gda_web_meta_domains,
	_gda_web_meta__constraints_dom,
	_gda_web_meta_constraints_dom,
	_gda_web_meta__el_types,
	_gda_web_meta_el_types,
	_gda_web_meta__collations,
	_gda_web_meta_collations,
	_gda_web_meta__character_sets,
	_gda_web_meta_character_sets,
	_gda_web_meta__schemata,
	_gda_web_meta_schemata,
	_gda_web_meta__tables_views,
	_gda_web_meta_tables_views,
	_gda_web_meta__columns,
	_gda_web_meta_columns,
	_gda_web_meta__view_cols,
	_gda_web_meta_view_cols,
	_gda_web_meta__constraints_tab,
	_gda_web_meta_constraints_tab,
	_gda_web_meta__constraints_ref,
	_gda_web_meta_constraints_ref,
	_gda_web_meta__key_columns,
	_gda_web_meta_key_columns,
	_gda_web_meta__check_columns,
	_gda_web_meta_check_columns,
	_gda_web_meta__triggers,
	_gda_web_meta_triggers,
	_gda_web_meta__routines,
	_gda_web_meta_routines,
	_gda_web_meta__routine_col,
	_gda_web_meta_routine_col,
	_gda_web_meta__routine_par,
	_gda_web_meta_routine_par,
	_gda_web_meta__indexes_tab,
        _gda_web_meta_indexes_tab,
        _gda_web_meta__index_cols,
        _gda_web_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_web_provider_class_init (GdaWebProviderClass *klass)
{
	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &web_base_functions);

	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &web_meta_functions);

	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) NULL);
}

static void
gda_web_provider_init (G_GNUC_UNUSED GdaWebProvider *web_prv)
{
}

static GdaWorker *
gda_web_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* Let's assume for now that this provider is not thread safe... */
	static GdaWorker *unique_worker = NULL;
	return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_web_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return WEB_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_web_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

static gboolean
do_server_setup (GdaConnection *cnc, WebConnectionData *cdata)
{
	SoupMessage *msg;
	guint status;
	gchar *real_url;

	real_url = g_strdup_printf ("%s/gda-setup.php", cdata->server_base_url);
	msg = soup_message_new ("GET", real_url);
	if (!msg) {
		gda_connection_add_event_string (cnc, _("Invalid HOST/SCRIPT '%s'"), real_url);
		g_free (real_url);
		return FALSE;
	}
	g_free (real_url);

	g_object_set (G_OBJECT (cdata->front_session), SOUP_SESSION_TIMEOUT, 5, NULL);
	status = soup_session_send_message (cdata->front_session, msg);
	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		gda_connection_add_event_string (cnc, msg->reason_phrase);
		g_object_unref (msg);
		return FALSE;
	}

	xmlDocPtr doc;
	gchar out_status_chr;
	doc = _gda_web_decode_response (cnc, cdata, msg->response_body, &out_status_chr, NULL);
	g_object_unref (msg);
	if (doc) {
		if (out_status_chr != 'O') {
			_gda_web_set_connection_error_from_xmldoc (cnc, doc, NULL);
			xmlFreeDoc (doc);
			return FALSE;
		}
		xmlFreeDoc (doc);
		return TRUE;
	}
	return FALSE;
}

/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a WebConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_web_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				  GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_WEB_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Check for connection parameters */
	const gchar *db_name, *host, *path, *port, *serversecret, *pass = NULL, *use_ssl;

	if (auth)
		pass = gda_quark_list_find (auth, "PASSWORD");
	if (!pass) {
		gda_connection_add_event_string (cnc, _("The connection string must contain the %s value"), "PASSWORD");
                return FALSE;
	}
	host = gda_quark_list_find (params, "HOST");
	if (!host) {
		gda_connection_add_event_string (cnc,
						 _("The connection string must contain the %s value"), "HOST");
		return FALSE;
	}
	serversecret = gda_quark_list_find (params, "SECRET");
	if (!serversecret) {
		gda_connection_add_event_string (cnc,
						 _("The connection string must contain the %s value"), "SECRET");
		return FALSE;
	}
	path = gda_quark_list_find (params, "PATH");
	port = gda_quark_list_find (params, "PORT");
	db_name = gda_quark_list_find (params, "DB_NAME");
	if (!db_name) {
		gda_connection_add_event_string (cnc,
						 _("The connection string must contain the %s value"), "DB_NAME");
		return FALSE;
	}
	use_ssl = gda_quark_list_find (params, "USE_SSL");
	if (use_ssl && (*use_ssl != 'T') && (*use_ssl != 't'))
		use_ssl = NULL;
	
	/* open Libsoup session */
	WebConnectionData *cdata;
	GString *server_url;

	cdata = g_new0 (WebConnectionData, 1);
	g_rec_mutex_init (& (cdata->mutex));
	cdata->server_id = NULL;
	cdata->forced_closing = FALSE;
	cdata->worker_session = soup_session_new_with_options ("ssl-use-system-ca-file", TRUE, NULL);
	cdata->front_session = soup_session_new_with_options ("max-conns-per-host", 1, "ssl-use-system-ca-file", TRUE, NULL);
	if (use_ssl) {
		server_url = g_string_new ("https://");
		g_print ("USING SSL\n");
	}
	else
		server_url = g_string_new ("http://");
	g_string_append (server_url, host);
	if (port)
		g_string_append_printf (server_url, ":%s", port);
	if (path)
		g_string_append_printf (server_url, "/%s", path);
	cdata->front_url = g_strdup_printf ("%s/gda-front.php", server_url->str);
	cdata->worker_url = g_strdup_printf ("%s/gda-worker.php", server_url->str);
	cdata->server_base_url = g_string_free (server_url, FALSE);
	if (serversecret)
		cdata->key = g_strdup (serversecret);
	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata, (GDestroyNotify) _gda_web_free_cnc_data);

	/*
	 * perform setup
	 */
	if (! do_server_setup (cnc, cdata))
		return FALSE;

	/*
	 * send HELLO
	 */
	xmlDocPtr doc;
	gchar status;
#define HELLO_MSG "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n" \
		"<request>\n"						\
		"  <cmd>HELLO</cmd>\n"					\
		"</request>"
	doc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_HELLO, HELLO_MSG, NULL, &status);
	if (!doc) {
		gda_connection_internal_set_provider_data (cnc, NULL, NULL);
		_gda_web_do_server_cleanup (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, doc, NULL);
		xmlFreeDoc (doc);
		gda_connection_internal_set_provider_data (cnc, NULL, NULL);
		_gda_web_do_server_cleanup (cnc, cdata);
		return FALSE;
	}
	xmlFreeDoc (doc);

	/*
	 * send CONNECT
	 */
	gchar *tmp, *token;
#define CONNECT_MSG "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" \
		"<request>\n"						\
		"  <token>%s</token>\n"					\
		"  <cmd>CONNECT</cmd>\n"				\
		"</request>"
	if (cdata->key)
		g_free (cdata->key);
	cdata->key = g_strdup_printf ("%s/AND/%s", db_name, pass);
	
	token = _gda_web_compute_token (cdata);
	tmp = g_strdup_printf (CONNECT_MSG, token);
	g_free (token);

	cdata->server_secret = g_strdup (serversecret);
	doc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_CONNECT, tmp, serversecret, &status);
	g_free (tmp);
	if (!doc) {
		gda_connection_internal_set_provider_data (cnc, NULL, NULL);
		_gda_web_do_server_cleanup (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, doc, NULL);
		xmlFreeDoc (doc);
		gda_connection_internal_set_provider_data (cnc, NULL, NULL);
		_gda_web_do_server_cleanup (cnc, cdata);
		return FALSE;
	}
	xmlFreeDoc (doc);

	/*
	 * change key: cdata->key = MD5(cdata->key)
	 */
	gchar *md5str;
	md5str = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar*) cdata->key, strlen (cdata->key));
	g_free (cdata->key);
	cdata->key = md5str;

	return TRUE;
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated WebConnectionData structure
 *   - Free the WebConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_web_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	g_rec_mutex_lock (& (cdata->mutex));
	if (!cdata->forced_closing && cdata->worker_running) {
		g_rec_mutex_unlock (& (cdata->mutex));
		/* send BYE message */
		xmlDocPtr doc;
		gchar status;
		gchar *tmp, *token;
#define BYE_MSG "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" \
			"<request>\n"					\
			"  <token>%s</token>\n"				\
			"  <cmd>BYE</cmd>\n"				\
			"</request>"	
		token = _gda_web_compute_token (cdata);
		tmp = g_strdup_printf (BYE_MSG, token);
		g_free (token);
		
		doc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_BYE, tmp, cdata->key, &status);
		g_free (tmp);
		if (!doc)
			return FALSE;
		if (status != 'C') {
			_gda_web_set_connection_error_from_xmldoc (cnc, doc, NULL);
			xmlFreeDoc (doc);
			return FALSE;
		}
		xmlFreeDoc (doc);
	}
	else
		g_rec_mutex_unlock (& (cdata->mutex));

	_gda_web_do_server_cleanup (cnc, cdata);

	/* Free the WebConnectionData structure and its contents*/
	_gda_web_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated WebConnectionData structure
 */
static const gchar *
gda_web_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return NULL;
	return cdata->server_version;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a web_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_web_provider_render_operation() and gda_web_provider_perform_operation() methods
 *
 * For now no server operation is supported, it can be added if cdata->reuseable is not %NULL
 */
static gboolean
gda_web_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

        switch (type) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
        case GDA_SERVER_OPERATION_CREATE_TABLE:
        case GDA_SERVER_OPERATION_DROP_TABLE:
        case GDA_SERVER_OPERATION_RENAME_TABLE:

        case GDA_SERVER_OPERATION_ADD_COLUMN:

        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:

        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:
        default:
		TO_IMPLEMENT;
                return FALSE;
        }
}

/*
 * Create operation request
 *
 * Creates a #GdaServerOperation. The following code is generic and should only be changed
 * if some further initialization is required, or if operation's contents is dependent on @cnc
 */
static GdaServerOperation *
gda_web_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				   G_GNUC_UNUSED GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options,
				   GError **error)
{
	WebConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}
	if (!cdata) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Not supported"));
		return NULL;
	}

	TO_IMPLEMENT;
	g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
		     GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
		     "%s", _("Server operations not yet implemented"));
	return NULL;
}

/*
 * Render operation request
 */
static gchar *
gda_web_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
				   G_GNUC_UNUSED GdaServerOperation *op, GError **error)
{
	WebConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}
	if (!cdata) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Not supported"));
		return NULL;
	}

	TO_IMPLEMENT;
	g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
		     "%s", _("Server operations not yet implemented"));
	return NULL;
}

/*
 * Perform operation request
 */
static gboolean
gda_web_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaServerOperation *op, GError **error)
{
        GdaServerOperationType optype;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}
        optype = gda_server_operation_get_op_type (op);
	switch (optype) {
	case GDA_SERVER_OPERATION_CREATE_DB: 
	case GDA_SERVER_OPERATION_DROP_DB: 
        default: 
		/* use the SQL from the provider to perform the action */
		return gda_server_provider_perform_operation_default (provider, cnc, op, error);
	}
}

/*
 * Begin transaction request
 */
static gboolean
gda_web_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				    const gchar *name, GdaTransactionIsolation level,
				    GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	if (name && *name) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Named transaction is not supported"));
		return FALSE;
	}
	if (level != GDA_TRANSACTION_ISOLATION_UNKNOWN) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Transaction level is not supported"));
		return FALSE;
	}
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "BEGIN");

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}

	return TRUE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_web_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				     const gchar *name, GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	if (name && *name) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Named transaction is not supported"));
		return FALSE;
	}
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "COMMIT");

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}

	return TRUE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_web_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *name, GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	if (name && *name) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Named transaction is not supported"));
		return FALSE;
	}
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "ROLLBACK");

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}

	return TRUE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_web_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				const gchar *name, GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	if (!name || !(*name)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Unnamed savepoint is not supported"));
		return FALSE;
	}
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root, cmdnode;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "BEGIN");
	xmlSetProp (cmdnode, BAD_CAST "svpname", BAD_CAST name);

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}

	return TRUE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_web_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				     const gchar *name, GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	if (!name || !(*name)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s", _("Unnamed savepoint is not supported"));
		return FALSE;
	}
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root, cmdnode;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "ROLLBACK");
	xmlSetProp (cmdnode, BAD_CAST "svpname", BAD_CAST name);

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		return FALSE;
	}

	return TRUE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_web_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				   G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_web_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	switch (feature) {
	case GDA_CONNECTION_FEATURE_SQL :
		return TRUE;
	default: 
		return FALSE;
	}
}

/*
 * Get data handler request
 *
 * This method allows one to obtain a pointer to a #GdaDataHandler object specific to @type or @dbms_type (@dbms_type
 * must be considered only if @type is not a valid GType).
 *
 * A data handler allows one to convert a value between its different representations which are a human readable string,
 * an SQL representation and a GValue.
 *
 * The recommended method is to create GdaDataHandler objects only when they are needed and to keep a reference to them
 * for further usage, using the gda_server_provider_handler_declare() method.
 *
 * The implementation shown here does not define any specific data handler, but there should be some for at least 
 * binary and time related types.
 */
static GdaDataHandler *
gda_web_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
				   G_GNUC_UNUSED GType type, G_GNUC_UNUSED const gchar *dbms_type)
{
	WebConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	}
	if (!cdata)
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

/*
 * Get default DBMS type request
 *
 * This method returns the "preferred" DBMS type for GType
 */
static const gchar*
gda_web_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, G_GNUC_UNUSED GType type)
{
	WebConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	}
	if (!cdata)
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

/*
 * Create parser request
 *
 * This method is responsible for creating a #GdaSqlParser object specific to the SQL dialect used
 * by the database. See the PostgreSQL provider implementation for an example.
 */
static GdaSqlParser *
gda_web_provider_create_parser (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc)
{
	WebConnectionData *cdata = NULL;

	if (cnc)
		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;
	if (cdata->reuseable && cdata->reuseable->operations->re_create_parser)
		return cdata->reuseable->operations->re_create_parser (cdata->reuseable);
	else
		return NULL;
}

/*
 * GdaStatement to SQL request
 * 
 * This method renders a #GdaStatement into its SQL representation.
 *
 * The implementation show here simply calls gda_statement_to_sql_extended() but the rendering
 * can be specialized to the database's SQL dialect, see the implementation of gda_statement_to_sql_extended()
 * and SQLite's specialized rendering for more details
 *
 * NOTE: This implementation can call gda_statement_to_sql_extended() _ONLY_ if it passes a %NULL @cnc argument
 */
static gchar *
gda_web_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				   GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				   GSList **params_used, GError **error)
{
	WebConnectionData *cdata = NULL;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}
	if (!cdata)
		return gda_statement_to_sql_extended (stmt, NULL, params, flags, params_used, error);

	/*TO_IMPLEMENT;*/
	return gda_statement_to_sql_extended (stmt, NULL, params, flags, params_used, error);
}

static const gchar*
gtype_to_webtype (GType type)
{
	if (type == G_TYPE_INT64)
                return "integer";
        if (type == G_TYPE_UINT64)
                return "integer";
        if (type == GDA_TYPE_BINARY)
                return "text";
        if (type == GDA_TYPE_BLOB)
                return "blob";
        if (type == G_TYPE_BOOLEAN)
                return "boolean";
        if (type == G_TYPE_DATE)
                return "date";
        if (type == G_TYPE_DOUBLE)
                return "float";
        if (type == GDA_TYPE_GEOMETRIC_POINT)
                return "text";
        if (type == G_TYPE_OBJECT)
                return "text";
        if (type == G_TYPE_INT)
                return "integer";
        if (type == GDA_TYPE_NUMERIC)
                return "decimal";
        if (type == G_TYPE_FLOAT)
                return "float";
        if (type == GDA_TYPE_SHORT)
		return "integer";
        if (type == GDA_TYPE_USHORT)
                return "integer";
        if (type == G_TYPE_STRING)
                return "text";
        if (type == GDA_TYPE_TIME)
                return "time";
        if (type == G_TYPE_DATE_TIME)
                return "timestamp";
        if (type == G_TYPE_CHAR)
                return "integer";
        if (type == G_TYPE_UCHAR)
                return "integer";
        if (type == G_TYPE_ULONG)
                return "integer";
        if (type == G_TYPE_GTYPE)
                return "text";
        if (type == G_TYPE_UINT)
                return "integer";
        if (type == GDA_TYPE_NULL)
                return "text";
        if (type == G_TYPE_INVALID)
                return "text";

        return "text";
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaWebPStmt object and declare it to @cnc.
 */
static gboolean
gda_web_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaStatement *stmt, GError **error)
{
	GdaWebPStmt *ps;
	gboolean retval = FALSE;
	WebConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);


	/* fetch prepares stmt if already done */
	ps = (GdaWebPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* render as SQL understood by the provider */
	GdaSet *params = NULL;
	gchar *sql;
	GSList *used_params = NULL;
	if (! gda_statement_get_parameters (stmt, &params, error))
                return FALSE;
        sql = gda_web_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_UQMARK,
						 &used_params, error);
        if (!sql) 
		goto out;

	/* make a list of the parameter names used in the statement */
	GSList *param_ids = NULL;
        if (used_params) {
                GSList *list;
                for (list = used_params; list; list = list->next) {
                        const gchar *cid;
                        cid = gda_holder_get_id (GDA_HOLDER (list->data));
                        if (cid) {
                                param_ids = g_slist_append (param_ids, g_strdup (cid));
                                /*g_print ("PREPARATION: param ID: %s\n", cid);*/
                        }
                        else {
                                g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
                                             "%s", _("Unnamed parameter is not allowed in prepared statements"));
                                g_slist_free_full (param_ids, (GDestroyNotify) g_free);
                                goto out;
                        }
                }
        }

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root, cmdnode, node;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "PREPARE");
	node = xmlNewTextChild (cmdnode, NULL, BAD_CAST "sql", BAD_CAST sql);
	if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND))
		xmlSetProp (node, BAD_CAST "type", BAD_CAST "SELECT");
	else if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_UNKNOWN) {
		if (! g_ascii_strncasecmp (sql, "select", 6) ||
		    ! g_ascii_strncasecmp (sql, "pragma", 6) ||
		    ! g_ascii_strncasecmp (sql, "show", 4) ||
		    ! g_ascii_strncasecmp (sql, "describe", 8))
			xmlSetProp (node, BAD_CAST "type", BAD_CAST "SELECT");
	}
	if (param_ids) {
		GSList *list;
		xmlNodePtr argsnode;
		argsnode = xmlNewChild (cmdnode, NULL, BAD_CAST "arguments", NULL);
		for (list = used_params; list; list = list->next) {
			node = xmlNewChild (argsnode, NULL, BAD_CAST "arg", NULL);
			xmlSetProp (node, BAD_CAST "type",
				    BAD_CAST gtype_to_webtype (gda_holder_get_g_type (GDA_HOLDER (list->data))));
		}
	}

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_PREPARE, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);

	if (!replydoc) {
		_gda_web_change_connection_to_closed (cnc, cdata);
		goto out;
	}
	if (status != 'O') {
		_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
		xmlFreeDoc (replydoc);

		if (status == 'C')
			_gda_web_change_connection_to_closed (cnc, cdata);
		goto out;
	}
	
	/* create a prepared statement object */
	ps = NULL;
	root = xmlDocGetRootElement (replydoc);
	for (node = root->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "preparehash")) {
			xmlChar *contents;
			contents = xmlNodeGetContent (node);
			ps = gda_web_pstmt_new (cnc, (gchar*) contents);
			xmlFree (contents);
			break;
		}
	}
	xmlFreeDoc (replydoc);
	
	if (!ps) 
		goto out;

	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
	gda_pstmt_set_param_ids (_GDA_PSTMT (ps), param_ids);
	gda_pstmt_set_sql (_GDA_PSTMT (ps), sql);

	gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
	g_object_unref (ps);

	retval = TRUE;

 out:
	if (used_params)
                g_slist_free (used_params);
        if (params)
                g_object_unref (params);
	return retval;
}

/*
 * Execute statement request
 *
 * Executes a statement. This method should do the following:
 *    - try to prepare the statement if not yet done
 *    - optionnally reset the prepared statement
 *    - bind the variables's values (which are in @params)
 *    - add a connection event to log the execution
 *    - execute the prepared statement
 *
 * If @stmt is an INSERT statement and @last_inserted_row is not NULL then additional actions must be taken to return the
 * actual inserted row
 */
static GObject *
gda_web_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaStatement *stmt, GdaSet *params,
				    GdaStatementModelUsage model_usage, 
				    GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaWebPStmt *ps;
	WebConnectionData *cdata;
	gboolean allow_noparam;
        gboolean empty_rs = FALSE; /* TRUE when @allow_noparam is TRUE and there is a problem with @params
                                      => resulting data model will be empty (0 row) */

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

        allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
                (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);
	
        if (last_inserted_row)
                *last_inserted_row = NULL;

	/* Get private data */
	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return NULL;


	/* get/create new prepared statement */
	ps = (GdaWebPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_web_provider_statement_prepare (provider, cnc, stmt, error)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaWebPStmt object.
			 *
			 * Don't call gda_connection_add_prepared_statement() with this new prepared statement
			 * as it will be destroyed once used.
			 */
			return NULL;
		}
		else {
			ps = (GdaWebPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
			g_object_ref (ps);
		}
	}
	else
		g_object_ref (ps);
	g_assert (ps);

	/* prepare XML command */
	xmlDocPtr doc;
	xmlNodePtr root, cmdnode, node;
	gchar *token;
	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "request");
	xmlDocSetRootElement (doc, root);
	token = _gda_web_compute_token (cdata);
	xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
	g_free (token);
	cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "EXEC");
	node = xmlNewTextChild (cmdnode, NULL, BAD_CAST "sql", BAD_CAST gda_pstmt_get_sql (_GDA_PSTMT (ps)));
	if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND))
		xmlSetProp (node, BAD_CAST "type", BAD_CAST "SELECT");
	else if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_UNKNOWN) {
		if (! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "select", 6) ||
		    ! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "pragma", 6) ||
		    ! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "show", 4) ||
		    ! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "describe", 8))
			xmlSetProp (node, BAD_CAST "type", BAD_CAST "SELECT");
	}
	xmlNewChild (cmdnode, NULL, BAD_CAST "preparehash", BAD_CAST (gda_web_pstmt_get_pstmt_hash (ps)));

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	xmlNodePtr argsnode;
	if (gda_pstmt_get_param_ids (_GDA_PSTMT (ps)))
		argsnode = xmlNewChild (cmdnode, NULL, BAD_CAST "arguments", NULL);

	for (i = 1, list = gda_pstmt_get_param_ids (_GDA_PSTMT (ps)); list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h;
		
		/* find requested parameter */
		if (!params) {
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     "%s", _("Missing parameter(s) to execute query"));
			break;
		}

		h = gda_set_get_holder (params, pname);
		if (!h) {
			gchar *tmp = gda_alphanum_to_text (g_strdup (pname + 1));
			if (tmp) {
				h = gda_set_get_holder (params, tmp);
				g_free (tmp);
			}
		}
		if (!h) {
			if (allow_noparam) {
                                /* bind param to NULL */
				node = xmlNewChild (argsnode, NULL, BAD_CAST "arg", NULL);
				xmlSetProp (node, BAD_CAST "type", BAD_CAST "NULL");
                                empty_rs = TRUE;
                                continue;
			}
			else {

				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
                        }
		}
		if (!gda_holder_is_valid (h)) {
			if (allow_noparam) {
                                /* bind param to NULL */
				xmlSetProp (node, BAD_CAST "type", BAD_CAST "NULL");
                                empty_rs = TRUE;
                                continue;
			}
			else {
				gchar *str;
				str = g_strdup_printf (_("Parameter '%s' is invalid"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
                        }
		}
		else if (gda_holder_value_is_default (h) && !gda_holder_get_value (h)) {
			/* create a new GdaStatement to handle all default values and execute it instead
			 * needs to be adapted to take into account how the database server handles default
			 * values (some accept the DEFAULT keyword), changing the 3rd argument of the
			 * gda_statement_rewrite_for_default_values() call
			 */
			GdaSqlStatement *sqlst;
			GError *lerror = NULL;
			sqlst = gda_statement_rewrite_for_default_values (stmt, params, TRUE, &lerror);
			if (!sqlst) {
				event = gda_connection_point_available_event (cnc,
									      GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, lerror && lerror->message ? 
								      lerror->message :
								      _("Can't rewrite statement handle default values"));
				g_propagate_error (error, lerror);
				break;
			}
			
			GdaStatement *rstmt;
			GObject *res;
			rstmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
			gda_sql_statement_free (sqlst);
			g_object_unref (ps);
			xmlFreeDoc (doc);
			res = gda_web_provider_statement_execute (provider, cnc,
								  rstmt, params,
								  model_usage,
								  col_types, last_inserted_row, error);
			g_object_unref (rstmt);
			return res;
		}

		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		if (!value || gda_value_is_null (value)) {
			GdaStatement *rstmt;
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error)) {
				gchar *tmp;
				tmp = gda_value_stringify (value);
				node = xmlNewTextChild (argsnode, NULL, BAD_CAST "arg", BAD_CAST tmp);
				g_free (tmp);
				xmlSetProp (node, BAD_CAST "type",
					    BAD_CAST gtype_to_webtype (gda_holder_get_g_type (h)));
			}
			else if (!rstmt)
				return NULL;
			else {
				xmlFreeDoc (doc);

				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaWebPStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_web_provider_statement_prepare (provider, cnc,
									 rstmt, error))
					return NULL;
				tps = (GdaWebPStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gda_pstmt_get_param_ids (_GDA_PSTMT (gtps));
				gda_pstmt_set_param_ids (_GDA_PSTMT (gtps), NULL);
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids = gda_pstmt_get_param_ids (_GDA_PSTMT (gtps));
				gda_pstmt_set_param_ids (_GDA_PSTMT (gtps), prep_param_ids);

				/* execute */
				obj = gda_web_provider_statement_execute (provider, cnc,
									  rstmt, params,
									  model_usage,
									  col_types,
									  last_inserted_row, error);
				/* clear original @param_ids and restore copied one */
				g_slist_free_full (prep_param_ids, (GDestroyNotify) g_free);

				gda_pstmt_set_param_ids (gtps, copied_param_ids);

				/*if (GDA_IS_DATA_MODEL (obj))
				  gda_data_model_dump ((GdaDataModel*) obj, NULL);*/

				g_object_unref (rstmt);
				g_object_unref (ps);
				return obj;
			}
		}
		else {
			gchar *tmp;
			tmp = gda_value_stringify (value);
			node = xmlNewTextChild (argsnode, NULL, BAD_CAST "arg", BAD_CAST tmp);
			g_free (tmp);
			xmlSetProp (node, BAD_CAST "type",
				    BAD_CAST gtype_to_webtype (gda_holder_get_g_type (h)));
		}
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
		xmlFreeDoc (doc);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, gda_pstmt_get_sql (_GDA_PSTMT (ps)));
        gda_connection_add_event (cnc, event);

	if (empty_rs) {
		/* There are some missing parameters, so the SQL can't be executed but we still want
		 * to execute something to get the columns correctly. A possibility is to actually
		 * execute another SQL which is the code shown here.
		 *
		 * To adapt depending on the C API and its features */
		GdaStatement *estmt;
                gchar *esql;
                estmt = gda_select_alter_select_for_empty (stmt, error);
                if (!estmt) {
			g_object_unref (ps);
                        return NULL;
		}
                esql = gda_statement_to_sql (estmt, NULL, error);
                g_object_unref (estmt);
                if (!esql) {
			g_object_unref (ps);
                        return NULL;
		}

		/* Execute the 'esql' SQL code */
                g_free (esql);

		/* modify @doc */
		TO_IMPLEMENT;
	}

	/* send command */
	xmlChar *cmde;
	xmlDocPtr replydoc;
	int size;
	gchar status;
	
	xmlDocDumpMemory (doc, &cmde, &size);
	xmlFreeDoc (doc);
	replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_EXEC, (gchar*) cmde, cdata->key, &status);
	xmlFree (cmde);
	
	if (!replydoc)
		status = 'E';
	if (status != 'O') {
		if (replydoc) {
			_gda_web_set_connection_error_from_xmldoc (cnc, replydoc, error);
			xmlFreeDoc (replydoc);
			if (status == 'C')
				_gda_web_change_connection_to_closed (cnc, cdata);
		}
		else
			_gda_web_change_connection_to_closed (cnc, cdata);
		return NULL;
	}

	/* required: help @cnc keep some stats */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
	gda_connection_event_set_description (event, "Command OK");
	gda_connection_add_event (cnc, event);
	gda_connection_internal_statement_executed (cnc, stmt, params, event);

	root = xmlDocGetRootElement (replydoc);
	GObject *retval = NULL;
	for (node = root->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "impacted_rows")) {
			xmlChar *contents;
			GdaSet *set = NULL;

			contents = xmlNodeGetContent (node);
			set = gda_set_new_inline (1, "IMPACTED_ROWS", G_TYPE_INT, atoi ((gchar*) contents));
			xmlFree (contents);
			retval = (GObject*) set;
		}
		else if (!strcmp ((gchar*) node->name, "gda_array")) {
			GdaDataModel *data_model;
			g_rec_mutex_lock (& (cdata->mutex));
			data_model = gda_web_recordset_new (cnc, ps, params, model_usage,
							    col_types, cdata->session_id, node, error);
			g_rec_mutex_unlock (& (cdata->mutex));
			retval = (GObject*) data_model;

			if (! gda_web_recordset_store (GDA_WEB_RECORDSET (data_model), node, error)) {
				g_object_unref (G_OBJECT (data_model));
				retval = NULL;
			}
		}
		else if (!strcmp ((gchar*) node->name, "preparehash")) {
			xmlChar *contents;
			contents = xmlNodeGetContent (node);
			gda_web_pstmt_set_pstmt_hash (ps, (gchar*) contents);
			xmlFree (contents);
		}
	}

	xmlFreeDoc (replydoc);
	g_object_unref (ps);
	return retval;
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Removes any default value inserted or updated
 */
static GdaSqlStatement *
gda_web_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaStatement *stmt, GdaSet *params, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}
	return gda_statement_rewrite_for_default_values (stmt, params, TRUE, error);
}

static gchar *
gda_web_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
				   const gchar *id,
				   gboolean for_meta_store, gboolean force_quotes)
{
	WebConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	}
	if (!cdata)
		return gda_sql_identifier_quote (id, NULL, NULL, for_meta_store, force_quotes);

	/*TO_IMPLEMENT;*/
	return gda_sql_identifier_quote (id, NULL, NULL, for_meta_store, force_quotes);
}
