/* GDA Capi provider
 * Copyright (C) 2008 - 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
#include "gda-capi.h"
#include "gda-capi-provider.h"
#include "gda-capi-recordset.h"
#include "gda-capi-ddl.h"
#include "gda-capi-meta.h"
#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_capi_provider_class_init (GdaCapiProviderClass *klass);
static void gda_capi_provider_init       (GdaCapiProvider *provider,
					  GdaCapiProviderClass *klass);
static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_capi_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaQuarkList *params, GdaQuarkList *auth,
							      guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data);
static gboolean            gda_capi_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_capi_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_capi_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_capi_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_capi_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperationType type,
							       GdaSet *options, GError **error);
static gchar              *gda_capi_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperation *op, GError **error);

static gboolean            gda_capi_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								GdaServerOperation *op, guint *task_id, 
								GdaServerProviderAsyncCallback async_cb, gpointer cb_data,
								GError **error);
/* transactions */
static gboolean            gda_capi_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_capi_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_capi_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								   const gchar *name, GError **error);
static gboolean            gda_capi_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							    const gchar *name, GError **error);
static gboolean            gda_capi_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);
static gboolean            gda_capi_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_capi_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_capi_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

static const gchar        *gda_capi_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_capi_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
							       GType g_type, const gchar *dbms_type);

static const gchar*        gda_capi_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								    GType type);
/* statements */
static GdaSqlParser        *gda_capi_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_capi_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params, 
								 GdaStatementSqlFlag flags,
								 GSList **params_used, GError **error);
static gboolean             gda_capi_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GError **error);
static GObject             *gda_capi_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params,
								 GdaStatementModelUsage model_usage, 
								 GType *col_types, GdaSet **last_inserted_row, 
								 guint *task_id, GdaServerProviderExecCallback async_cb, 
								 gpointer cb_data, GError **error);
static GdaSqlStatement     *gda_capi_statement_rewrite          (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params, GError **error);


/* distributed transactions */
static gboolean gda_capi_provider_xa_start    (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_capi_provider_xa_end      (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_capi_provider_xa_prepare  (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_capi_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_capi_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
						   const GdaXaTransactionId *xid, GError **error);

static GList   *gda_capi_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc, 
						   GError **error);

/* 
 * private connection data destroy 
 */
static void gda_capi_free_cnc_data (CapiConnectionData *cdata);


/*
 * Prepared internal statements
 * TO_ADD: any prepared statement to be used internally by the provider should be
 *         declared here, as constants and as SQL statements
 */
static GdaStatement **internal_stmt;

typedef enum {
	INTERNAL_STMT1
} InternalStatementItem;

static gchar *internal_sql[] = {
	"SQL for INTERNAL_STMT1"
};

/*
 * GdaCapiProvider class implementation
 */
static void
gda_capi_provider_class_init (GdaCapiProviderClass *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	provider_class->get_version = gda_capi_provider_get_version;
	provider_class->get_server_version = gda_capi_provider_get_server_version;
	provider_class->get_name = gda_capi_provider_get_name;
	provider_class->supports_feature = gda_capi_provider_supports_feature;

	provider_class->get_data_handler = gda_capi_provider_get_data_handler;
	provider_class->get_def_dbms_type = gda_capi_provider_get_default_dbms_type;

	provider_class->open_connection = gda_capi_provider_open_connection;
	provider_class->close_connection = gda_capi_provider_close_connection;
	provider_class->get_database = gda_capi_provider_get_database;

	provider_class->supports_operation = gda_capi_provider_supports_operation;
        provider_class->create_operation = gda_capi_provider_create_operation;
        provider_class->render_operation = gda_capi_provider_render_operation;
        provider_class->perform_operation = gda_capi_provider_perform_operation;

	provider_class->begin_transaction = gda_capi_provider_begin_transaction;
	provider_class->commit_transaction = gda_capi_provider_commit_transaction;
	provider_class->rollback_transaction = gda_capi_provider_rollback_transaction;
	provider_class->add_savepoint = gda_capi_provider_add_savepoint;
        provider_class->rollback_savepoint = gda_capi_provider_rollback_savepoint;
        provider_class->delete_savepoint = gda_capi_provider_delete_savepoint;

	provider_class->create_parser = gda_capi_provider_create_parser;
	provider_class->statement_to_sql = NULL; /* don't use gda_capi_provider_statement_to_sql()
						  * because it only calls gda_statement_to_sql_extended().
						  * Set it to gda_capi_provider_statement_to_sql() if it does
						  * not call calls gda_statement_to_sql_extended() */
	provider_class->statement_prepare = gda_capi_provider_statement_prepare;
	provider_class->statement_execute = gda_capi_provider_statement_execute;
	provider_class->statement_rewrite = gda_capi_statement_rewrite;

	provider_class->is_busy = NULL;
	provider_class->cancel = NULL;
	provider_class->create_connection = NULL;

	memset (&(provider_class->meta_funcs), 0, sizeof (GdaServerProviderMeta));
	provider_class->meta_funcs._info = _gda_capi_meta__info;
	provider_class->meta_funcs._btypes = _gda_capi_meta__btypes;
	provider_class->meta_funcs._udt = _gda_capi_meta__udt;
	provider_class->meta_funcs.udt = _gda_capi_meta_udt;
	provider_class->meta_funcs._udt_cols = _gda_capi_meta__udt_cols;
	provider_class->meta_funcs.udt_cols = _gda_capi_meta_udt_cols;
	provider_class->meta_funcs._enums = _gda_capi_meta__enums;
	provider_class->meta_funcs.enums = _gda_capi_meta_enums;
	provider_class->meta_funcs._domains = _gda_capi_meta__domains;
	provider_class->meta_funcs.domains = _gda_capi_meta_domains;
	provider_class->meta_funcs._constraints_dom = _gda_capi_meta__constraints_dom;
	provider_class->meta_funcs.constraints_dom = _gda_capi_meta_constraints_dom;
	provider_class->meta_funcs._el_types = _gda_capi_meta__el_types;
	provider_class->meta_funcs.el_types = _gda_capi_meta_el_types;
	provider_class->meta_funcs._collations = _gda_capi_meta__collations;
	provider_class->meta_funcs.collations = _gda_capi_meta_collations;
	provider_class->meta_funcs._character_sets = _gda_capi_meta__character_sets;
	provider_class->meta_funcs.character_sets = _gda_capi_meta_character_sets;
	provider_class->meta_funcs._schemata = _gda_capi_meta__schemata;
	provider_class->meta_funcs.schemata = _gda_capi_meta_schemata;
	provider_class->meta_funcs._tables_views = _gda_capi_meta__tables_views;
	provider_class->meta_funcs.tables_views = _gda_capi_meta_tables_views;
	provider_class->meta_funcs._columns = _gda_capi_meta__columns;
	provider_class->meta_funcs.columns = _gda_capi_meta_columns;
	provider_class->meta_funcs._view_cols = _gda_capi_meta__view_cols;
	provider_class->meta_funcs.view_cols = _gda_capi_meta_view_cols;
	provider_class->meta_funcs._constraints_tab = _gda_capi_meta__constraints_tab;
	provider_class->meta_funcs.constraints_tab = _gda_capi_meta_constraints_tab;
	provider_class->meta_funcs._constraints_ref = _gda_capi_meta__constraints_ref;
	provider_class->meta_funcs.constraints_ref = _gda_capi_meta_constraints_ref;
	provider_class->meta_funcs._key_columns = _gda_capi_meta__key_columns;
	provider_class->meta_funcs.key_columns = _gda_capi_meta_key_columns;
	provider_class->meta_funcs._check_columns = _gda_capi_meta__check_columns;
	provider_class->meta_funcs.check_columns = _gda_capi_meta_check_columns;
	provider_class->meta_funcs._triggers = _gda_capi_meta__triggers;
	provider_class->meta_funcs.triggers = _gda_capi_meta_triggers;
	provider_class->meta_funcs._routines = _gda_capi_meta__routines;
	provider_class->meta_funcs.routines = _gda_capi_meta_routines;
	provider_class->meta_funcs._routine_col = _gda_capi_meta__routine_col;
	provider_class->meta_funcs.routine_col = _gda_capi_meta_routine_col;
	provider_class->meta_funcs._routine_par = _gda_capi_meta__routine_par;
	provider_class->meta_funcs.routine_par = _gda_capi_meta_routine_par;
	provider_class->meta_funcs._indexes_tab = _gda_capi_meta__indexes_tab;
        provider_class->meta_funcs.indexes_tab = _gda_capi_meta_indexes_tab;
        provider_class->meta_funcs._index_cols = _gda_capi_meta__index_cols;
        provider_class->meta_funcs.index_cols = _gda_capi_meta_index_cols;

	/* distributed transactions: if not supported, then provider_class->xa_funcs should be set to NULL */
	provider_class->xa_funcs = g_new0 (GdaServerProviderXa, 1);
	provider_class->xa_funcs->xa_start = gda_capi_provider_xa_start;
	provider_class->xa_funcs->xa_end = gda_capi_provider_xa_end;
	provider_class->xa_funcs->xa_prepare = gda_capi_provider_xa_prepare;
	provider_class->xa_funcs->xa_commit = gda_capi_provider_xa_commit;
	provider_class->xa_funcs->xa_rollback = gda_capi_provider_xa_rollback;
	provider_class->xa_funcs->xa_recover = gda_capi_provider_xa_recover;
}

static void
gda_capi_provider_init (GdaCapiProvider *capi_prv, GdaCapiProviderClass *klass)
{
	InternalStatementItem i;
	GdaSqlParser *parser;

	parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) capi_prv);
	internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
	for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
		if (!internal_stmt[i]) 
			g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
	}

	/* meta data init */
	_gda_capi_provider_meta_init ((GdaServerProvider*) capi_prv);

	/* TO_ADD: any other provider's init should be added here */
}

GType
gda_capi_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static GTypeInfo info = {
			sizeof (GdaCapiProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_capi_provider_class_init,
			NULL, NULL,
			sizeof (GdaCapiProvider),
			0,
			(GInstanceInitFunc) gda_capi_provider_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaCapiProvider", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}


/*
 * Get provider name request
 */
static const gchar *
gda_capi_provider_get_name (GdaServerProvider *provider)
{
	return CAPI_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_capi_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a CapiConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_capi_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				   GdaQuarkList *params, GdaQuarkList *auth,
				   guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data)
{
	g_return_val_if_fail (GDA_IS_CAPI_PROVIDER (provider), FALSE);
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
	
	/* open the real connection to the database */
	/* TO_ADD: C API specific function calls;
	 * if it fails, add a connection event and return FALSE */
	TO_IMPLEMENT;

	/* Create a new instance of the provider specific data associated to a connection (CapiConnectionData),
	 * and set its contents */
	CapiConnectionData *cdata;
	cdata = g_new0 (CapiConnectionData, 1);
	gda_connection_internal_set_provider_data (cnc, cdata, (GDestroyNotify) gda_capi_free_cnc_data);
	TO_IMPLEMENT; /* cdata->... = ... */

	/* Optionnally set some attributes for the newly opened connection (encoding to UTF-8 for example )*/
	TO_IMPLEMENT;

	return TRUE;
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated CapiConnectionData structure
 *   - Free the CapiConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_capi_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	TO_IMPLEMENT;

	/* Free the CapiConnectionData structure and its contents*/
	gda_capi_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated CapiConnectionData structure
 */
static const gchar *
gda_capi_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;
	TO_IMPLEMENT;
	return NULL;
}

/*
 * Get database request
 *
 * Returns the database name as a string, which should be stored in @cnc's associated CapiConnectionData structure
 */
static const gchar *
gda_capi_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;
	TO_IMPLEMENT;
	return NULL;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a capi_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_capi_provider_render_operation() and gda_capi_provider_perform_operation() methods
 *
 * In this example, the GDA_SERVER_OPERATION_CREATE_TABLE is implemented
 */
static gboolean
gda_capi_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperationType type, GdaSet *options)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
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
 * if some further initialization is required, or if operation's contents is dependent on @cnc
 */
static GdaServerOperation *
gda_capi_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaServerOperationType type, GdaSet *options, GError **error)
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
        str = g_strdup_printf ("capi_specs_%s.xml", file);
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
gda_capi_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaServerOperation *op, GError **error)
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
        str = g_strdup_printf ("capi_specs_%s.xml", file);
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
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = NULL;
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_capi_render_CREATE_TABLE (provider, cnc, op, error);
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
gda_capi_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaServerOperation *op, guint *task_id, 
				     GdaServerProviderAsyncCallback async_cb, gpointer cb_data, GError **error)
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
gda_capi_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				     const gchar *name, GdaTransactionIsolation level,
				     GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_capi_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_capi_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_capi_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				 const gchar *name, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_capi_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_capi_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				    const gchar *name, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_capi_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
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
gda_capi_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
				    GType type, const gchar *dbms_type)
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
gda_capi_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
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
gda_capi_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	TO_IMPLEMENT;
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
 * NOTE: This implementation MUST NOT call gda_statement_to_sql_extended() if it is
 *       the GdaServerProvider::statement_to_sql() virtual method's implementation
 */
static gchar *
gda_capi_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				    GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				    GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	return gda_statement_to_sql_extended (stmt, cnc, params, flags, params_used, error);
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaCapiPStmt object and declare it to @cnc.
 */
static gboolean
gda_capi_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GError **error)
{
	GdaCapiPStmt *ps;
	gboolean retval = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaCapiPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* render as SQL understood by the provider */
	GdaSet *params = NULL;
	gchar *sql;
	GSList *used_params = NULL;
	if (! gda_statement_get_parameters (stmt, &params, error))
                return FALSE;
        sql = gda_capi_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_UQMARK,
						  &used_params, error);
        if (!sql) 
		goto out;

	/* prepare @stmt using the C API, creates @ps */
	TO_IMPLEMENT;

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
                                g_slist_foreach (param_ids, (GFunc) g_free, NULL);
                                g_slist_free (param_ids);
                                goto out;
                        }
                }
        }
	
	/* create a prepared statement object */
	/*ps = gda_capi_pstmt_new (...);*/
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
        _GDA_PSTMT (ps)->param_ids = param_ids;
        _GDA_PSTMT (ps)->sql = sql;

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
gda_capi_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GdaSet *params,
				     GdaStatementModelUsage model_usage, 
				     GType *col_types, GdaSet **last_inserted_row, 
				     guint *task_id, 
				     GdaServerProviderExecCallback async_cb, gpointer cb_data, GError **error)
{
	GdaCapiPStmt *ps;
	CapiConnectionData *cdata;
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
                return NULL;
	}

        allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
                (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);
	
        if (last_inserted_row)
                *last_inserted_row = NULL;

	/* Get private data */
	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;


	/* get/create new prepared statement */
	ps = (GdaCapiPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_capi_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaCapiPStmt object.
			 *
			 * Don't call gda_connection_add_prepared_statement() with this new prepared statement
			 * as it will be destroyed once used.
			 */
			TO_IMPLEMENT;
			return NULL;
		}
		else {
			ps = (GdaCapiPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
			g_object_ref (ps);
		}
	}
	else
		g_object_ref (ps);
	g_assert (ps);

	/* optionnally reset the prepared statement if required by the API */
	TO_IMPLEMENT;
	
	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	for (i = 1, list = _GDA_PSTMT (ps)->param_ids; list; list = list->next, i++) {
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
			if (! allow_noparam) {
				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
			else {
                                /* bind param to NULL */
                                TO_IMPLEMENT;
                                empty_rs = TRUE;
                                continue;
                        }

		}
		if (!gda_holder_is_valid (h)) {
			if (! allow_noparam) {
				gchar *str;
				str = g_strdup_printf (_("Parameter '%s' is invalid"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
			else {
                                /* bind param to NULL */
				TO_IMPLEMENT;
                                empty_rs = TRUE;
                                continue;
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
			res = gda_capi_provider_statement_execute (provider, cnc,
								   rstmt, params,
								   model_usage,
								   col_types, last_inserted_row,
								   task_id,
								   async_cb, cb_data, error);
			g_object_unref (rstmt);
			return res;
		}


		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		TO_IMPLEMENT;
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
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

		TO_IMPLEMENT;
	}
	else {
		/* Execute the _GDA_PSTMT (ps)->sql SQL code */
		TO_IMPLEMENT;
	}
	
	/* execute prepared statement using C API depending on its kind */
	if (! g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SELECT", 6) ||
            ! g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "EXPLAIN", 7)) {
		GObject *data_model;
		GdaDataModelAccessFlags flags;

		if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

                data_model = (GObject *) gda_capi_recordset_new (cnc, ps, params, flags, col_types);
		gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
		g_object_unref (ps);
		return data_model;
        }
	else {
		GdaSet *set = NULL;

		TO_IMPLEMENT;
                /* Create a #GdaSet containing "IMPACTED_ROWS" */
		/* Create GdaConnectionEvent notice with the type of command and impacted rows */

		gda_connection_internal_statement_executed (cnc, stmt, params, event); /* required: help @cnc keep some stats */
		g_object_unref (ps);
		return (GObject*) set;
	}
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Usually it uses the DEFAULT keyword or removes any default value inserted or updated, see
 * gda_statement_rewrite_for_default_values()
 */
static GdaSqlStatement *
gda_capi_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
			    GdaStatement *stmt, GdaSet *params, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}
	return gda_statement_rewrite_for_default_values (stmt, params, TRUE, error);
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
static gboolean
gda_capi_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc, 
				const GdaXaTransactionId *xid, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
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
gda_capi_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc, 
			      const GdaXaTransactionId *xid, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_capi_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc, 
				  const GdaXaTransactionId *xid, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
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
gda_capi_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc, 
				 const GdaXaTransactionId *xid, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_capi_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
				   const GdaXaTransactionId *xid, GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
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
gda_capi_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				  GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata) 
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

/*
 * Free connection's specific data
 */
static void
gda_capi_free_cnc_data (CapiConnectionData *cdata)
{
	if (!cdata)
		return;

	TO_IMPLEMENT;
	g_free (cdata);
}
