/*
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2004 Jeronimo Albi <jeronimoalbi@yahoo.com.ar>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2004 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
#include "gda-firebird.h"
#include "gda-firebird-provider.h"
#include "gda-firebird-recordset.h"
#include "gda-firebird-ddl.h"
#include "gda-firebird-meta.h"
#include "gda-firebird-parser.h"
#include "gda-firebird-util.h"
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

#define FILE_EXTENSION ".fdb"

/*
 * GObject methods
 */
static void gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass);
static void gda_firebird_provider_init       (GdaFirebirdProvider *provider,
					      GdaFirebirdProviderClass *klass);
static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_firebird_provider_open_connection (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GdaQuarkList *params,
								  GdaQuarkList *auth);
static gboolean            gda_firebird_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_firebird_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_firebird_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_firebird_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperationType type,
								   GdaSet *options, GError **error);
static gchar              *gda_firebird_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperation *op, GError **error);

static gboolean            gda_firebird_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								    GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_firebird_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								    const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_firebird_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								     const gchar *name, GError **error);
static gboolean            gda_firebird_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								       const gchar *name, GError **error);
static gboolean            gda_firebird_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GError **error);
static gboolean            gda_firebird_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								     const gchar *name, GError **error);
static gboolean            gda_firebird_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_firebird_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_firebird_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaConnectionFeature feature);

static GdaWorker          *gda_firebird_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static const gchar        *gda_firebird_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_firebird_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
								   GType g_type, const gchar *dbms_type);

static const gchar*        gda_firebird_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
									GType type);
/* statements */
static GdaSqlParser        *gda_firebird_provider_create_parser (GdaServerProvider *provider,
								 GdaConnection *cnc);
static gchar               *gda_firebird_provider_statement_to_sql  (GdaServerProvider *provider,
								     GdaConnection *cnc,
								     GdaStatement *stmt,
								     GdaSet *params,
								     GdaStatementSqlFlag flags,
								     GSList **params_used,
								     GError **error);
static gboolean             gda_firebird_provider_statement_prepare (GdaServerProvider *provider,
								     GdaConnection *cnc,
								     GdaStatement *stmt, GError **error);
static GObject             *gda_firebird_provider_statement_execute (GdaServerProvider *provider,
								     GdaConnection *cnc,
								     GdaStatement *stmt,
								     GdaSet *params,
								     GdaStatementModelUsage model_usage,
								     GType *col_types,
								     GdaSet **last_inserted_row, GError **error);

/* distributed transactions */
static gboolean gda_firebird_provider_xa_start    (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   const GdaXaTransactionId *xid,
						   GError **error);

static gboolean gda_firebird_provider_xa_end      (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   const GdaXaTransactionId *xid,
						   GError **error);

static gboolean gda_firebird_provider_xa_prepare  (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   const GdaXaTransactionId *xid,
						   GError **error);

static gboolean gda_firebird_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_firebird_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);

static GList   *gda_firebird_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc,
						   GError **error);

/* 
 * private connection data destroy 
 */
static void gda_firebird_free_cnc_data (FirebirdConnectionData *cdata);


static gchar	*fb_server_get_version (FirebirdConnectionData *fcnc);


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

gchar *internal_sql[] = {
	"SQL 'firebird' FROM RDB$DATABASE"
};

/*
 * GdaFirebirdProvider class implementation
 */
GdaServerProviderBase firebird_base_functions = {
	gda_firebird_provider_get_name,
	gda_firebird_provider_get_version,
	gda_firebird_provider_get_server_version,
	gda_firebird_provider_supports_feature,
	gda_firebird_provider_create_worker,
	NULL,
	gda_firebird_provider_create_parser,
	gda_firebird_provider_get_data_handler,
	gda_firebird_provider_get_default_dbms_type,
	gda_firebird_provider_supports_operation,
	gda_firebird_provider_create_operation,
	gda_firebird_provider_render_operation,
	gda_firebird_provider_statement_to_sql,
	NULL,
	NULL,
	gda_firebird_provider_open_connection,
	NULL,
	gda_firebird_provider_close_connection,
	NULL,
	NULL,
	gda_firebird_provider_perform_operation,
	gda_firebird_provider_begin_transaction,
	gda_firebird_provider_commit_transaction,
	gda_firebird_provider_rollback_transaction,
	gda_firebird_provider_add_savepoint,
	gda_firebird_provider_rollback_savepoint,
	gda_firebird_provider_delete_savepoint,
	gda_firebird_provider_statement_prepare,
	gda_firebird_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta firebird_meta_functions = {
	_gda_firebird_meta__info,
	_gda_firebird_meta__btypes,
	_gda_firebird_meta__udt,
	_gda_firebird_meta_udt,
	_gda_firebird_meta__udt_cols,
	_gda_firebird_meta_udt_cols,
	_gda_firebird_meta__enums,
	_gda_firebird_meta_enums,
	_gda_firebird_meta__domains,
	_gda_firebird_meta_domains,
	_gda_firebird_meta__constraints_dom,
	_gda_firebird_meta_constraints_dom,
	_gda_firebird_meta__el_types,
	_gda_firebird_meta_el_types,
	_gda_firebird_meta__collations,
	_gda_firebird_meta_collations,
	_gda_firebird_meta__character_sets,
	_gda_firebird_meta_character_sets,
	_gda_firebird_meta__schemata,
	_gda_firebird_meta_schemata,
	_gda_firebird_meta__tables_views,
	_gda_firebird_meta_tables_views,
	_gda_firebird_meta__columns,
	_gda_firebird_meta_columns,
	_gda_firebird_meta__view_cols,
	_gda_firebird_meta_view_cols,
	_gda_firebird_meta__constraints_tab,
	_gda_firebird_meta_constraints_tab,
	_gda_firebird_meta__constraints_ref,
	_gda_firebird_meta_constraints_ref,
	_gda_firebird_meta__key_columns,
	_gda_firebird_meta_key_columns,
	_gda_firebird_meta__check_columns,
	_gda_firebird_meta_check_columns,
	_gda_firebird_meta__triggers,
	_gda_firebird_meta_triggers,
	_gda_firebird_meta__routines,
	_gda_firebird_meta_routines,
	_gda_firebird_meta__routine_col,
	_gda_firebird_meta_routine_col,
	_gda_firebird_meta__routine_par,
	_gda_firebird_meta_routine_par,
	_gda_firebird_meta__indexes_tab,
        _gda_firebird_meta_indexes_tab,
        _gda_firebird_meta__index_cols,
        _gda_firebird_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderXa firebird_xa_functions = {
	gda_firebird_provider_xa_start,
	gda_firebird_provider_xa_end,
	gda_firebird_provider_xa_prepare,
	gda_firebird_provider_xa_commit,
	gda_firebird_provider_xa_rollback,
	gda_firebird_provider_xa_recover,

	NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &firebird_base_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &firebird_meta_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) &firebird_xa_functions);
}

static void
gda_firebird_provider_init (GdaFirebirdProvider *firebird_prv, GdaFirebirdProviderClass *klass)
{
	g_mutex_lock (&init_mutex);
	if (!internal_stmt) {
		InternalStatementItem i;
		GdaSqlParser *parser;

		parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) firebird_prv);
		internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
		for (i = INTERNAL_STMT1; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
			internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
			if (!internal_stmt[i])
				g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
	}

	/* meta data init */
	_gda_firebird_provider_meta_init ((GdaServerProvider*) firebird_prv);

	g_mutex_unlock (&init_mutex);
}

GType
gda_firebird_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaFirebirdProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_provider_class_init,
			NULL, NULL,
			sizeof (GdaFirebirdProvider),
			0,
			(GInstanceInitFunc) gda_firebird_provider_init
		};
		g_mutex_lock (&registering);
		if (type == 0) {
#ifdef FIREBIRD_EMBED
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaFirebirdProviderEmbed", &info, 0);
#else
			type = g_type_register_static (GDA_TYPE_SERVER_PROVIDER, "GdaFirebirdProvider", &info, 0);
#endif
		}
		g_mutex_unlock (&registering);
	}

	return type;
}

static GdaWorker *
gda_firebird_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* For a start see http://www.firebirdsql.org/file/documentation/drivers_documentation/python/3.3.0/thread-safety-overview.html
	 * For now we consider the client to be non thread safe */

	static GdaWorker *unique_worker = NULL;
	return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_firebird_provider_get_name (GdaServerProvider *provider)
{
	return FIREBIRD_PROVIDER_NAME;
}

/* 
 * Get provider's version, no need to change this
 */
static const gchar *
gda_firebird_provider_get_version (GdaServerProvider *provider)
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
 *   - create a FirebirdConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_firebird_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Check for connection parameters */
	const gchar *fb_db, *fb_dir, *fb_user, *fb_password, *fb_host;
	gchar *fb_conn;
	
	fb_db = (gchar *) gda_quark_list_find (params, "DB_NAME");
	fb_dir = (gchar *) gda_quark_list_find (params, "DB_DIR");
	fb_host	= (gchar *) gda_quark_list_find (params, "HOST");
	fb_user	= (gchar *) gda_quark_list_find (auth, "USERNAME");
	fb_password = (gchar *) gda_quark_list_find (auth, "PASSWORD");
	
	if (!fb_db) {
		gda_connection_add_event_string (cnc, "%s", _("The connection string must contain the DB_NAME values"));
		return FALSE;
	}
	if (!fb_dir)
		fb_dir = ".";
			
	/* prepare DPB */
	GString *dpb_string;
	dpb_string = g_string_new ("");
	g_string_append_c (dpb_string, isc_dpb_version1);

	/* Set user name */
	if (fb_user) {
		size_t len;
		len = strlen (fb_user);
		if (len > 256) {
			gda_connection_add_event_string (cnc, _("The parameter '%s' is too long"), "USERNAME");
			g_string_free (dpb_string, TRUE);
			return FALSE;
		}
		g_string_append_c (dpb_string, isc_dpb_user_name);
		g_string_append_c (dpb_string, len);
		g_string_append (dpb_string, fb_user);
	}

	/* Set password */
	if (fb_password) {
		size_t len;
		len = strlen (fb_password);
		if (len > 256) {
			gda_connection_add_event_string (cnc, _("The parameter '%s' is too long"), "PASSWORD");
			g_string_free (dpb_string, TRUE);
			return FALSE;
		}
		g_string_append_c (dpb_string, isc_dpb_password);
		g_string_append_c (dpb_string, len);
		g_string_append (dpb_string, fb_password);
		
	}

	/* Set character set */
	g_string_append_c (dpb_string, isc_dpb_lc_ctype);
	g_string_append_c (dpb_string, strlen ("UTF8"));
	g_string_append (dpb_string, "UTF8");

	if (fb_host)
		fb_conn = g_strconcat (fb_host, ":", fb_db, NULL);
	else {
		gchar *tmp;
		tmp = g_strdup_printf ("%s%s", fb_db, FILE_EXTENSION);
		fb_conn = g_build_filename (fb_dir, tmp, NULL);
		g_free (tmp);

		if (! g_file_test (fb_conn, G_FILE_TEST_EXISTS)) {
			g_free (fb_conn);
			fb_conn = g_build_filename (fb_dir, fb_db, NULL);
		}
	}

	ISC_STATUS_ARRAY status_vector;
	isc_db_handle handle = 0L;
	if (isc_attach_database (status_vector, strlen (fb_conn), fb_conn, &handle,
				 dpb_string->len, dpb_string->str)) {
		ISC_SCHAR *msg;
		const ISC_STATUS *p = status_vector;
		GdaConnectionEvent *ev;

		msg = g_new0 (ISC_SCHAR, 512);
		fb_interpret (msg, 511, &p);
		ev = gda_connection_add_event_string (cnc, "%s", msg);
		g_free (msg);
		gda_connection_event_set_code (ev, isc_sqlcode (status_vector));

		g_free (fb_conn);
		g_string_free (dpb_string, TRUE);
		return FALSE;
	}

	/* connection is now opened:
	 * Create a new instance of the provider specific data associated to a connection (FirebirdConnectionData),
	 * and set its contents, and open the real connection to the database */
	FirebirdConnectionData *cdata;
	cdata = g_new0 (FirebirdConnectionData, 1);
	cdata->handle = handle;
	cdata->dpb = g_string_free (dpb_string, FALSE);
	cdata->dbname = fb_conn;
	cdata->server_version = fb_server_get_version (cdata);

	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_firebird_free_cnc_data);
	
	return TRUE;
}

/* 
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated FirebirdConnectionData structure
 *   - Free the FirebirdConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_firebird_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	/* detach from database */
	isc_detach_database (cdata->status, &(cdata->handle));
	cdata->handle = 0L;

	/* Free the FirebirdConnectionData structure and its contents*/
	gda_firebird_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated FirebirdConnectionData structure
 */
static const gchar *
gda_firebird_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) 
		return FALSE;

	return ((const gchar *) cdata->server_version);
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a firebird_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_firebird_provider_render_operation() and gda_firebird_provider_perform_operation() methods
 *
 */
static gboolean
gda_firebird_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
					  GdaServerOperationType type, GdaSet *options)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	return FALSE;

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
gda_firebird_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
					GdaServerOperationType type, GdaSet *options, GError **error)
{
        gchar *file, *stype;
        GdaServerOperation *op;
        gchar *str;
	gchar *dir;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

        stype = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
        str = g_strdup_printf ("firebird_specs_%s.xml", stype);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

  if (!file)
    file = g_strdup_printf ("/spec/firebird/%s.raw.xml", stype);
        
  op = GDA_SERVER_OPERATION (g_object_new (GDA_TYPE_SERVER_OPERATION, 
                                           "op-type", type, 
                                           "spec-resource", file, 
                                           "connection", cnc,
                                           "provider", provider,
                                           NULL));
  g_free (stype);
  g_free (file);

        return op;
}

/*
 * Render operation request
 */
static gchar *
gda_firebird_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
        str = g_strdup_printf ("firebird_specs_%s.xml", file);
        g_free (file);

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        file = gda_server_provider_find_file (provider, dir, str);
	g_free (dir);
        g_free (str);

        if (! file) {
                g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_FILE_NOT_FOUND_ERROR,
			     _("Missing spec. file '%s'"), file);
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
                sql = gda_firebird_render_CREATE_TABLE (provider, cnc, op, error);
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
gda_firebird_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *name,
					 GdaTransactionIsolation level,
					 GError **error)
{
	FirebirdConnectionData *cdata;
        static char tpb[] = {
                isc_tpb_version3,
                isc_tpb_write,
                isc_tpb_read_committed,
                isc_tpb_rec_version,
                isc_tpb_wait
        };

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	if (level != GDA_TRANSACTION_ISOLATION_UNKNOWN) {
		gda_connection_add_event_string (cnc, "Provider does not handle that kind of transaction");
		return FALSE;
	}

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	if (cdata->ftr) {
		gda_connection_add_event_string (cnc, _("Transaction already started"));
		return FALSE;
	}

	/* start the transaction */
	cdata->ftr = g_new0 (isc_tr_handle, 1);
	if (isc_start_transaction (cdata->status, (cdata->ftr), 1, &(cdata->handle),
				   (unsigned short) sizeof (tpb), &tpb)) {
		_gda_firebird_make_error (cnc, 0);
		g_free (cdata->ftr);
		cdata->ftr = NULL;

		return FALSE;
	}

	gda_connection_internal_transaction_started (cnc, NULL, name, level);

	return TRUE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_firebird_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	FirebirdConnectionData *cdata;
	gboolean result;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	if (!cdata->ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_commit_transaction (cdata->status, cdata->ftr)) {
		_gda_firebird_make_error (cnc, 0);
		result = FALSE;
	}
	else {
		gda_connection_internal_transaction_committed (cnc, name);
		result = TRUE;
	}

	g_free (cdata->ftr);
	cdata->ftr = NULL;

	return result;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_firebird_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					    const gchar *name, GError **error)
{
	FirebirdConnectionData *cdata;
	gboolean result = FALSE;
	//WHERE_AM_I;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	if (!cdata->ftr) {
		gda_connection_add_event_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_rollback_transaction (cdata->status, cdata->ftr)) {
		_gda_firebird_make_error (cnc, 0);
		result = FALSE;
	}
	else {
		gda_connection_internal_transaction_committed (cnc, name);
		result = TRUE;
	}

	g_free (cdata->ftr);
	cdata->ftr = NULL;

	return result;
}

/*
 * Add savepoint request
 */
static gboolean
gda_firebird_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				     const gchar *name, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_firebird_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_firebird_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;

	return FALSE;
}

/*
 * Feature support request
 */
static gboolean
gda_firebird_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
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
gda_firebird_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_firebird_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
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
		return "smallint";
	
	if ((type == G_TYPE_DATE) || 
	    (type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TIME) ||
	    (type == GDA_TYPE_TIMESTAMP))
		return "string";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "double";
	
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
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
gda_firebird_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	return (GdaSqlParser*) g_object_new (GDA_TYPE_FIREBIRD_PARSER, "tokenizer-flavour",
                                             GDA_SQL_PARSER_FLAVOUR_STANDARD, NULL);
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
gda_firebird_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
					GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	return gda_statement_to_sql_extended (stmt, NULL, params, flags, params_used, error);
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaFirebirdPStmt object and declare it to @cnc.
 */
static gboolean
gda_firebird_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
					 GdaStatement *stmt, GError **error)
{
	GdaFirebirdPStmt *ps;
	FirebirdConnectionData	*cdata;
	gboolean result = FALSE;

	int            buffer[2048];

	XSQLVAR         *var;
	short           num_cols, i;
	short           length, alignment, type, offset;
	int             fetch_stat;
	static char     stmt_info[] = { isc_info_sql_stmt_type };
	char            info_buffer[20];
	short           l;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaFirebirdPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* render as SQL understood by Firebird */
	GdaSet *params = NULL;
	gchar *sql = NULL;
	GSList *used_params = NULL;
	gboolean trans_started = FALSE;

	if (!gda_statement_get_parameters (stmt, &params, error))
		goto out_err;

	sql = gda_firebird_provider_statement_to_sql (provider, NULL, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_UQMARK,
						      &used_params, error);
	if (!sql)
		goto out_err;

	/* get private connection data */
	cdata = (FirebirdConnectionData *) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		goto out_err;

	/* create the stmt object */
	ps = (GdaFirebirdPStmt *) g_object_new (GDA_TYPE_FIREBIRD_PSTMT, NULL);
	ps->stmt_h = 0L;

	/* actually prepare statement */
	if (isc_dsql_allocate_statement (cdata->status, &(cdata->handle), &(ps->stmt_h)))
		goto out_err;

	if (! cdata->ftr) {
		if (!gda_firebird_provider_begin_transaction (provider, cnc, "prepare_tr",
							      GDA_TRANSACTION_ISOLATION_UNKNOWN, error))
			goto out_err;
		trans_started = TRUE;
	}

	if (! ps->sqlda){
		/* 
		 * Allocate enough space for 20 fields.  
		 * If more fields get selected, re-allocate SQLDA later.
		 */
		ps->sqlda = (XSQLDA *) g_malloc (XSQLDA_LENGTH(20)); //g_new0(XSQLDA, 20);
		ps->sqlda->sqln	= 20;
		ps->sqlda->version = SQLDA_VERSION1;
	}

	/* now prepare the fb statement */
	if (isc_dsql_prepare (cdata->status, cdata->ftr, &(ps->stmt_h), 0, sql, SQL_DIALECT_CURRENT, ps->sqlda)) {
		_gda_firebird_make_error (cnc, 0);
		goto out_err;
	}

	/* What is the statement type of this statement?
	 *
	 * stmt_info is a 1 byte info request.  info_buffer is a buffer
	 * large enough to hold the returned info packet
	 * The info_buffer returned contains a isc_info_sql_stmt_type in the first byte, 
	 * two bytes of length, and a statement_type token.
	 */

	if (!isc_dsql_sql_info (cdata->status, &(ps->stmt_h), sizeof (stmt_info), stmt_info,
				sizeof (info_buffer), info_buffer)) {
		l = (short) isc_vax_integer ((char *) info_buffer + 1, 2);
		ps->statement_type = isc_vax_integer ((char *) info_buffer + 3, l);
	}

	ps->is_non_select = !ps->sqlda->sqld;

	if (!ps->is_non_select){
		/*
		 * Process select statements.
		 */

		num_cols = ps->sqlda->sqld;

		/* Need more room. */
		if (ps->sqlda->sqln < num_cols)	{
			//g_print("Only have space for %d columns, need to re-allocate more space for %d columns\n", ps->sqlda->sqln, ps->sqlda->sqld );
			g_free (ps->sqlda);
			ps->sqlda = (XSQLDA *) g_malloc(XSQLDA_LENGTH(num_cols)); //g_new0(XSQLDA, num_cols);
			ps->sqlda->sqln	= num_cols;
			ps->sqlda->version = SQLDA_VERSION1;

			if (isc_dsql_describe (cdata->status, &(ps->stmt_h), SQL_DIALECT_V6, ps->sqlda))
				goto out_err;
			num_cols = ps->sqlda->sqld;
		}

		/*
		 * Set up SQLDA.
		 */
		//g_print("set up SQLDA\n");
		for (var = ps->sqlda->sqlvar, offset = 0, i = 0; i < num_cols; var++, i++) {
			length = alignment = var->sqllen;
			type = var->sqltype & ~1;
			var->sqlname[var->sqlname_length + 1] = '\0';
			var->relname[var->relname_length + 1] = '\0';
			var->ownname[var->ownname_length + 1] = '\0';
			var->aliasname[var->aliasname_length + 1] = '\0';

			if (type == SQL_TEXT)
				alignment = 1;
			else if (type == SQL_VARYING) {
				length += sizeof (short) + 1;
				alignment = sizeof (short);
			}

			/*  RISC machines are finicky about word alignment
			 *  So the output buffer values must be placed on
			 *  word boundaries where appropriate
			 */
			gchar *buff = g_new0(gchar, 2048);

			offset = FB_ALIGN(offset, alignment);
			var->sqldata = (char *) buff + offset;
			offset += length;
			offset = FB_ALIGN(offset, sizeof (short));
			var->sqlind = (short*) ((char *) buff + offset);
			offset += sizeof (short);
		}
	}

	//SETUP INPUT-SQLDA
	//NOW ALLOCATE SPACE FOR THE INPUT PARAMETERS
	if (ps->input_sqlda)
		g_free(ps->input_sqlda);

	ps->input_sqlda	= (XSQLDA *) g_new0(XSQLDA, 1);
	ps->input_sqlda->version = SQLDA_VERSION1;
	ps->input_sqlda->sqln = 1;
	
	isc_dsql_describe_bind(cdata->status, &(ps->stmt_h), 1, ps->input_sqlda);

	if ((cdata->status[0] == 1) && cdata->status[1]){
		// Process error
		isc_print_status (cdata->status);
		goto out_err;
	}

	if (ps->input_sqlda->sqld > ps->input_sqlda->sqln){
		gint n = ps->input_sqlda->sqld;
		g_free(ps->input_sqlda);
		ps->input_sqlda	= (XSQLDA *) g_new0(XSQLDA, n);
		ps->input_sqlda->sqln= n;
		ps->input_sqlda->version = SQLDA_VERSION1;

		isc_dsql_describe_bind(cdata->status, &(ps->stmt_h), n, ps->input_sqlda);
	
		if ((cdata->status[0] == 1) && cdata->status[1]){
			// Process error
			isc_print_status(cdata->status);
			goto out_err;
		}
	}

	if (!params){
		/* no input paramaters so clear the input_sqlda */
		g_free (ps->input_sqlda);
		ps->input_sqlda = NULL;
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
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
					     "%s", _("Unnamed parameter is not allowed in prepared statements"));
				g_slist_foreach (param_ids, (GFunc) g_free, NULL);
				g_slist_free (param_ids);
				goto out_err;
			}
		}

		g_slist_free (used_params);
	}


	/* create a prepared statement */
	//g_print("Adding the prepared statement to GDA.\n");
	//ps = (GdaFirebirdPStmt *) g_object_new (GDA_TYPE_FIREBIRD_PSTMT, NULL);
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
	_GDA_PSTMT (ps)->param_ids = param_ids;
	_GDA_PSTMT (ps)->sql = sql;

	gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
	//g_object_unref (ps);
	
	result  = TRUE;

 out_err:
 	if (!result){
		if (trans_started)
			gda_firebird_provider_rollback_transaction (provider, cnc, "prepare_tr", NULL);
		g_free (sql);
	}
	
	if (params)
		g_object_unref (params);
	
	return result;
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
gda_firebird_provider_statement_execute (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaStatement *stmt, 
					 GdaSet *params,
					 GdaStatementModelUsage model_usage,
					 GType *col_types,
					 GdaSet **last_inserted_row,
					 GError **error)
{
	GdaFirebirdPStmt	*ps;
	FirebirdConnectionData	*cdata;
	GSList			*mem_to_free = NULL;
	XSQLVAR			*fbvar;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	/* get/create new prepared statement */
	ps = (GdaFirebirdPStmt *)gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		g_print("prepare statement in execute stage\n");
		if (!gda_firebird_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaFirebirdPStmt object.
			 */
			g_print("Could not prepare the statement :-(.\n");
			//TO_IMPLEMENT;
			return NULL;
		}
		
		ps = (GdaFirebirdPStmt *)gda_connection_get_prepared_statement (cnc, stmt);
	}
	g_object_ref(ps);
	g_assert (ps);

	/* optionnally reset the prepared statement if required by the API */

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;

	for (i = 1, list = _GDA_PSTMT (ps)->param_ids; list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h;
		g_print("binding paramater %s\n", pname);
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
			gchar *str;
			str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, str);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
			g_free (str);
			break;
		}

		/* actual binding using the C API, for parameter at position @i */
		if (ps->input_sqlda == NULL) continue;
		
		g_print("bind the value to SQLDA\n");
		const GValue* value = gda_holder_get_value (h);
		
		fbvar = &(ps->input_sqlda->sqlvar[i-1]);
		if (!value || gda_value_is_null (value)) {
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			fbvar->sqlind = flag0; //TELLS FIREBIRD THAT THE COLUMN IS NULL
			*flag0 = -1; //TELLS FIREBIRD THAT THE COLUMN IS NULL
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_TIMESTAMP) {
			/*
			  const GdaTimestamp *ts;

			  ts = gda_value_get_timestamp (value);
			  if (!ts) {
			  fbvar->sqldata_type = MYSQL_TYPE_NULL;
			  fbvar.is_null = (my_bool*)1;
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

			  fbvar->sqldata_type= MYSQL_TYPE_TIMESTAMP;
			  fbvar->sqldata= (char *)mtime;
			  fbvar->sqllen = sizeof (MYSQL_TIME);
			  }
			*/
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_TIME) {
			/*
			  const GdaTime *ts;

			  ts = gda_value_get_time (value);
			  if (!ts) {
			  fbvar->sqldata_type = MYSQL_TYPE_NULL;
			  fbvar.is_null = (my_bool*)1;
			  }
			  else {
			  MYSQL_TIME *mtime;
			  mtime = g_new0 (MYSQL_TIME, 1);
			  mem_to_free = g_slist_prepend (mem_to_free, mtime);
			  mtime->hour = ts->hour;
			  mtime->minute = ts->minute;
			  mtime->second = ts->second;
			  mtime->second_part = ts->fraction;

			  fbvar->sqldata_type= MYSQL_TYPE_TIME;
			  fbvar->sqldata= (char *)mtime;
			  fbvar->sqllen = sizeof (MYSQL_TIME);
			  }
			*/
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_DATE) {
			/*
			  const GDate *ts;

			  ts = (GDate*) g_value_get_boxed (value);
			  if (!ts) {
			  fbvar->sqldata_type = MYSQL_TYPE_NULL;
			  fbvar.is_null = (my_bool*)1;
			  }
			  else {
			  MYSQL_TIME *mtime;
			  mtime = g_new0 (MYSQL_TIME, 1);
			  mem_to_free = g_slist_prepend (mem_to_free, mtime);
			  mtime->year = g_date_get_year (ts);
			  mtime->month = g_date_get_month (ts);
			  mtime->day = g_date_get_day (ts);

			  fbvar->sqldata_type= MYSQL_TYPE_DATE;
			  fbvar->sqldata= (char *)mtime;
			  fbvar->sqllen = sizeof (MYSQL_TIME);
			  }
			*/
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
			
			const gchar *str;// = "CLIENTS";
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			str = g_value_get_string (value);

			if (!str) {
				fbvar->sqlind = flag0;
				*flag0 = -1;
			}
			else {
				fbvar->sqldata		= g_value_dup_string(value); //str;
				//g_print("string-len: %d\n", fbvar->sqllen);
				fbvar->sqllen		= strlen (fbvar->sqldata);
				fbvar->sqlind		= flag0;
				*flag0 = 0;
			}
			
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE) {
			gdouble *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (gdouble, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = g_value_get_double (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (gdouble);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_FLOAT) {
			gfloat *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (gfloat, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = g_value_get_float (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (gfloat);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_CHAR) {
			gchar *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (gchar, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = g_value_get_schar (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (gchar);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_SHORT) {
			gshort *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (gshort, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = gda_value_get_short (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (gshort);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_LONG) {
			glong *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (glong, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = g_value_get_long (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (glong);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_INT64) {
			gint64 *pv;
			short *flag0 = g_new0 (short, 1);
			mem_to_free = g_slist_prepend (mem_to_free, flag0);
			
			pv = g_new (gint64, 1);
			mem_to_free = g_slist_prepend (mem_to_free, pv);
			*pv = g_value_get_long (value);
			fbvar->sqldata = (char*) pv;
			fbvar->sqllen = sizeof (gint64);
			fbvar->sqlind = flag0;
			*flag0 = 0;
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			/*
			  const GdaBinary *bin = NULL;
			  GdaBlob *blob = (GdaBlob*) gda_value_get_blob (value);

			  bin = ((GdaBinary*) blob);
			  if (!bin) {
			  fbvar->sqldata_type = MYSQL_TYPE_NULL;
			  fbvar.is_null = (my_bool*)1;
			  }
			  else {
			  gchar *str = NULL;
			  glong blob_len;
			  if (blob->op) {
			  blob_len = gda_blob_op_get_length (blob->op);
			  if ((blob_len != bin->binary_length) &&
			  ! gda_blob_op_read_all (blob->op, blob)) {
			  // force reading the complete BLOB into memory
			  str = _("Can't read whole BLOB into memory");
			  }
			  }
			  else
			  blob_len = bin->binary_length;
			  if (blob_len < 0)
			  str = _("Can't get BLOB's length");
			  else if (blob_len >= G_MAXINT)
			  str = _("BLOB is too big");
				
			  if (str) {
			  event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			  gda_connection_event_set_description (event, str);
			  g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			  GDA_SERVER_PROVIDER_DATA_ERROR, "%s", str);
			  break;
			  }
				
			  else {
			  fbvar->sqldata_type= MYSQL_TYPE_BLOB;
			  fbvar->sqldata= (char *) bin->data;
			  fbvar->sqllen = bin->binary_length;
			  fbvar.length = NULL;
			  }
			  }
			*/
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			/*
			  const GdaBinary *bin;
			  bin = gda_value_get_binary (value);
			  if (!bin) {
			  fbvar->sqldata_type = MYSQL_TYPE_NULL;
			  fbvar.is_null = (my_bool*)1;
			  }
			  else {
			  fbvar->sqldata_type= MYSQL_TYPE_BLOB;
			  fbvar->sqldata= (char *) bin->data;
			  fbvar->sqllen = bin->binary_length;
			  fbvar.length = NULL;
			  }
			*/
		}
		else {
			GdaDataHandler *data_handler =
				gda_server_provider_get_data_handler_g_type (provider, cnc, 
									     G_VALUE_TYPE (value));
			if (data_handler == NULL) {
				/* there is an error here */
				gchar *str;
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				str = g_strdup_printf (_("Unhandled data type '%s'"),
						       gda_g_type_to_string (G_VALUE_TYPE (value)));
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR, "%s", str);
				g_free (str);
				break;
			}
			else {
				gchar *str;
				short *flag0 = g_new0 (short, 1);
				mem_to_free = g_slist_prepend (mem_to_free, flag0);
				
				str = gda_data_handler_get_str_from_value (data_handler, value);
				mem_to_free = g_slist_prepend (mem_to_free, str);
				fbvar->sqldata = str;
				fbvar->sqllen = strlen (str);
				fbvar->sqlind = flag0;
				*flag0 = 0;
			}
		}
	}

	if (event) {
		gda_connection_add_event (cnc, event);
		return NULL;
	}
	
	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event, _GDA_PSTMT (ps)->sql);
	gda_connection_add_event (cnc, event);
	
	/* execute prepared statement using C API depending on its kind */
	if (! g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "SELECT", 6) ||
            ! g_ascii_strncasecmp (_GDA_PSTMT (ps)->sql, "EXPLAIN", 7)) {
		GObject *data_model;
		GdaDataModelAccessFlags flags;

		if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

		g_print("get the data model\n");

		data_model = (GObject *) gda_firebird_recordset_new (cnc, ps, params, flags, col_types);
		
		g_print("assign the data model\n");
		gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
		
		
		if (mem_to_free){
			g_print("free the memory for the input variables\n");
			g_slist_foreach	(mem_to_free, (GFunc) g_free, NULL);
			g_slist_free	(mem_to_free);
		}
		g_object_unref(ps);
		return data_model;
        }
	else {
		GdaSet *set = NULL;
		//TODO: What the hell must I do here?
		//TO_IMPLEMENT;
		g_print("SQL: %s\n\n", _GDA_PSTMT (ps)->sql);
		if (isc_dsql_execute(cdata->status, cdata->ftr, &(ps->stmt_h), SQL_DIALECT_V6, NULL)) {
			isc_print_status(cdata->status);
			g_print("\n");
		}
		/*
		  gchar		count_item[]	= { isc_info_sql_records };
		  char		res_buffer[64];
		  gint		affected = 0;
		  gint		length = 0;

		  isc_dsql_sql_info (cdata->status
		  , &(ps->stmt_h)
		  , sizeof (count_item)
		  , count_item
		  , sizeof (res_buffer)
		  , res_buffer);
		  if (res_buffer[0] ==  isc_info_sql_records){
		  unsigned i = 3, result_size = isc_vax_integer(&res_buffer[1],2);

		  while (res_buffer[i] != isc_info_end && i < result_size) {
		  short len = (short)isc_vax_integer(&res_buffer[i+1],2);
		  if (res_buffer[i] != isc_info_req_select_count) {
		  affected += isc_vax_integer(&res_buffer[i+3],len);
		  }
		  i += len+3;
		  }
		  }
		*/
	
		/* Create a #GdaSet containing "IMPACTED_ROWS" */
		/* Create GdaConnectionEvent notice with the type of command and impacted rows */

		if (mem_to_free){
			g_slist_foreach	(mem_to_free, (GFunc) g_free, NULL);
			g_slist_free	(mem_to_free);
		}

		gda_connection_internal_statement_executed (cnc, stmt, params, event); /* required: help @cnc keep some stats */
		return (GObject*) set;
	}
}

/*
 * starts a distributed transaction: put the XA transaction in the ACTIVE state
 */
static gboolean
gda_firebird_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc,
				const GdaXaTransactionId *xid, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_firebird_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc,
			      const GdaXaTransactionId *xid, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_firebird_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				  const GdaXaTransactionId *xid, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_firebird_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc,
				 const GdaXaTransactionId *xid, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return FALSE;

	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_firebird_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc,
				   const GdaXaTransactionId *xid, GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
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
gda_firebird_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				  GError **error)
{
	FirebirdConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata) 
		return NULL;

	TO_IMPLEMENT;
	return NULL;
}

/*
 * Free connection's specific data
 */
static void
gda_firebird_free_cnc_data (FirebirdConnectionData *cdata)
{
	if (!cdata)
		return;

	if (cdata->handle)
		isc_detach_database (cdata->status, &(cdata->handle));
	g_free (cdata->dpb);
	g_free (cdata->dbname);
	g_free (cdata->server_version);

	g_free (cdata);
}

/*
 *  fb_server_get_version
 *
 *  Gets Firebird connection's server version number
 *
 *  Returns: A string containing server version, or NULL if error
 *           String must be released after use
 */
static gchar *
fb_server_get_version (FirebirdConnectionData *fcnc)
{
	gchar buffer[254], item, *p_buffer;
	gint length;
	gchar fdb_info[] = {
		isc_info_isc_version,
		isc_info_end
	};

	/* Try to get database version */
	if (! isc_database_info (fcnc->status, &(fcnc->handle), sizeof (fdb_info), fdb_info,
				 sizeof (buffer), buffer)) {
		p_buffer = buffer;
		if (*p_buffer != isc_info_end) {
			item = *p_buffer++;
			length = isc_vax_integer (p_buffer, 2);
			p_buffer += 2;
			if (item == isc_info_isc_version)
				return g_strndup ((const gchar *) &p_buffer[2], length-2);
		}
	}

	return NULL;
}
