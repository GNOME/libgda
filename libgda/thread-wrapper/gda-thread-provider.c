/* GDA Thread provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-connection-internal.h>
#include "gda-thread-provider.h"
#include "gda-thread-wrapper.h"
#include "gda-thread-recordset.h"

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

/*
 * Per connection private data is defined as ThreadConnectionData
 */

/*
 * GObject methods
 */
static void gda_thread_provider_class_init (GdaThreadProviderClass *klass);
static void gda_thread_provider_init       (GdaThreadProvider *provider,
					    GdaThreadProviderClass *klass);

static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static GdaConnection      *gda_thread_provider_create_connection (GdaServerProvider *provider);
static gboolean            gda_thread_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaQuarkList *params, GdaQuarkList *auth,
							      guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data);
static gboolean            gda_thread_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_thread_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_thread_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_thread_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_thread_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperationType type,
							       GdaSet *options, GError **error);
static gchar              *gda_thread_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperation *op, GError **error);

static gboolean            gda_thread_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								GdaServerOperation *op, guint *task_id, 
								GdaServerProviderAsyncCallback async_cb, gpointer cb_data,
								GError **error);
/* transactions */
static gboolean            gda_thread_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_thread_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_thread_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								   const gchar *name, GError **error);
static gboolean            gda_thread_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							    const gchar *name, GError **error);
static gboolean            gda_thread_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_thread_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *name, GError **error);

/* information retreival */
static const gchar        *gda_thread_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_thread_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

static const gchar        *gda_thread_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_thread_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
							       GType g_type, const gchar *dbms_type);

static const gchar*        gda_thread_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								    GType type);
/* statements */
static GdaSqlParser        *gda_thread_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_thread_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params, 
								 GdaStatementSqlFlag flags,
								 GSList **params_used, GError **error);
static gboolean             gda_thread_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GError **error);
static GObject             *gda_thread_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params,
								 GdaStatementModelUsage model_usage, 
								 GType *col_types, GdaSet **last_inserted_row, 
								 guint *task_id, GdaServerProviderExecCallback async_cb, 
								 gpointer cb_data, GError **error);

/* distributed transactions */
static gboolean gda_thread_provider_xa_start    (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_thread_provider_xa_end      (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_thread_provider_xa_prepare  (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_thread_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_thread_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static GList   *gda_thread_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc, 
						   GError **error);

static void gda_thread_free_cnc_data (ThreadConnectionData *cdata);

/*
 * GdaThreadProvider class implementation
 */
static void
gda_thread_provider_class_init (GdaThreadProviderClass *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	provider_class->get_version = gda_thread_provider_get_version;
	provider_class->get_server_version = gda_thread_provider_get_server_version;
	provider_class->get_name = gda_thread_provider_get_name;
	provider_class->supports_feature = gda_thread_provider_supports_feature;

	provider_class->get_data_handler = gda_thread_provider_get_data_handler;
	provider_class->get_def_dbms_type = gda_thread_provider_get_default_dbms_type;

	provider_class->open_connection = gda_thread_provider_open_connection;
	provider_class->close_connection = gda_thread_provider_close_connection;
	provider_class->get_database = gda_thread_provider_get_database;

	provider_class->supports_operation = gda_thread_provider_supports_operation;
        provider_class->create_operation = gda_thread_provider_create_operation;
        provider_class->render_operation = gda_thread_provider_render_operation;
        provider_class->perform_operation = gda_thread_provider_perform_operation;

	provider_class->begin_transaction = gda_thread_provider_begin_transaction;
	provider_class->commit_transaction = gda_thread_provider_commit_transaction;
	provider_class->rollback_transaction = gda_thread_provider_rollback_transaction;
	provider_class->add_savepoint = gda_thread_provider_add_savepoint;
        provider_class->rollback_savepoint = gda_thread_provider_rollback_savepoint;
        provider_class->delete_savepoint = gda_thread_provider_delete_savepoint;

	provider_class->create_parser = gda_thread_provider_create_parser;
	provider_class->statement_to_sql = gda_thread_provider_statement_to_sql;
	provider_class->statement_prepare = gda_thread_provider_statement_prepare;
	provider_class->statement_execute = gda_thread_provider_statement_execute;

	provider_class->is_busy = NULL;
	provider_class->cancel = NULL;
	provider_class->create_connection = gda_thread_provider_create_connection;

	memset (&(provider_class->meta_funcs), 0, sizeof (GdaServerProviderMeta));

	/* distributed transactions: if not supported, then provider_class->xa_funcs should be set to NULL */
	provider_class->xa_funcs = g_new0 (GdaServerProviderXa, 1);
	provider_class->xa_funcs->xa_start = gda_thread_provider_xa_start;
	provider_class->xa_funcs->xa_end = gda_thread_provider_xa_end;
	provider_class->xa_funcs->xa_prepare = gda_thread_provider_xa_prepare;
	provider_class->xa_funcs->xa_commit = gda_thread_provider_xa_commit;
	provider_class->xa_funcs->xa_rollback = gda_thread_provider_xa_rollback;
	provider_class->xa_funcs->xa_recover = gda_thread_provider_xa_recover;
}

static void
gda_thread_provider_init (GdaThreadProvider *thread_prv, GdaThreadProviderClass *klass)
{
}

GType
gda_thread_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static GTypeInfo info = {
			sizeof (GdaThreadProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_thread_provider_class_init,
			NULL, NULL,
			sizeof (GdaThreadProvider),
			0,
			(GInstanceInitFunc) gda_thread_provider_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaThreadProvider", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}


/*
 * Get provider name request
 */
static const gchar *
gda_thread_provider_get_name (GdaServerProvider *provider)
{
	return "ThreadWrapper";
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_thread_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

static GdaConnection *
gda_thread_provider_create_connection (GdaServerProvider *provider)
{
	GdaConnection *cnc;
        g_return_val_if_fail (GDA_IS_THREAD_PROVIDER (provider), NULL);

        cnc = g_object_new (GDA_TYPE_CONNECTION, "provider", provider, "is-wrapper", TRUE, NULL);

        return cnc;
}

/* 
 * Open connection request
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
typedef struct {
	const gchar *dsn;

	const gchar *prov_name;
	const gchar *cnc_string;

	const gchar *auth_string;

	GdaConnectionOptions options;

	GdaServerProvider *out_cnc_provider;
} OpenConnectionData;

static GdaConnection *
sub_thread_open_connection (OpenConnectionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GdaConnection *cnc;
	if (data->dsn)
		cnc = gda_connection_open_from_dsn (data->dsn, data->auth_string, data->options, error);
	else
		cnc = gda_connection_open_from_string (data->prov_name, data->cnc_string, 
						       data->auth_string, data->options, error);
	if (cnc)
		data->out_cnc_provider = gda_connection_get_provider (cnc);
	g_print ("/%s() => %p\n", __FUNCTION__, cnc);
	return cnc;
}

static void setup_signals (GdaConnection *cnc, ThreadConnectionData *cdata);

static gboolean
gda_thread_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, GdaQuarkList *auth,
				     guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data)
{
	g_return_val_if_fail (GDA_IS_THREAD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		gda_connection_add_event_string (cnc, 
						 _("Provider does not support asynchronous connection open"));
                return FALSE;
	}
		
	/* test if connection has to be opened using a DSN or a connection string */
	gchar *dsn, *auth_string, *cnc_string;
	GdaConnectionOptions options;
	OpenConnectionData *data = NULL;
	g_object_get (cnc, "dsn", &dsn, 
		      "auth_string", &auth_string,
		      "cnc-string", &cnc_string, 
		      "options", &options,
		      NULL);
	if (dsn) {
		data = g_new0 (OpenConnectionData, 1);
		data->dsn = dsn;
	}
	else if (cnc_string) {
		data = g_new0 (OpenConnectionData, 1);
		data->prov_name = gda_quark_list_find (params, "PROVIDER_NAME");
		data->cnc_string = cnc_string;
	}
	
	/* open sub connection */
	GdaThreadWrapper *wr;
	GdaConnection *sub_cnc;
	GError *error = NULL;
	g_assert (data);
	data->auth_string = auth_string;
	data->options = options & (~GDA_CONNECTION_OPTIONS_THREAD_SAFE);
	wr = gda_thread_wrapper_new ();
	gda_thread_wrapper_execute (wr, (GdaThreadWrapperFunc) sub_thread_open_connection, data, NULL, NULL);
	sub_cnc = gda_thread_wrapper_fetch_result (wr, TRUE, NULL, &error);
	g_free (dsn);
	g_free (cnc_string);
	g_free (auth_string);
	if (!sub_cnc) {
		TO_IMPLEMENT; /* create a GdaConnectionEvent from @error using 
				 gda_connection_add_event_string () */
		g_object_unref (wr);
		g_free (data);
		return FALSE;
	}
	
	ThreadConnectionData *cdata;
	cdata = g_new0 (ThreadConnectionData, 1);
	cdata->sub_connection = sub_cnc;
	cdata->cnc_provider = data->out_cnc_provider;
	cdata->wrapper = wr;
	cdata->handlers_ids = g_array_sized_new (FALSE, FALSE, sizeof (gulong), 2);
	g_free (data);
	gda_connection_internal_set_provider_data (cnc, cdata, (GDestroyNotify) gda_thread_free_cnc_data);
	setup_signals (cnc, cdata);

	return TRUE;
}

static void
sub_cnc_error_cb (GdaThreadWrapper *wrapper, GdaConnection *sub_cnc, const gchar *signal,
		  gint n_param_values, const GValue *param_values,
		  gpointer gda_reserved, GdaConnection *wrapper_cnc)
{
	GdaConnectionEvent *ev;
	g_assert (n_param_values == 1);
	ev = g_value_get_object (param_values);
	g_object_ref (ev);
	gda_connection_add_event (wrapper_cnc, ev);
}

static void
sub_cnc_transaction_status_changed_cb (GdaThreadWrapper *wrapper, GdaConnection *sub_cnc, const gchar *signal,
				       gint n_param_values, const GValue *param_values,
				       gpointer gda_reserved, GdaConnection *wrapper_cnc)
{
	_gda_connection_force_transaction_status (wrapper_cnc, sub_cnc);
}

static void
setup_signals (GdaConnection *cnc, ThreadConnectionData *cdata)
{
	gulong hid;
	hid = gda_thread_wrapper_connect_raw (cdata->wrapper, cdata->sub_connection, "error",
					      (GdaThreadWrapperCallback) sub_cnc_error_cb, cnc);
	g_array_prepend_val (cdata->handlers_ids, hid);
	hid = gda_thread_wrapper_connect_raw (cdata->wrapper, cdata->sub_connection, "transaction-status-changed",
					      (GdaThreadWrapperCallback) sub_cnc_transaction_status_changed_cb, cnc);
	g_array_prepend_val (cdata->handlers_ids, hid);
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated ThreadConnectionData structure
 *   - Free the ThreadConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
} CncProvData;

static gpointer
sub_thread_close_connection (CncProvData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->close_connection (data->prov, data->cnc);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	ThreadConnectionData *cdata;
	CncProvData wdata;
	gpointer res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_close_connection, &wdata, NULL, NULL);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated ThreadConnectionData structure
 */

static const gchar *
sub_thread_get_server_version (CncProvData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	const gchar *retval;
	retval = PROV_CLASS (data->prov)->get_server_version (data->prov, data->cnc);
	g_print ("/%s() => %s\n", __FUNCTION__, retval);
	return retval;
}
static const gchar *
gda_thread_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	ThreadConnectionData *cdata;
	CncProvData wdata;

	if (!cnc) 
		return NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_get_server_version, &wdata, NULL, NULL);
	return (const gchar*) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
}

/*
 * Get database request
 *
 * Returns the database name as a string, which should be stored in @cnc's associated ThreadConnectionData structure
 */
static const gchar *
sub_thread_get_database (CncProvData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	const gchar *retval;
	retval = PROV_CLASS (data->prov)->get_database (data->prov, data->cnc);
	g_print ("/%s() => %s\n", __FUNCTION__, retval);
	return retval;
}

static const gchar *
gda_thread_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	ThreadConnectionData *cdata;
	CncProvData wdata;

	if (!cnc) 
		return NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_get_database, &wdata, NULL, NULL);
	return (const gchar *) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a thread_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_thread_provider_render_operation() and gda_thread_provider_perform_operation() methods
 *
 * In this example, the GDA_SERVER_OPERATION_CREATE_TABLE is implemented
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaServerOperationType type;
	GdaSet *options;
} SupportsOperationData;

static gpointer
sub_thread_supports_operation (SupportsOperationData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->supports_operation (data->prov, data->cnc, data->type, data->options);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
					GdaServerOperationType type, GdaSet *options)
{
	ThreadConnectionData *cdata;
	SupportsOperationData wdata;
	gpointer res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_supports_operation, &wdata, NULL, NULL);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Create operation request
 *
 * Creates a #GdaServerOperation. The following code is generic and should only be changed
 * if some further initialization is required, or if operation's contents is dependant on @cnc
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaServerOperationType type;
	GdaSet *options;
} CreateOperationData;

static GdaServerOperation *
sub_thread_create_operation (CreateOperationData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GdaServerOperation *op;
	op = PROV_CLASS (data->prov)->create_operation (data->prov,
							data->cnc,
							data->type, 
							data->options,
							error);
	g_print ("/%s() => %p\n", __FUNCTION__, op);
	return op;
}

static GdaServerOperation *
gda_thread_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperationType type, GdaSet *options, GError **error)
{
	ThreadConnectionData *cdata;
	CreateOperationData wdata;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return NULL;
	}
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.type = type;
	wdata.options = options;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_create_operation, &wdata, NULL, NULL);
	return (GdaServerOperation*) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, error);
}

/*
 * Render operation request
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaServerOperation *op;
} RenderOperationData;

static gchar *
sub_thread_render_operation (RenderOperationData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gchar *str;
	str = PROV_CLASS (data->prov)->render_operation (data->prov,
							 data->cnc,
							 data->op, 
							 error);
	g_print ("/%s() => %s\n", __FUNCTION__, str);
	return str;
}

static gchar *
gda_thread_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperation *op, GError **error)
{
	ThreadConnectionData *cdata;
	RenderOperationData wdata;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return NULL;
	}
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.op = op;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_render_operation, &wdata, NULL, NULL);
	return (gchar*) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, error);
}

/*
 * Perform operation request
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaServerOperation *op;
} PerformOperationData;

static gpointer
sub_thread_perform_operation (PerformOperationData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->perform_operation (data->prov, data->cnc, data->op, 
							     NULL, NULL, NULL, error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaServerOperation *op, guint *task_id, 
				       GdaServerProviderAsyncCallback async_cb, gpointer cb_data, GError **error)
{
	ThreadConnectionData *cdata;
	PerformOperationData wdata;
	gpointer res;

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Provider does not support asynchronous server operation"));
                return FALSE;
	}

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.op = op;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_perform_operation, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Begin transaction request
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	const gchar *name; 
	GdaTransactionIsolation level;
} BeginTransactionData;

static gpointer
sub_thread_begin_transaction (BeginTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->begin_transaction (data->prov, data->cnc, data->name, 
							     data->level, error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *name, GdaTransactionIsolation level,
				       GError **error)
{
	ThreadConnectionData *cdata;
	BeginTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;
	wdata.level = level;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_begin_transaction, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, error);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Commit transaction request
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	const gchar *name; 
	GdaTransactionIsolation level;
} TransactionData;

static gpointer
sub_thread_commit_transaction (TransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->commit_transaction (data->prov, data->cnc, data->name, 
							      error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	ThreadConnectionData *cdata;
	TransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_commit_transaction, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Rollback transaction request
 */
static gpointer
sub_thread_rollback_transaction (TransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->rollback_transaction (data->prov, data->cnc, data->name, 
								error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	ThreadConnectionData *cdata;
	TransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_rollback_transaction, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Add savepoint request
 */
static gpointer
sub_thread_add_savepoint (TransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->add_savepoint (data->prov, data->cnc, data->name, 
							 error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				   const gchar *name, GError **error)
{
	ThreadConnectionData *cdata;
	TransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_add_savepoint, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Rollback savepoint request
 */
static gpointer
sub_thread_rollback_savepoint (TransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->rollback_savepoint (data->prov, data->cnc, data->name, 
							 error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	ThreadConnectionData *cdata;
	TransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_rollback_savepoint, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Delete savepoint request
 */
static gpointer
sub_thread_delete_savepoint (TransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->delete_savepoint (data->prov, data->cnc, data->name, 
							 error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				    const gchar *name, GError **error)
{
	ThreadConnectionData *cdata;
	TransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.name = name;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_delete_savepoint, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_thread_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
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
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GType type;
	const gchar *dbms_type;
} GetDataHandlerData;

static GdaDataHandler *
sub_thread_get_data_handler (GetDataHandlerData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GdaDataHandler *retval;
	retval = PROV_CLASS (data->prov)->get_data_handler (data->prov, data->cnc, data->type, data->dbms_type);
	g_print ("/%s() => %p\n", __FUNCTION__, retval);
	return retval;
}

static GdaDataHandler *
gda_thread_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
				      GType type, const gchar *dbms_type)
{
	ThreadConnectionData *cdata;
	GetDataHandlerData wdata;
	GdaDataHandler *res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.type = type;
	wdata.dbms_type = dbms_type;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_get_data_handler, &wdata, NULL, NULL);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return res;
}

/*
 * Get default DBMS type request
 *
 * This method returns the "preferred" DBMS type for GType
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GType type;
} GetDefaultDbmsTypeData;

static const gchar *
sub_thread_get_default_dbms_type (GetDefaultDbmsTypeData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	const gchar *retval;
	retval = PROV_CLASS (data->prov)->get_def_dbms_type (data->prov, data->cnc, data->type);
	g_print ("/%s() => %s\n", __FUNCTION__, retval);
	return retval;
}

static const gchar*
gda_thread_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	ThreadConnectionData *cdata;
	GetDefaultDbmsTypeData wdata;
	const gchar *res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.type = type;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_get_default_dbms_type, &wdata, NULL, NULL);
	res = (const gchar *) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return res;
}

/*
 * Create parser request
 *
 * This method is responsible for creating a #GdaSqlParser object specific to the SQL dialect used
 * by the database. See the PostgreSQL provider implementation for an example.
 */

static GdaSqlParser *
sub_thread_create_parser (CncProvData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GdaSqlParser *parser;
	parser = PROV_CLASS (data->prov)->create_parser (data->prov, data->cnc);
	g_print ("/%s() => %p\n", __FUNCTION__, parser);
	return parser;
}

static GdaSqlParser *
gda_thread_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	ThreadConnectionData *cdata;
	CncProvData wdata;

	if (!cnc) 
		return NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_create_parser, &wdata, NULL, NULL);
	return (GdaSqlParser *) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
}

/*
 * GdaStatement to SQL request
 * 
 * This method renders a #GdaStatement into its SQL representation.
 *
 * The implementation show here simply calls gda_statement_to_sql_extended() but the rendering
 * can be specialized to the database's SQL dialect, see the implementation of gda_statement_to_sql_extended()
 * and SQLite's specialized rendering for more details
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaStatement *stmt;
	GdaSet *params;
	GdaStatementSqlFlag flags;
	GSList **params_used;
} StmtToSqlData;

static const gchar *
sub_thread_statement_to_sql (StmtToSqlData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	const gchar *retval;
	retval = PROV_CLASS (data->prov)->statement_to_sql (data->prov, data->cnc, data->stmt,
							    data->params, data->flags, data->params_used, error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval);
	return retval;
}

static gchar *
gda_thread_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				      GSList **params_used, GError **error)
{
	ThreadConnectionData *cdata;
	StmtToSqlData wdata;

	if (!cnc) 
		return NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.stmt = stmt;
	wdata.params = params;
	wdata.flags = flags;
	wdata.params_used = params_used;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_statement_to_sql, &wdata, NULL, NULL);
	return (gchar *) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaThreadPStmt object and declare it to @cnc.
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaStatement *stmt;
} PrepareStatementData;

static gpointer
sub_thread_prepare_statement (PrepareStatementData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = PROV_CLASS (data->prov)->statement_prepare (data->prov,
							     data->cnc,
							     data->stmt, 
							     error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GError **error)
{
	ThreadConnectionData *cdata;
	PrepareStatementData wdata;
	gpointer res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.stmt = stmt;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_prepare_statement, &wdata, NULL, NULL);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, error);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
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
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	GdaStatement *stmt;
	GdaSet *params;
	GdaStatementModelUsage model_usage;
	GType *col_types;

	GdaConnection *real_cnc;
	GdaThreadWrapper *wrapper;

	GdaSet **last_inserted_row;
} ExecuteStatementData;

static gpointer
sub_thread_execute_statement (ExecuteStatementData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GObject *retval;
	retval = PROV_CLASS (data->prov)->statement_execute (data->prov,
							     data->cnc,
							     data->stmt,
							     data->params,
							     data->model_usage,
							     data->col_types,
							     data->last_inserted_row,
							     NULL, NULL, NULL,
							     error);
	g_print ("/%s() => %p\n", __FUNCTION__, retval);

	if (GDA_IS_DATA_MODEL (retval)) {
		/* substitute the GdaDataSelect with a GdaThreadRecordset */
		GdaDataModel *model;
		model = _gda_thread_recordset_new (data->real_cnc, data->wrapper, GDA_DATA_MODEL (retval));
		g_object_unref (retval);
		retval = (GObject*) model;
	}

	return retval;
}
static GObject *
gda_thread_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params,
				       GdaStatementModelUsage model_usage, 
				       GType *col_types, GdaSet **last_inserted_row, 
				       guint *task_id, 
				       GdaServerProviderExecCallback async_cb, gpointer cb_data, GError **error)
{
	ThreadConnectionData *cdata;
	ExecuteStatementData wdata;

	/* FIXME: handle async requests */
	if (async_cb) {
		TO_IMPLEMENT;
		return NULL;
	}

	GObject *res;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.stmt = stmt;
	wdata.params = params;
	wdata.model_usage = model_usage;
	wdata.col_types = col_types;
	wdata.last_inserted_row = last_inserted_row;

	wdata.real_cnc = cnc;
	wdata.wrapper = cdata->wrapper;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_execute_statement, &wdata, NULL, NULL);
	res = (GObject*) gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, error);

	return res;
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
typedef struct {
	GdaServerProvider *prov;
	GdaConnection *cnc;
	const GdaXaTransactionId *xid;
} XaTransactionData;

static gpointer
sub_thread_xa_start (XaTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval = FALSE;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_start (data->prov, data->cnc, data->xid, 
							      error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc, 
			      const GdaXaTransactionId *xid, GError **error)
{
	ThreadConnectionData *cdata;
	XaTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.xid = xid;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_start, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * put the XA transaction in the IDLE state: the connection won't accept any more modifications.
 * This state is required by some database providers before actually going to the PREPARED state
 */
static gpointer
sub_thread_xa_end (XaTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval = FALSE;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_end (data->prov, data->cnc, data->xid, 
							    error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc, 
			      const GdaXaTransactionId *xid, GError **error)
{
	ThreadConnectionData *cdata;
	XaTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.xid = xid;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_end, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gpointer
sub_thread_xa_prepare (XaTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval = FALSE;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_prepare (data->prov, data->cnc, data->xid, 
								error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc, 
				const GdaXaTransactionId *xid, GError **error)
{
	ThreadConnectionData *cdata;
	XaTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.xid = xid;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_prepare, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * commits the distributed transaction: actually write the prepared data to the database and
 * terminates the XA transaction
 */
static gpointer
sub_thread_xa_commit (XaTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval = FALSE;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_commit (data->prov, data->cnc, data->xid, 
								error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc, 
				 const GdaXaTransactionId *xid, GError **error)
{
	ThreadConnectionData *cdata;
	XaTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.xid = xid;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_commit, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gpointer
sub_thread_xa_rollback (XaTransactionData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval = FALSE;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_rollback (data->prov, data->cnc, data->xid, 
								error);
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static gboolean
gda_thread_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
				   const GdaXaTransactionId *xid, GError **error)
{
	ThreadConnectionData *cdata;
	XaTransactionData wdata;
	gpointer res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;
	wdata.xid = xid;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_rollback, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return GPOINTER_TO_INT (res) ? TRUE : FALSE;
}

/*
 * Lists all XA transactions that are in the PREPARED state
 *
 * Returns: a list of GdaXaTransactionId structures, which will be freed by the caller
 */
static GList *
sub_thread_xa_recover (CncProvData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	GList *retval;
	retval = PROV_CLASS (data->prov)->xa_funcs->xa_recover (data->prov, data->cnc, error);
	g_print ("/%s() => %p\n", __FUNCTION__, retval);
	return GINT_TO_POINTER (retval ? 1 : 0);
}

static GList *
gda_thread_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				GError **error)
{
	ThreadConnectionData *cdata;
	CncProvData wdata;
	GList *res;

	if (!cnc) {
		g_set_error (error, 0, 0, _("A connection is required"));
		return FALSE;
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ThreadConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	
	wdata.prov = cdata->cnc_provider;
	wdata.cnc = cdata->sub_connection;

	gda_thread_wrapper_execute (cdata->wrapper, 
				    (GdaThreadWrapperFunc) sub_thread_xa_recover, &wdata, NULL, error);
	res = gda_thread_wrapper_fetch_result (cdata->wrapper, TRUE, NULL, NULL);
	return res;
}

/*
 * Free connection's specific data
 */
static void
gda_thread_free_cnc_data (ThreadConnectionData *cdata)
{
	if (!cdata)
		return;

	/*disconnect signals handlers */
	gint i;
	for (i = 0; i < cdata->handlers_ids->len; i++) {
		gulong hid = g_array_index (cdata->handlers_ids, gulong, i);
		gda_thread_wrapper_disconnect (cdata->wrapper, hid);
	}

	/* unref cdata->sub_connection in sub thread */
	gda_thread_wrapper_execute_void (cdata->wrapper, 
					 (GdaThreadWrapperVoidFunc) g_object_unref,
					 cdata->sub_connection, NULL, NULL);
	g_object_unref (cdata->wrapper);
	g_free (cdata);
}
