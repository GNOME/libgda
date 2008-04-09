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
								  GdaServerProviderAsyncCallback   async_cb, 
								  gpointer                         cb_data,
								  GError                         **error);

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
	g_print ("*** %s\n", __func__);
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
        provider_class->meta_funcs.el_types = _gda_mysql_meta_el_types;
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
}

static void
gda_mysql_provider_init (GdaMysqlProvider       *mysql_prv,
			 GdaMysqlProviderClass  *klass)
{
	g_print ("*** %s\n", __func__);
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
		type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaMysqlProvider",
					       &info, 0);
	}

	return type;
}


/*
 * Get provider name request
 */
static const gchar *
gda_mysql_provider_get_name (GdaServerProvider  *provider)
{
	g_print ("*** %s\n", __func__);
	return MYSQL_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_mysql_provider_get_version (GdaServerProvider  *provider)
{
	g_print ("*** %s\n", __func__);
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
		      GError      **error)
{
	g_print ("*** %s\n", __func__);
	unsigned int flags = 0;

	/* Exclusive: host/pair otherwise unix socket. */
	if ((host || port >= 0) && socket) {
		g_set_error (error, 0, 0,
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
		g_set_error (error, 0, 0, mysql_error (mysql));
		g_free (mysql);
		mysql = NULL;
	}

	/* Optionnally set some attributes for the newly opened connection (encoding to UTF-8 for example )*/

#if MYSQL_VERSION_ID < 32200
	if (mysql &&
	    mysql_select_db (mysql, db) != 0) {
		g_set_error (error, 0, 0, mysql_error (mysql));
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
	g_print ("*** %s\n", __func__);
	g_return_val_if_fail (mysql != NULL, NULL);
	unsigned long version_long;
	version_long = mysql_get_server_version (mysql);
	return g_strdup_printf ("%lu.%lu.%lu",
				version_long/10000,
				(version_long%10000)/100,
				(version_long%100));
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
	g_print ("*** %s\n", __func__);
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
	user = gda_quark_list_find (params, "USER");
	password = gda_quark_list_find (params, "PASSWORD");

	const gchar *port, *unix_socket, *use_ssl;
	port = gda_quark_list_find (params, "PORT");
	unix_socket = gda_quark_list_find (params, "UNIX_SOCKET");
	use_ssl = gda_quark_list_find (params, "USE_SSL");
	
	
	/* open the real connection to the database */
	/* TO_ADD: C API specific function calls;
	 * if it fails, add a connection event and return FALSE */
	// TO_IMPLEMENT;
	
	GError *error = NULL;
	MYSQL *mysql = real_open_connection (host, (port != NULL) ? atoi (port) : 0,
					     unix_socket, db_name,
					     user, password,
					     (use_ssl != NULL) ? TRUE : FALSE,
					     &error);
	if (!mysql) {
		_gda_mysql_make_error (cnc, mysql, NULL);
		return FALSE;
	}

	MYSQL_STMT *mysql_stmt = mysql_stmt_init (mysql);
	if (!mysql_stmt) {
		_gda_mysql_make_error (cnc, mysql, NULL);
		return FALSE;
	}

	my_bool update_max_length = 1;
	if (mysql_stmt_attr_set (mysql_stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (const void *) &update_max_length)) {
		_gda_mysql_make_error (cnc, mysql, NULL);
		return FALSE;
	}
	

	/* Create a new instance of the provider specific data associated to a connection (MysqlConnectionData),
	 * and set its contents */
	MysqlConnectionData *cdata;
	cdata = g_new0 (MysqlConnectionData, 1);
	gda_connection_internal_set_provider_data (cnc, cdata, (GDestroyNotify) gda_mysql_free_cnc_data);
	// TO_IMPLEMENT; /* cdata->... = ... */
	
	cdata->cnc = cnc;
	cdata->mysql = mysql;
	cdata->mysql_stmt = mysql_stmt;

	cdata->version_long = mysql_get_server_version (mysql);
	cdata->version = get_mysql_version (mysql);
	

	/* Optionnally set some attributes for the newly opened connection (encoding to UTF-8 for example )*/
	// TO_IMPLEMENT;

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);

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
	g_print ("*** %s\n", __func__);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	}

        switch (type) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
		return FALSE;

        case GDA_SERVER_OPERATION_CREATE_TABLE:
		return TRUE;
        case GDA_SERVER_OPERATION_DROP_TABLE:
        case GDA_SERVER_OPERATION_RENAME_TABLE:

        case GDA_SERVER_OPERATION_ADD_COLUMN:

        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:

        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:
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
	g_print ("*** %s\n", __func__);
        gchar *file;
        GdaServerOperation *op;
        gchar *str;
	gchar *dir;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	}

        file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
        str = g_strdup_printf ("mysql_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
                return NULL;
        }

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
	g_print ("*** %s\n", __func__);
        gchar *sql = NULL;
        gchar *file;
        gchar *str;
	gchar *dir;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	}

	/* test @op's validity */
        file = g_utf8_strdown (gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)), -1);
        str = g_strdup_printf ("mysql_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

        if (! file) {
                g_set_error (error, 0, 0, _("Missing spec. file '%s'"), file);
                return NULL;
        }
        if (!gda_server_operation_is_valid (op, file, error)) {
                g_free (file);
                return NULL;
        }
        g_free (file);

	/* actual rendering */
        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = NULL;
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_mysql_render_CREATE_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_TABLE:
        case GDA_SERVER_OPERATION_RENAME_TABLE:
        case GDA_SERVER_OPERATION_ADD_COLUMN:
        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:
        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:
                sql = NULL;
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
	g_print ("*** %s\n", __func__);
        GdaServerOperationType optype;

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     _("Provider does not support asynchronous server operation"));
                return FALSE;
	}

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);

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
	g_print ("*** %s\n", __func__);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
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
	g_print ("*** %s\n", __func__);
	GdaDataHandler *dh;
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
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
	g_print ("*** %s\n", __func__);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
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
	    (type == G_TYPE_INVALID))
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
	g_print ("*** %s\n", __func__);
	// TO_IMPLEMENT;
	return (GdaSqlParser *) g_object_new (GDA_TYPE_MYSQL_PARSER,
					      "tokenizer-flavour", GDA_SQL_PARSER_FLAVOUR_MYSQL,
					      NULL);
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
static gchar *
gda_mysql_provider_statement_to_sql (GdaServerProvider    *provider,
				     GdaConnection        *cnc,
				     GdaStatement         *stmt,
				     GdaSet               *params,
				     GdaStatementSqlFlag   flags,
				     GSList              **params_used,
				     GError              **error)
{
	g_print ("*** %s\n", __func__);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	}

	return gda_statement_to_sql_extended (stmt, cnc, params, flags, params_used, error);
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
	g_print ("*** %s\n", __func__);
	GdaMysqlPStmt *ps;
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* prepare @stmt using the C API, creates @ps */
	// TO_IMPLEMENT;
	

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	/* Render as SQL understood by Mysql. */
	GdaSet *set;
	if (!gda_statement_get_parameters (stmt, &set, error))
		return FALSE;

	GSList *used_set = NULL;
	gchar *sql = gda_mysql_provider_statement_to_sql
		(provider, cnc, stmt, set,
		 GDA_STATEMENT_SQL_PARAMS_AS_QMARK, &used_set, error);

	
	if (!sql)
		goto cleanup;

	if (mysql_stmt_prepare (cdata->mysql_stmt, sql, strlen (sql))) {
		GdaConnectionEvent *event = _gda_mysql_make_error
			(cdata->cnc, cdata->mysql, NULL);
		goto cleanup;
	}

	GSList *param_ids = NULL, *current;
	for (current = used_set; current; current = current->next) {
		const gchar *id = gda_holder_get_id
			(GDA_HOLDER(current->data));
		if (id) {
			param_ids = g_slist_append (param_ids,
						    g_strdup (id));
			g_print ("MYSQL preparation: param id=%s\n", id);
		} else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
				     _("Unnamed holder is not allowed in prepared statement."));
			g_slist_foreach (param_ids, (GFunc) g_free, NULL);
			g_slist_free (param_ids);
			param_ids = NULL;

			goto cleanup;
		}
	}

	/* Create prepared statement. */
	ps = gda_mysql_pstmt_new (cnc, cdata->mysql, cdata->mysql_stmt);
	
	if (!ps)
		return FALSE;
	else {
		
		gda_pstmt_set_gda_statement (_GDA_PSTMT(ps), stmt);
		_GDA_PSTMT(ps)->param_ids = param_ids;
		_GDA_PSTMT(ps)->sql = sql;
		
		gda_connection_add_prepared_statement (cnc, stmt, ps);
		return TRUE;
	}
	
 cleanup:

	if (set)
		g_object_unref (G_OBJECT(set));
	if (used_set)
		g_slist_free (used_set);
	g_free (sql);
	
	return FALSE;
}


static GdaMysqlPStmt *
prepare_stmt_simple (MysqlConnectionData  *cdata,
		     const gchar          *sql,
		     GError              **error)
{
	GdaMysqlPStmt *ps = NULL;
	g_return_val_if_fail (sql != NULL, NULL);
	
	if (mysql_stmt_prepare (cdata->mysql_stmt, sql, strlen (sql))) {
		GdaConnectionEvent *event = _gda_mysql_make_error
			(cdata->cnc, cdata->mysql, NULL);
		ps = NULL;
	} else {
		ps = gda_mysql_pstmt_new (cdata->cnc, cdata->mysql, cdata->mysql_stmt);
		_GDA_PSTMT(ps)->param_ids = NULL;
		_GDA_PSTMT(ps)->sql = g_strdup (sql);
	}
	
	return ps;
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
				      GdaServerProviderAsyncCallback   async_cb,
				      gpointer                         cb_data,
				      GError                         **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlPStmt *ps;
	MysqlConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	/* If asynchronous connection opening is not supported, then exit now */
	if (async_cb) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     _("Provider does not support asynchronous statement execution"));
                return FALSE;
	}

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;


	/* get/create new prepared statement */
	ps = gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_mysql_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaMysqlPStmt object.
			 */
			// TO_IMPLEMENT;
			
			gchar *sql = gda_mysql_provider_statement_to_sql
				(provider, cnc, stmt, params, 0, NULL, error);
			if (!sql)
				return NULL;
			ps = prepare_stmt_simple (cdata, sql, error);
			if (!ps)
				return NULL;
			
		}
		else
			ps = gda_connection_get_prepared_statement (cnc, stmt);
	}
	g_assert (ps);

	/* optionnally reset the prepared statement if required by the API */
	// TO_IMPLEMENT;
	
	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	
	gint nb_params = g_slist_length (_GDA_PSTMT (ps)->param_ids);
	char **param_values = g_new0 (char *, nb_params + 1);
        int *param_lengths = g_new0 (int, nb_params + 1);
        int *param_formats = g_new0 (int, nb_params + 1);
	g_print ("NB=%d\n", nb_params);
	
	for (i = 1, list = _GDA_PSTMT (ps)->param_ids; list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h;

		
		g_print ("PNAME=%s\n", pname);
		//		
		/* find requested parameter */
		if (!params) {
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     _("Missing parameter(s) to execute query"));
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
			gchar *str;
			str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, str);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, str);
			g_free (str);
			break;
		}

		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		TO_IMPLEMENT;
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
        gda_connection_add_event (cnc, event);

	
	GObject *return_value = NULL;
	if (mysql_stmt_execute (cdata->mysql_stmt)) {
		event = _gda_mysql_make_error (cnc, cdata->mysql, error);
	} else {
		//	
		/* execute prepared statement using C API depending on its kind */
		if (!g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SELECT", 6) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SHOW", 4) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "DESCRIBE", 8) ||
		    !g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "EXPLAIN", 7)) {
			
			if (mysql_stmt_store_result (cdata->mysql_stmt)) {
				_gda_mysql_make_error (cnc, cdata->mysql, error);
			} else {
				GdaDataModelAccessFlags flags;

				if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
					flags = GDA_DATA_MODEL_ACCESS_RANDOM;
				else
					flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

				return_value = (GObject *) gda_mysql_recordset_new (cnc, ps, flags, col_types);
				gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
			}
			
		} else {
			my_ulonglong affected_rows = (my_ulonglong)~0;

			// TO_IMPLEMENT;
			/* Create a #GdaSet containing "IMPACTED_ROWS" */
			/* Create GdaConnectionEvent notice with the type of command and impacted rows */

			
			affected_rows = mysql_stmt_affected_rows (cdata->mysql_stmt);
			if (affected_rows >= 0) {
				GdaConnectionEvent *event;
                                gchar *str;
                                event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
                                str = g_strdup_printf ("%lu", affected_rows);
                                gda_connection_event_set_description (event, str);
                                g_free (str);

                                return_value = (GObject *) gda_set_new_inline
                                        (1, "IMPACTED_ROWS", G_TYPE_INT, (int) affected_rows);

				gda_connection_internal_statement_executed (cnc, stmt, params, event); /* required: help @cnc keep some stats */
			} else {
				return_value = (GObject *) gda_data_model_array_new (0);
			}
			
		}
		
	}
	return return_value;
	
}

/*
 * Free connection's specific data
 */
static void
gda_mysql_free_cnc_data (MysqlConnectionData  *cdata)
{
	g_print ("*** %s\n", __func__);
	if (!cdata)
		return;

	// TO_IMPLEMENT;
	
	if (cdata->mysql) {
		mysql_close (cdata->mysql);
		cdata->mysql = NULL;
	}
	if (cdata->mysql_stmt) {
		mysql_stmt_close (cdata->mysql_stmt);
		cdata->mysql_stmt = NULL;
	}

	g_free (cdata->version);
	cdata->version_long = -1;
	
	g_free (cdata);
}
