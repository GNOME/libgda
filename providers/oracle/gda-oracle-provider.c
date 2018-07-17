/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Christian Neumair <cneumair@src.gnome.org>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2005 Magnus Bergman <magnus.bergman@observer.net>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#undef GDA_DISABLE_DEPRECATED
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
#include "gda-oracle.h"
#include "gda-oracle-provider.h"
#include "gda-oracle-recordset.h"
#include "gda-oracle-ddl.h"
#include "gda-oracle-meta.h"
#include "gda-oracle-util.h"
#include "gda-oracle-parser.h"
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_oracle_provider_class_init (GdaOracleProviderClass *klass);
static void gda_oracle_provider_init       (GdaOracleProvider *provider,
					    GdaOracleProviderClass *klass);
static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_oracle_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
								GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_oracle_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_oracle_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_oracle_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_oracle_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_oracle_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type,
								 GdaSet *options, GError **error);
static gchar              *gda_oracle_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperation *op, GError **error);

static gboolean            gda_oracle_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								  GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_oracle_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								  const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_oracle_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);
static gboolean            gda_oracle_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								     const gchar *name, GError **error);
static gboolean            gda_oracle_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							      const gchar *name, GError **error);
static gboolean            gda_oracle_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);
static gboolean            gda_oracle_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_oracle_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_oracle_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaConnectionFeature feature);

static GdaWorker          *gda_oracle_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static const gchar        *gda_oracle_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_oracle_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
								 GType g_type, const gchar *dbms_type);

static const gchar*        gda_oracle_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								      GType type);
/* statements */
static GdaSqlParser        *gda_oracle_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_oracle_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params, 
								   GdaStatementSqlFlag flags,
								   GSList **params_used, GError **error);
static gboolean             gda_oracle_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GError **error);
static GObject             *gda_oracle_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params,
								   GdaStatementModelUsage model_usage, 
								   GType *col_types, GdaSet **last_inserted_row, GError **error);
static GdaSqlStatement     *gda_oracle_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params, GError **error);

/* Quoting */
static gchar               *gda_oracle_provider_identifier_quote    (GdaServerProvider *provider, GdaConnection *cnc,
								     const gchar *id,
								     gboolean meta_store_convention, gboolean force_quotes);

/* distributed transactions */
static gboolean gda_oracle_provider_xa_start    (GdaServerProvider *provider, GdaConnection *cnc, 
						 const GdaXaTransactionId *xid, GError **error);

static gboolean gda_oracle_provider_xa_end      (GdaServerProvider *provider, GdaConnection *cnc, 
						 const GdaXaTransactionId *xid, GError **error);
static gboolean gda_oracle_provider_xa_prepare  (GdaServerProvider *provider, GdaConnection *cnc, 
						 const GdaXaTransactionId *xid, GError **error);

static gboolean gda_oracle_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc, 
						 const GdaXaTransactionId *xid, GError **error);
static gboolean gda_oracle_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
						 const GdaXaTransactionId *xid, GError **error);

static GList   *gda_oracle_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc, 
						 GError **error);

/* 
 * private connection data destroy 
 */
static void gda_oracle_free_cnc_data (OracleConnectionData *cdata);


/*
 * Prepared internal statements
 * TO_ADD: any prepared statement to be used internally by the provider should be
 *         declared here, as constants and as SQL statements
 */
static GMutex init_mutex;
static GdaStatement **internal_stmt = NULL;

typedef enum {
	INTERNAL_STMT1
} InternalStatementItem;

static gchar *internal_sql[] = {
	"SQL for INTERNAL_STMT1"
};

/*
 * GdaOracleProvider class implementation
 */
GdaServerProviderBase oracle_base_functions = {
	gda_oracle_provider_get_name,
	gda_oracle_provider_get_version,
	gda_oracle_provider_get_server_version,
	gda_oracle_provider_supports_feature,
	gda_oracle_provider_create_worker,
	NULL,
	gda_oracle_provider_create_parser,
	gda_oracle_provider_get_data_handler,
	gda_oracle_provider_get_default_dbms_type,
	gda_oracle_provider_supports_operation,
	gda_oracle_provider_create_operation,
	gda_oracle_provider_render_operation,
	gda_oracle_provider_statement_to_sql,
	gda_oracle_provider_identifier_quote,
	gda_oracle_provider_statement_rewrite,
	gda_oracle_provider_open_connection,
	gda_oracle_provider_prepare_connection,
	gda_oracle_provider_close_connection,
	NULL,
	NULL,
	gda_oracle_provider_perform_operation,
	gda_oracle_provider_begin_transaction,
	gda_oracle_provider_commit_transaction,
	gda_oracle_provider_rollback_transaction,
	gda_oracle_provider_add_savepoint,
	gda_oracle_provider_rollback_savepoint,
	gda_oracle_provider_delete_savepoint,
	gda_oracle_provider_statement_prepare,
	gda_oracle_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta oracle_meta_functions = {
	_gda_oracle_meta__info,
	_gda_oracle_meta__btypes,
	_gda_oracle_meta__udt,
	_gda_oracle_meta_udt,
	_gda_oracle_meta__udt_cols,
	_gda_oracle_meta_udt_cols,
	_gda_oracle_meta__enums,
	_gda_oracle_meta_enums,
	_gda_oracle_meta__domains,
	_gda_oracle_meta_domains,
	_gda_oracle_meta__constraints_dom,
	_gda_oracle_meta_constraints_dom,
	_gda_oracle_meta__el_types,
	_gda_oracle_meta_el_types,
	_gda_oracle_meta__collations,
	_gda_oracle_meta_collations,
	_gda_oracle_meta__character_sets,
	_gda_oracle_meta_character_sets,
	_gda_oracle_meta__schemata,
	_gda_oracle_meta_schemata,
	_gda_oracle_meta__tables_views,
	_gda_oracle_meta_tables_views,
	_gda_oracle_meta__columns,
	_gda_oracle_meta_columns,
	_gda_oracle_meta__view_cols,
	_gda_oracle_meta_view_cols,
	_gda_oracle_meta__constraints_tab,
	_gda_oracle_meta_constraints_tab,
	_gda_oracle_meta__constraints_ref,
	_gda_oracle_meta_constraints_ref,
	_gda_oracle_meta__key_columns,
	_gda_oracle_meta_key_columns,
	_gda_oracle_meta__check_columns,
	_gda_oracle_meta_check_columns,
	_gda_oracle_meta__triggers,
	_gda_oracle_meta_triggers,
	_gda_oracle_meta__routines,
	_gda_oracle_meta_routines,
	_gda_oracle_meta__routine_col,
	_gda_oracle_meta_routine_col,
	_gda_oracle_meta__routine_par,
	_gda_oracle_meta_routine_par,
	_gda_oracle_meta__indexes_tab,
        _gda_oracle_meta_indexes_tab,
        _gda_oracle_meta__index_cols,
        _gda_oracle_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderXa oracle_xa_functions = {
	gda_oracle_provider_xa_start,
	gda_oracle_provider_xa_end,
	gda_oracle_provider_xa_prepare,
	gda_oracle_provider_xa_commit,
	gda_oracle_provider_xa_rollback,
	gda_oracle_provider_xa_recover,

	NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_oracle_provider_class_init (GdaOracleProviderClass *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &oracle_base_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &(oracle_meta_functions));
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) &oracle_xa_functions);

	/* static types */
	static_types = g_new (GType, GDA_STYPE_NULL + 1);
	static_types[GDA_STYPE_INT] = G_TYPE_INT;
	static_types[GDA_STYPE_STRING] = G_TYPE_STRING;
	static_types[GDA_STYPE_BOOLEAN] = G_TYPE_BOOLEAN;
	static_types[GDA_STYPE_DATE] = G_TYPE_DATE;
	static_types[GDA_STYPE_TIME] = GDA_TYPE_TIME;
	static_types[GDA_STYPE_TIMESTAMP] = GDA_TYPE_TIMESTAMP;
	static_types[GDA_STYPE_INT64] = G_TYPE_INT64;
	static_types[GDA_STYPE_UINT64] = G_TYPE_UINT64;
	static_types[GDA_STYPE_UINT] = G_TYPE_UINT;
	static_types[GDA_STYPE_FLOAT] = G_TYPE_FLOAT;
	static_types[GDA_STYPE_DOUBLE] = G_TYPE_DOUBLE;
	static_types[GDA_STYPE_LONG] = G_TYPE_LONG;
	static_types[GDA_STYPE_ULONG] = G_TYPE_ULONG;
	static_types[GDA_STYPE_NUMERIC] = GDA_TYPE_NUMERIC;
	static_types[GDA_STYPE_BINARY] = GDA_TYPE_BINARY;
	static_types[GDA_STYPE_BLOB] = GDA_TYPE_BLOB;
	static_types[GDA_STYPE_CHAR] = G_TYPE_CHAR;
	static_types[GDA_STYPE_SHORT] = GDA_TYPE_SHORT;
	static_types[GDA_STYPE_GTYPE] = G_TYPE_GTYPE;
	static_types[GDA_STYPE_GEOMETRIC_POINT] = GDA_TYPE_GEOMETRIC_POINT;
	static_types[GDA_STYPE_NULL] = GDA_TYPE_NULL;
}

static void
gda_oracle_provider_init (GdaOracleProvider *oracle_prv, G_GNUC_UNUSED GdaOracleProviderClass *klass)
{
	g_mutex_lock (&init_mutex);

	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) oracle_prv);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
	}

	/* meta data init */
	_gda_oracle_provider_meta_init ((GdaServerProvider*) oracle_prv);

	g_mutex_unlock (&init_mutex);
}

GType
gda_oracle_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaOracleProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_provider_class_init,
			NULL, NULL,
			sizeof (GdaOracleProvider),
			0,
			(GInstanceInitFunc) gda_oracle_provider_init,
			NULL
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaOracleProvider", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

static GdaWorker *
gda_oracle_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* See http://docs.oracle.com/cd/B10501_01/appdev.920/a96584/oci09adv.htm */
	static GdaWorker *unique_worker = NULL;
	return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_oracle_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return ORACLE_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_oracle_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/*
 * NOT FOR SELECT statements!
 */
static gboolean
execute_raw_command (GdaConnection *cnc, const gchar *sql)
{
	OracleConnectionData *cdata;
	OCIStmt *hstmt = NULL;
	int result;

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;
	
	result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) &hstmt,
                                 (ub4) OCI_HTYPE_STMT,
                                 (size_t) 0,
                                 (dvoid **) 0);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
                                     _("Could not allocate the Oracle statement handle")))
		return FALSE;

	result = OCIStmtPrepare ((dvoid *) hstmt,
                                 (dvoid *) cdata->herr,
                                 (text *) sql,
                                 (ub4) strlen(sql),
                                 (ub4) OCI_NTV_SYNTAX,
                                 (ub4) OCI_DEFAULT);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not prepare the Oracle statement"))) {
                OCIHandleFree ((dvoid *) hstmt, OCI_HTYPE_STMT);
                return FALSE;
        }
	
#ifdef GDA_DEBUG
	ub2 stmt_type;
	result = OCIAttrGet (hstmt, OCI_HTYPE_STMT,
                             (dvoid *) &stmt_type, NULL,
                             OCI_ATTR_STMT_TYPE, cdata->herr);
	if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not get the Oracle statement type"))) {
                OCIHandleFree ((dvoid *) hstmt, OCI_HTYPE_STMT);
                return FALSE;
        }
	if (stmt_type == OCI_STMT_SELECT) {
		g_warning ("Internal implementation error: the %s() function can't be used to execute SELECT commands\n",
			   __FUNCTION__);
		OCIHandleFree ((dvoid *) hstmt, OCI_HTYPE_STMT);
                return FALSE;
	}
#endif
	result = OCIStmtExecute (cdata->hservice,
				 hstmt,
				 cdata->herr,
				 (ub4) 1,
				 (ub4) 0,
				 (CONST OCISnapshot *) NULL,
				 (OCISnapshot *) NULL,
				 OCI_DEFAULT);
	if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
				     _("Could not execute the Oracle statement"))) {
                OCIHandleFree ((dvoid *) hstmt, OCI_HTYPE_STMT);
                return FALSE;
        }

	OCIHandleFree ((dvoid *) hstmt, OCI_HTYPE_STMT);
	return TRUE;
}

/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a OracleConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_oracle_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, GdaQuarkList *auth)
{
	OracleConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_ORACLE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Check for connection parameters */
	const gchar *tnsname, *username, *password, *schema;
	gchar *easy = NULL;
 	tnsname = gda_quark_list_find (params, "TNSNAME");
	if (!tnsname) {
		const gchar *host, *port, *dbname;
		host = gda_quark_list_find (params, "HOST");
		dbname = gda_quark_list_find (params, "DB_NAME");
		port = gda_quark_list_find (params, "PORT");
		if (host) {
			GString *string;
			string = g_string_new (host);
			if (port)
				g_string_append_printf (string, ":%s", port);
			if (dbname)
				g_string_append_printf (string, "/%s", dbname);
			easy = g_string_free (string, FALSE);
		}
		else {
			gda_connection_add_event_string (cnc,
							 _("The connection string must contain the TNSNAME or the HOST values"));
			return FALSE;
		}
	}
	username = gda_quark_list_find (auth, "USERNAME");
	if (!username)
		username = g_get_user_name ();
	password = gda_quark_list_find (auth, "PASSWORD");
	schema = gda_quark_list_find (params, "SCHEMA");
	
	/* open the real connection to the database */
	gint result;
	result = OCIInitialize ((ub4) OCI_THREADED,
                                (dvoid *) 0,
                                (dvoid * (*)(dvoid *, size_t)) 0,
                                (dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
                                (void (*)(dvoid *, dvoid *)) 0);

        if (result != OCI_SUCCESS) {
                gda_connection_add_event_string (cnc,
                                                 _("Could not initialize Oracle"));
                return FALSE;
        }

        /* initialize the Oracle environment */
	cdata = g_new0 (OracleConnectionData, 1);
	cdata->autocommit = TRUE;
        result = OCIEnvInit ((OCIEnv **) & cdata->henv,
                             (ub4) OCI_DEFAULT,
                             (size_t) 0,
                             (dvoid **) 0);
        if (result != OCI_SUCCESS) {
                gda_connection_add_event_string (cnc,
                                                 _("Could not initialize the Oracle environment"));
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

        /* create the service context */
        result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) &cdata->hservice,
                                 (ub4) OCI_HTYPE_SVCCTX,
                                 (size_t) 0,
                                 (dvoid **) 0);
	if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
                                     _("Could not allocate the Oracle service handle"))) {
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
	}

	/* create the error handle */
        result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) &(cdata->herr),
                                 (ub4) OCI_HTYPE_ERROR,
                                 (size_t) 0,
                                 (dvoid **) 0);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
                                     _("Could not allocate the Oracle error handle"))) {
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

        /* we use the Multiple Sessions/Connections OCI paradigm for this server */
        result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) & cdata->hserver,
                                 (ub4) OCI_HTYPE_SERVER,
                                 (size_t) 0,
                                 (dvoid **) 0);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
                                     _("Could not allocate the Oracle server handle"))) {
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

        /* create the session handle */
        result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) &cdata->hsession,
                                 (ub4) OCI_HTYPE_SESSION,
                                 (size_t) 0,
                                 (dvoid **) 0);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
                                     _("Could not allocate the Oracle session handle"))) {
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

	/* attach to Oracle server */
        result = OCIServerAttach (cdata->hserver,
                                  cdata->herr,
                                  (text *) (tnsname ? tnsname : easy),
                                  (ub4) strlen (tnsname ? tnsname : easy),
                                  OCI_DEFAULT);
	g_free (easy);

        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not attach to the Oracle server"))) {
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

        /* set the server attribute in the service context */
        result = OCIAttrSet ((dvoid *) cdata->hservice,
                             (ub4) OCI_HTYPE_SVCCTX,
                             (dvoid *) cdata->hserver,
                             (ub4) 0,
                             (ub4) OCI_ATTR_SERVER,
                             cdata->herr);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not set the Oracle server attribute in the service context"))) {
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

	/* set the username attribute */
	result = OCIAttrSet ((dvoid *) cdata->hsession,
			     (ub4) OCI_HTYPE_SESSION,
			     (dvoid *) username,
			     (ub4) strlen (username),
			     (ub4) OCI_ATTR_USERNAME,
			     cdata->herr);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not set the Oracle username attribute"))) {
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

        /* set the password attribute */
	if (password) {
		result = OCIAttrSet ((dvoid *) cdata->hsession,
				     (ub4) OCI_HTYPE_SESSION,
				     (dvoid *) password,
				     (ub4) strlen (password),
				     (ub4) OCI_ATTR_PASSWORD,
				     cdata->herr);
		if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					     _("Could not set the Oracle password attribute"))) {
			OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
			OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
			OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
			OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
			gda_oracle_free_cnc_data (cdata);
			return FALSE;
		}
	}

	/* begin the session */
        result = OCISessionBegin (cdata->hservice,
                                  cdata->herr,
                                  cdata->hsession,
                                  (ub4) OCI_CRED_RDBMS,
                                  (ub4) OCI_STMT_CACHE);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not begin the Oracle session"))) {
                OCIServerDetach (cdata->hserver, cdata->herr, OCI_DEFAULT);
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                cdata->hsession = NULL;
                return FALSE;
        }

        /* set the session attribute in the service context */
        result = OCIAttrSet ((dvoid *) cdata->hservice,
                             (ub4) OCI_HTYPE_SVCCTX,
                             (dvoid *) cdata->hsession,
                             (ub4) 0,
                             (ub4) OCI_ATTR_SESSION,
                             cdata->herr);
        if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                     _("Could not set the Oracle session attribute in the service context"))) {
                OCIServerDetach (cdata->hserver, cdata->herr, OCI_DEFAULT);
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
		gda_oracle_free_cnc_data (cdata);
                return FALSE;
        }

	/* Create a new instance of the provider specific data associated to a connection (OracleConnectionData),
	 * and set its contents */
	if (schema)
		cdata->schema = g_ascii_strup (schema, -1);
	else
		cdata->schema = g_ascii_strup (username, -1);

	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_oracle_free_cnc_data);
	return TRUE;
}

static gboolean
gda_oracle_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
					GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	OracleConnectionData *cdata;
	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

	const gchar *schema;
	schema = gda_quark_list_find (params, "SCHEMA");
	
	/* get version */
	gchar version [512];
	cdata->version = NULL;
	cdata->major_version = 8;
	cdata->minor_version = 0;

	gint result;
	result = OCIServerVersion (cdata->hservice, cdata->herr, (text*) version, 511, OCI_HTYPE_SVCCTX);
	if ((result == OCI_SUCCESS) || (result = OCI_SUCCESS_WITH_INFO)) {
		cdata->version = g_strdup (version);
		gchar *tmp, *ptr;
		tmp = g_ascii_strdown (version, -1);
		ptr = strstr (tmp, "release");
		if (ptr) {
			for (ptr += 7; *ptr; ptr++) {
				if (! g_ascii_isspace (*ptr))
					break;
			}
			if (g_ascii_isdigit (*ptr)) {
				cdata->major_version = *ptr - '0';
				ptr++;
				if (*ptr) {
					ptr++;
					if (g_ascii_isdigit (*ptr))
						cdata->minor_version = *ptr - '0';
				}
			}
		}
		g_free (tmp);
	}

	/* Optionnally set some attributes for the newly opened connection (encoding to UTF-8 for example )*/
	if (! execute_raw_command (cnc, "ALTER SESSION SET NLS_DATE_FORMAT = 'MM/DD/YYYY'") ||
	    ! execute_raw_command (cnc, "ALTER SESSION SET NLS_NUMERIC_CHARACTERS = \". \"") ||
	    (schema && ! execute_raw_command (cnc, g_strdup_printf ("ALTER SESSION SET CURRENT_SCHEMA = \"%s\"", schema))))
		return FALSE;

	return TRUE;
}


/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated OracleConnectionData structure
 *   - Free the OracleConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_oracle_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	/* end the session */
	int result;
        if ((result = OCISessionEnd (cdata->hservice,
				     cdata->herr,
				     cdata->hsession,
				     OCI_DEFAULT))) {
                gda_connection_add_event (cnc,
                                          _gda_oracle_make_error (cnc, cdata->herr, OCI_HTYPE_ERROR, __FILE__, __LINE__));
                return FALSE;
        }

	/* Free the OracleConnectionData structure and its contents*/
	gda_oracle_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated OracleConnectionData structure
 */
static const gchar *
gda_oracle_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return NULL;

	return cdata->version;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a oracle_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_oracle_provider_render_operation() and gda_oracle_provider_perform_operation() methods
 *
 * In this example, the GDA_SERVER_OPERATION_CREATE_TABLE is implemented
 */
static gboolean
gda_oracle_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_oracle_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options, GError **error)
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
        str = g_strdup_printf ("oracle_specs_%s", file);
        g_free (file);

	gchar *tmp;
	tmp = g_strdup_printf ("%s.xml", str);
	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, tmp);
	g_free (dir);
	g_free (tmp);

  if (!file) 
    file = g_strdup_printf ("/spec/oracle/%s.raw.xml", str);

  op = GDA_SERVER_OPERATION (g_object_new (GDA_TYPE_SERVER_OPERATION, 
                                           "op-type", type, 
                                           "spec-resource", file, 
                                           "connection", cnc,
                                           "provider", provider,
                                           NULL));

  g_free (str);
  g_free (file);

        return op;
}

/*
 * Render operation request
 */
static gchar *
gda_oracle_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
        str = g_strdup_printf ("oracle_specs_%s", file);
        g_free (file);

	gchar *tmp;
	tmp = g_strdup_printf ("%s.xml", str);
	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, tmp);
	g_free (dir);
	g_free (tmp);

	if (file) {
		g_free (str);
		if (!gda_server_operation_is_valid (op, file, error)) {
			g_free (file);
			return NULL;
		}
	}
	else {
		file = g_strdup_printf ("/spec/oracle/%s.raw.xml", str);
		g_free (str);
		if (!gda_server_operation_is_valid_from_resource (op, file, error)) {
			g_free (file);
			return NULL;
		}
        }
	g_free (file);

	/* actual rendering */
        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = NULL;
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_oracle_render_CREATE_TABLE (provider, cnc, op, error);
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
gda_oracle_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_oracle_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *name, GdaTransactionIsolation level,
				       GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	cdata->autocommit = FALSE;
	return FALSE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_oracle_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	cdata->autocommit = TRUE;
	return FALSE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_oracle_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	cdata->autocommit = TRUE;
	return FALSE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_oracle_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				   const gchar *name, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_oracle_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_oracle_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_oracle_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
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
gda_oracle_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
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
		dh = gda_server_provider_handler_use_default (provider, type);

	return dh;
}

/*
 * Get default DBMS type request
 *
 * This method returns the "preferred" DBMS type for GType
 */
static const gchar*
gda_oracle_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
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
		return "NUMBER";

	if ((type == GDA_TYPE_BINARY) ||
	    (type == GDA_TYPE_BLOB))
		return "BLOB";

	if (type == G_TYPE_BOOLEAN)
		return "BOOLEAN";
	
	if ((type == G_TYPE_DATE) || 
	    (type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == GDA_TYPE_TIMESTAMP) ||
	    (type == G_TYPE_GTYPE))
		return "VARCHAR2";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "FLOAT";
	
	if (type == GDA_TYPE_TIME)
		return "TIME";
	if (type == GDA_TYPE_TIMESTAMP)
		return "TIMESTAMP";
	if (type == G_TYPE_DATE)
		return "DATE";

	if ((type == GDA_TYPE_NULL) ||
	    (type == G_TYPE_GTYPE))
		return NULL;

	return "VARCHAR2";
}

/*
 * Create parser request
 *
 * This method is responsible for creating a #GdaSqlParser object specific to the SQL dialect used
 * by the database. See the PostgreSQL provider implementation for an example.
 */
static GdaSqlParser *
gda_oracle_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	return (GdaSqlParser*) g_object_new (GDA_TYPE_ORACLE_PARSER, "tokenizer-flavour",
                                             GDA_SQL_PARSER_FLAVOUR_ORACLE, NULL);
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
static gchar *oracle_render_select (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error);
static gchar *oracle_render_select_target (GdaSqlSelectTarget *target, GdaSqlRenderingContext *context, GError **error);
static gchar *oracle_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context,
                                  gboolean *is_default, gboolean *is_null,
                                  GError **error);

static gchar *
gda_oracle_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				      GSList **params_used, GError **error)
{
	gchar *str;
	GdaSqlRenderingContext context;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	memset (&context, 0, sizeof (context));
	context.provider = provider;
	context.cnc = cnc;
	context.params = params;
	context.flags = flags;
	context.render_select = (GdaSqlRenderingFunc) oracle_render_select;
	context.render_select_target = (GdaSqlRenderingFunc) oracle_render_select_target;
	context.render_expr = oracle_render_expr; /* render "FALSE" as 0 and TRUE as 1 */

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

/* the difference from the common implementation is to avoid rendering the "AS" in target alias */
static gchar *
oracle_render_select_target (GdaSqlSelectTarget *target, GdaSqlRenderingContext *context, GError **error)
{
        GString *string;
        gchar *str;

        g_return_val_if_fail (target, NULL);
        g_return_val_if_fail (GDA_SQL_ANY_PART (target)->type == GDA_SQL_ANY_SQL_SELECT_TARGET, NULL);

        /* can't have: target->expr == NULL */
        if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (target), error)) return NULL;

	if (! target->expr->value || (G_VALUE_TYPE (target->expr->value) != G_TYPE_STRING)) {
		str = context->render_expr (target->expr, context, NULL, NULL, error);
		if (!str)
			return NULL;
		string = g_string_new (str);
		g_free (str);
	}
	else {
		gboolean tmp;
		tmp = target->expr->value_is_ident;
		target->expr->value_is_ident = TRUE;
		str = context->render_expr (target->expr, context, NULL, NULL, error);
		target->expr->value_is_ident = tmp;
		string = g_string_new (str);
		g_free (str);
	}

        if (target->as) {
		g_string_append_c (string, ' ');
                g_string_append (string, target->as);
	}

        str = string->str;
        g_string_free (string, FALSE);
        return str;
}

static gchar *
oracle_render_select (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_SELECT, NULL);

	string = g_string_new ("SELECT ");
	/* distinct */
	if (stmt->distinct) {
		g_string_append (string, "DISTINCT ");
		if (stmt->distinct_expr) {
			str = context->render_expr (stmt->distinct_expr, context, NULL, NULL, error);
			if (!str) goto err;
			g_string_append (string, "ON ");
			g_string_append (string, str);
			g_string_append_c (string, ' ');
			g_free (str);
		}
	}
	
	/* selected expressions */
	for (list = stmt->expr_list; list; list = list->next) {
		str = context->render_select_field (GDA_SQL_ANY_PART (list->data), context, error);
		if (!str) goto err;
		if (list != stmt->expr_list)
			g_string_append (string, ", ");
		g_string_append (string, str);
		g_free (str);
	}

	/* FROM */
	if (stmt->from) {
		str = context->render_select_from (GDA_SQL_ANY_PART (stmt->from), context, error);
		if (!str) goto err;
		g_string_append_c (string, ' ');
		g_string_append (string, str);
		g_free (str);
	}
	else {
		g_string_append (string, " FROM DUAL");
	}

	/* WHERE */
	if (stmt->where_cond) {
		g_string_append (string, " WHERE ");
		str = context->render_expr (stmt->where_cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	/* LIMIT */
	if (stmt->limit_count) {
		if (stmt->where_cond)
			g_string_append (string, " AND");
		else
			g_string_append (string, " WHERE");
		
		if (stmt->limit_offset) {
			g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_SYNTAX_ERROR,
				     "%s", _("Oracle does not support the offset with a limit"));
			goto err;
		}
		else {
			g_string_append (string, " ROWNUM <= (");
			str = context->render_expr (stmt->limit_count, context, NULL, NULL, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
			g_string_append_c (string, ')');
		}
	}

	/* GROUP BY */
	for (list = stmt->group_by; list; list = list->next) {
		str = context->render_expr (list->data, context, NULL, NULL, error);
		if (!str) goto err;
		if (list != stmt->group_by)
			g_string_append (string, ", ");
		else
			g_string_append (string, " GROUP BY ");
		g_string_append (string, str);
		g_free (str);
	}

	/* HAVING */
	if (stmt->having_cond) {
		g_string_append (string, " HAVING ");
		str = context->render_expr (stmt->having_cond, context, NULL, NULL, error);
		if (!str) goto err;
		g_string_append (string, str);
		g_free (str);
	}

	/* ORDER BY */
	for (list = stmt->order_by; list; list = list->next) {
		str = context->render_select_order (GDA_SQL_ANY_PART (list->data), context, error);
		if (!str) goto err;
		if (list != stmt->order_by)
			g_string_append (string, ", ");
		else
			g_string_append (string, " ORDER BY ");
		g_string_append (string, str);
		g_free (str);
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

/*
 * The difference with the default implementation is to render TRUE and FALSE as 0 and 1
 */
static gchar *
oracle_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context, gboolean *is_default,
		    gboolean *is_null, GError **error)
{
	GString *string;
	gchar *str = NULL;

	g_return_val_if_fail (expr, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR, NULL);

	if (is_default)
		*is_default = FALSE;
	if (is_null)
		*is_null = FALSE;

	/* can't have:
	 *  - expr->cast_as && expr->param_spec
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (expr), error)) return NULL;

	string = g_string_new ("");
	if (expr->param_spec) {
		str = context->render_param_spec (expr->param_spec, expr, context, is_default, is_null, error);
		if (!str) goto err;
	}
	else if (expr->value) {
		if (G_VALUE_TYPE (expr->value) == G_TYPE_STRING) {
			/* specific treatment for strings, see documentation about GdaSqlExpr's value attribute */
			const gchar *vstr;
			vstr = g_value_get_string (expr->value);
			if (vstr) {
				if (expr->value_is_ident) {
					gchar **ids_array;
					gint i;
					GString *string = NULL;
					GdaConnectionOptions cncoptions = 0;
					if (context->cnc)
						g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
					ids_array = gda_sql_identifier_split (vstr);
					if (!ids_array)
						str = g_strdup (vstr);
					else if (!(ids_array[0])) goto err;
					else {
						for (i = 0; ids_array[i]; i++) {
							gchar *tmp;
							if (!string)
								string = g_string_new ("");
							else
								g_string_append_c (string, '.');
							tmp = gda_sql_identifier_quote (ids_array[i], context->cnc,
											context->provider, FALSE,
											cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
							g_string_append (string, tmp);
							g_free (tmp);
						}
						g_strfreev (ids_array);
						str = g_string_free (string, FALSE);
					}
				}
				else {
					/* we don't have an identifier */
					if (!g_ascii_strcasecmp (vstr, "default")) {
						if (is_default)
							*is_default = TRUE;
						str = g_strdup ("DEFAULT");
					}
					else if (!g_ascii_strcasecmp (vstr, "FALSE")) {
						g_free (str);
						str = g_strdup ("0");
					}
					else if (!g_ascii_strcasecmp (vstr, "TRUE")) {
						g_free (str);
						str = g_strdup ("1");
					}
					else
						str = g_strdup (vstr);
				}
			}
			else {
				str = g_strdup ("NULL");
				if (is_null)
					*is_null = TRUE;
			}
		}
		if (!str) {
			/* use a GdaDataHandler to render the value as valid SQL */
			GdaDataHandler *dh;
			if (context->cnc) {
				GdaServerProvider *prov;
				prov = gda_connection_get_provider (context->cnc);
				dh = gda_server_provider_get_data_handler_g_type (prov, context->cnc,
										  G_VALUE_TYPE (expr->value));
				if (!dh) goto err;
			}
			else
				dh = gda_data_handler_get_default (G_VALUE_TYPE (expr->value));

			if (dh)
				str = gda_data_handler_get_sql_from_value (dh, expr->value);
			else
				str = gda_value_stringify (expr->value);
			if (!str) goto err;
		}
	}
	else if (expr->func) {
		str = context->render_function (GDA_SQL_ANY_PART (expr->func), context, error);
		if (!str) goto err;
	}
	else if (expr->cond) {
		gchar *tmp;
		tmp = context->render_operation (GDA_SQL_ANY_PART (expr->cond), context, error);
		if (!tmp) goto err;
		str = NULL;
		if (GDA_SQL_ANY_PART (expr)->parent) {
			if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_SELECT) {
				GdaSqlStatementSelect *selst;
				selst = (GdaSqlStatementSelect*) (GDA_SQL_ANY_PART (expr)->parent);
				if ((expr == selst->where_cond) ||
				    (expr == selst->having_cond))
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_DELETE) {
				GdaSqlStatementDelete *delst;
				delst = (GdaSqlStatementDelete*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == delst->cond)
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_UPDATE) {
				GdaSqlStatementUpdate *updst;
				updst = (GdaSqlStatementUpdate*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == updst->cond)
					str = tmp;
			}
		}

		if (!str) {
			str = g_strconcat ("(", tmp, ")", NULL);
			g_free (tmp);
		}
	}
	else if (expr->select) {
		gchar *str1;
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			str1 = context->render_select (GDA_SQL_ANY_PART (expr->select), context, error);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str1 = context->render_compound (GDA_SQL_ANY_PART (expr->select), context, error);
		else
			g_assert_not_reached ();
		if (!str1) goto err;

		if (! GDA_SQL_ANY_PART (expr)->parent ||
		    (GDA_SQL_ANY_PART (expr)->parent->type != GDA_SQL_ANY_SQL_FUNCTION)) {
			str = g_strconcat ("(", str1, ")", NULL);
			g_free (str1);
		}
		else
			str = str1;
	}
	else if (expr->case_s) {
		str = context->render_case (GDA_SQL_ANY_PART (expr->case_s), context, error);
		if (!str) goto err;
	}
	else {
		if (is_null)
			*is_null = TRUE;
		str = g_strdup ("NULL");
	}

	if (!str) goto err;

	if (expr->cast_as)
		g_string_append_printf (string, "CAST (%s AS %s)", str, expr->cast_as);
	else
		g_string_append (string, str);
	g_free (str);

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaOraclePStmt object and declare it to @cnc.
 */
static gboolean
gda_oracle_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GError **error)
{
	GdaOraclePStmt *ps;
	gboolean retval = FALSE;
	OracleConnectionData *cdata;
	GdaConnectionEvent *event;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaOraclePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* render as SQL understood by the provider */
	GdaSet *params = NULL;
	gchar *sql;
	GSList *used_params = NULL;
	if (! gda_statement_get_parameters (stmt, &params, error))
                return FALSE;
        sql = gda_oracle_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_COLON,
						    &used_params, error);
        if (!sql) 
		goto out;

	/* prepare @stmt using the C API, creates @ps */
	OCIStmt *hstmt;
	int result;

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	result = OCIHandleAlloc ((dvoid *) cdata->henv,
                                 (dvoid **) &hstmt,
                                 (ub4) OCI_HTYPE_STMT,
                                 (size_t) 0,
                                 (dvoid **) NULL);
        if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ENV,
					      _("Could not allocate the Oracle statement handle")))) {
		g_free (sql);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     "%s", gda_connection_event_get_description (event));
                return FALSE;
	}
	
	/*g_print ("Really PREPARED: %s\n", sql);*/
	result = OCIStmtPrepare2 (cdata->hservice,
				  &hstmt,
				  (dvoid *) cdata->herr,
				  (text *) sql,
				  (ub4) strlen (sql),
				  (text *) sql,
				  (ub4) strlen (sql),
				  (ub4) OCI_NTV_SYNTAX,
				  (ub4) OCI_DEFAULT);
        if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					      _("Could not prepare the Oracle statement")))) {
		OCIStmtRelease ((dvoid *) hstmt, cdata->herr, NULL, 0, result ? OCI_STRLS_CACHE_DELETE : OCI_DEFAULT);
		g_free (sql);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     "%s", gda_connection_event_get_description (event));
                return FALSE;
        }

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
	ps = gda_oracle_pstmt_new (hstmt);
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

static void
clear_ora_values_list (GSList *oravalues_list)
{
	if (oravalues_list) {
		g_slist_foreach (oravalues_list, (GFunc) _gda_oracle_value_free, NULL);
		g_slist_free (oravalues_list);
		oravalues_list = NULL;
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
gda_oracle_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params,
				       GdaStatementModelUsage model_usage, 
				       GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaOraclePStmt *ps;
	OracleConnectionData *cdata;
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
	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* get/create new prepared statement */
	ps = (GdaOraclePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		GError *lerror = NULL;
		if (!gda_oracle_provider_statement_prepare (provider, cnc, stmt, &lerror)) {
			if (lerror && (lerror->domain == GDA_STATEMENT_ERROR) &&
			    (lerror->code == GDA_STATEMENT_SYNTAX_ERROR)) {
				g_propagate_error (error, lerror);
				return NULL;
			}
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaOraclePStmt object.
			 *
			 * Don't call gda_connection_add_prepared_statement() with this new prepared statement
			 * as it will be destroyed once used.
			 */
			
			TO_IMPLEMENT;
			return NULL;
		}
		else {
			ps = (GdaOraclePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
			g_object_ref (ps);
		}
	}
	else
		g_object_ref (ps);
	g_assert (ps);

	/* bind statement's parameters */
	GSList *list, *oravalues_list;
	GdaConnectionEvent *event = NULL;
	int i;
	for (i = 1, list = _GDA_PSTMT (ps)->param_ids, oravalues_list = NULL;
	     list;
	     list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		gchar *real_pname = NULL;
		GdaHolder *h;
		GdaOracleValue *ora_value = NULL;

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
				if (h)
					real_pname = tmp;
				else
					g_free (tmp);
			}
		}
		if (!h) {
			if (allow_noparam) {
                                /* bind param to NULL */
				/* Hint: Indicator Variables, see p. 118 */
				ora_value = g_new0 (GdaOracleValue, 1);
				ora_value->indicator = -1;
                                empty_rs = TRUE;
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
		else if (!gda_holder_is_valid (h)) {
			if (allow_noparam) {
                                /* bind param to NULL */
				ora_value = g_new0 (GdaOracleValue, 1);
				ora_value->indicator = -1;
                                empty_rs = TRUE;
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
			/* create a new GdaStatement to handle all default values and execute it instead */
			GdaSqlStatement *sqlst;
			GError *lerror = NULL;
			sqlst = gda_statement_rewrite_for_default_values (stmt, params, FALSE, &lerror);
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
			res = gda_oracle_provider_statement_execute (provider, cnc,
								     rstmt, params,
								     model_usage,
								     col_types, last_inserted_row, error);
			g_object_unref (rstmt);
			return res;
		}

		/* actual binding using the C API, for parameter at position @i */
		const GValue *value = gda_holder_get_value (h);
		if (!ora_value)
			ora_value = _gda_value_to_oracle_value (value);

		if (!value || gda_value_is_null (value)) {
			GdaStatement *rstmt;
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error)) {
				OCIBind *bindpp = (OCIBind *) 0;
				int result;
				result = OCIBindByName ((dvoid *) ps->hstmt,
							(OCIBind **) &bindpp,
							(OCIError *) cdata->herr,
							/* param name */
							(text *) (real_pname ? real_pname : pname),
							(sb4) strlen (real_pname ? real_pname : pname),
							/* bound value */
							(dvoid *) ora_value->value,
							(sb4) ora_value->defined_size,
							(ub2) ora_value->sql_type,
							(dvoid *) &ora_value->indicator,
							(ub2 *) 0,
							(ub2) 0,
							(ub4) 0,
							(ub4 *) 0,
							(ub4) OCI_DEFAULT);
				
				oravalues_list = g_slist_prepend (oravalues_list, ora_value);
				if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
								      _("Could not bind the Oracle statement parameter"))))
					break;
			}
			else if (!rstmt)
				return NULL;
			else {
				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaOraclePStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_oracle_provider_statement_prepare (provider, cnc,
									    rstmt, error))
					return NULL;
				tps = (GdaOraclePStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gtps->param_ids;
				gtps->param_ids = NULL;
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids = gtps->param_ids;
				gtps->param_ids = prep_param_ids;

				/* execute */
				obj = gda_oracle_provider_statement_execute (provider, cnc,
									     rstmt, params,
									     model_usage,
									     col_types,
									     last_inserted_row, error);
				/* clear original @param_ids and restore copied one */
				g_slist_foreach (prep_param_ids, (GFunc) g_free, NULL);
				g_slist_free (prep_param_ids);

				gtps->param_ids = copied_param_ids;

				/*if (GDA_IS_DATA_MODEL (obj))
				  gda_data_model_dump ((GdaDataModel*) obj, NULL);*/

				g_object_unref (rstmt);
				g_object_unref (ps);
				return obj;
			}
		}
		else {
			OCIBind *bindpp = (OCIBind *) 0;
			int result;
			result = OCIBindByName ((dvoid *) ps->hstmt,
						(OCIBind **) &bindpp,
						(OCIError *) cdata->herr,
						/* param name */
						(text *) (real_pname ? real_pname : pname),
						(sb4) strlen (real_pname ? real_pname : pname),
						/* bound value */
						(dvoid *) ora_value->value,
						(sb4) ora_value->defined_size,
						(ub2) ora_value->sql_type,
						(dvoid *) &ora_value->indicator,
						(ub2 *) 0,
						(ub2) 0,
						(ub4) 0,
						(ub4 *) 0,
						(ub4) OCI_DEFAULT);

			oravalues_list = g_slist_prepend (oravalues_list, ora_value);
			if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							      _("Could not bind the Oracle statement parameter"))))
				break;
		}
	}
		
	if (event) {
		gda_connection_add_event (cnc, event);
		g_object_unref (ps);
		clear_ora_values_list (oravalues_list);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
        gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
        gda_connection_add_event (cnc, event);
	event = NULL;

	/* get statement type */
	ub2 stmt_type = 0;
	int result;
	result = OCIAttrGet (ps->hstmt, OCI_HTYPE_STMT,
			     (dvoid *) &stmt_type, NULL,
			     OCI_ATTR_STMT_TYPE, cdata->herr);
	event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					 _("Could not get the Oracle statement type"));
	if (event) {
		clear_ora_values_list (oravalues_list);
		return NULL;
	}

	if (empty_rs) {
		/* There are some missing parameters, so the SQL can't be executed but we still want
		 * to execute something to get the columns correctly. A possibility is to actually
		 * execute another SQL which is the code shown here.
		 *
		 * To adapt depending on the C API and its features */

		/* HINT: use the OCIStmtExecute()' OCI_DESCRIBE_ONLY flag */

		GdaStatement *estmt;
                gchar *esql;
                estmt = gda_select_alter_select_for_empty (stmt, error);
                if (!estmt) {
			g_object_unref (ps);
			clear_ora_values_list (oravalues_list);
                        return NULL;
		}
                esql = gda_statement_to_sql (estmt, NULL, error);
                g_object_unref (estmt);
                if (!esql) {
			g_object_unref (ps);
			clear_ora_values_list (oravalues_list);
                        return NULL;
		}

		/* Execute the 'esql' SQL code */
                g_free (esql);

		TO_IMPLEMENT;
	}
	else {
		/* Execute the @ps prepared statement */
		result = OCIStmtExecute (cdata->hservice,
					 ps->hstmt,
					 cdata->herr,
					 (ub4) ((stmt_type == OCI_STMT_SELECT) ? 0 : 1),
					 (ub4) 0,
					 (CONST OCISnapshot *) NULL,
					 (OCISnapshot *) NULL,
					 OCI_DEFAULT);
		/* or OCI_STMT_SCROLLABLE_READONLY, not OCI_COMMIT_ON_SUCCESS because transactions are
		   handled separately */
		
		event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						 _("Could not execute the Oracle statement"));
	}

	clear_ora_values_list (oravalues_list);
	if (event) {
		g_object_unref (ps);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
			     gda_connection_event_get_description (event));
		return NULL;
	}
	
	/* execute prepared statement using C API depending on its kind */
	if (stmt_type == OCI_STMT_SELECT) {
		GdaDataModel *data_model;
		GdaDataModelAccessFlags flags;
		GList *columns = NULL;
                ub4 ncolumns;
                ub4 i;
		
		if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

                /* get the number of columns in the result set */
                result = OCIAttrGet ((dvoid *) ps->hstmt,
                                     (ub4) OCI_HTYPE_STMT,
                                     (dvoid *) &ncolumns,
                                     (ub4 *) 0,
                                     (ub4) OCI_ATTR_PARAM_COUNT,
                                     cdata->herr);
                if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						      _("Could not get the number of columns in the result set")))) {
			g_object_unref (ps);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
				     gda_connection_event_get_description (event));
                        return NULL;
                }

                for (i = 0; i < ncolumns; i += 1) {
                        text *dummy = (text *) 0;
                        ub4 col_name_len;
                        OCIParam *pard = (OCIParam *) 0;
                        gchar *name_buffer;

                        result = OCIParamGet ((dvoid *) ps->hstmt,
                                              OCI_HTYPE_STMT,
                                              cdata->herr,
                                              (dvoid **) &pard,
                                              (ub4) i + 1);
                        if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							      _("Could not get the parameter descripter in the result set")))) {
				g_object_unref (ps);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
					     gda_connection_event_get_description (event));
                                return NULL;
                        }

                        result = OCIAttrGet ((dvoid *) pard,
                                             (ub4) OCI_DTYPE_PARAM,
                                             (dvoid **) &dummy,
                                             (ub4 *) &col_name_len,
                                             (ub4) OCI_ATTR_NAME,
                                             (OCIError *) cdata->herr);
                        if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							      _("Could not get the column name in the result set")))) {
				g_object_unref (ps);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
					     gda_connection_event_get_description (event));
                                return NULL;
                        }

                        name_buffer = g_malloc0 (col_name_len + 1);
			memcpy (name_buffer, dummy, col_name_len);
                        name_buffer[col_name_len] = '\0';
                        columns = g_list_append (columns, name_buffer);
                }

                data_model = gda_oracle_recordset_new (cnc, ps, params, flags, col_types);
		if (data_model)
			gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
		g_object_unref (ps);
		return (GObject*) data_model;
        }
	else {
		GdaSet *set = NULL;
		int nrows = -2; /* number of rows non reported */

		result = OCIAttrGet (ps->hstmt, OCI_HTYPE_STMT,
				     (dvoid *) &nrows, NULL,
				     OCI_ATTR_ROW_COUNT, cdata->herr);
		if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					     _("Could not get the number of affected rows")))
			nrows = -2;

		if (cdata->autocommit) {
			result = OCITransCommit (cdata->hservice, cdata->herr, OCI_DEFAULT);
			if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							      _("Error auto-commiting transaction")))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
					     gda_connection_event_get_description (event));
                                return NULL;
                        }
		}

		set = gda_set_new_inline (1, "IMPACTED_ROWS", G_TYPE_INT, nrows);

		if (nrows >= 0) {
			gchar *str = NULL;
			switch (stmt_type) {
			case OCI_STMT_UPDATE:
				str = g_strdup_printf ("UPDATE %d", nrows);
				break;
			case OCI_STMT_DELETE:
				str = g_strdup_printf ("DELETE %d", nrows);
				break;
			case OCI_STMT_INSERT:
				str = g_strdup_printf ("INSERT %d", nrows);
				break;
			default:
				break;
			}
			if (str) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
				gda_connection_event_set_description (event, str);
				g_free (str);
				gda_connection_add_event (cnc, event);
			}
		}

		gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
		g_object_unref (ps);
		return (GObject*) set;
	}
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Uses the DEFAULT keyword
 */
static GdaSqlStatement *
gda_oracle_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}
	return gda_statement_rewrite_for_default_values (stmt, params, FALSE, error);
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
static gboolean
gda_oracle_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc, 
			      const GdaXaTransactionId *xid, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_oracle_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc, 
			    const GdaXaTransactionId *xid, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_oracle_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc, 
				const GdaXaTransactionId *xid, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_oracle_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc, 
			       const GdaXaTransactionId *xid, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_oracle_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc, 
				 const GdaXaTransactionId *xid, GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_oracle_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				GError **error)
{
	OracleConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
        *retval = '"';
        for (rptr = retval+1, sptr = str; *sptr; sptr++, rptr++) {
                if (*sptr == '"') {
                        *rptr = '\\';
                        rptr++;
                        *rptr = *sptr;
                }
                else
                        *rptr = *sptr;
        }
        *rptr = '"';
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

/* Returns: @str */
static gchar *
ora_remove_quotes (gchar *str)
{
        glong total;
        gchar *ptr;
        glong offset = 0;
	char delim;
	
	if (!str)
		return NULL;
	delim = *str;
	if ((delim != '\'') && (delim != '"'))
		return str;


        total = strlen (str);
        if (str[total-1] == delim) {
		/* string is correctly terminated */
		memmove (str, str+1, total-2);
		total -=2;
	}
	else {
		/* string is _not_ correctly terminated */
		memmove (str, str+1, total-1);
		total -=1;
	}
        str[total] = 0;

        ptr = (gchar *) str;
        while (offset < total) {
                /* we accept the "''" as a synonym of "\'" */
                if (*ptr == delim) {
                        if (*(ptr+1) == delim) {
                                memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                *str = 0;
                                return str;
                        }
                }
                if (*ptr == '\\') {
                        if (*(ptr+1) == '\\') {
                                memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                if (*(ptr+1) == delim) {
                                        *ptr = delim;
                                        memmove (ptr+1, ptr+2, total - offset);
                                        offset += 2;
                                }
                                else {
                                        *str = 0;
                                        return str;
                                }
                        }
                }
                else
                        offset ++;

                ptr++;
        }

        return str;
}

static gchar *
gda_oracle_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *id,
				      gboolean for_meta_store, gboolean force_quotes)
{
        GdaSqlReservedKeywordsFunc kwfunc;
        OracleConnectionData *cdata = NULL;

        if (cnc)
                cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);

        kwfunc = _gda_oracle_get_reserved_keyword_func (cdata);

	if (for_meta_store) {
		gchar *tmp, *ptr;
		tmp = ora_remove_quotes (g_strdup (id));
		if (kwfunc (tmp)) {
			ptr = gda_sql_identifier_force_quotes (tmp);
			g_free (tmp);
			return ptr;
		}
		else if (force_quotes) {
			/* quote if non UC characters or digits at the 1st char or non allowed characters */
			for (ptr = tmp; *ptr; ptr++) {
				if (((*ptr >= 'A') && (*ptr <= 'Z')) ||
				    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
				    (*ptr == '_'))
					continue;
				else {
					ptr = gda_sql_identifier_force_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			for (ptr = tmp; *ptr; ptr++) {
				if ((*ptr >= 'A') && (*ptr <= 'Z'))
					*ptr += 'a' - 'A';
			}
			return tmp;
		}
		else {
			for (ptr = tmp; *ptr; ptr++) {
				if (*id == '"') {
					if (((*ptr >= 'A') && (*ptr <= 'Z')) ||
					    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
					    (*ptr == '_'))
						continue;
					else {
						ptr = gda_sql_identifier_force_quotes (tmp);
						g_free (tmp);
						return ptr;
					}
				}
				else if ((*ptr >= 'A') && (*ptr <= 'Z'))
					*ptr += 'a' - 'A';
				else if ((*ptr >= '0') && (*ptr <= '9') && (ptr == tmp)) {
					ptr = gda_sql_identifier_force_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			if (*id == '"') {
				for (ptr = tmp; *ptr; ptr++) {
					if ((*ptr >= 'A') && (*ptr <= 'Z'))
						*ptr += 'a' - 'A';
				}
			}
			return tmp;
		}
	}
	else {
		if (*id == '"') {
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
gda_oracle_free_cnc_data (OracleConnectionData *cdata)
{
	if (!cdata)
		return;

	if (cdata->hsession)
                OCIHandleFree ((dvoid *) cdata->hsession, OCI_HTYPE_SESSION);
        if (cdata->hservice)
                OCIHandleFree ((dvoid *) cdata->hservice, OCI_HTYPE_SVCCTX);
        if (cdata->hserver)
                OCIHandleFree ((dvoid *) cdata->hserver, OCI_HTYPE_SERVER);
        if (cdata->herr)
                OCIHandleFree ((dvoid *) cdata->herr, OCI_HTYPE_ERROR);
        if (cdata->henv)
                OCIHandleFree ((dvoid *) cdata->henv, OCI_HTYPE_ENV);

	g_free (cdata->schema);
	g_free (cdata->version);

	g_free (cdata);
}
