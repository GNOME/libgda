/* GDA Mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-mysql.h"
#include "gda-mysql-provider.h"
#include "gda-mysql-recordset.h"
#include "gda-mysql-ddl.h"
#include "gda-mysql-meta.h"

#include "gda-mysql-util.h"
#include "gda-mysql-parser.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_mysql_provider_class_init (GdaMysqlProviderClass  *klass);
static void gda_mysql_provider_init       (GdaMysqlProvider       *provider,
					   GdaMysqlProviderClass  *klass);
static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_mysql_provider_open_connection (GdaServerProvider               *provider,
							       GdaConnection                   *cnc,
							       GdaQuarkList                    *params,
							       GdaQuarkList                    *auth,
							       guint                           *task_id,
							       GdaServerProviderAsyncCallback   async_cb,
							       gpointer                         cb_data);
static gboolean            gda_mysql_provider_close_connection (GdaServerProvider  *provider,
								GdaConnection      *cnc);
static const gchar        *gda_mysql_provider_get_server_version (GdaServerProvider  *provider,
								  GdaConnection      *cnc);
static const gchar        *gda_mysql_provider_get_database (GdaServerProvider  *provider,
							    GdaConnection      *cnc);

/* DDL operations */
static gboolean            gda_mysql_provider_supports_operation (GdaServerProvider       *provider,
								  GdaConnection           *cnc,
								  GdaServerOperationType   type,
								  GdaSet                  *options);
static GdaServerOperation *gda_mysql_provider_create_operation (GdaServerProvider       *provider,
								GdaConnection           *cnc,
								GdaServerOperationType   type,
								GdaSet                  *options,
								GError                **error);
static gchar              *gda_mysql_provider_render_operation (GdaServerProvider   *provider,
								GdaConnection       *cnc,
								GdaServerOperation  *op,
								GError             **error);

static gboolean            gda_mysql_provider_perform_operation (GdaServerProvider               *provider,
								 GdaConnection                   *cnc,
								 GdaServerOperation              *op,
								 guint                           *task_id, 
								 GdaServerProviderAsyncCallback   async_cb,
								 gpointer                         cb_data,
								 GError                         **error);
/* transactions */
static gboolean            gda_mysql_provider_begin_transaction (GdaServerProvider        *provider,
								 GdaConnection            *cnc,
								 const gchar              *name,
								 GdaTransactionIsolation   level,
								 GError                  **error);
static gboolean            gda_mysql_provider_commit_transaction (GdaServerProvider  *provider,
								  GdaConnection *cnc,
								  const gchar *name, GError **error);
static gboolean            gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
								    GdaConnection     *cnc,
								    const gchar       *name,
								    GError           **error);
static gboolean            gda_mysql_provider_add_savepoint (GdaServerProvider  *provider,
							     GdaConnection      *cnc,
							     const gchar        *name,
							     GError            **error);
static gboolean            gda_mysql_provider_rollback_savepoint (GdaServerProvider  *provider,
								  GdaConnection      *cnc,
								  const gchar        *name,
								  GError            **error);
static gboolean            gda_mysql_provider_delete_savepoint (GdaServerProvider  *provider,
								GdaConnection      *cnc,
								const gchar        *name,
								GError            **error);

/* information retrieval */
static const gchar        *gda_mysql_provider_get_version (GdaServerProvider  *provider);
static gboolean            gda_mysql_provider_supports_feature (GdaServerProvider     *provider,
								GdaConnection         *cnc,
								GdaConnectionFeature   feature);

static const gchar        *gda_mysql_provider_get_name (GdaServerProvider  *provider);

static GdaDataHandler     *gda_mysql_provider_get_data_handler (GdaServerProvider  *provider,
								GdaConnection      *cnc,
								GType               g_type,
								const gchar        *dbms_type);

static const gchar*        gda_mysql_provider_get_default_dbms_type (GdaServerProvider  *provider,
								     GdaConnection      *cnc,
								     GType               type);
/* statements */
static GdaSqlParser        *gda_mysql_provider_create_parser (GdaServerProvider  *provider,
							      GdaConnection      *cnc);
static gchar               *gda_mysql_provider_statement_to_sql  (GdaServerProvider    *provider,
								  GdaConnection        *cnc,
								  GdaStatement         *stmt,
								  GdaSet               *params, 
								  GdaStatementSqlFlag   flags,
								  GSList              **params_used,
								  GError              **error);
static gboolean             gda_mysql_provider_statement_prepare (GdaServerProvider  *provider,
								  GdaConnection      *cnc,
								  GdaStatement       *stmt,
								  GError            **error);
static GObject             *gda_mysql_provider_statement_execute (GdaServerProvider               *provider,
								  GdaConnection                   *cnc,
								  GdaStatement                    *stmt,
								  GdaSet                          *params,
								  GdaStatementModelUsage           model_usage, 
								  GType                           *col_types,
								  GdaSet                         **last_inserted_row, 
								  guint                           *task_id,
								  GdaServerProviderExecCallback    async_cb, 
								  gpointer                         cb_data,
								  GError                         **error);

/* Quoting */
static gchar               *gda_mysql_identifier_quote    (GdaServerProvider *provider, GdaConnection *cnc,
							   const gchar *id,
							   gboolean meta_store_convention, gboolean force_quotes);

/* distributed transactions */
static gboolean gda_mysql_provider_xa_start    (GdaServerProvider         *provider,
						GdaConnection             *cnc, 
						const GdaXaTransactionId  *xid,
						GError                   **error);

static gboolean gda_mysql_provider_xa_end      (GdaServerProvider         *provider,
						GdaConnection             *cnc, 
						const GdaXaTransactionId  *xid,
						GError                   **error);
static gboolean gda_mysql_provider_xa_prepare  (GdaServerProvider         *provider,
						GdaConnection             *cnc, 
						const GdaXaTransactionId  *xid,
						GError                   **error);

static gboolean gda_mysql_provider_xa_commit   (GdaServerProvider        *provider,
						GdaConnection            *cnc, 
						const GdaXaTransactionId  *xid,
						GError                   **error);
static gboolean gda_mysql_provider_xa_rollback (GdaServerProvider         *provider,
						GdaConnection             *cnc, 
						const GdaXaTransactionId  *xid,
						GError                   **error);

static GList   *gda_mysql_provider_xa_recover  (GdaServerProvider  *provider,
						GdaConnection      *cnc, 
						GError            **error);


/* 
 * private connection data destroy 
 */
static void gda_mysql_free_cnc_data (MysqlConnectionData  *cdata);


/*
 * Prepared internal statements
 * TO_ADD: any prepared statement to be used internally by the provider should be
 *         declared here, as constants and as SQL statements
 */
GdaStatement **internal_stmt;

typedef enum {
	INTERNAL_STMT1
} InternalStatementItem;

gchar *internal_sql[] = {
	"SQL for INTERNAL_STMT1"
};

/*
 * GdaMysqlProvider class implementation
 */
static void
gda_mysql_provider_class_init (GdaMysqlProviderClass  *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	provider_class->get_version = gda_mysql_provider_get_version;
	provider_class->get_server_version = gda_mysql_provider_get_server_version;
	provider_class->get_name = gda_mysql_provider_get_name;
	provider_class->supports_feature = gda_mysql_provider_supports_feature;

	provider_class->get_data_handler = gda_mysql_provider_get_data_handler;
	provider_class->get_def_dbms_type = gda_mysql_provider_get_default_dbms_type;

	provider_class->open_connection = gda_mysql_provider_open_connection;
	provider_class->close_connection = gda_mysql_provider_close_connection;
	provider_class->get_database = gda_mysql_provider_get_database;

	provider_class->supports_operation = gda_mysql_provider_supports_operation;
        provider_class->create_operation = gda_mysql_provider_create_operation;
        provider_class->render_operation = gda_mysql_provider_render_operation;
        provider_class->perform_operation = gda_mysql_provider_perform_operation;

	provider_class->begin_transaction = gda_mysql_provider_begin_transaction;
	provider_class->commit_transaction = gda_mysql_provider_commit_transaction;
	provider_class->rollback_transaction = gda_mysql_provider_rollback_transaction;
	provider_class->add_savepoint = gda_mysql_provider_add_savepoint;
        provider_class->rollback_savepoint = gda_mysql_provider_rollback_savepoint;
        provider_class->delete_savepoint = gda_mysql_provider_delete_savepoint;

	provider_class->create_parser = gda_mysql_provider_create_parser;
	provider_class->statement_to_sql = gda_mysql_provider_statement_to_sql;
	provider_class->statement_prepare = gda_mysql_provider_statement_prepare;
	provider_class->statement_execute = gda_mysql_provider_statement_execute;

	provider_class->is_busy = NULL;
	provider_class->cancel = NULL;
	provider_class->create_connection = NULL;

	provider_class->identifier_quote = gda_mysql_identifier_quote;

	memset (&(provider_class->meta_funcs), 0, sizeof (GdaServerProviderMeta));
	provider_class->meta_funcs._info = _gda_mysql_meta__info;
        provider_class->meta_funcs._btypes = _gda_mysql_meta__btypes;
        provider_class->meta_funcs._udt = _gda_mysql_meta__udt;
        provider_class->meta_funcs.udt = _gda_mysql_meta_udt;
        provider_class->meta_funcs._udt_cols = _gda_mysql_meta__udt_cols;
        provider_class->meta_funcs.udt_cols = _gda_mysql_meta_udt_cols;
        provider_class->meta_funcs._enums = _gda_mysql_meta__enums;
        provider_class->meta_funcs.enums = _gda_mysql_meta_enums;
        provider_class->meta_funcs._domains = _gda_mysql_meta__domains;
        provider_class->meta_funcs.domains = _gda_mysql_meta_domains;
        provider_class->meta_funcs._constraints_dom = _gda_mysql_meta__constraints_dom;
        provider_class->meta_funcs.constraints_dom = _gda_mysql_meta_constraints_dom;
        provider_class->meta_funcs._el_types = _gda_mysql_meta__el_types;
        provider_class->meta_funcs._collations = _gda_mysql_meta__collations;
        provider_class->meta_funcs.collations = _gda_mysql_meta_collations;
        provider_class->meta_funcs._character_sets = _gda_mysql_meta__character_sets;
        provider_class->meta_funcs.character_sets = _gda_mysql_meta_character_sets;
        provider_class->meta_funcs._schemata = _gda_mysql_meta__schemata;
        provider_class->meta_funcs.schemata = _gda_mysql_meta_schemata;
        provider_class->meta_funcs._tables_views = _gda_mysql_meta__tables_views;
        provider_class->meta_funcs.tables_views = _gda_mysql_meta_tables_views;
        provider_class->meta_funcs._columns = _gda_mysql_meta__columns;
        provider_class->meta_funcs.columns = _gda_mysql_meta_columns;
        provider_class->meta_funcs._view_cols = _gda_mysql_meta__view_cols;
        provider_class->meta_funcs.view_cols = _gda_mysql_meta_view_cols;
        provider_class->meta_funcs._constraints_tab = _gda_mysql_meta__constraints_tab;
        provider_class->meta_funcs.constraints_tab = _gda_mysql_meta_constraints_tab;
        provider_class->meta_funcs._constraints_ref = _gda_mysql_meta__constraints_ref;
        provider_class->meta_funcs.constraints_ref = _gda_mysql_meta_constraints_ref;
        provider_class->meta_funcs._key_columns = _gda_mysql_meta__key_columns;
        provider_class->meta_funcs.key_columns = _gda_mysql_meta_key_columns;
        provider_class->meta_funcs._check_columns = _gda_mysql_meta__check_columns;
        provider_class->meta_funcs.check_columns = _gda_mysql_meta_check_columns;
        provider_class->meta_funcs._triggers = _gda_mysql_meta__triggers;
        provider_class->meta_funcs.triggers = _gda_mysql_meta_triggers;
        provider_class->meta_funcs._routines = _gda_mysql_meta__routines;
        provider_class->meta_funcs.routines = _gda_mysql_meta_routines;
        provider_class->meta_funcs._routine_col = _gda_mysql_meta__routine_col;
        provider_class->meta_funcs.routine_col = _gda_mysql_meta_routine_col;
        provider_class->meta_funcs._routine_par = _gda_mysql_meta__routine_par;
        provider_class->meta_funcs.routine_par = _gda_mysql_meta_routine_par;
	

	/* distributed transactions: if not supported, then provider_class->xa_funcs should be set to NULL */
	provider_class->xa_funcs = g_new0 (GdaServerProviderXa, 1);
	provider_class->xa_funcs->xa_start = gda_mysql_provider_xa_start;
	provider_class->xa_funcs->xa_end = gda_mysql_provider_xa_end;
	provider_class->xa_funcs->xa_prepare = gda_mysql_provider_xa_prepare;
	provider_class->xa_funcs->xa_commit = gda_mysql_provider_xa_commit;
	provider_class->xa_funcs->xa_rollback = gda_mysql_provider_xa_rollback;
	provider_class->xa_funcs->xa_recover = gda_mysql_provider_xa_recover;
	
	if (! mysql_thread_safe ()) {
		gda_log_message ("MySQL was not compiled with the --enable-thread-safe-client flag, "
				 "only one thread can access the provider");
		provider_class->limiting_thread = g_thread_self ();
	}
	else
		provider_class->limiting_thread = NULL;
	
}

static void
gda_mysql_provider_init (GdaMysqlProvider       *mysql_prv,
			 GdaMysqlProviderClass  *klass)
{
	InternalStatementItem i;
	GdaSqlParser *parser;

	parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) mysql_prv);
	internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
	for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
		if (!internal_stmt[i]) 
			g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
	}

	/* meta data init */
	_gda_mysql_provider_meta_init ((GdaServerProvider*) mysql_prv);

	/* TO_ADD: any other provider's init should be added here */
}

GType
gda_mysql_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static GTypeInfo info = {
			sizeof (GdaMysqlProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_provider_class_init,
			NULL, NULL,
			sizeof (GdaMysqlProvider),
			0,
			(GInstanceInitFunc) gda_mysql_provider_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaMysqlProvider", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}


/*
 * Get provider name request
 */
static const gchar *
gda_mysql_provider_get_name (GdaServerProvider  *provider)
{
	return MYSQL_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_mysql_provider_get_version (GdaServerProvider  *provider)
{
	return PACKAGE_VERSION;
}


/*
 * Open a MYSQL connection.
 */
static MYSQL *
real_open_connection (const gchar  *host,
		      gint          port,
		      const gchar  *socket,
		      const gchar  *db,
		      const gchar  *username,
		      const gchar  *password,
		      gboolean      use_ssl,
		      gboolean      compress,
		      GError      **error)
{
	unsigned int flags = 0;

	/* Exclusive: host/pair otherwise unix socket. */
	if ((host || port >= 0) && socket) {
		g_set_error (error, 0, 0, "%s", 
			     _("Cannot give a UNIX SOCKET if you also provide "
			       "either a HOST or a PORT"));
		return NULL;
	}

	/* Defaults. */
	if (!socket) {
		if (!host)
			host = "localhost";
		else if (port <= 0)
			port = 3306;
	}

	if (use_ssl)
		flags |= CLIENT_SSL;
	if (compress)
		flags |= CLIENT_COMPRESS;
	
	MYSQL *mysql = g_new0 (MYSQL, 1);
	mysql_init (mysql);

	MYSQL *return_mysql = mysql_real_connect (mysql, host,
						  username, password,
#if MYSQL_VERSION_ID >= 32200
						  db,
#endif
						  (port > 0) ? port : 0,
						  socket, flags);
	if (!return_mysql || mysql != return_mysql) {
		g_set_error (error, 0, 0, "%s", mysql_error (mysql));
		g_free (mysql);
		mysql = NULL;
	}

	/* Optionnally set some attributes for the newly opened connection (encoding to UTF-8 for example )*/

#if MYSQL_VERSION_ID < 32200
	if (mysql &&
	    mysql_select_db (mysql, db) != 0) {
		g_set_error (error, 0, 0, "%s", mysql_error (mysql));
		g_free (mysql);
		mysql = NULL;
	}
#endif

#if MYSQL_VERSION_ID >= 50007
	if (mysql &&
	    mysql_set_character_set (mysql, "utf8")) {
		g_warning (_("Could not set client charset to UTF8. "
			     "Using %s. It'll be problems with non UTF-8 characters"),
			   mysql_character_set_name (mysql));
	}
#endif

	return mysql;
}

static gchar *
get_mysql_version (MYSQL  *mysql)
{
	g_return_val_if_fail (mysql != NULL, NULL);
	unsigned long version_long;
	version_long = mysql_get_server_version (mysql);
	return g_strdup_printf ("%lu.%lu.%lu",
				version_long/10000,
				(version_long%10000)/100,
				(version_long%100));
}

static gchar *
get_mysql_short_version (MYSQL  *mysql)
{
	g_return_val_if_fail (mysql != NULL, NULL);
	unsigned long version_long;
	version_long = mysql_get_server_version (mysql);
	return g_strdup_printf ("%lu%lu",
				version_long/10000,
				(version_long%10000)/100);
}


/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a MysqlConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_mysql_provider_open_connection (GdaServerProvider               *provider,
				    GdaConnection                   *cnc,
				    GdaQuarkList                    *params,
				    GdaQuarkList                    *auth,
				    guint                           *task_id,
				    GdaServerProviderAsyncCallback   async_cb,
				    gpointer                         cb_data)
{
	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		gda_connection_add_event_string (cnc, _("Provider does not support asynchronous connection open"));
                return FALSE;
	}

	/* Check for connection parameters */
	/* TO_ADD: your own connection parameters */
	const gchar *db_name;
	db_name = gda_quark_list_find (params, "DB_NAME");
	if (!db_name) {
		gda_connection_add_event_string (cnc,
						 _("The connection string must contain the DB_NAME values"));
		return FALSE;
	}
	
	const gchar *host;
	host = gda_quark_list_find (params, "HOST");

	const gchar *user, *password;
	user = gda_quark_list_find (auth, "USERNAME");
	if (!user)
		user = gda_quark_list_find (params, "USERNAME");
	password = gda_quark_list_find (auth, "PASSWORD");
	if (!password)
		password = gda_quark_list_find (params, "PASSWORD");

	const gchar *port, *unix_socket, *use_ssl, *compress;
	port = gda_quark_list_find (params, "PORT");
	unix_socket = gda_quark_list_find (params, "UNIX_SOCKET");
	use_ssl = gda_quark_list_find (params, "USE_SSL");
	compress = gda_quark_list_find (params, "COMPRESS");
	
	/* open the real connection to the database */
	/* TO_ADD: C API specific function calls;
	 * if it fails, add a connection event and return FALSE */
	// TO_IMPLEMENT;
	
	GError *error = NULL;
	MYSQL *mysql = real_open_connection (host, (port != NULL) ? atoi (port) : 0,
					     unix_socket, db_name,
					     user, password,
					     (use_ssl && ((*use_ssl == 't') || (*use_ssl == 'T'))) ? TRUE : FALSE,
					     (compress && ((*compress == 't') || (*compress == 'T'))) ? TRUE : FALSE,
					     &error);
	if (!mysql) {
		_gda_mysql_make_error (cnc, mysql, NULL, NULL);
		return FALSE;
	}

	/* Set some attributes for the newly opened connection (encoding to UTF-8)*/
	int res;
	res = mysql_query (mysql, "SET NAMES 'utf8'");
	if (res != 0) {
		_gda_mysql_make_error (cnc, mysql, NULL, NULL);
		return FALSE;
	}

	/* get case sensitiveness of tables */
	res = mysql_query (mysql, "SHOW VARIABLES WHERE Variable_name = 'lower_case_table_names'");
	if (res != 0) {
		_gda_mysql_make_error (cnc, mysql, NULL, NULL);
		return FALSE;
	}

	MYSQL_RES *mdata;
	mdata = mysql_store_result (mysql);
	if (!mdata) {
		_gda_mysql_make_error (cnc, mysql, NULL, NULL);
		return FALSE;
	}

	MYSQL_ROW mrow;
	gboolean case_sensitive = FALSE;
	mrow = mysql_fetch_row (mdata);
	if (!mrow) {
		_gda_mysql_make_error (cnc, mysql, NULL, NULL);
		mysql_free_result (mdata);
		return FALSE;
	}
	switch (atoi (mrow[1])) {
	case 0:
		case_sensitive = TRUE;
		break;
	case 1:
	case 2:
		break;
	default:
		g_warning ("Unknown 'lower_case_table_names' variable value: %s\n", mrow[1]);
	}
	
	mysql_free_result (mdata);
	
	

	/* Create a new instance of the provider specific data associated to a connection (MysqlConnectionData),
	 * and set its contents */
	MysqlConnectionData *cdata;
	cdata = g_new0 (MysqlConnectionData, 1);
	gda_connection_internal_set_provider_data (cnc, cdata, (GDestroyNotify) gda_mysql_free_cnc_data);
	
	cdata->cnc = cnc;
	cdata->mysql = mysql;

	cdata->version_long = mysql_get_server_version (mysql);
	cdata->version = get_mysql_version (mysql);
	cdata->short_version = get_mysql_short_version (mysql);
	cdata->tables_case_sensitive = case_sensitive;

	return TRUE;
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated MysqlConnectionData structure
 *   - Free the MysqlConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_mysql_provider_close_connection (GdaServerProvider  *provider,
				     GdaConnection      *cnc)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	// TO_IMPLEMENT;

	/* Free the MysqlConnectionData structure and its contents*/
	gda_mysql_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated MysqlConnectionData structure
 */
static const gchar *
gda_mysql_provider_get_server_version (GdaServerProvider  *provider,
				       GdaConnection      *cnc)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	TO_IMPLEMENT;
	return NULL;
}

/*
 * Get database request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated MysqlConnectionData structure
 */
static const gchar *
gda_mysql_provider_get_database (GdaServerProvider  *provider,
				 GdaConnection      *cnc)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	TO_IMPLEMENT;
	return NULL;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a mysql_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_mysql_provider_render_operation() and gda_mysql_provider_perform_operation() methods
 *
 * In this example, the GDA_SERVER_OPERATION_CREATE_TABLE is implemented
 */
static gboolean
gda_mysql_provider_supports_operation (GdaServerProvider       *provider,
				       GdaConnection           *cnc,
				       GdaServerOperationType   type,
				       GdaSet                  *options)
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
        case GDA_SERVER_OPERATION_COMMENT_TABLE:
        case GDA_SERVER_OPERATION_ADD_COLUMN:
	case GDA_SERVER_OPERATION_DROP_COLUMN:
	case GDA_SERVER_OPERATION_COMMENT_COLUMN:
        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:
        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:
		return TRUE;
        default:
                return FALSE;
        }
}

/*
 * Create operation request
 *
 * Creates a #GdaServerOperation. The following code is generic and should only be changed
 * if some further initialization is required, or if operation's contents is dependant on @cnc
 */
static GdaServerOperation *
gda_mysql_provider_create_operation (GdaServerProvider       *provider,
				     GdaConnection           *cnc,
				     GdaServerOperationType   type,
				     GdaSet                  *options,
				     GError                 **error)
{
        gchar *file;
        GdaServerOperation *op;
        gchar *str;
	gchar *dir;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

        file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
        str = g_strdup_printf ("mysql_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), str);
		g_free (str);
                return NULL;
        }
        g_free (str);

        op = gda_server_operation_new (type, file);
        g_free (file);

        return op;
}

/*
 * Render operation request
 */
static gchar *
gda_mysql_provider_render_operation (GdaServerProvider   *provider,
				     GdaConnection       *cnc,
				     GdaServerOperation  *op,
				     GError             **error)
{
        gchar *sql = NULL;
        gchar *file;
        gchar *str;
	gchar *dir;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	/* test @op's validity */
        file = g_utf8_strdown (gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)), -1);
        str = g_strdup_printf ("mysql_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), str);
		g_free (str);
                return NULL;
        }
        g_free (str);
        if (!gda_server_operation_is_valid (op, file, error)) {
                g_free (file);
                return NULL;
        }
        g_free (file);

	/* actual rendering */
        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
		sql = gda_mysql_render_CREATE_DB (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = gda_mysql_render_DROP_DB (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_mysql_render_CREATE_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_TABLE:
		sql = gda_mysql_render_DROP_TABLE (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_RENAME_TABLE:
		sql = gda_mysql_render_RENAME_TABLE (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_COMMENT_TABLE:
		sql = gda_mysql_render_COMMENT_TABLE (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_ADD_COLUMN:
		sql = gda_mysql_render_ADD_COLUMN (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_DROP_COLUMN:
		sql = gda_mysql_render_DROP_COLUMN (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_COMMENT_COLUMN:
		sql = gda_mysql_render_COMMENT_COLUMN (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_CREATE_INDEX:
		sql = gda_mysql_render_CREATE_INDEX (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_DROP_INDEX:
		sql = gda_mysql_render_DROP_INDEX (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_CREATE_VIEW:
		sql = gda_mysql_render_CREATE_VIEW (provider, cnc, op, error);
		break;
        case GDA_SERVER_OPERATION_DROP_VIEW:
		sql = gda_mysql_render_DROP_VIEW (provider, cnc, op, error);
		break;
        default:
                g_assert_not_reached ();
        }
        return sql;
}

/*
 * Perform operation request
 */
static gboolean
gda_mysql_provider_perform_operation (GdaServerProvider               *provider,
				      GdaConnection                   *cnc,
				      GdaServerOperation              *op,
				      guint                           *task_id, 
				      GdaServerProviderAsyncCallback   async_cb,
				      gpointer                         cb_data,
				      GError                         **error)
{
        GdaServerOperationType optype;

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Provider does not support asynchronous server operation"));
                return FALSE;
	}

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
gda_mysql_provider_begin_transaction (GdaServerProvider        *provider,
				      GdaConnection            *cnc,
				      const gchar              *name,
				      GdaTransactionIsolation   level,
				      GError                  **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_mysql_provider_commit_transaction (GdaServerProvider  *provider,
				       GdaConnection      *cnc,
				       const gchar        *name,
				       GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_mysql_provider_rollback_transaction (GdaServerProvider  *provider,
					 GdaConnection      *cnc,
					 const gchar        *name,
					 GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_mysql_provider_add_savepoint (GdaServerProvider  *provider,
				  GdaConnection      *cnc,
				  const gchar        *name,
				  GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_mysql_provider_rollback_savepoint (GdaServerProvider  *provider,
				       GdaConnection      *cnc,
				       const gchar        *name,
				       GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_mysql_provider_delete_savepoint (GdaServerProvider  *provider,
				     GdaConnection      *cnc,
				     const gchar        *name,
				     GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_mysql_provider_supports_feature (GdaServerProvider     *provider,
				     GdaConnection         *cnc,
				     GdaConnectionFeature   feature)
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
gda_mysql_provider_get_data_handler (GdaServerProvider  *provider,
				     GdaConnection      *cnc,
				     GType               type,
				     const gchar        *dbms_type)
{
	GdaDataHandler *dh;
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	if (type == G_TYPE_INVALID) {
		TO_IMPLEMENT; /* use @dbms_type */
		dh = NULL;
	}
	else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		TO_IMPLEMENT; /* define data handlers for these types */
		dh = NULL;
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP) ||
		 (type == G_TYPE_DATE)) {
		TO_IMPLEMENT; /* define data handlers for these types */
		dh = NULL;
	}
	else
		dh = gda_server_provider_get_data_handler_default (provider, cnc, type, dbms_type);

	return dh;
}

/*
 * Get default DBMS type request
 *
 * This method returns the "preferred" DBMS type for GType
 */
static const gchar*
gda_mysql_provider_get_default_dbms_type (GdaServerProvider  *provider,
					  GdaConnection      *cnc,
					  GType               type)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	TO_IMPLEMENT;

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_UINT64))
		return "integer";

	if ((type == GDA_TYPE_BINARY) ||
	    (type == GDA_TYPE_BLOB))
		return "blob";

	if (type == G_TYPE_BOOLEAN)
		return "boolean";
	
	if ((type == G_TYPE_DATE) || 
	    (type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == GDA_TYPE_LIST) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == GDA_TYPE_TIMESTAMP) ||
	    (type == G_TYPE_INVALID) ||
	    (type == G_TYPE_GTYPE))
		return "string";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "real";
	
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	if (type == G_TYPE_DATE)
		return "date";

	return "text";
}

/*
 * Create parser request
 *
 * This method is responsible for creating a #GdaSqlParser object specific to the SQL dialect used
 * by the database. See the PostgreSQL provider implementation for an example.
 */
static GdaSqlParser *
gda_mysql_provider_create_parser (GdaServerProvider  *provider,
				  GdaConnection      *cnc)
{
	return (GdaSqlParser *) g_object_new (GDA_TYPE_MYSQL_PARSER,
					      "tokenizer-flavour", GDA_SQL_PARSER_FLAVOUR_MYSQL,
					      NULL);
}

/*
 * GdaStatement to SQL request
 * 
 * This method renders a #GdaStatement into its SQL representation.
 */
static gchar *
gda_mysql_provider_statement_to_sql (GdaServerProvider    *provider,
				     GdaConnection        *cnc,
				     GdaStatement         *stmt,
				     GdaSet               *params,
				     GdaStatementSqlFlag   flags,
				     GSList              **params_used,
				     GError              **error)
{
	gchar *str;
        GdaSqlRenderingContext context;

        g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
        if (cnc) {
                g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
                g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
        }

        memset (&context, 0, sizeof (context));
        context.params = params;
        context.flags = flags;
	if (cnc) {
		context.cnc = cnc;
		context.provider = gda_connection_get_provider (cnc);
	}

        str = gda_statement_to_sql_real (stmt, &context, error);

        if (str) {
                if (params_used)
                        *params_used = context.params_used;
                else
                        g_slist_free (context.params_used);
        }
        else {
                if (params_used)
                        *params_used = NULL;
                g_slist_free (context.params_used);
        }
        return str;
}

static GdaMysqlPStmt *
real_prepare (GdaServerProvider *provider, GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	GdaMysqlPStmt *ps = NULL;
	MysqlConnectionData *cdata;

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;

	/* Render as SQL understood by Mysql. */
	GdaSet *set;
	if (!gda_statement_get_parameters (stmt, &set, error))
		return NULL;

	GSList *used_set = NULL;
	gchar *sql = gda_mysql_provider_statement_to_sql (provider, cnc, stmt, set,
							  GDA_STATEMENT_SQL_PARAMS_AS_UQMARK, &used_set, error);

	if (!sql)
		goto cleanup;

	MYSQL_STMT *mysql_stmt = mysql_stmt_init (cdata->mysql);
	if (!mysql_stmt) {
		_gda_mysql_make_error (cnc, NULL, mysql_stmt, error);
		return FALSE;
	}

	my_bool update_max_length = 1;
	if (mysql_stmt_attr_set (mysql_stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (const void *) &update_max_length)) {
		_gda_mysql_make_error (cnc, NULL, mysql_stmt, error);
		mysql_stmt_close (mysql_stmt);
		return FALSE;
	}

	if (mysql_stmt_prepare (mysql_stmt, sql, strlen (sql))) {
		_gda_mysql_make_error (cdata->cnc, NULL, mysql_stmt, error);
		mysql_stmt_close (mysql_stmt);
		goto cleanup;
	}

	GSList *param_ids = NULL, *current;
	for (current = used_set; current; current = current->next) {
		const gchar *id = gda_holder_get_id
			(GDA_HOLDER(current->data));
		if (id) {
			param_ids = g_slist_append (param_ids, g_strdup (id));
		}
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
				     "%s", _("Unnamed statement parameter is not allowed in prepared statement."));
			g_slist_foreach (param_ids, (GFunc) g_free, NULL);
			g_slist_free (param_ids);
			param_ids = NULL;
			mysql_stmt_close (mysql_stmt);
			goto cleanup;
		}
	}

	/* Create prepared statement. */
	ps = gda_mysql_pstmt_new (cnc, cdata->mysql, mysql_stmt);
	
	if (!ps)
		return NULL;
	else {
		gda_pstmt_set_gda_statement (_GDA_PSTMT(ps), stmt);
		_GDA_PSTMT(ps)->param_ids = param_ids;
		_GDA_PSTMT(ps)->sql = sql;
		return ps;
	}
	
 cleanup:

	if (set)
		g_object_unref (G_OBJECT(set));
	if (used_set)
		g_slist_free (used_set);
	g_free (sql);
	
	return NULL;
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaMysqlPStmt object and declare it to @cnc.
 */
static gboolean
gda_mysql_provider_statement_prepare (GdaServerProvider  *provider,
				      GdaConnection      *cnc,
				      GdaStatement       *stmt,
				      GError            **error)
{
	GdaMysqlPStmt *ps;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaMysqlPStmt*) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	ps = real_prepare (provider, cnc, stmt, error);
        if (!ps)
                return FALSE;
        else {
                gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
                g_object_unref (ps);
                return TRUE;
        }
}


static GdaMysqlPStmt *
prepare_stmt_simple (MysqlConnectionData  *cdata,
		     const gchar          *sql,
		     GError              **error)
{
	GdaMysqlPStmt *ps = NULL;
	g_return_val_if_fail (sql != NULL, NULL);

	MYSQL_STMT *mysql_stmt = mysql_stmt_init (cdata->mysql);
	if (!mysql_stmt) {
		_gda_mysql_make_error (cdata->cnc, NULL, mysql_stmt, error);
		return FALSE;
	}

	my_bool update_max_length = 1;
	if (mysql_stmt_attr_set (mysql_stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (const void *) &update_max_length)) {
		_gda_mysql_make_error (cdata->cnc, NULL, mysql_stmt, error);
		mysql_stmt_close (mysql_stmt);
		return FALSE;
	}
	
	if (mysql_stmt_prepare (mysql_stmt, sql, strlen (sql))) {
		_gda_mysql_make_error (cdata->cnc, NULL, mysql_stmt, error);
		mysql_stmt_close (mysql_stmt);
		ps = NULL;
	}
	else {
		ps = gda_mysql_pstmt_new (cdata->cnc, cdata->mysql, mysql_stmt);
		_GDA_PSTMT(ps)->param_ids = NULL;
		_GDA_PSTMT(ps)->sql = g_strdup (sql);
	}
	
	return ps;
}

static void
free_bind_param_data (GSList *mem_to_free)
{
	if (mem_to_free) {
		g_slist_foreach (mem_to_free, (GFunc) g_free, NULL);
		g_slist_free (mem_to_free);
	}
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
gda_mysql_provider_statement_execute (GdaServerProvider               *provider,
				      GdaConnection                   *cnc,
				      GdaStatement                    *stmt,
				      GdaSet                          *params,
				      GdaStatementModelUsage           model_usage, 
				      GType                           *col_types,
				      GdaSet                         **last_inserted_row, 
				      guint                           *task_id, 
				      GdaServerProviderExecCallback    async_cb,
				      gpointer                         cb_data,
				      GError                         **error)
{
	GdaMysqlPStmt *ps;
	MysqlConnectionData *cdata;
	gboolean allow_noparam;
        gboolean empty_rs = FALSE; /* TRUE when @allow_noparam is TRUE and there is a problem with @params
                                      => resulting data model will be empty (0 row) */

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Provider does not support asynchronous statement execution"));
                return FALSE;
	}

	allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
		(gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);

	if (last_inserted_row)
		*last_inserted_row = NULL;

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	/* get/create new prepared statement */
	ps = (GdaMysqlPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_mysql_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaMysqlPStmt object.
			 */
			gchar *sql = gda_mysql_provider_statement_to_sql (provider, cnc, stmt, 
									  params, 0, NULL, error);
			if (!sql)
				return NULL;
			ps = prepare_stmt_simple (cdata, sql, error);
			g_free (sql);
			if (!ps)
				return NULL;
		}
		else {
			ps = (GdaMysqlPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
			g_object_ref (ps);
		}
	}
	else
		g_object_ref (ps);

	g_assert (ps);
	if (ps->stmt_used) {
                /* Don't use @ps => prepare stmt again */
                GdaMysqlPStmt *nps;
                nps = real_prepare (provider, cnc, stmt, error);
                if (!nps)
                        return NULL;
                gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) nps);
		g_object_unref (ps);
		ps = nps;
	}

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	
	gint nb_params = g_slist_length (_GDA_PSTMT (ps)->param_ids);
	/*g_print ("NB=%d, SQL=%s\n", nb_params, _GDA_PSTMT(ps)->sql);*/

	MYSQL_BIND *mysql_bind_param = NULL;
	GSList *mem_to_free = NULL;

	if (nb_params > 0) {
		mysql_bind_param = g_new0 (MYSQL_BIND, nb_params);
		mem_to_free = g_slist_prepend (mem_to_free, mysql_bind_param);
	}

	for (i = 0, list = _GDA_PSTMT (ps)->param_ids; list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;

		/* find requested parameter */
		if (!params) {
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     "%s", _("Missing parameter(s) to execute query"));
			break;
		}

		GdaHolder *h = gda_set_get_holder (params, pname);
		if (!h) {
			gchar *tmp = gda_alphanum_to_text (g_strdup (pname + 1));
			if (tmp) {
				h = gda_set_get_holder (params, tmp);
				g_free (tmp);
			}
		}
		if (!h) {
			if (!allow_noparam) {
				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
				event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
			else {
                                /* bind param to NULL */
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
                                empty_rs = TRUE;
                                continue;
			}
		}

		if (!gda_holder_is_valid (h)) {
			if (!allow_noparam) {
				gchar *str;
				str = g_strdup_printf (_("Parameter '%s' is invalid"), pname);
				event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
			else {
                                /* bind param to NULL */
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
                                empty_rs = TRUE;
                                continue;
                        }
		}

		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);

		if (value == NULL || gda_value_is_null (value)) {
			mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
			mysql_bind_param[i].is_null = (my_bool*)1;
		}
		else if (G_VALUE_TYPE(value) == GDA_TYPE_TIMESTAMP) {
			const GdaTimestamp *ts;

			ts = gda_value_get_timestamp (value);
			if (!ts) {
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
			}
			else {
				MYSQL_TIME *mtime;
				mtime = g_new0 (MYSQL_TIME, 1);
				mem_to_free = g_slist_prepend (mem_to_free, mtime);
				mtime->year = ts->year;
				mtime->month = ts->month;
				mtime->day = ts->day;
				mtime->hour = ts->hour;
				mtime->minute = ts->minute;
				mtime->second = ts->second;
				mtime->second_part = ts->fraction;

				mysql_bind_param[i].buffer_type= MYSQL_TYPE_TIMESTAMP;
				mysql_bind_param[i].buffer= (char *)mtime;
				mysql_bind_param[i].buffer_length = sizeof (MYSQL_TIME);
			}
		}
		else if (G_VALUE_TYPE(value) == GDA_TYPE_TIME) {
			const GdaTime *ts;

			ts = gda_value_get_time (value);
			if (!ts) {
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
			}
			else {
				MYSQL_TIME *mtime;
				mtime = g_new0 (MYSQL_TIME, 1);
				mem_to_free = g_slist_prepend (mem_to_free, mtime);
				mtime->hour = ts->hour;
				mtime->minute = ts->minute;
				mtime->second = ts->second;
				mtime->second_part = ts->fraction;

				mysql_bind_param[i].buffer_type= MYSQL_TYPE_TIME;
				mysql_bind_param[i].buffer= (char *)mtime;
				mysql_bind_param[i].buffer_length = sizeof (MYSQL_TIME);
			}
		}
		else if (G_VALUE_TYPE(value) == G_TYPE_DATE) {
			const GDate *ts;

			ts = (GDate*) g_value_get_boxed (value);
			if (!ts) {
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
			}
			else {
				MYSQL_TIME *mtime;
				mtime = g_new0 (MYSQL_TIME, 1);
				mem_to_free = g_slist_prepend (mem_to_free, mtime);
				mtime->year = ts->year;
				mtime->month = ts->month;
				mtime->day = ts->day;

				mysql_bind_param[i].buffer_type= MYSQL_TYPE_DATE;
				mysql_bind_param[i].buffer= (char *)mtime;
				mysql_bind_param[i].buffer_length = sizeof (MYSQL_TIME);
			}
		}
		else if (G_VALUE_TYPE(value) == G_TYPE_STRING) {
			const gchar *str;
			str = g_value_get_string (value);
			if (!str) {
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_NULL;
				mysql_bind_param[i].is_null = (my_bool*)1;
			}
			else {
				mysql_bind_param[i].buffer_type= MYSQL_TYPE_STRING;
				mysql_bind_param[i].buffer= (char *) str;
				mysql_bind_param[i].buffer_length = strlen (str);
				mysql_bind_param[i].length = NULL; /* str is 0 terminated */
			}
		}
		else {
			gchar *str;
			GdaDataHandler *data_handler =
				gda_server_provider_get_data_handler_g_type (provider, cnc, 
									     G_VALUE_TYPE (value));
			if (data_handler == NULL) {
				TO_IMPLEMENT;
				/* there is an error here */
				str = NULL;
			}
			else {
				str = gda_data_handler_get_str_from_value (data_handler, value);
				mem_to_free = g_slist_prepend (mem_to_free, str);
				mysql_bind_param[i].buffer_type = MYSQL_TYPE_STRING;
				mysql_bind_param[i].buffer = str;
				mysql_bind_param[i].buffer_length = strlen (str);
				mysql_bind_param[i].length = NULL; /* str is 0 terminated */
			}
		}
	}
		
	if (mysql_bind_param && mysql_stmt_bind_param (ps->mysql_stmt, mysql_bind_param)) {
		//g_warning ("mysql_stmt_bind_param failed: %s\n", mysql_stmt_error (ps->mysql_stmt));
		event = _gda_mysql_make_error (cnc, cdata->mysql, ps->mysql_stmt, error);
	}

	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
		free_bind_param_data (mem_to_free);
		return NULL;
	}


	/* use cursor when retrieving result */
	if ((model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) == 0 &&
	    gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) {
#if MYSQL_VERSION_ID >= 50002
		const unsigned long cursor_type = CURSOR_TYPE_READ_ONLY;
		if (mysql_stmt_attr_set (ps->mysql_stmt, STMT_ATTR_CURSOR_TYPE, (void *) &cursor_type)) {
			_gda_mysql_make_error (cnc, NULL, ps->mysql_stmt, NULL);
			g_object_unref (ps);
			free_bind_param_data (mem_to_free);
			return NULL;
		}
#else
		model_usage = GDA_STATEMENT_MODEL_RANDOM_ACCESS;
		g_warning (_("Could not use CURSOR. Mysql version 5.0 at least is required. "
			     "Using random access anyway."));
#endif
	}

	/* add a connection event for the execution */
	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
        gda_connection_add_event (cnc, event);

	if (empty_rs) {
		/* There are some missing parameters, so the SQL can't be executed but we still want
		 * to execute something to get the columns correctly. A possibility is to actually
		 * execute another SQL which is the code shown here.
		 *
		 * To adapt depending on the C API and its features */
		GdaStatement *stmt_for_empty;
                gchar *sql_for_empty;
                stmt_for_empty = gda_select_alter_select_for_empty (stmt, error);
                if (!stmt_for_empty) {
			g_object_unref (ps);
			free_bind_param_data (mem_to_free);
                        return NULL;
		}
                sql_for_empty = gda_statement_to_sql (stmt_for_empty, NULL, error);
                g_object_unref (stmt_for_empty);
                if (!sql_for_empty) {
			g_object_unref (ps);
			free_bind_param_data (mem_to_free);
                        return NULL;
		}

		/* This is a re-prepare of the statement.  The function mysql_stmt_prepare
		 * will handle this on the server side. */
		if (mysql_stmt_prepare (ps->mysql_stmt, sql_for_empty, strlen (sql_for_empty))) {
			_gda_mysql_make_error (cdata->cnc, NULL, ps->mysql_stmt, error);
			g_object_unref (ps);
			free_bind_param_data (mem_to_free);
			return NULL;
		}

		/* Execute the 'sql_for_empty' SQL code */
                g_free (sql_for_empty);
	}

	
	GObject *return_value = NULL;
	if (mysql_stmt_execute (ps->mysql_stmt)) {
		event = _gda_mysql_make_error (cnc, NULL, ps->mysql_stmt, error);
	}
	else {
		/* execute prepared statement using C API depending on its kind */
		if (!g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SELECT", 6) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SHOW", 4) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "DESCRIBE", 8) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "EXPLAIN", 7)) {
			
			if (mysql_stmt_store_result (ps->mysql_stmt)) {
				_gda_mysql_make_error (cnc, NULL, ps->mysql_stmt, error);
			}
			else {
				GdaDataModelAccessFlags flags;

				if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
					flags = GDA_DATA_MODEL_ACCESS_RANDOM;
				else
					flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

				return_value = (GObject *) gda_mysql_recordset_new (cnc, ps, params, flags, col_types);
				gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
			}
		}
		else {
			my_ulonglong affected_rows = (my_ulonglong)~0;

			/* Create a #GdaSet containing "IMPACTED_ROWS" */
			/* Create GdaConnectionEvent notice with the type of command and impacted rows */
			affected_rows = mysql_stmt_affected_rows (ps->mysql_stmt);
			if (affected_rows >= 0) {
				GdaConnectionEvent *event;
                                gchar *str;
                                event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
                                str = g_strdup_printf ("%llu", affected_rows);
                                gda_connection_event_set_description (event, str);
                                g_free (str);

                                return_value = (GObject *) gda_set_new_inline
                                        (1, "IMPACTED_ROWS", G_TYPE_INT, (int) affected_rows);

				gda_connection_internal_statement_executed (cnc, stmt, params, event); /* required: help @cnc keep some stats */
			}
			else {
				return_value = (GObject *) gda_data_model_array_new (0);
			}
		}
	}
	g_object_unref (ps);
	free_bind_param_data (mem_to_free);
	return return_value;
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
static gboolean
gda_mysql_provider_xa_start (GdaServerProvider         *provider,
			     GdaConnection             *cnc, 
			     const GdaXaTransactionId  *xid,
			     GError                   **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * put the XA transaction in the IDLE state: the connection won't accept any more modifications.
 * This state is required by some database providers before actually going to the PREPARED state
 */
static gboolean
gda_mysql_provider_xa_end (GdaServerProvider         *provider,
			   GdaConnection             *cnc, 
			   const GdaXaTransactionId  *xid,
			   GError                   **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_mysql_provider_xa_prepare (GdaServerProvider         *provider,
			       GdaConnection             *cnc, 
			       const GdaXaTransactionId  *xid,
			       GError                   **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * commits the distributed transaction: actually write the prepared data to the database and
 * terminates the XA transaction
 */
static gboolean
gda_mysql_provider_xa_commit (GdaServerProvider         *provider,
			      GdaConnection             *cnc, 
			      const GdaXaTransactionId  *xid,
			      GError                   **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_mysql_provider_xa_rollback (GdaServerProvider         *provider,
				GdaConnection             *cnc, 
				const GdaXaTransactionId  *xid,
				GError                   **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Lists all XA transactions that are in the PREPARED state
 *
 * Returns: a list of GdaXaTransactionId structures, which will be freed by the caller
 */
static GList *
gda_mysql_provider_xa_recover (GdaServerProvider  *provider,
			       GdaConnection      *cnc,
			       GError            **error)
{
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

static gchar *
identifier_add_quotes (const gchar *str)
{
        gchar *retval, *rptr;
        const gchar *sptr;
        gint len;

        if (!str)
                return NULL;

        len = strlen (str);
        retval = g_new (gchar, 2*len + 3);
        *retval = '`';
        for (rptr = retval+1, sptr = str; *sptr; sptr++, rptr++) {
                if (*sptr == '`') {
                        *rptr = '\\';
                        rptr++;
                        *rptr = *sptr;
                }
                else
                        *rptr = *sptr;
        }
        *rptr = '`';
        rptr++;
        *rptr = 0;
        return retval;
}

static gboolean
_sql_identifier_needs_quotes (const gchar *str)
{
	const gchar *ptr;

	g_return_val_if_fail (str, FALSE);
	for (ptr = str; *ptr; ptr++) {
		/* quote if 1st char is a number */
		if ((*ptr <= '9') && (*ptr >= '0')) {
			if (ptr == str)
				return TRUE;
			continue;
		}
		if (((*ptr >= 'A') && (*ptr <= 'Z')) ||
		    ((*ptr >= 'a') && (*ptr <= 'z')))
			continue;

		if ((*ptr != '$') && (*ptr != '_') && (*ptr != '#'))
			return TRUE;
	}
	return FALSE;
}

static gchar *
gda_mysql_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
			    const gchar *id,
			    gboolean meta_store_convention, gboolean force_quotes)
{
	GdaSqlReservedKeywordsFunc kwfunc;
	MysqlConnectionData *cdata = NULL;

	if (cnc) {
		cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
		if (!cdata) 
			return NULL;
	}

	kwfunc = _gda_mysql_get_reserved_keyword_func (cdata);
	if (meta_store_convention) {
		/* @id can either:
		 * - be enquoted between double quotes
		 * - not enquoted and in lower case
		 */
		if (*id == '"') {
			/* must be quoted, replace '"' with '`' */
			gchar *ptr, *retval;
			retval = g_strdup (id);
			*retval = '`';
			for (ptr = retval + 1; *ptr; ptr++) {
				if (*ptr == '"')
					*ptr = '`';
			}
			return retval;
		}
		
		if (kwfunc (id) || _sql_identifier_needs_quotes (id) || force_quotes)
			return identifier_add_quotes (id);

		/* nothing to do */
		return g_strdup (id);
	}
	else {
		if (*id == '`') {
			/* there are already some quotes */
			return g_strdup (id);
		}
		if (kwfunc (id) || _sql_identifier_needs_quotes (id) || force_quotes)
			return identifier_add_quotes (id);
		
		/* nothing to do */
		return g_strdup (id);
	}
}

/*
 * Free connection's specific data
 */
static void
gda_mysql_free_cnc_data (MysqlConnectionData  *cdata)
{
	if (!cdata)
		return;

	if (cdata->mysql) {
		mysql_close (cdata->mysql);
		cdata->mysql = NULL;
	}

	g_free (cdata->version);
	g_free (cdata->short_version);
	
	g_free (cdata);
}
