/*
 * Copyright (C) 2001 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 Denis Loginov <dloginov@crl.nmsu.edu>
 * Copyright (C) 2004 José María Casanova Crespo <jmcasanova@igalia.com>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2015 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2017-2019 Daniel Espinosa <esodan@gmail.com>
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
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-statement-extra.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-postgres.h"
#include "gda-postgres-provider.h"
#include "gda-postgres-handler-bin.h"
#include "gda-postgres-recordset.h"
#include "gda-postgres-ddl.h"
#include "gda-postgres-meta.h"
#include "gda-postgres-util.h"
#include "gda-postgres-blob-op.h"
#include "gda-postgres-parser.h"
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

/*
 * GObject methods
 */
static void gda_postgres_provider_class_init (GdaPostgresProviderClass *klass);
static void gda_postgres_provider_init       (GdaPostgresProvider *provider);
static void gda_postgres_provider_dispose (GObject *object);


typedef struct {
  /*
   * Prepared internal statements
   * TO_ADD: any prepared statement to be used internally by the provider should be
   *         declared here, as constants and as SQL statements
   */
  GdaStatement **internal_stmt;
} GdaPostgresProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaPostgresProvider, gda_postgres_provider, GDA_TYPE_SERVER_PROVIDER)

/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_postgres_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
								  GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_postgres_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_postgres_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_postgres_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_postgres_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_postgres_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperationType type,
								   GdaSet *options, GError **error);
static gchar              *gda_postgres_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperation *op, GError **error);

static gboolean            gda_postgres_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								    GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_postgres_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								    const gchar *name, GdaTransactionIsolation level, GError **error);
static gboolean            gda_postgres_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								     const gchar *name, GError **error);
static gboolean            gda_postgres_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								       const gchar *name, GError **error);
static gboolean            gda_postgres_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								const gchar *name, GError **error);
static gboolean            gda_postgres_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								     const gchar *name, GError **error);
static gboolean            gda_postgres_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_postgres_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_postgres_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaConnectionFeature feature);

static GdaWorker          *gda_postgres_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static const gchar        *gda_postgres_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_postgres_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
								   GType g_type, const gchar *dbms_type);

static const gchar*        gda_postgres_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
									GType type);
/* statements */
static GdaSqlParser        *gda_postgres_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_postgres_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaStatement *stmt, GdaSet *params,
								     GdaStatementSqlFlag flags,
								     GSList **params_used, GError **error);
static gboolean             gda_postgres_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaStatement *stmt, GError **error);
static GObject             *gda_postgres_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								     GdaStatement *stmt, GdaSet *params,
								     GdaStatementModelUsage model_usage,
								     GType *col_types, GdaSet **last_inserted_row, GError **error);

/* Quoting */
static gchar               *gda_postgres_provider_identifier_quote    (GdaServerProvider *provider, GdaConnection *cnc,
								       const gchar *id,
								       gboolean meta_store_convention, gboolean force_quotes);

static GdaSqlStatement     *gda_postgres_provider_statement_rewrite   (GdaServerProvider *provider, GdaConnection *cnc,
								       GdaStatement *stmt, GdaSet *params, GError **error);

/* string escaping */
static gchar               *gda_postgres_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *string);
static gchar               *gda_postgres_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *string);

/* distributed transactions */
static gboolean gda_postgres_provider_xa_start    (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_postgres_provider_xa_end      (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_postgres_provider_xa_prepare  (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);

static gboolean gda_postgres_provider_xa_commit   (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);
static gboolean gda_postgres_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc,
						   const GdaXaTransactionId *xid, GError **error);

static GList   *gda_postgres_provider_xa_recover  (GdaServerProvider *provider, GdaConnection *cnc,
						   GError **error);

/*
 * private connection data destroy
 */
static void gda_postgres_free_cnc_data (PostgresConnectionData *cdata);

typedef enum {
	I_STMT_BEGIN,
	I_STMT_COMMIT,
	I_STMT_ROLLBACK,
	I_STMT_XA_PREPARE,
	I_STMT_XA_COMMIT,
	I_STMT_XA_ROLLBACK,
	I_STMT_XA_RECOVER
} InternalStatementItem;

static gchar *internal_sql[] = {
	"BEGIN",
	"COMMIT",
	"ROLLBACK",
	"PREPARE TRANSACTION ##xid::string",
	"COMMIT PREPARED ##xid::string",
	"ROLLBACK PREPARED ##xid::string",
	"SELECT gid FROM pg_catalog.pg_prepared_xacts"
};

/*
 * GdaPostgresProvider class implementation
 */
GdaServerProviderBase postgres_base_functions = {
	gda_postgres_provider_get_name,
	gda_postgres_provider_get_version,
	gda_postgres_provider_get_server_version,
	gda_postgres_provider_supports_feature,
	gda_postgres_provider_create_worker,
	NULL,
	gda_postgres_provider_create_parser,
	gda_postgres_provider_get_data_handler,
	gda_postgres_provider_get_default_dbms_type,
	gda_postgres_provider_supports_operation,
	gda_postgres_provider_create_operation,
	gda_postgres_provider_render_operation,
	gda_postgres_provider_statement_to_sql,
	gda_postgres_provider_identifier_quote,
	gda_postgres_provider_statement_rewrite,
	gda_postgres_provider_open_connection,
	gda_postgres_provider_prepare_connection,
	gda_postgres_provider_close_connection,
	gda_postgres_provider_escape_string,
	gda_postgres_provider_unescape_string,
	gda_postgres_provider_perform_operation,
	gda_postgres_provider_begin_transaction,
	gda_postgres_provider_commit_transaction,
	gda_postgres_provider_rollback_transaction,
	gda_postgres_provider_add_savepoint,
	gda_postgres_provider_rollback_savepoint,
	gda_postgres_provider_delete_savepoint,
	gda_postgres_provider_statement_prepare,
	gda_postgres_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderXa postgres_xa_functions = {
	gda_postgres_provider_xa_start,
	gda_postgres_provider_xa_end,
	gda_postgres_provider_xa_prepare,
	gda_postgres_provider_xa_commit,
	gda_postgres_provider_xa_rollback,
	gda_postgres_provider_xa_recover,

	NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_postgres_provider_class_init (GdaPostgresProviderClass *klass)
{
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (provider_class,
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &postgres_base_functions);
	GdaProviderReuseableOperations *ops;
	ops = _gda_postgres_reuseable_get_ops ();
	gda_server_provider_set_impl_functions (provider_class,
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &(ops->re_meta_funcs));

	gda_server_provider_set_impl_functions (provider_class,
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) &postgres_xa_functions);
  G_OBJECT_CLASS (provider_class)->dispose = gda_postgres_provider_dispose;
}

static void
gda_postgres_provider_init (GdaPostgresProvider *postgres_prv)
{
	GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (postgres_prv);
	InternalStatementItem i;
	GdaSqlParser *parser;

	parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) postgres_prv);
	priv->internal_stmt = g_new0 (GdaStatement *, sizeof (internal_sql) / sizeof (gchar*));
	for (i = I_STMT_BEGIN; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		priv->internal_stmt[i] = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
		if (!priv->internal_stmt[i])
			g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
	}

	/* meta data init */
	_gda_postgres_provider_meta_init ((GdaServerProvider*) postgres_prv);
}


static void gda_postgres_provider_dispose (GObject *object) {
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (object));
  if (priv->internal_stmt != NULL) {
    for (gulong i = I_STMT_BEGIN; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		  g_object_unref (priv->internal_stmt[i]);
	  }
    priv->internal_stmt = NULL;
  }
}

static GdaWorker *
gda_postgres_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* If PostgreSQL was not compiled with the --enable-thread-safety flag, then libPQ is not
	 * considered thread safe, and we limit the usage of the provider to one single thread */

	static GdaWorker *unique_worker = NULL;
	if (PQisthreadsafe ()) {
		if (for_cnc)
			return gda_worker_new ();
		else
			return gda_worker_new_unique (&unique_worker, TRUE);
	}
	else
		return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_postgres_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return POSTGRES_PROVIDER_NAME;
}

/*
 * Get provider's version, no need to change this
 */
static const gchar *
gda_postgres_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

static void
pq_notice_processor (GdaConnection *cnc, const char *message)
{
        GdaConnectionEvent *error;
	PostgresConnectionData *cdata;

        if (!message)
                return;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return;
        error = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
        gda_connection_event_set_description (error, message);
        gda_connection_event_set_code (error, -1);
        gda_connection_event_set_source (error, gda_connection_get_provider_name (cnc));
        gda_connection_event_set_sqlstate (error, "-1");

        gda_connection_add_event (cnc, error);
}

gboolean
determine_date_style (const gchar *str, guint year, guint month, guint day, GDateDMY *out_first,
		      GDateDMY *out_second, GDateDMY *out_third, gchar *out_sep)
{
	if (!str)
		return FALSE;

	guint nb;
	const gchar *ptr;
	GDateDMY order[3];
	gchar sep;

	/* 1st part */
	for (nb = 0, ptr = str; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == year)
		order[0] = G_DATE_YEAR;
	else if (nb == month)
		order[0] = G_DATE_MONTH;
	else if (nb == day)
		order[0] = G_DATE_DAY;
	else if (nb == year % 100)
		order[0] = G_DATE_YEAR;
	else
		return FALSE;

	/* separator */
	sep = *ptr;
	if (!sep)
		return FALSE;

	/* 2nd part */
	for (nb = 0, ptr++; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == year)
		order[1] = G_DATE_YEAR;
	else if (nb == month)
		order[1] = G_DATE_MONTH;
	else if (nb == day)
		order[1] = G_DATE_DAY;
	else if (nb == year % 100)
		order[1] = G_DATE_YEAR;
	else
		return FALSE;

	if (sep != *ptr)
		return FALSE;

	/* 3rd part */
	for (nb = 0, ptr++; *ptr; ptr++) {
		if ((*ptr <= '9') && (*ptr >= '0'))
			nb = nb * 10 + (*ptr - '0');
		else
			break;
	}
	if (nb == year)
		order[2] = G_DATE_YEAR;
	else if (nb == month)
		order[2] = G_DATE_MONTH;
	else if (nb == day)
		order[2] = G_DATE_DAY;
	else if (nb == year % 100)
		order[2] = G_DATE_YEAR;
	else
		return FALSE;

	/* result */
	if (out_first)
		*out_first = order [0];
	if (out_second)
		*out_second = order [1];
	if (out_third)
		*out_third = order [2];
	if (out_sep)
		*out_sep = sep;

	/*g_print ("POSTGRESQL date format recognized for [%s] is: %s%c%s%c%s\n", str,
	  (order [0] == G_DATE_DAY) ? "D" : ((order [0] == G_DATE_MONTH) ? "M" : "Y"), sep,
	  (order [1] == G_DATE_DAY) ? "D" : ((order [1] == G_DATE_MONTH) ? "M" : "Y"), sep,
	  (order [2] == G_DATE_DAY) ? "D" : ((order [2] == G_DATE_MONTH) ? "M" : "Y"));
	*/

	return TRUE;
}

static gboolean
adapt_to_date_format (GdaServerProvider *provider, GdaConnection *cnc, GError **error)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

	PGresult *pg_res;
	gboolean retval = FALSE;
        pg_res = _gda_postgres_PQexec_wrap (cnc, cdata->pconn, "SELECT DATE 'epoch' + 966334000 * INTERVAL '1 second'");
	if (pg_res && (PQresultStatus (pg_res) == PGRES_TUPLES_OK) &&
	    (PQntuples (pg_res) == 1) && (PQnfields (pg_res) == 1)) {
		gchar *str;
		GDateDMY parts[3];
		gchar sep;
		str = PQgetvalue (pg_res, 0, 0);
		retval = determine_date_style (str, 2000, 8, 15, &parts[0], &parts[1], &parts[2], &sep);
		if (retval) {
			cdata->date_first = parts[0];
			cdata->date_second = parts[1];
			cdata->date_third = parts[2];
			cdata->date_sep = sep;

			GdaDataHandler *dh;
			dh = gda_postgres_provider_get_data_handler (provider, cnc, G_TYPE_DATE_TIME, NULL);
                        gda_handler_time_set_sql_spec ((GdaHandlerTime *) dh, parts[0],
                                                       parts[1], parts[2], sep, FALSE);
                        gda_handler_time_set_str_spec ((GdaHandlerTime *) dh, parts[0],
                                                       parts[1], parts[2], sep, FALSE);
		}
		else
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", _("Could not determine the default date format"));
	}
	if (pg_res)
		PQclear (pg_res);

	return retval;
}

/*
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked
 *   - create a PostgresConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_postgres_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Check for connection parameters */
	const gchar *pq_host;
	const gchar *pq_db;
        const gchar *pq_port;
        const gchar *pq_options;
        const gchar *pq_tty;
        const gchar *pq_user = NULL;
        const gchar *pq_pwd = NULL;
        const gchar *pq_hostaddr;
        const gchar *pq_requiressl;
	const gchar *pq_connect_timeout;
        gchar *conn_string;
	pq_host = gda_quark_list_find (params, "HOST");
        pq_hostaddr = gda_quark_list_find (params, "HOSTADDR");
        pq_db = gda_quark_list_find (params, "DB_NAME");
        if (!pq_db) {
                const gchar *str;

                str = gda_quark_list_find (params, "DATABASE");
                if (!str) {
                        gda_connection_add_event_string (cnc,
                                                         _("The connection string must contain a DB_NAME value"));
                        return FALSE;
                }
                else {
                        g_warning (_("The connection string format has changed: replace DATABASE with "
                                     "DB_NAME and the same contents"));
                        pq_db = str;
                }
        }
        pq_port = gda_quark_list_find (params, "PORT");
        pq_options = gda_quark_list_find (params, "OPTIONS");
        pq_tty = gda_quark_list_find (params, "TTY");
	pq_user = gda_quark_list_find (auth, "USERNAME");
	if (!pq_user)
		pq_user = gda_quark_list_find (params, "USERNAME");

	pq_pwd = gda_quark_list_find (auth, "PASSWORD");
	if (!pq_pwd)
		pq_pwd = gda_quark_list_find (params, "PASSWORD");

        pq_requiressl = gda_quark_list_find (params, "USE_SSL");
	if (pq_requiressl && (*pq_requiressl != 'T') && (*pq_requiressl != 't'))
		pq_requiressl = NULL;
	pq_connect_timeout = gda_quark_list_find (params, "CONNECT_TIMEOUT");

	/* TODO: Escape single quotes and backslashes in the user name and password: */
        conn_string = g_strconcat ("",
                                   /* host: */
                                   pq_host ? "host='" : "",
                                   pq_host ? pq_host : "",
                                   pq_host ? "'" : "",
                                   /* hostaddr: */
                                   pq_hostaddr ? " hostaddr=" : "",
                                   pq_hostaddr ? pq_hostaddr : "",
                                   /* db: */
                                   pq_db ? " dbname='" : "",
                                   pq_db ? pq_db : "",
                                   pq_db ? "'" : "",
                                   /* port: */
                                   pq_port ? " port=" : "",
                                   pq_port ? pq_port : "",
                                   /* options: */
                                   pq_options ? " options='" : "",
                                   pq_options ? pq_options : "",
                                   pq_options ? "'" : "",
                                   /* tty: */
                                   pq_tty ? " tty=" : "",
                                   pq_tty ? pq_tty : "",
                                   /* user: */
                                   (pq_user && *pq_user) ? " user='" : "",
                                   (pq_user && *pq_user)? pq_user : "",
                                   (pq_user && *pq_user)? "'" : "",
                                   /* password: */
                                   (pq_pwd && *pq_pwd) ? " password='" : "",
                                   (pq_pwd && *pq_pwd) ? pq_pwd : "",
                                   (pq_pwd && *pq_pwd) ? "'" : "",
                                   pq_requiressl ? " requiressl=" : "",
                                   pq_requiressl ? pq_requiressl : "",
				   pq_connect_timeout ? " connect_timeout=" : "",
				   pq_connect_timeout ? pq_connect_timeout : "",
                                   NULL);

	/* open the real connection to the database */
	PGconn *pconn;
	pconn = PQconnectdb (conn_string);
        g_free(conn_string);

        if (PQstatus (pconn) != CONNECTION_OK) {
                _gda_postgres_make_error (cnc, pconn, NULL, NULL);
                PQfinish (pconn);
                return FALSE;
        }

	/* Create a new instance of the provider specific data associated to a connection (PostgresConnectionData),
	 * and set its contents */
	PostgresConnectionData *cdata;
	cdata = g_new0 (PostgresConnectionData, 1);
	cdata->cnc = cnc;
        cdata->pconn = pconn;

	/* attach connection data */
	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_postgres_free_cnc_data);

	return TRUE;
}

static gboolean
gda_postgres_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
					  GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	PostgresConnectionData *cdata;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

        const gchar *pg_searchpath;
        pg_searchpath = gda_quark_list_find (params, "SEARCHPATH");

	/*
         * Determine the date format
         */
	GError *lerror = NULL;
	if (!adapt_to_date_format (provider, cnc, &lerror)) {
		if (lerror) {
			if (lerror->message)
				gda_connection_add_event_string (cnc, "%s", lerror->message);
			g_clear_error (&lerror);
		}
		return FALSE;
	}

        /*
         * Unicode is the default character set now
         */
	PGresult *pg_res;
        pg_res = _gda_postgres_PQexec_wrap (cnc, cdata->pconn, "SET CLIENT_ENCODING TO 'UNICODE'");
	if (!pg_res)
		return FALSE;
        PQclear (pg_res);

	/* handle LibPQ's notices */
        PQsetNoticeProcessor (cdata->pconn, (PQnoticeProcessor) pq_notice_processor, cnc);

	/* handle the reuseable part */
	GdaProviderReuseableOperations *ops;
	ops = _gda_postgres_reuseable_get_ops ();
	cdata->reuseable = (GdaPostgresReuseable*) ops->re_new_data ();
	_gda_postgres_compute_types (cnc, cdata->reuseable);

	/*
         * Set the search_path
         */
        if ((cdata->reuseable->version_float >= 7.3) && pg_searchpath) {
                const gchar *ptr;
                gboolean path_valid = TRUE;

                ptr = pg_searchpath;
                while (*ptr) {
                        if (*ptr == ';')
                                path_valid = FALSE;
                        ptr++;
                }

                if (path_valid) {
                        gchar *query = g_strdup_printf ("SET search_path TO %s", pg_searchpath);
                        pg_res = _gda_postgres_PQexec_wrap (cnc, cdata->pconn, query);
                        g_free (query);

                        if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK)) {
                                gda_connection_add_event_string (cnc, _("Could not set search_path to %s"), pg_searchpath);
                                PQclear (pg_res);

                                return FALSE;
                        }
                        PQclear (pg_res);
                }
                else {
			gda_connection_add_event_string (cnc, _("Search path %s is invalid"), pg_searchpath);
                        return FALSE;
                }
        }

	return TRUE;
}


/*
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated PostgresConnectionData structure
 *   - Free the PostgresConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR gonnection event must be added to @cnc)
 */
static gboolean
gda_postgres_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

	/* Free the PostgresConnectionData structure and its contents
	 * (will also actually close the connection to the server )*/
	gda_postgres_free_cnc_data (cdata);
	gda_connection_internal_set_provider_data (cnc, NULL, NULL);

	return TRUE;
}

/*
 * Server version request
 *
 * Returns the server version as a string, which should be stored in @cnc's associated PostgresConnectionData structure
 */
static const gchar *
gda_postgres_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	if (! ((GdaProviderReuseable*)cdata->reuseable)->server_version)
		_gda_postgres_compute_version (cnc, cdata->reuseable, NULL);
	return ((GdaProviderReuseable*)cdata->reuseable)->server_version;
}

/*
 * Support operation request
 *
 * Tells what the implemented server operations are. To add support for an operation, the following steps are required:
 *   - create a postgres_specs_....xml.in file describing the required and optional parameters for the operation
 *   - add it to the Makefile.am
 *   - make this method return TRUE for the operation type
 *   - implement the gda_postgres_provider_render_operation() and gda_postgres_provider_perform_operation() methods
 *
 * In this example, the GDA_SERVER_OPERATION_CREATE_TABLE is implemented
 */
static gboolean
gda_postgres_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
        case GDA_SERVER_OPERATION_DROP_COLUMN:

        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:

        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:

        case GDA_SERVER_OPERATION_CREATE_USER:
        case GDA_SERVER_OPERATION_DROP_USER:
                return TRUE;
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
gda_postgres_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
					GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options,
					GError **error)
{
        gchar *file;
        GdaServerOperation *op;
        gchar *str;
	PostgresConnectionData *cdata = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	}

	if (type == GDA_SERVER_OPERATION_CREATE_USER) {
		if (cdata && (cdata->reuseable->version_float < 8.1))
			str = g_strdup ("postgres_specs_create_user");
		else
			str = g_strdup ("postgres_specs_create_role");
	}
	else if (type == GDA_SERVER_OPERATION_DROP_USER) {
		if (cdata && (cdata->reuseable->version_float < 8.1))
			str = g_strdup ("postgres_specs_drop_user");
		else
			str = g_strdup ("postgres_specs_drop_role");
	}
	else {
		file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
		str = g_strdup_printf ("postgres_specs_%s", file);
		g_free (file);
	}


  file = g_strdup_printf ("/spec/postgres/%s.raw.xml", str);

  op = GDA_SERVER_OPERATION (g_object_new (GDA_TYPE_SERVER_OPERATION, 
                                           "op-type", type, 
                                           "spec-resource", file, 
                                           "connection",cnc,
                                           "provider",provider,
                                           NULL));
      
  g_free (str);
  g_free (file);

        return op;
}

/*
 * Render operation request
 */
static gchar *
gda_postgres_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
					GdaServerOperation *op, GError **error)
{
        gchar *sql = NULL;
        gchar *file;
        gchar *str;
	PostgresConnectionData *cdata = NULL;
	GdaServerOperationType type;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	type = gda_server_operation_get_op_type (op);
	if (type == GDA_SERVER_OPERATION_CREATE_USER) {
		if (cdata && (cdata->reuseable->version_float < 8.1))
			str = g_strdup ("postgres_specs_create_user");
		else
			str = g_strdup ("postgres_specs_create_role");
	}
	else if (type == GDA_SERVER_OPERATION_DROP_USER) {
		if (cdata && (cdata->reuseable->version_float < 8.1))
			str = g_strdup ("postgres_specs_drop_user");
		else
			str = g_strdup ("postgres_specs_drop_role");
	}
	else {
		file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
		str = g_strdup_printf ("postgres_specs_%s", file);
		g_free (file);
	}

	/* test @op's validity */
	file = g_strdup_printf ("/spec/postgres/%s.raw.xml", str);
	g_free (str);

	if (!gda_server_operation_is_valid_from_resource (op, file, error)) {
		g_free (file);
		return NULL;
	}
	g_free (file);

	/* actual rendering */
        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
                sql = gda_postgres_render_CREATE_DB (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_DB:
                sql = gda_postgres_render_DROP_DB (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = gda_postgres_render_CREATE_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_TABLE:
                sql = gda_postgres_render_DROP_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_RENAME_TABLE:
                sql = gda_postgres_render_RENAME_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_ADD_COLUMN:
                sql = gda_postgres_render_ADD_COLUMN (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_COLUMN:
                sql = gda_postgres_render_DROP_COLUMN (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_INDEX:
                sql = gda_postgres_render_CREATE_INDEX (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_INDEX:
                sql = gda_postgres_render_DROP_INDEX (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_VIEW:
                sql = gda_postgres_render_CREATE_VIEW (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_VIEW:
                sql = gda_postgres_render_DROP_VIEW (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_USER:
                sql = gda_postgres_render_CREATE_USER (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_USER:
                sql = gda_postgres_render_DROP_USER (provider, cnc, op, error);
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
gda_postgres_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
					 GdaServerOperation *op,GError **error)
{
        GdaServerOperationType optype;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}
        optype = gda_server_operation_get_op_type (op);
	if (!cnc &&
	    ((optype == GDA_SERVER_OPERATION_CREATE_DB) ||
	     (optype == GDA_SERVER_OPERATION_DROP_DB))) {
		GValue *value;
		PGconn *pconn;
		PGresult *pg_res;
		GString *string;

		const gchar *pq_host = NULL;
		const gchar *pq_db = NULL;
		gint         pq_port = -1;
		const gchar *pq_options = NULL;
		const gchar *pq_user = NULL;
		const gchar *pq_pwd = NULL;
		gboolean     pq_ssl = FALSE;

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/HOST");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			pq_host = g_value_get_string (value);

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/PORT");
		if (value && G_VALUE_HOLDS (value, G_TYPE_INT) && (g_value_get_int (value) > 0))
			pq_port = g_value_get_int (value);

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/OPTIONS");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			pq_options = g_value_get_string (value);

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/TEMPLATE");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			pq_db = g_value_get_string (value);

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/USE_SSL");
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			pq_ssl = TRUE;

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/ADM_LOGIN");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			pq_user = g_value_get_string (value);

		value = (GValue *) gda_server_operation_get_value_at (op, "/SERVER_CNX_P/ADM_PASSWORD");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			pq_pwd = g_value_get_string (value);

		string = g_string_new ("");
                if (pq_host && *pq_host)
                        g_string_append_printf (string, "host='%s'", pq_host);
                if (pq_port > 0)
                        g_string_append_printf (string, " port=%d", pq_port);
                g_string_append_printf (string, " dbname='%s'", pq_db ? pq_db : "postgres");
                if (pq_options && *pq_options)
                        g_string_append_printf (string, " options='%s'", pq_options);
                if (pq_user && *pq_user)
                        g_string_append_printf (string, " user='%s'", pq_user);
                if (pq_pwd && *pq_pwd)
                        g_string_append_printf (string, " password='%s'", pq_pwd);
                if (pq_ssl)
                        g_string_append (string, " requiressl=1");

                pconn = PQconnectdb (string->str);
                g_string_free (string, TRUE);

		if (PQstatus (pconn) != CONNECTION_OK) {
                        g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_OPERATION_ERROR, "%s", PQerrorMessage (pconn));
                        PQfinish(pconn);

                        return FALSE;
                }
		else {
			gchar *sql;

			sql = gda_server_provider_render_operation (provider, cnc, op, error);
			if (!sql)
				return FALSE;
			pg_res = _gda_postgres_PQexec_wrap (cnc, pconn, sql);
			g_free (sql);
			if (!pg_res || PQresultStatus (pg_res) != PGRES_COMMAND_OK) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_OPERATION_ERROR, "%s", PQresultErrorMessage (pg_res));
				PQfinish (pconn);
				return FALSE;
			}

			PQclear(pg_res);
			PQfinish (pconn);
			return TRUE;
		}
	}
	else {
		/* use the SQL from the provider to perform the action */
		return gda_server_provider_perform_operation_default (provider, cnc, op, error);
	}
}

/*
 * Begin transaction request
 */
static gboolean
gda_postgres_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					 const gchar *name, GdaTransactionIsolation level,
					 GError **error)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* transaction's parameters */
	gchar *write_option = NULL;
        gchar *isolation_level = NULL;
	GdaStatement *stmt = NULL;

	if (cdata->reuseable->version_float >= 6.5){
                if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
                        if (cdata->reuseable->version_float >= 7.4)
                                write_option = "READ ONLY";
                        else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
					     "%s", _("Transactions are not supported in read-only mode"));
                                gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
                                return FALSE;
                        }
                }
                switch (level) {
                case GDA_TRANSACTION_ISOLATION_READ_COMMITTED :
                        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL READ COMMITTED ", write_option, NULL);
                        break;
                case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
				     "%s", _("Transactions are not supported in read uncommitted isolation level"));
                        gda_connection_add_event_string (cnc,
							 _("Transactions are not supported in read uncommitted isolation level"));
                        return FALSE;
                case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ:
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
				     "%s", _("Transactions are not supported in repeatable read isolation level"));
                        gda_connection_add_event_string (cnc,
							 _("Transactions are not supported in repeatable read isolation level"));
                        return FALSE;
                case GDA_TRANSACTION_ISOLATION_SERIALIZABLE :
                        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE ", write_option, NULL);
                        break;
                default:
                        isolation_level = NULL;
                }
        }

	if (isolation_level) {
		GdaSqlParser *parser;
		parser = gda_server_provider_internal_get_parser (provider);
		stmt = gda_sql_parser_parse_string (parser, isolation_level, NULL, NULL);
		g_free (isolation_level);
		if (!stmt) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", _("Internal error"));
			return FALSE;
		}
	}

	/* BEGIN statement */
	if (gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_BEGIN], NULL, NULL, error) == -1)
		return FALSE;

	if (stmt) {
		if (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error) == -1) {
			g_object_unref (stmt);
			gda_postgres_provider_rollback_transaction (provider, cnc, name, NULL);
			return FALSE;
		}
		g_object_unref (stmt);
	}

	return TRUE;
}

/*
 * Commit transaction request
 */
static gboolean
gda_postgres_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					  G_GNUC_UNUSED const gchar *name, GError **error)
{
	PostgresConnectionData *cdata;
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* COMMIT statement */
	if (gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_COMMIT], NULL, NULL, error) == -1)
		return FALSE;

	return TRUE;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    G_GNUC_UNUSED const gchar *name, GError **error)
{
	PostgresConnectionData *cdata;
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* ROLLBACK statement */
	if (gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_ROLLBACK], NULL, NULL, error) == -1)
		return FALSE;

	return TRUE;
}

/*
 * Add savepoint request
 */
static gboolean
gda_postgres_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				     const gchar *name, GError **error)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* Statement */
	GdaStatement *stmt;
	GdaSqlParser *parser;
	gchar *str;
	const gchar *remain;

	parser = gda_server_provider_internal_get_parser (provider);
	str = g_strdup_printf ("SAVEPOINT %s", name);
	stmt = gda_sql_parser_parse_string (parser, str, &remain, NULL);
	g_free (str);

	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Internal error"));
		return FALSE;
	}

	if (remain) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     _("Wrong savepoint name '%s'"), name);
		g_object_unref (stmt);
		return FALSE;
	}

	if (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error) == -1) {
		g_object_unref (stmt);
		return FALSE;
	}

	g_object_unref (stmt);
	return TRUE;
}

/*
 * Rollback savepoint request
 */
static gboolean
gda_postgres_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* Statement */
	GdaStatement *stmt;
	GdaSqlParser *parser;
	gchar *str;
	const gchar *remain;

	parser = gda_server_provider_internal_get_parser (provider);
	str = g_strdup_printf ("ROLLBACK TO SAVEPOINT %s", name);
	stmt = gda_sql_parser_parse_string (parser, str, &remain, NULL);
	g_free (str);

	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Internal error"));
		return FALSE;
	}

	if (remain) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     _("Wrong savepoint name '%s'"), name);
		g_object_unref (stmt);
		return FALSE;
	}

	if (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error) == -1) {
		g_object_unref (stmt);
		return FALSE;
	}

	g_object_unref (stmt);
	return TRUE;
}

/*
 * Delete savepoint request
 */
static gboolean
gda_postgres_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	PostgresConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* Statement */
	GdaStatement *stmt;
	GdaSqlParser *parser;
	gchar *str;
	const gchar *remain;

	parser = gda_server_provider_internal_get_parser (provider);
	str = g_strdup_printf ("RELEASE SAVEPOINT %s", name);
	stmt = gda_sql_parser_parse_string (parser, str, &remain, NULL);
	g_free (str);

	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Internal error"));
		return FALSE;
	}

	if (remain) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     _("Wrong savepoint name '%s'"), name);
		g_object_unref (stmt);
		return FALSE;
	}

	if (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, error) == -1) {
		g_object_unref (stmt);
		return FALSE;
	}

	g_object_unref (stmt);
	return TRUE;
}

/*
 * Feature support request
 */
static gboolean
gda_postgres_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc, GdaConnectionFeature feature)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	switch (feature) {
        case GDA_CONNECTION_FEATURE_AGGREGATES:
        case GDA_CONNECTION_FEATURE_INDEXES:
        case GDA_CONNECTION_FEATURE_INHERITANCE:
        case GDA_CONNECTION_FEATURE_PROCEDURES:
        case GDA_CONNECTION_FEATURE_SEQUENCES:
        case GDA_CONNECTION_FEATURE_SQL:
        case GDA_CONNECTION_FEATURE_TRANSACTIONS:
        case GDA_CONNECTION_FEATURE_SAVEPOINTS:
        case GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE:
        case GDA_CONNECTION_FEATURE_TRIGGERS:
        case GDA_CONNECTION_FEATURE_USERS:
        case GDA_CONNECTION_FEATURE_VIEWS:
        case GDA_CONNECTION_FEATURE_BLOBS:
        case GDA_CONNECTION_FEATURE_XA_TRANSACTIONS:
                return TRUE;
        case GDA_CONNECTION_FEATURE_NAMESPACES:
                if (cnc) {
			PostgresConnectionData *cdata;

			cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
			if (!cdata)
				return FALSE;
                        if (cdata->reuseable->version_float >= 7.3)
                                return TRUE;
                }
                else
                        return TRUE;
        default:
                break;
        }

        return FALSE;
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
gda_postgres_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
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
	else if (type == GDA_TYPE_BINARY) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
                if (!dh) {
                        dh = gda_postgres_handler_bin_new (cnc);
			gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
			g_object_unref (dh);
                }
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == G_TYPE_DATE_TIME) ||
		 (type == G_TYPE_DATE)) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
                if (!dh) {
                        dh = gda_handler_time_new ();
                        gda_handler_time_set_sql_spec ((GdaHandlerTime *) dh, G_DATE_YEAR,
                                                       G_DATE_MONTH, G_DATE_DAY, '-', FALSE);
                        gda_server_provider_handler_declare (provider, dh, cnc, G_TYPE_DATE, NULL);
                        gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
                        gda_server_provider_handler_declare (provider, dh, cnc, G_TYPE_DATE_TIME, NULL);
                        g_object_unref (dh);
                }
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
gda_postgres_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	if (type == G_TYPE_INT64)
                return "bigint";
        if (type == G_TYPE_UINT64)
                return "bigint";
        if (type == GDA_TYPE_BINARY)
                return "bytea";
        if (type == GDA_TYPE_BLOB)
                return "bytea";
        if (type == G_TYPE_BOOLEAN)
                return "bool";
        if (type == G_TYPE_DATE)
                return "date";
        if (type == G_TYPE_DOUBLE)
                return "float8";
        if (type == GDA_TYPE_GEOMETRIC_POINT)
                return "point";
        if (type == G_TYPE_OBJECT)
                return "text";
        if (type == G_TYPE_INT)
                return "int4";
        if (type == GDA_TYPE_NUMERIC)
                return "numeric";
        if (type == G_TYPE_FLOAT)
                return "float4";
        if (type == GDA_TYPE_SHORT)
                return "int2";
        if (type == GDA_TYPE_USHORT)
                return "int2";
        if (type == G_TYPE_STRING)
                return "varchar";
        if (type == GDA_TYPE_TEXT)
                return "text";
        if (type == GDA_TYPE_TIME)
                return "time";
        if (type == G_TYPE_DATE_TIME)
                return "timestamp";
        if (type == G_TYPE_CHAR)
                return "smallint";
        if (type == G_TYPE_UCHAR)
                return "smallint";
        if (type == G_TYPE_ULONG)
                return "int8";
        if (type == G_TYPE_GTYPE)
                return "varchar";
        if (type == G_TYPE_UINT)
                return "int4";

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
gda_postgres_provider_create_parser (G_GNUC_UNUSED GdaServerProvider *provider,
				     G_GNUC_UNUSED GdaConnection *cnc)
{
	return (GdaSqlParser*) g_object_new (GDA_TYPE_POSTGRES_PARSER, "tokenizer-flavour",
                                             GDA_SQL_PARSER_FLAVOUR_POSTGRESQL, NULL);
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
gda_postgres_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
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

static gboolean
sql_can_cause_date_format_change (const gchar *sql)
{
	if (!sql)
		return FALSE;
	const gchar *ptr;
	for (ptr = sql; *ptr && g_ascii_isspace (*ptr); ptr++);
	if (((ptr[0] == 's') || (ptr[0] == 'S')) &&
	    ((ptr[1] == 'e') || (ptr[1] == 'E')) &&
	    ((ptr[2] == 't') || (ptr[2] == 'T'))) {
		gchar *tmp;
		tmp = g_ascii_strdown (ptr, -1);
		if (g_strrstr (tmp, "datestyle")) {
			g_free (tmp);
			return TRUE;
		}
		g_free (tmp);
	}

	return FALSE;
}

/*
 * Statement prepare request
 *
 * This methods "converts" @stmt into a prepared statement. A prepared statement is a notion
 * specific in its implementation details to the C API used here. If successfull, it must create
 * a new #GdaPostgresPStmt object and declare it to @cnc.
 */
static gboolean
gda_postgres_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
					 GdaStatement *stmt, GError **error)
{
	GdaPostgresPStmt *ps;
	PostgresConnectionData *cdata;
	static guint counter = 0; /* each prepared statement MUST have a unique name, ensured with this counter */

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaPostgresPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	/* get private connection data */
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* render as SQL understood by PostgreSQL */
	GdaSet *params = NULL;
	gchar *sql;
	GSList *used_params = NULL;
	if (! gda_statement_get_parameters (stmt, &params, error))
                return FALSE;
        sql = gda_postgres_provider_statement_to_sql (provider, cnc, stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_DOLLAR,
						      &used_params, error);
        if (!sql)
                goto out_err;

	/* actually prepare statement */
	PGresult *pg_res;
	gchar *prep_stm_name;

	prep_stm_name = g_strdup_printf ("psc%d", counter ++);
	pg_res = PQprepare (cdata->pconn, prep_stm_name, sql, 0, NULL);
	if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK)) {
		_gda_postgres_make_error (cnc, cdata->pconn, pg_res, error);
		g_free (prep_stm_name);
		if (pg_res)
			PQclear (pg_res);
		goto out_err;
	}
	PQclear (pg_res);

	/* make a list of the parameter names used in the statement */
        GSList *param_ids = NULL;
        if (used_params) {
                GSList *list;
                for (list = used_params; list; list = list->next) {
                        const gchar *cid;
                        cid = gda_holder_get_id (GDA_HOLDER (list->data));
                        if (cid) {
                                param_ids = g_slist_append (param_ids, g_strdup (cid));
                                /*g_print ("PostgreSQL's PREPARATION: param ID: %s\n", cid);*/
                        }
                        else {
                                g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
                                             "%s", _("Unnamed parameter is not allowed in prepared statements"));
                                g_slist_free_full (param_ids, (GDestroyNotify) g_free);
				g_free (prep_stm_name);
                                goto out_err;
                        }
                }
        }

        /* create a prepared statement */
        ps = gda_postgres_pstmt_new (cnc, cdata->pconn, prep_stm_name);
	g_free (prep_stm_name);
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
        gda_pstmt_set_param_ids (_GDA_PSTMT (ps), param_ids);
        gda_pstmt_set_sql (_GDA_PSTMT (ps), sql);
	if (sql_can_cause_date_format_change (sql))
		gda_postgres_pstmt_set_date_format_change (ps, TRUE);

	gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
	g_object_unref (ps);

	return TRUE;

 out_err:
	if (used_params)
                g_slist_free (used_params);
        if (params)
                g_object_unref (params);
	g_free (sql);
	return FALSE;
}

static gboolean
check_transaction_started (GdaConnection *cnc, gboolean *out_started)
{
        GdaTransactionStatus *trans;

        trans = gda_connection_get_transaction_status (cnc);
        if (!trans) {
		if (!gda_connection_begin_transaction (cnc, NULL,
						       GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL))
			return FALSE;
		else
			*out_started = TRUE;
	}
	return TRUE;
}

/*
 * Creates a "simple" prepared statement which has no variable, but does not call
 * gda_connection_add_prepared_statement()
 */
static GdaPostgresPStmt *
prepare_stmt_simple (PostgresConnectionData *cdata, const gchar *sql, GError **error)
{
	static guint counter = 0; /* each prepared statement MUST have a unique name, ensured with this counter */
	PGresult *pg_res;
	gchar *prep_stm_name;

	prep_stm_name = g_strdup_printf ("pss%d", counter++);
	pg_res = PQprepare (cdata->pconn, prep_stm_name, sql, 0, NULL);
	if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK)) {
		_gda_postgres_make_error (cdata->cnc, cdata->pconn, pg_res, error);
		g_free (prep_stm_name);
		if (pg_res)
			PQclear (pg_res);
		return NULL;
	}
	else {
		GdaPostgresPStmt *ps;
		PQclear (pg_res);
		ps = gda_postgres_pstmt_new (cdata->cnc, cdata->pconn, prep_stm_name);
		gda_pstmt_set_param_ids (_GDA_PSTMT (ps), NULL);
		gda_pstmt_set_sql (_GDA_PSTMT (ps), sql);
		if (sql_can_cause_date_format_change (sql))
		  gda_postgres_pstmt_set_date_format_change (ps, TRUE);

		return ps;
	}
}

static GdaSet *
make_last_inserted_set (GdaConnection *cnc, GdaStatement *stmt, Oid last_id)
{
	GError *lerror = NULL;

	/* analyze @stmt */
	GdaSqlStatement *sql_insert;
	GdaSqlStatementInsert *insert;
	if (gda_statement_get_statement_type (stmt) != GDA_SQL_STATEMENT_INSERT)
		/* unable to compute anything */
		return NULL;
	g_object_get (G_OBJECT (stmt), "structure", &sql_insert, NULL);
	g_assert (sql_insert);
	insert = (GdaSqlStatementInsert *) sql_insert->contents;

	/* build corresponding SELECT statement */
	GdaSqlStatementSelect *select;
        GdaSqlSelectTarget *target;
        GdaSqlStatement *sql_statement = gda_sql_statement_new (GDA_SQL_STATEMENT_SELECT);

	select = g_new0 (GdaSqlStatementSelect, 1);
        GDA_SQL_ANY_PART (select)->type = GDA_SQL_ANY_STMT_SELECT;
        sql_statement->contents = select;

	/* FROM */
        select->from = gda_sql_select_from_new (GDA_SQL_ANY_PART (select));
        target = gda_sql_select_target_new (GDA_SQL_ANY_PART (select->from));
        gda_sql_select_from_take_new_target (select->from, target);

	/* Filling in the target */
        GValue *value;
        g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), insert->table->table_name);
        gda_sql_select_target_take_table_name (target, value);
	gda_sql_statement_free (sql_insert);

	/* selected fields */
        GdaSqlSelectField *field;
        GSList *fields_list = NULL;

	field = gda_sql_select_field_new (GDA_SQL_ANY_PART (select));
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "*");
	gda_sql_select_field_take_star_value (field, value);
	fields_list = g_slist_append (fields_list, field);

	gda_sql_statement_select_take_expr_list (sql_statement, fields_list);

	/* WHERE */
        GdaSqlExpr *where, *expr;
	GdaSqlOperation *cond;
	where = gda_sql_expr_new (GDA_SQL_ANY_PART (select));
	cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
	where->cond = cond;
	cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "oid");
	expr->value = value;
	cond->operands = g_slist_append (NULL, expr);
	gchar *str;
	str = g_strdup_printf ("%u", last_id);
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
	g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), str);
	expr->value = value;
	cond->operands = g_slist_append (cond->operands, expr);

	gda_sql_statement_select_take_where_cond (sql_statement, where);

	if (gda_sql_statement_check_structure (sql_statement, &lerror) == FALSE) {
                g_warning (_("Can't build SELECT statement to get last inserted row: %s"),
			   lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);
                gda_sql_statement_free (sql_statement);
                return NULL;
        }

	/* execute SELECT statement */
	GdaDataModel *model;
	GdaStatement *statement = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_statement, NULL);
        gda_sql_statement_free (sql_statement);
        model = gda_connection_statement_execute_select (cnc, statement, NULL, &lerror);
	g_object_unref (statement);
        if (!model) {
                g_warning (_("Can't execute SELECT statement to get last inserted row: %s"),
			   lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);
		return NULL;
        }
	else {
		GdaSet *set = NULL;
		GSList *holders = NULL;
		gint nrows, ncols, i;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows <= 0) {
			g_warning (_("SELECT statement to get last inserted row did not return any row"));
			return NULL;
		}
		else if (nrows > 1) {
			g_warning (_("SELECT statement to get last inserted row returned too many (%d) rows"),
				   nrows);
			return NULL;
		}
		ncols = gda_data_model_get_n_columns (model);
		for (i = 0; i < ncols; i++) {
			GdaHolder *h;
			GdaColumn *col;
			gchar *id;
			const GValue *cvalue;

			col = gda_data_model_describe_column (model, i);
			id = g_strdup_printf ("+%d", i);
			h = gda_holder_new (gda_column_get_g_type (col), id);
			g_object_set (G_OBJECT (h), "not-null", FALSE,
				      "name", gda_column_get_name (col), NULL);
			g_free (id);
			cvalue = gda_data_model_get_value_at (model, i, 0, NULL);
			if (!cvalue || !gda_holder_set_value (h, cvalue, NULL)) {
				if (holders) {
					g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
					holders = NULL;
				}
				break;
			}
			holders = g_slist_prepend (holders, h);
		}
		g_object_unref (model);

		if (holders) {
			holders = g_slist_reverse (holders);
			set = gda_set_new (holders);
			g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
		}

		return set;
	}
}

/*
 * Free:
 *  - all the *param_values
 *  - param_values
 *  - param_mem
 *
 */
static void
params_freev (gchar **param_values, gboolean *param_mem, gint size)
{
	gint i;

	for (i = 0; i < size; i++) {
		if (param_values [i] && ! param_mem [i])
			g_free (param_values [i]);
	}
	g_free (param_values);
	g_free (param_mem);
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
gda_postgres_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
					 GdaStatement *stmt, GdaSet *params,
					 GdaStatementModelUsage model_usage,
					 GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaPostgresPStmt *ps;
	PostgresConnectionData *cdata;
	gboolean allow_noparam;
	gboolean empty_rs = FALSE; /* TRUE when @allow_noparam is TRUE and there is a problem with @params
				      => resulting data model will be empty (0 row) */

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	g_assert ((model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) ||
		  (model_usage & GDA_STATEMENT_MODEL_CURSOR_FORWARD));

	allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
		(gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);

	if (last_inserted_row)
		*last_inserted_row = NULL;

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return NULL;

	/*
	 * execute prepared statement using C API: CURSOR based
	 */
	if (!(model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS) &&
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT)) {
		static guint counter = 0; /* each cursor MUST have a unique name, ensured with this counter */
		gchar *cursor_name;
		gchar *cursor_sql;
		PGresult *pg_res;
		gchar *sql;
		GdaDataModel *recset;
		GdaConnectionEvent *event;

		/* convert to SQL to remove any reference to a variable, using GMT for timezones */
		sql = gda_postgres_provider_statement_to_sql (provider, cnc, stmt, params,
							      GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, error);
		if (!sql)
			return NULL;

		/* create a prepared statement object for @sql */
		ps = prepare_stmt_simple (cdata, sql, error);
		if (!ps) {
			g_free (sql);
			return NULL;
		}

		/* declare a CURSOR and add a connection event for the execution*/
		cursor_name = g_strdup_printf ("CURSOR%d", counter++);
		cursor_sql = g_strdup_printf ("DECLARE %s SCROLL CURSOR WITH HOLD FOR %s", cursor_name, sql);
		g_free (sql);
		pg_res = _gda_postgres_PQexec_wrap (cnc, cdata->pconn, cursor_sql);
		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
		gda_connection_event_set_description (event, cursor_sql);
		gda_connection_add_event (cnc, event);
		g_free (cursor_sql);
		if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK)) {
			_gda_postgres_make_error (cnc, cdata->pconn, pg_res, error);
			if (pg_res)
				PQclear (pg_res);
			g_object_unref (ps);
			return NULL;
		}
		PQclear (pg_res);

		/* create data model in CURSOR mode */
		recset = gda_postgres_recordset_new_cursor (cnc, ps, params, cursor_name, col_types);
		gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
		return (GObject*) recset;
	}

	/* get/create new prepared statement */
	ps = (GdaPostgresPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_postgres_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* this case can appear for example if some variables are used in places
			 * where the C API cannot allow them (for example if the variable is the table name
			 * in a SELECT statement). The action here is to get the actual SQL code for @stmt,
			 * and use that SQL instead of @stmt to create another GdaPostgresPStmt object.
			 */
			gchar *sql;

			sql = gda_postgres_provider_statement_to_sql (provider, cnc, stmt, params,
								      GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, error);
			if (!sql)
				return NULL;
			ps = prepare_stmt_simple (cdata, sql, error); // FIXME: this @ps is leaked!
			g_free (sql);
			if (!ps)
				return NULL;
		}
		else
			ps = (GdaPostgresPStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	}
	g_assert (ps);

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	char **param_values = NULL;
        int *param_lengths = NULL;
        int *param_formats = NULL;
	gboolean *param_mem = NULL;
	gint nb_params;
	gboolean transaction_started = FALSE;

	nb_params = g_slist_length (gda_pstmt_get_param_ids (_GDA_PSTMT (ps)));
	param_values = g_new0 (char *, nb_params);
	param_lengths = g_new0 (int, nb_params);
	param_formats = g_new0 (int, nb_params);
	param_mem = g_new0 (gboolean, nb_params);

	for (i = 0, list = gda_pstmt_get_param_ids (_GDA_PSTMT (ps)); list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h = NULL;

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
				param_values [i] = NULL;
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
				param_values [i] = NULL;
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
			params_freev (param_values, param_mem, nb_params);
			g_free (param_lengths);
			g_free (param_formats);
			if (transaction_started)
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			res = gda_postgres_provider_statement_execute (provider, cnc,
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
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error))
				param_values [i] = NULL;
			else if (!rstmt)
				return NULL;
			else {
				params_freev (param_values, param_mem, nb_params);
				g_free (param_lengths);
				g_free (param_formats);
				if (transaction_started)
					gda_connection_rollback_transaction (cnc, NULL, NULL);

				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaPostgresPStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_postgres_provider_statement_prepare (provider, cnc,
									      rstmt, error))
					return NULL;
				tps = (GdaPostgresPStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gda_pstmt_get_param_ids (_GDA_PSTMT (gtps));
				gda_pstmt_set_param_ids (gtps, NULL);
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids =  gda_pstmt_get_param_ids (gtps);
				gda_pstmt_set_param_ids (gtps, prep_param_ids);

				/* execute */
				obj = gda_postgres_provider_statement_execute (provider, cnc,
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
				return obj;
			}
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			/* specific BLOB treatment */
			GdaBlob *blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
			GdaPostgresBlobOp *op;

			/* Postgres requires that a transaction be started for LOB operations */
			if (!check_transaction_started (cnc, &transaction_started)) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, _("Cannot start transaction"));
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					     "%s", _("Cannot start transaction"));
				break;
			}

			/* create GdaBlobOp */
			op = (GdaPostgresBlobOp *) gda_postgres_blob_op_new (cnc);

			/* always create a new blob as there is no way to truncate an existing blob */
			if (gda_postgres_blob_op_declare_blob (op) &&
			    gda_blob_op_write ((GdaBlobOp*) op, blob, 0))
				param_values [i] = gda_postgres_blob_op_get_id (op);
			else
				param_values [i] = NULL;
			g_object_unref (op);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			/* directly bin binary data */
			GdaBinary *bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
			param_values [i] = (gchar*) gda_binary_get_data (bin);
			param_lengths [i] = gda_binary_get_size (bin);
			param_formats [i] = 1; /* binary format */
			param_mem [i] = TRUE; /* don't free later */
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_DATE) {
			GdaHandlerTime *timdh;
			timdh = GDA_HANDLER_TIME (gda_server_provider_get_data_handler_g_type (provider, cnc,
											       G_VALUE_TYPE (value)));
			g_assert (timdh);
			param_values [i] = gda_handler_time_get_no_locale_str_from_value (timdh, value);
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_DATE_TIME) {
			GdaHandlerTime *timdh;
			timdh = GDA_HANDLER_TIME (gda_server_provider_get_data_handler_g_type (provider, cnc,
											       G_VALUE_TYPE (value)));
			g_assert (timdh);

			GDateTime *timestamp;
			timestamp = (GDateTime *) g_value_get_boxed (value);
			if (g_date_time_get_utc_offset (timestamp) != 0) { // FIXME: This should be supported now
				/* PostgreSQL does not store timezone information, so if timezone information is
				 * provided, we do our best and convert it to GMT */
				timestamp = g_date_time_to_utc (timestamp);
				GValue *rv;
				rv = gda_value_new (G_TYPE_DATE_TIME);
				g_value_set_boxed (rv, timestamp);
				g_date_time_unref (timestamp);
				param_values [i] = gda_handler_time_get_no_locale_str_from_value (timdh, rv);
				gda_value_free (rv);
			}
			else
				param_values [i] = gda_handler_time_get_no_locale_str_from_value (timdh, value);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_TIME) {
			GdaHandlerTime *timdh;
			timdh = GDA_HANDLER_TIME (gda_server_provider_get_data_handler_g_type (provider, cnc,
											       G_VALUE_TYPE (value)));
			g_assert (timdh);

			GdaTime *gdatime;
      GdaTime *ngdatime;

			gdatime = g_value_get_boxed (value);
			GValue *rv;
      ngdatime = gda_time_to_utc (gdatime);
			rv = gda_value_new (GDA_TYPE_TIME);
      g_value_take_boxed (rv, ngdatime);
			param_values [i] = gda_handler_time_get_no_locale_str_from_value (timdh, rv);
			gda_value_free (rv);
		}
		else {
			GdaDataHandler *dh;

			dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_VALUE_TYPE (value));
			if (dh)
				param_values [i] = gda_data_handler_get_str_from_value (dh, value);
			else
				param_values [i] = NULL;
		}
	}

	if (event) {
		gda_connection_add_event (cnc, event);
		params_freev (param_values, param_mem, nb_params);
                g_free (param_lengths);
                g_free (param_formats);
		if (transaction_started)
			gda_connection_rollback_transaction (cnc, NULL, NULL);
		return NULL;
	}

	/* add a connection event for the execution */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event,  gda_pstmt_get_sql (_GDA_PSTMT (ps)));
	gda_connection_add_event (cnc, event);

	/* execute prepared statement using C API: random access based */
	PGresult *pg_res;
	GObject *retval = NULL;
	gboolean date_format_change = FALSE;

	if (empty_rs) {
		GdaStatement *estmt;
		gchar *esql;
		estmt = gda_select_alter_select_for_empty (stmt, error);
		if (!estmt) {
			if (transaction_started)
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			return NULL;
		}
		esql = gda_statement_to_sql (estmt, NULL, error);
		g_object_unref (estmt);
		if (!esql) {
			if (transaction_started)
				gda_connection_rollback_transaction (cnc, NULL, NULL);
			return NULL;
		}

		pg_res = PQexec (cdata->pconn, esql);
		g_free (esql);
		date_format_change = sql_can_cause_date_format_change (esql);
	}
	else {
		pg_res = PQexecPrepared (cdata->pconn, gda_postgres_pstmt_get_prep_name (ps), nb_params, (const char * const *) param_values,
					 param_lengths, param_formats, 0);
		date_format_change = gda_postgres_pstmt_get_date_format_change (ps);
	}

	params_freev (param_values, param_mem, nb_params);
	g_free (param_lengths);
	g_free (param_formats);

	if (!pg_res)
		event = _gda_postgres_make_error (cnc, cdata->pconn, NULL, error);
	else {
		int status;
		status = PQresultStatus (pg_res);
		if (status == PGRES_EMPTY_QUERY ||
                    status == PGRES_TUPLES_OK ||
                    status == PGRES_COMMAND_OK) {
			if (date_format_change &&
			    !adapt_to_date_format (provider, cnc, error)) {
				event = _gda_postgres_make_error (cnc, cdata->pconn, NULL, error);
                                PQclear (pg_res);
			}
                        else if (status == PGRES_COMMAND_OK) {
                                gchar *str;
                                GdaConnectionEvent *event;

                                event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
                                str = g_strdup (PQcmdStatus (pg_res));
                                gda_connection_event_set_description (event, str);
                                g_free (str);
                                gda_connection_add_event (cnc, event);
                                retval = (GObject *) gda_set_new_inline (1, "IMPACTED_ROWS", G_TYPE_INT,
									 atoi (PQcmdTuples (pg_res)));

                                if ((PQoidValue (pg_res) != InvalidOid) && last_inserted_row)
					*last_inserted_row = make_last_inserted_set (cnc, stmt, PQoidValue (pg_res));

                                PQclear (pg_res);
                        }
                        else if (status == PGRES_TUPLES_OK) {
				retval = (GObject*) gda_postgres_recordset_new_random (cnc, ps, params, pg_res, col_types);
			}
                        else {
                                PQclear (pg_res);
                                retval = (GObject *) gda_data_model_array_new (0);
                        }
                }
		else
			event = _gda_postgres_make_error (cnc, cdata->pconn, NULL, error);
	}

	gda_connection_internal_statement_executed (cnc, stmt, params, NULL); /* required: help @cnc keep some stats */
	if (transaction_started)
		gda_connection_commit_transaction (cnc, NULL, NULL);

	return retval;
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Usually for INSERTS:
 *  - it removes any DEFAULT value
 *  - if there is no default value anymore, it uses the "DEFAULT VALUES" syntax
 */
static GdaSqlStatement *
gda_postgres_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_postgres_provider_xa_start (GdaServerProvider *provider, GdaConnection *cnc,
				const GdaXaTransactionId *xid, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	return gda_postgres_provider_begin_transaction (provider, cnc, NULL,
							GDA_TRANSACTION_ISOLATION_READ_COMMITTED, error);
}

/*
 * put the XA transaction in the IDLE state: the connection won't accept any more modifications.
 * This state is required by some database providers before actually going to the PREPARED state
 */
static gboolean
gda_postgres_provider_xa_end (GdaServerProvider *provider, GdaConnection *cnc,
			      const GdaXaTransactionId *xid, G_GNUC_UNUSED GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);

	/* nothing to do for PostgreSQL here */
	return TRUE;
}

/*
 * prepares the distributed transaction: put the XA transaction in the PREPARED state
 */
static gboolean
gda_postgres_provider_xa_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				  const GdaXaTransactionId *xid, GError **error)
{
	GdaSet *params;
	gint affected;
	gchar *str;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	if (!gda_statement_get_parameters (priv->internal_stmt [I_STMT_XA_PREPARE], &params, error))
		return FALSE;
	str = gda_xa_transaction_id_to_string (xid);
	if (!gda_set_set_holder_value (params, NULL, "xid", str)) {
		g_free (str);
		g_object_unref (params);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Could not set the XA transaction ID parameter"));
		return FALSE;
	}
	g_free (str);
	affected = gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_XA_PREPARE], params,
								NULL, error);
	g_object_unref (params);
	if (affected == -1)
		return FALSE;
	else
		return TRUE;
}

/*
 * commits the distributed transaction: actually write the prepared data to the database and
 * terminates the XA transaction
 */
static gboolean
gda_postgres_provider_xa_commit (GdaServerProvider *provider, GdaConnection *cnc,
				 const GdaXaTransactionId *xid, GError **error)
{
	GdaSet *params;
	gint affected;
	gchar *str;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	if (!gda_statement_get_parameters (priv->internal_stmt [I_STMT_XA_PREPARE], &params, error))
		return FALSE;
	str = gda_xa_transaction_id_to_string (xid);
	if (!gda_set_set_holder_value (params, NULL, "xid", str)) {
		g_free (str);
		g_object_unref (params);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Could not set the XA transaction ID parameter"));
		return FALSE;
	}
	g_free (str);
	affected = gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_XA_COMMIT], params,
								NULL, error);
	g_object_unref (params);
	if (affected == -1)
		return FALSE;
	else
		return TRUE;
}

/*
 * Rolls back an XA transaction, possible only if in the ACTIVE, IDLE or PREPARED state
 */
static gboolean
gda_postgres_provider_xa_rollback (GdaServerProvider *provider, GdaConnection *cnc,
				   const GdaXaTransactionId *xid, GError **error)
{
	GdaSet *params;
	gint affected;
	gchar *str;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (xid, FALSE);
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	if (!gda_statement_get_parameters (priv->internal_stmt [I_STMT_XA_PREPARE], &params, error))
		return FALSE;
	str = gda_xa_transaction_id_to_string (xid);
	if (!gda_set_set_holder_value (params, NULL, "xid", str)) {
		g_free (str);
		g_object_unref (params);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Could not set the XA transaction ID parameter"));
		return FALSE;
	}
	g_free (str);
	affected = gda_connection_statement_execute_non_select (cnc, priv->internal_stmt [I_STMT_XA_ROLLBACK], params,
								NULL, error);
	g_object_unref (params);
	if (affected == -1)
		return FALSE;
	else
		return TRUE;
}

/*
 * Lists all XA transactions that are in the PREPARED state
 *
 * Returns: a list of GdaXaTransactionId structures, which will be freed by the caller
 */
static GList *
gda_postgres_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc,
				  GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
  GdaPostgresProviderPrivate *priv = gda_postgres_provider_get_instance_private (GDA_POSTGRES_PROVIDER (provider));

	model = gda_connection_statement_execute_select (cnc, priv->internal_stmt [I_STMT_XA_RECOVER], NULL, error);
	if (!model)
		return NULL;
	else {
		GList *list = NULL;
		gint i, nrows;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			const GValue *cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
			if (cvalue && !gda_value_is_null (cvalue))
				list = g_list_prepend (list,
						       gda_xa_transaction_string_to_id (g_value_get_string (cvalue)));
		}
		g_object_unref (model);
		return list;
	}
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
                        *rptr = '"';
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
pg_remove_quotes (gchar *str)
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
                else if (*ptr == '"') {
                        if (*(ptr+1) == '"') {
                                memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
				*str = 0;
				return str;
                        }
                }
		else if (*ptr == '\\') {
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
gda_postgres_provider_identifier_quote (G_GNUC_UNUSED GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *id,
					gboolean for_meta_store, gboolean force_quotes)
{
        GdaSqlReservedKeywordsFunc kwfunc;
        PostgresConnectionData *cdata = NULL;

        if (cnc)
                cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);

        kwfunc = _gda_postgres_reuseable_get_reserved_keywords_func
		(cdata ? (GdaProviderReuseable*) cdata->reuseable : NULL);

	if (for_meta_store) {
		gchar *tmp, *ptr;
		tmp = pg_remove_quotes (g_strdup (id));
		if (kwfunc (tmp)) {
			ptr = gda_sql_identifier_force_quotes (tmp);
			g_free (tmp);
			return ptr;
		}
		else if (force_quotes) {
			/* quote if non LC characters or digits at the 1st char or non allowed characters */
			for (ptr = tmp; *ptr; ptr++) {
				if (((*ptr >= 'a') && (*ptr <= 'z')) ||
				    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
				    (*ptr == '_'))
					continue;
				else {
					ptr = gda_sql_identifier_force_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			return tmp;
		}
		else {
			for (ptr = tmp; *ptr; ptr++) {
				if (*id == '"') {
					if (((*ptr >= 'a') && (*ptr <= 'z')) ||
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
gda_postgres_free_cnc_data (PostgresConnectionData *cdata)
{
	if (!cdata)
		return;

	if (cdata->pconn)
                PQfinish (cdata->pconn);

	if (cdata->reuseable) {
		GdaProviderReuseable *rdata = (GdaProviderReuseable*)cdata->reuseable;
		rdata->operations->re_reset_data (rdata);
		g_free (cdata->reuseable);
	}

	g_free (cdata);
}

static gchar *
gda_postgres_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *string)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	if (!string)
		return NULL;

	PostgresConnectionData *cdata = NULL;
	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);

	char *tmp;
	gchar *retval;
	tmp = PQescapeLiteral (cdata->pconn, string, strlen (string));
	retval = g_strdup (tmp);
	PQfreemem (tmp);
	return retval;
}
gchar *
gda_postgres_provider_unescape_string (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, const gchar *string)
{
	glong total;
	gchar *ptr;
	gchar *retval;
	glong offset = 0;

	if (!string)
		return NULL;

	total = strlen (string);
	retval = g_memdup2 (string, total+1);
	ptr = (gchar *) retval;
	while (offset < total) {
		if (*ptr == '\'') {
			if (*(ptr+1) == '\'') {
				memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				g_free (retval);
				return NULL;
			}
		}
		else
			offset ++;

		ptr++;
	}

	return retval;
}
