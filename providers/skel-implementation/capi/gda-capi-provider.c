/*
 * Copyright (C) YEAR The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
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
#include "gda-capi.h"
#include "gda-capi-provider.h"
#include "gda-capi-recordset.h"
#include "gda-capi-ddl.h"
#include "gda-capi-meta.h"
#include <libgda/gda-debug-macros.h>
#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_capi_provider_class_init (GdaCapiProviderClass *klass);
static void gda_capi_provider_init       (GdaCapiProvider *provider);

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_capi_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							      GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_capi_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_capi_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static gchar              *gda_capi_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str);
static gchar              *gda_capi_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str);

static const gchar        *gda_capi_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_capi_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_capi_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperationType type,
							       GdaSet *options, GError **error);
static gchar              *gda_capi_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaServerOperation *op, GError **error);

static gboolean            gda_capi_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								GdaServerOperation *op, GError **error);
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
static GdaWorker          *gda_capi_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static GdaConnection      *gda_capi_provider_create_connection (GdaServerProvider *provider);
static const gchar        *gda_capi_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_capi_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
							       GType g_type, const gchar *dbms_type);

static const gchar*        gda_capi_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								    GType type);
/* statements */
static GdaSqlParser        *gda_capi_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_capi_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
								GdaStatement *stmt, GdaSet *params, 
								GdaStatementSqlFlag flags,
								GSList **params_used, GError **error);
static gchar              *gda_capi_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *id,
							       gboolean for_meta_store, gboolean force_quotes);
static gboolean             gda_capi_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GError **error);
static GObject             *gda_capi_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaStatement *stmt, GdaSet *params,
								 GdaStatementModelUsage model_usage, 
								 GType *col_types, GdaSet **last_inserted_row, GError **error);
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
static GdaStatement **internal_stmt = NULL;

typedef enum {
	INTERNAL_STMT1
} InternalStatementItem;

static gchar *internal_sql[] = {
	"SQL for INTERNAL_STMT1"
};

/*
 * GdaCapiProvider class implementation
 */

GdaServerProviderBase base_functions = {
	gda_capi_provider_get_name,
	gda_capi_provider_get_version,
	gda_capi_provider_get_server_version,
	gda_capi_provider_supports_feature,
	gda_capi_provider_create_worker,
	gda_capi_provider_create_connection,
	gda_capi_provider_create_parser,
	gda_capi_provider_get_data_handler,
	gda_capi_provider_get_default_dbms_type,
	gda_capi_provider_supports_operation,
	gda_capi_provider_create_operation,
	gda_capi_provider_render_operation,
	gda_capi_provider_statement_to_sql,
	gda_capi_provider_identifier_quote,
	gda_capi_statement_rewrite,
	gda_capi_provider_open_connection,
	gda_capi_provider_prepare_connection,
	gda_capi_provider_close_connection,
	gda_capi_provider_escape_string,
	gda_capi_provider_unescape_string,
	gda_capi_provider_perform_operation,
	gda_capi_provider_begin_transaction,
	gda_capi_provider_commit_transaction,
	gda_capi_provider_rollback_transaction,
	gda_capi_provider_add_savepoint,
	gda_capi_provider_rollback_savepoint,
	gda_capi_provider_delete_savepoint,
	gda_capi_provider_statement_prepare,
	gda_capi_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta meta_functions = {
	_gda_capi_meta__info,
	_gda_capi_meta__btypes,
	_gda_capi_meta__udt,
	_gda_capi_meta_udt,
	_gda_capi_meta__udt_cols,
	_gda_capi_meta_udt_cols,
	_gda_capi_meta__enums,
	_gda_capi_meta_enums,
	_gda_capi_meta__domains,
	_gda_capi_meta_domains,
	_gda_capi_meta__constraints_dom,
	_gda_capi_meta_constraints_dom,
	_gda_capi_meta__el_types,
	_gda_capi_meta_el_types,
	_gda_capi_meta__collations,
	_gda_capi_meta_collations,
	_gda_capi_meta__character_sets,
	_gda_capi_meta_character_sets,
	_gda_capi_meta__schemata,
	_gda_capi_meta_schemata,
	_gda_capi_meta__tables_views,
	_gda_capi_meta_tables_views,
	_gda_capi_meta__columns,
	_gda_capi_meta_columns,
	_gda_capi_meta__view_cols,
	_gda_capi_meta_view_cols,
	_gda_capi_meta__constraints_tab,
	_gda_capi_meta_constraints_tab,
	_gda_capi_meta__constraints_ref,
	_gda_capi_meta_constraints_ref,
	_gda_capi_meta__key_columns,
	_gda_capi_meta_key_columns,
	_gda_capi_meta__check_columns,
	_gda_capi_meta_check_columns,
	_gda_capi_meta__triggers,
	_gda_capi_meta_triggers,
	_gda_capi_meta__routines,
	_gda_capi_meta_routines,
	_gda_capi_meta__routine_col,
	_gda_capi_meta_routine_col,
	_gda_capi_meta__routine_par,
	_gda_capi_meta_routine_par,
	_gda_capi_meta__indexes_tab,
        _gda_capi_meta_indexes_tab,
        _gda_capi_meta__index_cols,
        _gda_capi_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderXa xa_functions = {
	gda_capi_provider_xa_start,
	gda_capi_provider_xa_end,
	gda_capi_provider_xa_prepare,
	gda_capi_provider_xa_commit,
	gda_capi_provider_xa_rollback,
	gda_capi_provider_xa_recover,

	NULL, NULL, NULL, NULL, /* padding */
};

typedef struct {
  int dummy;
  /* ADD YOUR DETAILS ABOUT YOUR PROVIDER */
} GdaCapiProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaCapiProvider, gda_capi_provider, GDA_TYPE_SERVER_PROVIDER)

static void
gda_capi_provider_class_init (GdaCapiProviderClass *klass)
{
	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE, (gpointer) &base_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META, (gpointer) &meta_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA, (gpointer) &xa_functions);
}

static void
gda_capi_provider_init (GdaCapiProvider *capi_prv)
{
	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) capi_prv);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
	}

	/* meta data init */
	_gda_capi_provider_meta_init ((GdaServerProvider*) capi_prv);

	/* TO_ADD: any other provider's init should be added here */

}


/*
 * Get provider name request
 */
static const gchar *
gda_capi_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return CAPI_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_capi_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
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
 * Returns: TRUE if no error occurred (and the connection is opened), or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_capi_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				   GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_CAPI_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

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
	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_capi_free_cnc_data);
	TO_IMPLEMENT; /* cdata->... = ... */

	return TRUE;
}

/*
 * Prepare connection request
 *
 * In this function, the extra preparation steps can be done such as setting some connection parameters, etc.
 * If the preparation step failed, then the #GdaServerProvider will call the close_connection() method.
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_capi_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc, GdaQuarkList *params, GdaQuarkList *auth)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CAPI_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

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
	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Escape a string so it can be used in SQL statements
 */
static gchar *
gda_capi_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	TO_IMPLEMENT;
	return NULL;
}

/*
 * Does the reverse of gda_capi_provider_escape_string
 */
static gchar *
gda_capi_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	TO_IMPLEMENT;
	return NULL;
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

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
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
				      GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options)
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
				    GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options,
				    GError **error)
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
  str = g_strdup_printf ("capi_specs_%s", file);
  g_free (file);

	file = g_strdup_printf ("/spec/capi/%s.raw.xml", str);
	g_free (str);
	op = GDA_SERVER_OPERATION (g_object_new (GDA_TYPE_SERVER_OPERATION, "op-type", type,
						 "spec-resource", file, NULL));
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
  str = g_strdup_printf ("capi_specs_%s", file);
  g_free (file);
	
	file = g_strdup_printf ("/spec/mysql/%s.raw.xml", str);
	g_free (str);
	if (!gda_server_operation_is_valid_from_resource (op, file, error)) {
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
gda_capi_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				     G_GNUC_UNUSED const gchar *name,
				     G_GNUC_UNUSED GdaTransactionIsolation level,
				     G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				      G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
					G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				 G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				      G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				    G_GNUC_UNUSED const gchar *name, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
 * Create a #GdaWorker: all the calls to the C API will be made from within the GdaWorker's worker thread. This ensures that
 * each connection is manipulated from only one thread. If the C API does not support several threads each manipulating its
 * own connection, then this function should return a single GdaWorker for any request (using gda_worker_ref()).
 */
static GdaWorker *
gda_capi_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	TO_IMPLEMENT; /* We need to determine if the API is thread safe, 0 for now */

	static GdaWorker *unique_worker = NULL;
	if (0) {
		/* API is thread safe */
		if (for_cnc)
			return gda_worker_new ();
		else
			return gda_worker_new_unique (&unique_worker, TRUE);
	}
	else {
		gboolean onlyone = FALSE; /* if %TRUE then only 1 thread can ever access the API, if %FALSE, only 1 thread can
					   * access the API _at any given time_ (i.e. several threads can access the API but only one after
					   * another and never simultaneously) */
                return gda_worker_new_unique (&unique_worker, !onlyone);
	}
}

/*
 * For providers which require a specific GdaConnection object, seldom used.
 */
static GdaConnection *
gda_capi_provider_create_connection (GdaServerProvider *provider)
{
	TO_IMPLEMENT;
	return NULL;
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
		 (type == G_TYPE_DATE_TIME) ||
		 (type == G_TYPE_DATE)) {
		TO_IMPLEMENT; /* define data handlers for these types */
		dh = NULL;
	}
	else
		dh = gda_server_provider_handler_use_default (provider, type);

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
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
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
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == G_TYPE_DATE_TIME) ||
	    (type == G_TYPE_GTYPE))
		return "string";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "real";
	
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == G_TYPE_DATE_TIME)
		return "timestamp";
	if (type == G_TYPE_DATE)
		return "date";

	if ((type == GDA_TYPE_NULL) ||
	    (type == G_TYPE_GTYPE))
		return NULL;

	return "text";
}

/*
 * Create parser request
 *
 * This method is responsible for creating a #GdaSqlParser object specific to the SQL dialect used
 * by the database. See the PostgreSQL provider implementation for an example.
 */
static GdaSqlParser *
gda_capi_provider_create_parser (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc)
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
 * NOTE: This implementation can call gda_statement_to_sql_extended() _ONLY_ if it passes a %NULL @cnc argument
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

	TO_IMPLEMENT;

	return gda_statement_to_sql_extended (stmt, NULL, params, flags, params_used, error);
}

/*
 * Function to quote identifiers to be used safely in SQL statements
 */
static gchar *
gda_capi_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
				    const gchar *id,
				    gboolean for_meta_store, gboolean force_quotes)
{
	TO_IMPLEMENT;
	return NULL;
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
                                g_slist_free_full (param_ids, (GDestroyNotify) g_free);
                                goto out;
                        }
                }
        }
	
	/* create a prepared statement object */
	/*ps = gda_capi_pstmt_new (...);*/
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
       gda_pstmt_set_param_ids ( _GDA_PSTMT (ps), param_ids);
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
gda_capi_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GdaSet *params,
				     GdaStatementModelUsage model_usage, 
				     GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaCapiPStmt *ps;
	CapiConnectionData *cdata;
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
	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
                                TO_IMPLEMENT;
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
				TO_IMPLEMENT;
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
			res = gda_capi_provider_statement_execute (provider, cnc,
								   rstmt, params,
								   model_usage,
								   col_types, last_inserted_row,
								   error);
			g_object_unref (rstmt);
			return res;
		}


		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		if (!value || gda_value_is_null (value)) {
			GdaStatement *rstmt;
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error)) {
				TO_IMPLEMENT; /* bind to NULL */
			}
			else if (!rstmt)
				return NULL;
			else {
				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaCapiPStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_capi_provider_statement_prepare (provider, cnc,
									  rstmt, error))
					return NULL;
				tps = (GdaCapiPStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gda_pstmt_get_param_ids (gtps);
				gda_pstmt_set_param_ids (gtps, NULL);
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids = gda_pstmt_get_param_ids (gtps);
				gda_pstmt_set_param_ids (gtps, prep_param_ids);

				/* execute */
				obj = gda_capi_provider_statement_execute (provider, cnc,
									   rstmt, params,
									   model_usage,
									   col_types,
									   last_inserted_row,
									   error);
				/* clear original @param_ids and restore copied one */
				g_slist_free_full (prep_param_ids, (GDestroyNotify) g_free);

				gda_pstmt_set_param_ids (gtps, copied_param_ids);

				/*if (GDA_IS_DATA_MODEL (obj))
				  gda_data_model_dump ((GdaDataModel*) obj, NULL);*/

				g_object_unref (rstmt);
				return obj;
			}
		}
		else {
			/* usually the way to bind parameters is different depending on the type of @value.
			 * Also, if the database engine does not support storing timezone information for time and
			 * timestamp values, then before binding, the value must be converted to GMT using
			 * gda_time_change_timezone (xxx, 0) or gda_timestamp_change_timezone (xxx, 0)
			 */
			TO_IMPLEMENT;
		}
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
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

		TO_IMPLEMENT;
	}
	else {
		/* Execute the _GDA_PSTMT (ps)->sql SQL code */
		TO_IMPLEMENT;
	}
	
	/* execute prepared statement using C API depending on its kind */
	if (! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "SELECT", 6) ||
            ! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "EXPLAIN", 7)) {
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
				const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
			      const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				  const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				 const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				   const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
				  G_GNUC_UNUSED GError **error)
{
	CapiConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
