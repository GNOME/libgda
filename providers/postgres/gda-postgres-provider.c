/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Bas Driessen <bas.driessen@xobas.com>
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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-column-index.h>
#include <libgda/gda-server-operation.h>
#include <libgda/gda-query.h>
#include <libgda/gda-util.h>
#include <libgda/gda-renderer.h>
#include "gda-postgres.h"
#include "gda-postgres-parser.h"
#include "gda-postgres-provider.h"
#include "gda-postgres-ddl.h"
#include "gda-postgres-blob-op.h"
#include "gda-postgres-handler-bin.h"
#include <libpq/libpq-fs.h>

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/handlers/gda-handler-bin.h>

#include <libgda/sql-delimiter/gda-sql-delimiter.h>
#include <libgda/gda-connection-private.h>
#include <libgda/binreloc/gda-binreloc.h>

static void gda_postgres_provider_class_init (GdaPostgresProviderClass *klass);
static void gda_postgres_provider_init       (GdaPostgresProvider *provider,
					      GdaPostgresProviderClass *klass);
static void gda_postgres_provider_finalize   (GObject *object);

static const gchar *gda_postgres_provider_get_version (GdaServerProvider *provider);
static gboolean gda_postgres_provider_open_connection (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaQuarkList *params,
						       const gchar *username,
						       const gchar *password);

static gboolean gda_postgres_provider_close_connection (GdaServerProvider *provider,
							GdaConnection *cnc);

static const gchar *gda_postgres_provider_get_server_version (GdaServerProvider *provider,
							      GdaConnection *cnc);

static const gchar *gda_postgres_provider_get_database (GdaServerProvider *provider,
							GdaConnection *cnc);

static gboolean gda_postgres_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperationType type, GdaParameterList *options);
static GdaServerOperation *gda_postgres_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
								   GdaServerOperationType type, 
								   GdaParameterList *options, GError **error);
static gchar *gda_postgres_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
						      GdaServerOperation *op, GError **error);
static gboolean gda_postgres_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							 GdaServerOperation *op, GError **error);

static GList *gda_postgres_provider_execute_command (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaCommand *cmd,
						     GdaParameterList *params);
static GdaObject *gda_postgres_provider_execute_query (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaQuery *query,
						       GdaParameterList *params);

static gchar *gda_postgres_provider_get_last_insert_id (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaDataModel *recset);

static gboolean gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 const gchar *name, GdaTransactionIsolation level,
							 GError **error);

static gboolean gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  const gchar *name, GError **error);

static gboolean gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
							    GdaConnection *cnc,
							    const gchar *name, GError **error);

static gboolean gda_postgres_provider_add_savepoint (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *name,
						     GError **error);

static gboolean gda_postgres_provider_rollback_savepoint (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  const gchar *name,
							  GError **error);

static gboolean gda_postgres_provider_delete_savepoint (GdaServerProvider *provider,
							GdaConnection *cnc,
							const gchar *name,
							GError **error);

static gboolean gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
						      GdaConnection *cnc,
						      const gchar *command);

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaConnectionFeature feature);

static GdaServerProviderInfo *gda_postgres_provider_get_info (GdaServerProvider *provider,
							      GdaConnection *cnc);

static GdaDataModel *gda_postgres_provider_get_schema (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaConnectionSchema schema,
						       GdaParameterList *params);

static GdaDataHandler *gda_postgres_provider_get_data_handler (GdaServerProvider *provider,
							       GdaConnection *cnc,
							       GType g_type,
							       const gchar *dbms_type);
static const gchar* gda_postgres_provider_get_default_dbms_type (GdaServerProvider *provider,
								 GdaConnection *cnc,
								 GType type);
static gchar *gda_postgres_provider_escape_string (GdaServerProvider *provider, 
						   GdaConnection *cnc, const gchar *str);
static gchar *gda_postgres_provider_unescape_string (GdaServerProvider *provider, 
						   GdaConnection *cnc, const gchar *str);

static GdaSqlParser *gda_postgres_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
				 		 
typedef struct {
	gint ncolumns;
	gint *columns;
	gboolean primary;
	gboolean unique;
} GdaPostgresIdxData;

typedef enum {
	IDX_PRIMARY,
	IDX_UNIQUE
} IdxType;

typedef struct {
	gchar *colname;  /* used for PG < 7.3 */
	guint  colnum;   /* used for PG >= 7.3 */
	gchar *reference;
} GdaPostgresRefData;

static GObjectClass *parent_class = NULL;

/*
 * GdaPostgresProvider class implementation
 */
static void
gda_postgres_provider_class_init (GdaPostgresProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_provider_finalize;

	provider_class->get_version = gda_postgres_provider_get_version;
	provider_class->get_server_version = gda_postgres_provider_get_server_version;
	provider_class->get_info = gda_postgres_provider_get_info;
	provider_class->supports_feature = gda_postgres_provider_supports;
	provider_class->get_schema = gda_postgres_provider_get_schema;

	provider_class->get_data_handler = gda_postgres_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = gda_postgres_provider_get_default_dbms_type;
	provider_class->escape_string = gda_postgres_provider_escape_string;
	provider_class->unescape_string = gda_postgres_provider_unescape_string;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_postgres_provider_open_connection;
	provider_class->close_connection = gda_postgres_provider_close_connection;
	provider_class->get_database = gda_postgres_provider_get_database;
	provider_class->change_database = NULL;

	provider_class->supports_operation = gda_postgres_provider_supports_operation;
	provider_class->create_operation = gda_postgres_provider_create_operation;
	provider_class->render_operation = gda_postgres_provider_render_operation;
	provider_class->perform_operation = gda_postgres_provider_perform_operation;

	provider_class->execute_command = gda_postgres_provider_execute_command;
	provider_class->execute_query = gda_postgres_provider_execute_query;
	provider_class->get_last_insert_id = gda_postgres_provider_get_last_insert_id;

	provider_class->begin_transaction = gda_postgres_provider_begin_transaction;
	provider_class->commit_transaction = gda_postgres_provider_commit_transaction;
	provider_class->rollback_transaction = gda_postgres_provider_rollback_transaction;
	provider_class->add_savepoint = gda_postgres_provider_add_savepoint;
	provider_class->rollback_savepoint = gda_postgres_provider_rollback_savepoint;
	provider_class->delete_savepoint = gda_postgres_provider_delete_savepoint;

	provider_class->create_parser = gda_postgres_provider_create_parser;
        provider_class->statement_to_sql = NULL;
        provider_class->statement_prepare = NULL;
        provider_class->statement_execute = NULL;
}

static void
gda_postgres_provider_init (GdaPostgresProvider *pg_prv, GdaPostgresProviderClass *klass)
{
}

static void
gda_postgres_provider_finalize (GObject *object)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) object;

	g_return_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv));
	
	/* chain to parent class */
	parent_class->finalize(object);
}

GType
gda_postgres_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GTypeInfo info = {
			sizeof (GdaPostgresProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_provider_class_init,
			NULL, NULL,
			sizeof (GdaPostgresProvider),
			0,
			(GInstanceInitFunc) gda_postgres_provider_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaPostgresProvider",
					       &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_postgres_provider_new (void)
{
	GdaPostgresProvider *provider;

	provider = g_object_new (gda_postgres_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

static GType
postgres_name_to_g_type (const gchar *name, const gchar *conv_func_name)
{
	/* default built in data types */
	if (!strcmp (name, "bool"))
		return G_TYPE_BOOLEAN;
	else if (!strcmp (name, "int8"))
		return G_TYPE_INT64;
	else if (!strcmp (name, "int4") || !strcmp (name, "abstime"))
		return G_TYPE_INT;
	else if (!strcmp (name, "int2"))
		return GDA_TYPE_SHORT;
	else if (!strcmp (name, "float4"))
		return G_TYPE_FLOAT;
	else if (!strcmp (name, "float8"))
		return G_TYPE_DOUBLE;
	else if (!strcmp (name, "numeric"))
		return GDA_TYPE_NUMERIC;
	else if (!strncmp (name, "timestamp", 9))
		return GDA_TYPE_TIMESTAMP;
	else if (!strcmp (name, "date"))
		return G_TYPE_DATE;
	else if (!strncmp (name, "time", 4))
		return GDA_TYPE_TIME;
	else if (!strcmp (name, "point"))
		return GDA_TYPE_GEOMETRIC_POINT;
	else if (!strcmp (name, "oid"))
		return GDA_TYPE_BLOB;
	else if (!strcmp (name, "bytea"))
		return GDA_TYPE_BINARY;

	/* other data types, using the conversion function name as a hint */
	if (!conv_func_name)
		return G_TYPE_STRING;

	if (!strncmp (conv_func_name, "int2", 4))
		return GDA_TYPE_SHORT;
	if (!strncmp (conv_func_name, "int4", 4))
		return G_TYPE_INT;
	if (!strncmp (conv_func_name, "int8", 4))
		return G_TYPE_INT64;
	if (!strncmp (conv_func_name, "float4", 6))
		return G_TYPE_FLOAT;
	if (!strncmp (conv_func_name, "float8", 6))
		return G_TYPE_DOUBLE;
	if (!strncmp (conv_func_name, "timestamp", 9))
		return GDA_TYPE_TIMESTAMP;
	if (!strncmp (conv_func_name, "time", 4))
		return GDA_TYPE_TIME;
	if (!strncmp (conv_func_name, "date", 4))
		return G_TYPE_DATE;
	if (!strncmp (conv_func_name, "bool", 4))
		return G_TYPE_BOOLEAN;
	if (!strncmp (conv_func_name, "oid", 3))
		return GDA_TYPE_BLOB;
	if (!strncmp (conv_func_name, "bytea", 5))
		return GDA_TYPE_BINARY;
	return G_TYPE_STRING;
}

static int
get_connection_type_list (GdaPostgresConnectionData *priv_td)
{
	GHashTable *h_table;
	GdaPostgresTypeOid *td;
	PGresult *pg_res, *pg_res_avoid, *pg_res_anyoid = NULL;
	gint nrows, i;
	gchar *avoid_types = NULL;
	GString *string;

	if (priv_td->version_float < 7.3) {
		gchar *query;
		avoid_types = "'SET', 'cid', 'oid', 'int2vector', 'oidvector', 'regproc', 'smgr', 'tid', 'unknown', 'xid'";
		/* main query to fetch infos about the data types */
		query = g_strdup_printf ("SELECT pg_type.oid, typname, usename, obj_description(pg_type.oid) "
					 "FROM pg_type, pg_user "
					 "WHERE typowner=usesysid AND typrelid = 0 AND typname !~ '^_' "
					 "AND  typname not in (%s) "
					 "ORDER BY typname", avoid_types);
		pg_res = gda_postgres_PQexec_wrap (priv_td->cnc, priv_td->pconn, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT pg_type.oid FROM pg_type WHERE typname in (%s)", avoid_types);
		pg_res_avoid = gda_postgres_PQexec_wrap (priv_td->cnc, priv_td->pconn, query);
		g_free (query);
	}
	else {
		gchar *query;
		avoid_types = "'any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid'";

		/* main query to fetch infos about the data types */
		query = g_strdup_printf (
                          "SELECT t.oid, t.typname, u.usename, pg_catalog.obj_description(t.oid), t.typinput "
			  "FROM pg_catalog.pg_type t, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
			  "WHERE t.typowner=u.usesysid "
			  "AND n.oid = t.typnamespace "
			  "AND pg_catalog.pg_type_is_visible(t.oid) "
			  /*--AND (n.nspname = 'public' OR n.nspname = 'pg_catalog')*/
			  "AND typname !~ '^_' "
			  "AND (t.typrelid = 0 OR "
			  "(SELECT c.relkind = 'c' FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) "
			  "AND t.typname not in (%s) "
			  "ORDER BY typname", avoid_types);
		pg_res = gda_postgres_PQexec_wrap (priv_td->cnc, priv_td->pconn, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT t.oid FROM pg_catalog.pg_type t WHERE t.typname in (%s)",
					 avoid_types);
		pg_res_avoid = gda_postgres_PQexec_wrap (priv_td->cnc, priv_td->pconn, query);
		g_free (query);

		/* query to fetch the oid of the 'any' data type */
		pg_res_anyoid = gda_postgres_PQexec_wrap (priv_td->cnc, priv_td->pconn, 
					"SELECT t.oid FROM pg_catalog.pg_type t WHERE t.typname = 'any'");
	}

	if (!pg_res || (PQresultStatus (pg_res) != PGRES_TUPLES_OK) ||
	    !pg_res_avoid || (PQresultStatus (pg_res_avoid) != PGRES_TUPLES_OK) ||
	    ((priv_td->version_float >= 7.3) && 
	     (!pg_res_anyoid || (PQresultStatus (pg_res_anyoid) != PGRES_TUPLES_OK)))) {
		if (pg_res)
			PQclear (pg_res);
		if (pg_res_avoid)
			PQclear (pg_res_avoid);
		if (pg_res_anyoid)
			PQclear (pg_res_anyoid);
		return -1;
	}

	/* Data types returned */
	nrows = PQntuples (pg_res);
	td = g_new (GdaPostgresTypeOid, nrows);
	h_table = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < nrows; i++) {
		gchar *conv_func_name = NULL;
		if (PQnfields (pg_res) >= 5)
			conv_func_name = PQgetvalue (pg_res, i, 4);
		td[i].name = g_strdup (PQgetvalue (pg_res, i, 1));
		td[i].oid = atoi (PQgetvalue (pg_res, i, 0));
		td[i].type = postgres_name_to_g_type (td[i].name, conv_func_name);
		td[i].comments = g_strdup (PQgetvalue (pg_res, i, 3));
		td[i].owner = g_strdup (PQgetvalue (pg_res, i, 2));
		g_hash_table_insert (h_table, td[i].name, &td[i].type);
	}

	PQclear (pg_res);
	priv_td->ntypes = nrows;
	priv_td->type_data = td;
	priv_td->h_table = h_table;

	/* Make a string of data types internal to postgres and not returned, for future queries */
	string = NULL;
	nrows = PQntuples (pg_res_avoid);
	for (i = 0; i < nrows; i++) {
		if (!string) 
			string = g_string_new (PQgetvalue (pg_res_avoid, i, 0));
		else
			g_string_append_printf (string, ", %s", PQgetvalue (pg_res_avoid, i, 0));
	}
	PQclear (pg_res_avoid);
	priv_td->avoid_types = avoid_types;
	priv_td->avoid_types_oids = string->str;
	g_string_free (string, FALSE);

	/* make a string of the oid of type 'any' */
	priv_td->any_type_oid = "";
	if (pg_res_anyoid) {
		if (PQntuples (pg_res_anyoid) == 1) 
			priv_td->any_type_oid = g_strdup (PQgetvalue (pg_res_anyoid, 0, 0));
		PQclear (pg_res_anyoid);
	}
	return 0;
}

/* get_version handler for the GdaPostgresProvider class */
static const gchar *
gda_postgres_provider_get_version (GdaServerProvider *provider)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	return PACKAGE_VERSION;
}

/* get the float version of a Postgres version which looks like:
 * PostgreSQL 7.2.2 on i686-pc-linux-gnu, compiled by GCC 2.96 => returns 7.22
 * PostgreSQL 7.3 on i686-pc-linux-gnu, compiled by GCC 2.95.3 => returns 7.3
 * WARNING: no serious test is made on the validity of the string
 */
static gfloat 
get_pg_version_float (const gchar *str)
{
	gfloat retval = 0.;
	const gchar *ptr;
	gfloat div = 1;

	if (!str)
		return retval;

	/* go on  the first digit of version number */
	ptr = str;
	while (*ptr != ' ')
		ptr++;
	ptr++;
	
	/* elaborate the real version number */
	while (*ptr != ' ') {
		if (*ptr != '.') {
			retval += (*ptr - '0')/div;
			div *= 10;
		}
		ptr++;
	}

	return retval;
}


/* open_connection handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_open_connection (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaQuarkList *params,
				       const gchar *username,
				       const gchar *password)
{
	const gchar *pq_host;
	const gchar *pq_db;
	const gchar *pg_searchpath;
	const gchar *pq_port;
	const gchar *pq_options;
	const gchar *pq_tty;
	const gchar *pq_user;
	const gchar *pq_pwd;
	const gchar *pq_hostaddr;
	const gchar *pq_requiressl;
	gchar *conn_string;
	GdaPostgresConnectionData *priv_data;
	PGconn *pconn;
	PGresult *pg_res;
	gchar *version;
	gfloat version_float;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* parse connection string */
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
	pg_searchpath = gda_quark_list_find (params, "SEARCHPATH");
	pq_port = gda_quark_list_find (params, "PORT");
	pq_options = gda_quark_list_find (params, "OPTIONS");
	pq_tty = gda_quark_list_find (params, "TTY");
	if (!username || *username == '\0')
		pq_user = gda_quark_list_find (params, "USER");
	else
		pq_user = username;

	if (!password || *password == '\0')
		pq_pwd = gda_quark_list_find (params, "PASSWORD");
	else
		pq_pwd = password;

	pq_requiressl = gda_quark_list_find (params, "USE_SSL");

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
				   NULL);

	/* actually establish the connection */
	pconn = PQconnectdb (conn_string);
	g_free(conn_string);

	if (PQstatus (pconn) != CONNECTION_OK) {
		gda_postgres_make_error (cnc, pconn, NULL);
		PQfinish (pconn);
		return FALSE;
	}

	/*
	 * Sets the DATE format for all the current session to YYYY-MM-DD
	 */
	pg_res = gda_postgres_PQexec_wrap (cnc, pconn, "SET DATESTYLE TO 'ISO'");
	PQclear (pg_res);

	/*
	 * Unicode is the default character set now
	 */
	pg_res = gda_postgres_PQexec_wrap (cnc, pconn, "SET CLIENT_ENCODING TO 'UNICODE'");
	PQclear (pg_res);

	/*
	 * Get the vesrion as a float
	 */
	pg_res = gda_postgres_PQexec_wrap (cnc, pconn, "SELECT version ()");
	version = g_strdup (PQgetvalue(pg_res, 0, 0));
	version_float = get_pg_version_float (PQgetvalue(pg_res, 0, 0));
	PQclear (pg_res);

	/*
	 * Set the search_path
	 */
	if ((version_float >= 7.3) && pg_searchpath) {
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
			pg_res = gda_postgres_PQexec_wrap (cnc, pconn, query);
			g_free (query);

			if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK)) {
				g_warning ("Could not set search_path to %s\n", pg_searchpath);
				PQclear (pg_res);
				return FALSE;
			}
			PQclear (pg_res);
		}
		else {
			g_warning ("Search path %s is invalid\n", pg_searchpath);
			return FALSE;
		}
	}

	/*
	 * Associated data
	 */
	priv_data = g_new0 (GdaPostgresConnectionData, 1);
	priv_data->cnc = cnc;
	priv_data->pconn = pconn;
	priv_data->version = version;
	priv_data->version_float = version_float;
	if (get_connection_type_list (priv_data) != 0) {
		gda_postgres_make_error (cnc, pconn, NULL);
		PQfinish (pconn);
		g_free (priv_data);
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, priv_data);

	return TRUE;
}

/* close_connection handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaPostgresConnectionData *priv_data;
	gint i;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data)
		return FALSE;

	PQfinish (priv_data->pconn);

	for (i = 0; i < priv_data->ntypes; i++) {
		g_free (priv_data->type_data[i].name);
		g_free (priv_data->type_data[i].comments);
		g_free (priv_data->type_data[i].owner);
	}
	
	g_hash_table_destroy (priv_data->h_table);
	g_free (priv_data->type_data);
	g_free (priv_data->version);
	g_free (priv_data->avoid_types_oids);
	g_free (priv_data);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, NULL);
	return TRUE;
}

/* get_server_version handler for the GdaPostgresProvider class */
static const gchar *
gda_postgres_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return priv_data->version;
}

static GdaObject *
compute_retval_from_pg_res (GdaConnection *cnc, PGconn *pconn, const gchar *sql, PGresult *pg_res, const gchar *cursor_name)
{
	GdaConnectionEvent *error = NULL;
	GdaObject *retval = NULL;

	if (pg_res == NULL) 
		error = gda_postgres_make_error (cnc, pconn, NULL);
	else {
		GdaPostgresConnectionData *priv_data;
		gint status;

		priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);		
		status = PQresultStatus (pg_res);
		
		if (status == PGRES_EMPTY_QUERY ||
		    status == PGRES_TUPLES_OK ||
		    status == PGRES_COMMAND_OK) {
			if ((status == PGRES_COMMAND_OK) && !cursor_name) {
				gchar *str;
				GdaConnectionEvent *event;
				
				event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
				str = g_strdup (PQcmdStatus (pg_res));
				gda_connection_event_set_description (event, str);
				g_free (str);
				gda_connection_add_event (cnc, event);
				retval = (GdaObject *) gda_parameter_list_new_inline (NULL, 
										      "IMPACTED_ROWS", G_TYPE_INT, 
										      atoi (PQcmdTuples (pg_res)),
										      NULL);
				
				if ((PQoidValue (pg_res) != InvalidOid))
					priv_data->last_insert_id = PQoidValue (pg_res);
				else
					priv_data->last_insert_id = 0;

				PQclear (pg_res);
			}
			else if ((status == PGRES_TUPLES_OK)  ||
				 ((status == PGRES_COMMAND_OK) && cursor_name)) {
				GdaDataModel *recset;

				if (cursor_name) {
					recset = gda_postgres_cursor_recordset_new (cnc, cursor_name, 
										    priv_data->chunk_size);
					PQclear (pg_res);
				}
				else
					recset = gda_postgres_recordset_new (cnc, pg_res);

				if (GDA_IS_DATA_MODEL (recset)) {
					g_object_set (G_OBJECT (recset), 
						      "command_text", sql,
						      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
					
					retval = (GdaObject *) recset;
				}
			}
			else {
				PQclear (pg_res);
				retval = (GdaObject *) gda_data_model_array_new (0);	
			}
		}
		else {
			error = gda_postgres_make_error (cnc, pconn, pg_res);
			PQclear (pg_res);
		}
	}
	
	gda_connection_internal_treat_sql (cnc, sql, error);
	return retval;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc,
		      const gchar *sql, GdaCommandOptions options)
{
	GdaPostgresConnectionData *priv_data;
	PGconn *pconn;
	gchar **arr;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	pconn = priv_data->pconn;
	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		gint n = 0;
		gboolean allok = TRUE;

		while (arr[n] && allok) {
			PGresult *pg_res;
			GdaObject *obj;
			gchar *cursor_name = NULL;

			if (priv_data->use_cursor && !strncasecmp (arr[n], "SELECT", 6)) {
				gchar *str;
				struct timeval stm;
				static guint index = 0;

				gettimeofday (&stm, NULL);				
				cursor_name = g_strdup_printf ("gda_%d_%d_%d", stm.tv_sec, stm.tv_usec, index++);
				str = g_strdup_printf ("DECLARE %s SCROLL CURSOR WITH HOLD FOR %s", 
						       cursor_name, arr[n]);
				pg_res = gda_postgres_PQexec_wrap (cnc, pconn, str);
				g_free (str);
			}
			else 
				pg_res = gda_postgres_PQexec_wrap (cnc, pconn, arr[n]);
			obj = compute_retval_from_pg_res (cnc, pconn, arr[n], pg_res, cursor_name);
			g_free (cursor_name);
			reclist = g_list_append (reclist, obj);
			if (!obj && !(options & GDA_COMMAND_OPTION_IGNORE_ERRORS))
				allok = FALSE;
			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* get_database handler for the GdaPostgresProvider class */
static const gchar *
gda_postgres_provider_get_database (GdaServerProvider *provider,
				    GdaConnection *cnc)
{
	GdaPostgresConnectionData *priv_data;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return (const char *) PQdb ((const PGconn *) priv_data->pconn);
}

static gboolean
gda_postgres_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
					  GdaServerOperationType type, GdaParameterList *options)
{
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
		return TRUE;
	default:
		return FALSE;
	}
}

static GdaServerOperation *
gda_postgres_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					GdaServerOperationType type, 
					GdaParameterList *options, GError **error)
{
	gchar *file;
	GdaServerOperation *op;
	gchar *str;
	gchar *dir;
	
	file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
	str = g_strdup_printf ("postgres_specs_%s.xml", file);
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

static gchar *
gda_postgres_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					GdaServerOperation *op, GError **error)
{
	gchar *sql = NULL;
	gchar *file;
	gchar *str;
	gchar *dir;

	file = g_utf8_strdown (gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)), -1);
	str = g_strdup_printf ("postgres_specs_%s.xml", file);
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
	default:
		g_assert_not_reached ();
	}
	return sql;
}

static gboolean
gda_postgres_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					 GdaServerOperation *op, GError **error)
{
	GdaServerOperationType optype;

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
                g_string_append_printf (string, " dbname='%s'", pq_db ? pq_db : "template1");
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
                        g_set_error (error, 0, 0, PQerrorMessage (pconn));
                        PQfinish(pconn);

                        return FALSE;
                }
		else {
			gchar *sql;

			sql = gda_server_provider_render_operation (provider, cnc, op, error);
			if (!sql)
				return FALSE;
			pg_res = gda_postgres_PQexec_wrap (cnc, pconn, sql);
			g_free (sql);
			if (!pg_res || PQresultStatus (pg_res) != PGRES_COMMAND_OK) {
				g_set_error (error, 0, 0, PQresultErrorMessage (pg_res));
				PQfinish (pconn);
				return FALSE;
			}
			
			PQfinish (pconn);
			return TRUE;
		}
	}
	else {
		/* use the SQL from the provider to perform the action */
		gchar *sql;
		GdaCommand *cmd;
		
		sql = gda_server_provider_render_operation (provider, cnc, op, error);
		if (!sql)
			return FALSE;
		
		cmd = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		g_free (sql);
		if (gda_connection_execute_non_select_command (cnc, cmd, NULL, error) != -1) {
			gda_command_free (cmd);
			return TRUE;
		}
		else {
			gda_command_free (cmd);
			return FALSE;
		}
	}
}

/* execute_command handler for the GdaPostgresProvider class */
static GList *
gda_postgres_provider_execute_command (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaCommand *cmd,
				       GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaCommandOptions options;
	gboolean prev_use_cursor;
	gint prev_chunk_size;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* save previous settings */
	prev_use_cursor = priv_data->use_cursor;
	prev_chunk_size = priv_data->chunk_size;

	if (params) {
		GdaParameter *param;
		param = gda_parameter_list_find_param (params, "ITER_MODEL_ONLY");
		if (param) {
			const GValue *value;

			value = gda_parameter_get_value (param);
			if (value) {
				if (G_VALUE_TYPE (value) != G_TYPE_BOOLEAN)
					g_warning (_("Parameter ITER_MODEL_ONLY should be a boolean, not a '%s'"),
						   g_type_name (G_VALUE_TYPE (value)));
				else
					priv_data->use_cursor = g_value_get_boolean (value);
			}

			param = gda_parameter_list_find_param (params, "ITER_CHUNCK_SIZE");
			if (param) {
				value = gda_parameter_get_value (param);
				if (value) {
					if (G_VALUE_TYPE (value) != G_TYPE_INT)
						g_warning (_("Parameter ITER_CHUNCK_SIZE should be a gint, not a '%s'"),
							   g_type_name (G_VALUE_TYPE (value)));
					else
						priv_data->chunk_size = g_value_get_int (value);
				}
			}
		}
	}

	options = gda_command_get_options (cmd);
	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd),
						options);
		break;
	case GDA_COMMAND_TYPE_TABLE :
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = process_sql_commands (reclist, cnc, str, options);
		g_free (str);
		break;
	default:
		break;
	}

	/* restore previous settings */
	priv_data->use_cursor = prev_use_cursor;
	priv_data->chunk_size = prev_chunk_size;

	return reclist;
}

static PGresult *
fetch_existing_blobs (GdaConnection *cnc, PGconn *pconn, GdaQuery *query, GdaParameterList *params)
{
	PGresult *pg_update_blobs = NULL;

	if (gda_query_is_update_query (query) || gda_query_is_delete_query (query)) {
		GSList *list;
		GdaParameterList *plist = NULL;
		GdaQuery *select = NULL;

		if ((gda_query_is_update_query (query) && 
		     !gda_server_provider_blob_list_for_update (cnc, query, &select, NULL)) ||
		    (gda_query_is_delete_query (query) &&
		     !gda_server_provider_blob_list_for_delete (cnc, query, &select, NULL))) {
			if (select)
				g_object_unref (select);
			gda_connection_add_event_string (cnc, _("Could not create a SELECT query to "
								"fetch existing BLOB values"));
			return NULL;
		}

		/* execute select query to get BLOB ids */
		if (select) {
			GError *error = NULL;
			gchar *sql;
			
			plist = gda_query_get_parameter_list (select);
			if (plist) {
				for (list = plist->parameters; list; list = list->next) {
					GdaParameter *param = GDA_PARAMETER (list->data);
					GdaParameter *inc_param;
					
					inc_param = gda_parameter_list_find_param (params, 
										   gda_object_get_name (GDA_OBJECT (param)));
					if (!inc_param) {
						/* missing parameter */
						GdaConnectionEvent *event = NULL;
						gchar *str;
						str = g_strdup_printf (_("Missing parameter for '%s'"), 
								       gda_object_get_name (GDA_OBJECT (param)));
						event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
						gda_connection_event_set_description (event, str);
						gda_connection_add_event (cnc, event);
						g_free (str);
						return NULL;
					}
					else 
						gda_parameter_set_value (param, gda_parameter_get_value (inc_param));
				}
			}

			sql = gda_renderer_render_as_sql (GDA_RENDERER (select), plist, NULL, 0, &error);
			if (plist)
				g_object_unref (plist);
			g_object_unref (select);
			if (!sql || !*sql) {
				gchar *msg = g_strdup_printf (_("Could not render SQL for SELECT "
								"query to fetch existing BLOB values: %s"), 
							      error && error->message ? 
							      error->message : _("No detail"));
				gda_connection_add_event_string (cnc, msg);
				g_error_free (error);
				g_free (msg);
				return NULL;
			}
			pg_update_blobs = gda_postgres_PQexec_wrap (cnc, pconn, sql);
			g_free (sql);
			if (!pg_update_blobs || (PQresultStatus (pg_update_blobs) != PGRES_TUPLES_OK)) {
				gda_postgres_make_error (cnc, pconn, pg_update_blobs);
				return NULL;
			}
		}
	}

	return pg_update_blobs;
}

static GdaObject *
split_and_execute_update_query (GdaServerProvider *provider, GdaConnection *cnc, PGresult *pg_existing_blobs, 
				GdaQuery *query, GdaParameterList *params)
{
	GdaQuery *nquery;
	GError *error = NULL;
	GdaParameterList *ret = NULL;
	
	if (!gda_server_provider_split_update_query (cnc, query, &nquery, &error)) {
		GdaConnectionEvent *event;
		event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, 
						      error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
	}
	else {
		/* add params to @params to allow the execution of the nquery query with 
		 * parameters from pg_existing_blobs */
		GdaParameterList *iter_params;
		gint affected_rows = 0;
		GSList *list;
		gboolean allok = TRUE;
			
		iter_params = gda_query_get_parameter_list (nquery);
		g_return_val_if_fail (iter_params, NULL);
		for (list = params ? params->parameters : NULL; list; list = list->next) {
			GdaParameter *inc_param;
			inc_param = gda_parameter_list_find_param (iter_params, 
								   gda_object_get_name (GDA_OBJECT (list->data)));
			g_assert (inc_param);
			gda_parameter_set_value (inc_param,
						 gda_parameter_get_value (GDA_PARAMETER (list->data)));
		}

		/* run the new update query for each row in pg_existing_blobs */
		gint row, col, nrows, ncols, npkfields, i;
		nrows = PQntuples (pg_existing_blobs);
		ncols = PQnfields (pg_existing_blobs);
		npkfields = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (nquery), "_gda_nb_key_fields"));
		for (row = 0; (row < nrows) && allok; row++) {
			/* set some specific parameters for this run */
			for (i = 0, col = ncols - npkfields; col < ncols; col++, i++) {
				GdaParameter *param;
				gchar *str;
					
				str = g_strdup_printf ("_prov_EXTRA%d", i);
				param = gda_parameter_list_find_param (iter_params, str);
				g_free (str);
				g_assert (param);

				gboolean isnull;
				gchar *txtvalue = PQgetvalue (pg_existing_blobs, row, col);
				GValue *value = g_new0 (GValue, 1);
				isnull = *txtvalue != '\0' ? FALSE : PQgetisnull (pg_existing_blobs, row, col);
				gda_postgres_set_value (cnc, value, 
							gda_parameter_get_g_type (param), 
							txtvalue, isnull, PQgetlength (pg_existing_blobs, row, col));
				gda_parameter_set_value (param, value);
				gda_value_free (value);
			}
			ret = (GdaParameterList*) gda_postgres_provider_execute_query (provider, cnc, nquery, iter_params);
			if (ret) {
				GdaParameter *param;
				g_assert (GDA_IS_PARAMETER_LIST (ret));
				param = gda_parameter_list_find_param (ret, "IMPACTED_ROWS");
				if (param) 
					affected_rows += g_value_get_int (gda_parameter_get_value (param));
				g_object_unref (ret);
			}
			else
				allok = FALSE;
		}
		if (allok) 
			ret = gda_parameter_list_new_inline (gda_object_get_dict (GDA_OBJECT (nquery)),
							     "IMPACTED_ROWS", G_TYPE_INT, affected_rows, NULL);
		else
			ret = NULL;
		g_object_unref (nquery);
	}

	return (GdaObject *) ret;
}

static GdaObject *
gda_postgres_provider_execute_query (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaQuery *query,
				     GdaParameterList *params)
{
	GSList *list;
	GdaObject *retval = NULL;
	gchar *pseudo_sql;

	PGresult *pg_existing_blobs = NULL;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;
	PGconn *pconn;
	PGresult *pg_res;
	gchar *prep_stm_name;
	gint nb_params = 0;
	gboolean allok = TRUE;
	GSList *used_params = NULL;

	char **param_values = NULL;
	int *param_lengths = NULL;
	int *param_formats = NULL;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_QUERY (query), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}
	pconn = priv_data->pconn;
	
#ifdef GDA_DEBUG_NO
	pseudo_sql = gda_renderer_render_as_sql (GDA_RENDERER (query), params, NULL, GDA_RENDERER_PARAMS_AS_DETAILED , NULL);
	g_print ("Execute_query: %s\n", pseudo_sql);
	g_free (pseudo_sql);
#endif

	/* if the query is a SELECT, we need to start a transaction if the query returns BLOB fields */
	if (gda_query_is_select_query (query) && 
	    gda_server_provider_select_query_has_blobs (cnc, query, NULL))
		if (!gda_postgres_check_transaction_started (cnc))
			return NULL;


	/* if query is an update query, then create a SELECT query to be able to fetch blob references, if
	 * there are any blob parameters */
	pg_existing_blobs = fetch_existing_blobs (cnc, pconn, query, params);
	if (pg_existing_blobs && (PQntuples (pg_existing_blobs) > 1) && gda_query_is_update_query (query)) {
		/* As the UPDATE query concerns several rows, create several UPDATE queries 
		 * with one row each, and fail if it can't be done */
		retval = split_and_execute_update_query (provider, cnc, pg_existing_blobs, query, params);
		PQclear (pg_existing_blobs);
		return retval;
	}

	/* use or create a prepared statement */
	prep_stm_name = NULL; /* FIXME: store a ref to that prepared statement, 
				 which must be destroyed if @query changes */
	if (!prep_stm_name) {
		GError *error = NULL;
		GdaConnectionEvent *event = NULL;

		pseudo_sql = gda_renderer_render_as_sql (GDA_RENDERER (query), params, &used_params, 
							 GDA_RENDERER_PARAMS_AS_DOLLAR, &error);
		if (!pseudo_sql) {
			event = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, error && error->message ? 
							      error->message : _("No detail"));
			gda_connection_add_event (cnc, event);
			g_error_free (error);
		}

		prep_stm_name = g_strdup_printf ("gda_query_prep_stm");
		pg_res = PQprepare (pconn, prep_stm_name, pseudo_sql, 0, NULL);
		if (!pg_res || (PQresultStatus (pg_res) != PGRES_COMMAND_OK))
			event = gda_postgres_make_error (cnc, pconn, pg_res);
		PQclear (pg_res);

		if (event) 
			return NULL;
	}

	/* bind parameters to the prepared statement */
	if (used_params) {
		gint index, blob_index;
		nb_params = g_slist_length (used_params);
		param_values = g_new0 (char *, nb_params + 1);
		param_lengths = g_new0 (int, nb_params + 1);
		param_formats = g_new0 (int, nb_params + 1);
		
		for (index = 0, blob_index = 0, list = used_params; list && allok; list = list->next, index++) {
			GdaParameter *param = GDA_PARAMETER (list->data);
			const GValue *value;
				
			value = gda_parameter_get_value (param);
			if (!value && gda_value_is_null (value))
				param_values [index] = NULL;
			else {
				GType type = G_VALUE_TYPE (value);
				if (type == GDA_TYPE_BLOB) {
					/* specific BLOB treatment */
					GdaBlob *blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
					GdaPostgresBlobOp *op;
					
					/* Postgres requires that a transaction be started for LOB operations */
					allok = gda_postgres_check_transaction_started (cnc);
					if (!allok)
						continue;
					
					/* create GdaBlobOp */
					op = (GdaPostgresBlobOp *) gda_postgres_blob_op_new (cnc);
					
					/* always create a new blob as there is no way to truncate an existing blob */
					if (gda_postgres_blob_op_declare_blob (op) &&
					    gda_blob_op_write ((GdaBlobOp*) op, blob, 0)) 
						param_values [index] = gda_postgres_blob_op_get_id (op);
					else
						param_values [index] = NULL;
					g_object_unref (op);
					blob_index++;
				}
				else if (type == GDA_TYPE_BINARY) {
					/* directly bin binary data */
					GdaBinary *bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
					param_values [index] = bin->data;
					param_lengths [index] = bin->binary_length;
					param_formats [index] = 1; /* binary format */
				}
				else if ((type == G_TYPE_DATE) || (type == GDA_TYPE_TIMESTAMP) ||
					 (type == GDA_TYPE_TIME)) {
					GdaHandlerTime *timdh;

					timdh = GDA_HANDLER_TIME (gda_server_provider_get_data_handler_gtype (provider, cnc, type));
					g_assert (timdh);
					param_values [index] = gda_handler_time_get_no_locale_str_from_value (timdh,
													      value);
				}
				else {
					GdaDataHandler *dh;
					
					dh = gda_server_provider_get_data_handler_gtype (provider, cnc, type);
					if (dh) 
						param_values [index] = gda_data_handler_get_str_from_value (dh, value);
					else
						param_values [index] = NULL;
				}
			}
		}
		g_slist_free (used_params);
	}

	if (!allok) {
		g_free (prep_stm_name);
		gda_postgres_provider_single_command (pg_prv, cnc, "DEALLOCATE gda_query_prep_stm"); 
		g_strfreev (param_values);
		g_free (param_lengths);
		g_free (param_formats);
		if (pg_existing_blobs) 
			PQclear (pg_existing_blobs);
		return NULL;
	}

	GdaConnectionEvent *event;
	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event, pseudo_sql);
	gda_connection_add_event (cnc, event);

	pg_res = PQexecPrepared (pconn, prep_stm_name, nb_params, (const char * const *) param_values, 
				 param_lengths, param_formats, 0);
	g_free (prep_stm_name);
	gda_postgres_provider_single_command (pg_prv, cnc, "DEALLOCATE gda_query_prep_stm"); 
	g_strfreev (param_values);
	g_free (param_lengths);
	g_free (param_formats);
	retval = compute_retval_from_pg_res (cnc, pconn, pseudo_sql, pg_res, NULL);

	if (retval) {
		if (pg_existing_blobs) {
			/* delete the blobs from the existing blobs */
			gint row, nrows, col, ncols;
			nrows = PQntuples (pg_existing_blobs);
			ncols = PQnfields (pg_existing_blobs);
			for (row = 0; row < nrows; row++) {
				for (col = 0; col < ncols; col++) {
					if (PQftype (pg_existing_blobs, col) == 26 /*OIDOID*/) {
						gchar *blobid = PQgetvalue (pg_existing_blobs, row, col);
						if (atoi (blobid) != InvalidOid) {
							/* add a savepoint to prevent a blob unlink 
							   failure from rendering the transaction unuseable */
							gda_connection_add_savepoint (cnc, 
										      "__gda_blob_read_svp", 
										      NULL);
							if (lo_unlink (pconn, atoi (blobid)) !=  1)
								gda_connection_rollback_savepoint (cnc, 
								    "__gda_blob_read_svp", NULL);
							else
								gda_connection_delete_savepoint (cnc, 
								    "__gda_blob_read_svp", NULL);	
						}
					}
					else
						ncols = col;
				}
			}
			PQclear (pg_existing_blobs);
		}
	}

	return retval;
}

static gchar *
gda_postgres_provider_get_last_insert_id (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaDataModel *recset)
{
	Oid oid;
	PGresult *pgres;
	GdaPostgresConnectionData *priv_data;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (recset) {
		g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
		/* get the PQresult from the recordset */
		pgres = gda_postgres_recordset_get_pgresult (GDA_POSTGRES_RECORDSET (recset));
		if (pgres) {
			oid = PQoidValue (pgres);
			if (oid != InvalidOid)
				return g_strdup_printf ("%d", oid);
		}
		return NULL;
	}

	/* get the last inserted OID kept */
	if (priv_data->last_insert_id)
		return g_strdup_printf ("%d", priv_data->last_insert_id);
	else
		return NULL;
}

static gboolean 
gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
				         GdaConnection *cnc,
					 const gchar *name, GdaTransactionIsolation level,
					 GError **error)
{
        gboolean result;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;

	gchar * write_option=NULL;
	gchar * isolation_level=NULL;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float >= 6.5){
	        if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
	                if (priv_data->version_float >= 7.4){
		                write_option = "READ ONLY";
           	        } 
                        else {
		                gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
				return FALSE;
			}
		}
		switch (level) {
		case GDA_TRANSACTION_ISOLATION_READ_COMMITTED :
		        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL READ COMMITTED ", write_option, NULL);
		        break;
		case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED :
		        gda_connection_add_event_string (cnc, _("Transactions are not supported in read uncommitted isolation level"));
		        return FALSE;
		case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ :
		        gda_connection_add_event_string (cnc, _("Transactions are not supported in repeatable read isolation level"));
		        return FALSE;
		case GDA_TRANSACTION_ISOLATION_SERIALIZABLE :
		        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE ", write_option, NULL);
		        break;
		default: 
		        isolation_level = NULL;
		}
	}

	result = gda_postgres_provider_single_command (pg_prv, cnc, "BEGIN"); 
	if (result && isolation_level != NULL) {
	        result = gda_postgres_provider_single_command (pg_prv, cnc, isolation_level) ;
	} 
	g_free(isolation_level);

	return result;
}

static gboolean 
gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "COMMIT"); 
}

static gboolean 
gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    const gchar *name, GError **error)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "ROLLBACK"); 
}

static gboolean
gda_postgres_provider_add_savepoint (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name,
				     GError **error)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	gchar *str;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	str = g_strdup_printf ("SAVEPOINT %s", name);
	retval = gda_postgres_provider_single_command (pg_prv, cnc, str); 
	g_free (str);
	return retval;
}

static gboolean
gda_postgres_provider_rollback_savepoint (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *name,
					  GError **error)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	gchar *str;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	str = g_strdup_printf ("ROLLBACK TO SAVEPOINT %s", name);
	retval = gda_postgres_provider_single_command (pg_prv, cnc, str); 
	g_free (str);
	return retval;

}

static gboolean
gda_postgres_provider_delete_savepoint (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name,
					GError **error)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	gchar *str;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	str = g_strdup_printf ("RELEASE SAVEPOINT %s", name);
	retval = gda_postgres_provider_single_command (pg_prv, cnc, str); 
	g_free (str);
	return retval;
}

static gboolean 
gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
				      GdaConnection *cnc,
				      const gchar *command)
{
	GdaPostgresConnectionData *priv_data;
	PGconn *pconn;
	PGresult *pg_res;
	gboolean result;
	GdaConnectionEvent *error = NULL;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
		return FALSE;
	}

	pconn = priv_data->pconn;
	result = FALSE;
	pg_res = gda_postgres_PQexec_wrap (cnc, pconn, command);
	if (pg_res != NULL){		
		result = PQresultStatus (pg_res) == PGRES_COMMAND_OK;
		if (result == FALSE) 
			error = gda_postgres_make_error (cnc, pconn, pg_res);

		PQclear (pg_res);
	}
	else
		error = gda_postgres_make_error (cnc, pconn, NULL);

	gda_connection_internal_treat_sql (cnc, command, error);

	return result;
}

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaConnectionFeature feature)
{
	GdaPostgresProvider *pgprv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pgprv), FALSE);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);	

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
		return TRUE;
	case GDA_CONNECTION_FEATURE_NAMESPACES:
		if (cnc) {
			g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
			priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
			if (priv_data->version_float >= 7.3)
				return TRUE;
		}
		else
			return FALSE;
	default:
		break;
	}
 
	return FALSE;
}

static GdaServerProviderInfo *
gda_postgres_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"PostgreSQL",
		TRUE, 
		TRUE,
		TRUE,
		TRUE,
		TRUE,
		TRUE
	};
	
	return &info;
}

static const GdaPostgresTypeOid *
find_data_type_from_oid (GdaPostgresConnectionData *priv_data, const gchar *oid)
{
	GdaPostgresTypeOid *retval = NULL;
	gint i = 0;

	while ((i < priv_data->ntypes) && !retval) {
		if (priv_data->type_data[i].oid == atoi (oid))
			retval = &(priv_data->type_data[i]);
		i++;
	}
	
	return retval;
}

static GList *
gda_postgres_fill_procs_data (GdaDataModelArray *recset,
			      GdaPostgresConnectionData *priv_data)
{
	gchar *query;
	PGresult *pg_res;
	gint row_count, i;
	GList *list = NULL;

	if (priv_data->version_float < 7.3) 
		query = g_strdup_printf (
                         "SELECT pg_proc.oid, proname, usename, obj_description (pg_proc.oid), typname, "
			 "pronargs, proargtypes, prosrc "
			 "FROM pg_proc, pg_user, pg_type "
			 "WHERE pg_type.oid=prorettype AND usesysid=proowner AND "
			 "pg_type.oid in (SELECT ty.oid FROM pg_type ty WHERE ty.typrelid = 0 AND "
			 "ty.typname !~ '^_' AND  ty.oid not in (%s)) "
			 "AND proretset = 'f' AND ((pronargs != 0 AND oidvectortypes (proargtypes)!= '') "
			 "OR pronargs=0) "
			 "ORDER BY proname, pronargs",
			 priv_data->avoid_types_oids);
	else
		query = g_strdup_printf (
                         "SELECT p.oid, p.proname, u.usename, pg_catalog.obj_description (p.oid), "
			 "t.typname, p.pronargs, p.proargtypes, p.prosrc "
			 "FROM pg_catalog.pg_proc p, pg_catalog.pg_user u, pg_catalog.pg_type t, "
			 "pg_catalog.pg_namespace n "
			 "WHERE t.oid=p.prorettype AND u.usesysid=p.proowner AND n.oid = p.pronamespace "
			 "AND p.prorettype <> 'pg_catalog.cstring'::pg_catalog.regtype "
			 "AND p.proargtypes[0] <> 'pg_catalog.cstring'::pg_catalog.regtype "
			 "AND t.oid in (SELECT ty.oid FROM pg_catalog.pg_type ty WHERE ty.typrelid = 0 AND "
			 "ty.typname !~ '^_' AND (ty.oid not in (%s) OR ty.oid = '%s')) "
			 "AND p.proretset = 'f' "
			 "AND NOT p.proisagg "
			 "AND pg_catalog.pg_function_is_visible(p.oid) "
			 "ORDER BY proname, pronargs",
			 priv_data->avoid_types_oids, priv_data->any_type_oid);

	pg_res = gda_postgres_PQexec_wrap (priv_data->cnc, priv_data->pconn, query);
	g_free (query);
	if (pg_res == NULL)
		return NULL;

	row_count = PQntuples (pg_res);
	for (i = 0; i < row_count; i++) {
		GValue *value;
		gchar *thevalue, *procname, *instr, *ptr;
		GString *gstr;
		gint nbargs, argcounter;
		GList *rowlist = NULL;
		gboolean insert = TRUE;

		/* Proc name */
		procname = thevalue = PQgetvalue(pg_res, i, 1);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Proc_Id */
		thevalue = PQgetvalue (pg_res, i, 0);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Owner */
		thevalue = PQgetvalue(pg_res, i, 2);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Comments */ 
		thevalue = PQgetvalue(pg_res, i, 3);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Out type */ 
		thevalue = PQgetvalue(pg_res, i, 4);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Number of args */
		thevalue = PQgetvalue(pg_res, i, 5);
		nbargs = atoi (thevalue);
		g_value_set_int (value = gda_value_new (G_TYPE_INT), nbargs);
		rowlist = g_list_append (rowlist, value);

		/* In types */
		argcounter = 0;
		gstr = NULL;
		if (PQgetvalue(pg_res, i, 6)) {
			instr = g_strdup (PQgetvalue(pg_res, i, 6));
			ptr = strtok (instr, " ");
			while (ptr && *ptr && insert) {
				const GdaPostgresTypeOid *typeoid = NULL;

				if (!strcmp (ptr, priv_data->any_type_oid)) {
					thevalue = "-";
				}
				else {
					typeoid = find_data_type_from_oid (priv_data, ptr);
					if (typeoid) {
						thevalue = typeoid->name;
					}
					else {
						insert = FALSE;
					}
				}

				if (insert) {
					if (!gstr) 
						gstr = g_string_new (thevalue);
					else
						g_string_append_printf (gstr, ",%s", thevalue);
				}
					
				ptr = strtok (NULL, " ");
				argcounter ++;
			}
			g_free (instr);
		}
		else 
			insert = FALSE;

		if (gstr) {
			g_value_take_string (value = gda_value_new (G_TYPE_STRING), gstr->str);
			g_string_free (gstr, FALSE);
		}
		else
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), NULL);
		rowlist = g_list_append (rowlist, value);
		
		if (argcounter != nbargs)
			insert = FALSE;
		
		
		
		/* Definition */
		thevalue = PQgetvalue(pg_res, i, 7);
		if (!strcmp (thevalue, procname))
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), NULL);
		else
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		if (insert)
			list = g_list_append (list, rowlist);
		else {
			g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
			g_list_free (rowlist);
		}
			
		rowlist = NULL;
	}

	PQclear (pg_res);

	return list;
}

/* defined later */
static void add_g_list_row (gpointer data, gpointer user_data);

static GdaDataModel *
get_postgres_procedures (GdaConnection *cnc, GdaParameterList *params)
{
	GList *list;
	GdaPostgresConnectionData *priv_data;
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_PROCEDURES);
	list = gda_postgres_fill_procs_data (recset, priv_data);
	g_list_foreach (list, add_g_list_row, recset);
	g_list_free (list);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_postgres_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaPostgresConnectionData *priv_data;
	GdaDataModel *recset;
	GdaParameter *par = NULL;
	const gchar *namespace = NULL;
	const gchar *tablename = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params) {
		par = gda_parameter_list_find_param (params, "namespace");
		if (par)
			namespace = g_value_get_string ((GValue *) gda_parameter_get_value (par));

		par = gda_parameter_list_find_param (params, "name");
		if (par)
			tablename = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) {
		reclist = process_sql_commands (NULL, cnc,
						"SELECT relname, usename, obj_description(pg_class.oid), NULL "
						"FROM pg_class, pg_user "
						"WHERE usesysid=relowner AND relkind = 'r' AND relname !~ '^pg_' "
						"ORDER BY relname",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	}
	else {
		if (namespace) {
			gchar *part = NULL;
			gchar *query;

			if (tablename)
				part = g_strdup_printf ("AND c.relname = '%s' ", tablename);

			query = g_strdup_printf ("SELECT c.relname, u.usename, pg_catalog.obj_description(c.oid), NULL "
						 "FROM pg_catalog.pg_class c, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
						 "WHERE u.usesysid=c.relowner AND c.relkind = 'r' "
						 "AND c.relnamespace=n.oid "
						 "%s"
						 "AND n.nspname ='%s' "
						 "AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
						 "ORDER BY relname", part ? part : "", namespace);
			if (part) g_free (part);
			reclist = process_sql_commands (NULL, cnc,
							query,
							GDA_COMMAND_OPTION_STOP_ON_ERRORS);
			g_free (query);
		}
		else {
			gchar *part = NULL;
			gchar *query;

			if (tablename)
				part = g_strdup_printf ("AND c.relname = '%s' ", tablename);

			query = g_strdup_printf ("SELECT c.relname, u.usename, pg_catalog.obj_description(c.oid), NULL "
						 "FROM pg_catalog.pg_class c, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
						 "WHERE u.usesysid=c.relowner AND c.relkind = 'r' "
						 "AND c.relnamespace=n.oid "
						 "%s"
						 "AND pg_catalog.pg_table_is_visible (c.oid) "
						 "AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
						 "ORDER BY relname", 
						 part ? part : "");
			if (part) g_free (part);
			reclist = process_sql_commands (NULL, cnc,
							query,
							GDA_COMMAND_OPTION_STOP_ON_ERRORS);
			g_free (query);
		}
	}

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TABLES);

	return recset;
}

static GdaDataModel *
get_postgres_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaPostgresConnectionData *priv_data;
	gint i;
	static GHashTable *synonyms = NULL;

	if (!synonyms) {
		synonyms = g_hash_table_new (g_str_hash, g_str_equal);

		g_hash_table_insert (synonyms, "int4", "int,integer");
		g_hash_table_insert (synonyms, "int8", "bigint");
		g_hash_table_insert (synonyms, "serial8", "bigserial");
		g_hash_table_insert (synonyms, "varbit", "bit varying");
		g_hash_table_insert (synonyms, "bool", "boolean");
		g_hash_table_insert (synonyms, "varchar", "character varying");
		g_hash_table_insert (synonyms, "char", "character");
		g_hash_table_insert (synonyms, "float8", "double precision");
		g_hash_table_insert (synonyms, "numeric", "decimal");
		g_hash_table_insert (synonyms, "float4", "real");
		g_hash_table_insert (synonyms, "int2", "smallint");
		g_hash_table_insert (synonyms, "serial4", "serial");
	}

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES);

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	for (i = 0; i < priv_data->ntypes; i++) {
		GList *value_list = NULL;
		gchar *syn;
		GValue *tmpval;

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), priv_data->type_data[i].name);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), priv_data->type_data[i].owner);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), priv_data->type_data[i].comments);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), priv_data->type_data[i].type);
		value_list = g_list_append (value_list, tmpval);

		syn = g_hash_table_lookup (synonyms, priv_data->type_data[i].name);
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), syn);
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_postgres_views (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaPostgresConnectionData *priv_data;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) 
		reclist = process_sql_commands (
			       NULL, cnc, 
			       "SELECT relname, usename, obj_description(pg_class.oid), NULL "
			       "FROM pg_class, pg_user "
			       "WHERE usesysid=relowner AND relkind = 'v' AND relname !~ '^pg_' ORDER BY relname",
			       GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		reclist = process_sql_commands (
			       NULL, cnc, 
			       "SELECT c.relname, u.usename, pg_catalog.obj_description(c.oid), "
			       "pg_catalog.pg_get_viewdef (c.oid) "
			       "FROM pg_catalog.pg_class c, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
			       "WHERE u.usesysid=c.relowner AND c.relkind = 'v' "
			       "AND c.relnamespace=n.oid AND pg_catalog.pg_table_is_visible (c.oid) "
			       "AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
			       "ORDER BY relname",
			       GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_VIEWS);

	return recset;
}

static GdaDataModel *
get_postgres_indexes (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaPostgresConnectionData *priv_data;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) 
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT relname FROM pg_class WHERE relkind = 'i' AND "
						"relname !~ '^pg_' ORDER BY relname",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT c.relname "
						"FROM pg_catalog.pg_class c, pg_catalog.pg_namespace n "
						"WHERE relkind = 'i' "
						"AND n.oid = c.relnamespace "
						"AND pg_catalog.pg_table_is_visible(c.oid) "
						"AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
						"ORDER BY relname",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_INDEXES);

	return recset;
}

static GdaDataModel *
get_postgres_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaPostgresConnectionData *priv_data;
	GdaDataModel *recset;
	GdaParameter *par = NULL;
	const gchar *namespace = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params)
		par = gda_parameter_list_find_param (params, "namespace");

	if (par)
		namespace = g_value_get_string ((GValue *) gda_parameter_get_value (par));

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) {
		reclist = process_sql_commands (
			   NULL, cnc, 
			   "(SELECT a.aggname, a.oid::varchar, usename, obj_description (a.oid), t2.typname, t1.typname, NULL "
			   "FROM pg_aggregate a, pg_type t1, pg_type t2, pg_user u "
			   "WHERE a.aggbasetype = t1.oid AND a.aggfinaltype = t2.oid AND u.usesysid=aggowner) UNION "
			   "(SELECT a.aggname, a.oid, usename, obj_description (a.oid), t2.typname , '-', NULL "
			   "FROM pg_aggregate a, pg_type t2, pg_user u "
			   "WHERE a.aggfinaltype = t2.oid AND u.usesysid=aggowner AND a.aggbasetype = 0) ORDER BY 2",
			   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	}
	else {
		if (namespace) {
			gchar *query;

			query = g_strdup_printf ("%s", namespace);
			reclist = process_sql_commands (NULL, cnc, query, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
			g_free (query);
		}
		else {
			gchar *query;

			query = g_strdup_printf (
                                  "SELECT p.proname, p.oid::varchar, u.usename, pg_catalog.obj_description(p.oid),"
				  "typo.typname,"
				  "CASE p.proargtypes[0] "
				  "WHEN 'pg_catalog.\"any\"'::pg_catalog.regtype "
				  "THEN CAST('-' AS pg_catalog.text) "
				  "ELSE typi.typname "
				  "END,"
				  "NULL "
				  "FROM pg_catalog.pg_proc p, pg_catalog.pg_user u, pg_catalog.pg_namespace n, "
				  "pg_catalog.pg_type typi, pg_catalog.pg_type typo "
				  "WHERE u.usesysid=p.proowner "
				  "AND n.oid = p.pronamespace "
				  "AND p.prorettype = typo.oid "
				  "AND (typo.oid NOT IN (%s) OR typo.oid='%s') "
				  "AND p.proargtypes[0] = typi.oid "
				  "AND (typi.oid NOT IN (%s) OR typi.oid='%s') "
				  "AND p.proisagg "
				  "AND pg_catalog.pg_function_is_visible(p.oid) "
				  "ORDER BY 2", priv_data->avoid_types_oids, priv_data->any_type_oid,
				  priv_data->avoid_types_oids, priv_data->any_type_oid);
			reclist = process_sql_commands (NULL, cnc, query, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
			g_free (query);
		}
	}

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_AGGREGATES);

	return recset;
}

static GdaDataModel *
get_postgres_triggers (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) 
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT tgname FROM pg_trigger "
						"WHERE tgisconstraint = false "
						"ORDER BY tgname ",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		/* REM: the triggers don't seem to belong to a particular schema... */
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT t.tgname "
						"FROM pg_catalog.pg_trigger t "
						"WHERE t.tgisconstraint = false "
						"ORDER BY t.tgname",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TRIGGERS);

	return recset;
}

static GList *
gda_postgres_get_idx_data (GdaPostgresConnectionData *priv_data, const gchar *tblname)
{
	gint nidx, i;
	GList *idx_list = NULL;
	GdaPostgresIdxData *idx_data;
	gchar *query;
	PGresult *pg_idx;

	if (priv_data->version_float < 7.3)
		query = g_strdup_printf ("SELECT i.indkey, i.indisprimary, i.indisunique "
					 "FROM pg_class c, pg_class c2, pg_index i "
					 "WHERE c.relname = '%s' AND c.oid = i.indrelid "
					 "AND i.indexrelid = c2.oid", tblname);
	else
		query = g_strdup_printf ("SELECT i.indkey, i.indisprimary, i.indisunique "
					 /*",pg_get_indexdef (i.indexrelid, 1, false) ": to get index definition */
					 "FROM pg_catalog.pg_class c, pg_catalog.pg_class c2, pg_catalog.pg_index i "
					 "WHERE c.relname = '%s' "
					 "AND c.oid = i.indrelid "
					 "AND i.indexrelid = c2.oid "
					 "AND pg_catalog.pg_table_is_visible(c.oid) AND i.indkey [0] <> 0", tblname);

	pg_idx = gda_postgres_PQexec_wrap (priv_data->cnc, priv_data->pconn, query);
	g_free (query);
	if (pg_idx == NULL)
		return NULL;

	nidx = PQntuples (pg_idx);

	idx_data = g_new (GdaPostgresIdxData, nidx);
	for (i = 0; i < nidx; i++){
		gchar **arr, **ptr;
		gint ncolumns;
		gchar *value;
		gint k;

		value = PQgetvalue (pg_idx, i, 0);
		if (value && *value) 
			arr = g_strsplit (value, " ", 0);
		else {
			idx_data[i].ncolumns = 0;
			continue;
		}
		
		value = PQgetvalue (pg_idx, i, 1);
		idx_data[i].primary = (*value == 't') ? TRUE : FALSE;
		value = PQgetvalue (pg_idx, i, 2);
		idx_data[i].unique = (*value == 't') ? TRUE : FALSE;

		ptr = arr;
		ncolumns = 0;
		while (*ptr++)
			ncolumns++;

		idx_data[i].ncolumns = ncolumns;
		idx_data[i].columns = g_new (gint, ncolumns);
		for (k = 0; k < ncolumns; k++)
			idx_data[i].columns[k] = atoi (arr[k]) - 1;

		idx_list = g_list_append (idx_list, &idx_data[i]);
		g_strfreev (arr);
	}

	PQclear (pg_idx);

	return idx_list;
}

static gboolean
gda_postgres_index_type (gint colnum, GList *idx_list, IdxType type)
{
	GList *list;
	GdaPostgresIdxData *idx_data;
	gint i;

	if (idx_list == NULL || g_list_length (idx_list) == 0)
		return FALSE;

	for (list = idx_list; list; list = list->next) {
		idx_data = (GdaPostgresIdxData *) list->data;
		for (i = 0; i < idx_data->ncolumns; i++) {
			if (idx_data->columns[i] == colnum){
				return (type == IDX_PRIMARY) ? 
					idx_data->primary : idx_data->unique;
			}
		}
	}

	return FALSE;
}

/* Converts a single dimension array in the form of '{AA,BB,CC}' to a list of
 * strings (here ->"AA"->"BB"->"CC) which must be freed by the caller
 */
static GSList *
gda_postgres_itemize_simple_array (const gchar *array)
{
	GSList *list = NULL;
	gchar *str, *ptr, *tok;

	g_return_val_if_fail (array, NULL);

	str = g_strdup (array);
	ptr = str;

	/* stripping { and } */
	if (*str == '{')
		ptr++;
	if (str [strlen (str)-1] == '}')
		str [strlen (str)-1] = 0;

	/* splitting */
	ptr = strtok_r (ptr, ",", &tok);
	while (ptr && *ptr) {
		list = g_slist_append (list, g_strdup (ptr));
		ptr = strtok_r (NULL, ",", &tok);
	}
	
	g_free (str);

	return list;
}

static void
gda_postgres_itemize_simple_array_free (GSList *list)
{
	GSList *list2;

	list2 = list;
	while (list2) {
		g_free (list2->data);
		list2 = g_slist_next (list2);
	}
	g_slist_free (list);
}

static GList *
gda_postgres_get_ref_data (GdaPostgresConnectionData *priv_data, const gchar *tblname)
{
	gint nref, i;
	GList *ref_list = NULL;
	GdaPostgresRefData *ref_data;
	gchar *query;
	PGresult *pg_ref;

	if (priv_data->version_float < 7.3) 
		query = g_strdup_printf ("SELECT tr.tgargs "
					 "FROM pg_class c, pg_trigger tr "
					 "WHERE c.relname = '%s' AND c.oid = tr.tgrelid AND "
					 "tr.tgisconstraint = true AND tr.tgnargs = 6", tblname);
	else
		query = g_strdup_printf ("SELECT o.conkey, o.confkey, fc.relname "
					 "FROM pg_catalog.pg_class c "
					 "INNER JOIN pg_catalog.pg_constraint o ON (o.conrelid = c.oid) "
					 "LEFT JOIN pg_catalog.pg_class fc ON (fc.oid=o.confrelid) "
					 "WHERE c.relname = '%s' AND contype = 'f' "
					 "AND pg_catalog.pg_table_is_visible (c.oid) "
					 "AND pg_catalog.pg_table_is_visible (fc.oid)", tblname);

	pg_ref = gda_postgres_PQexec_wrap (priv_data->cnc, priv_data->pconn, query);
	g_free (query);
	if (pg_ref == NULL)
		return NULL;

	nref = PQntuples (pg_ref);

	for (i = 0; i < nref; i++){
		if (priv_data->version_float < 7.3) {
			gint lentblname;
			gchar **arr;
			gchar *value;
			
			lentblname = strlen (tblname);
			ref_data = g_new0 (GdaPostgresRefData, 1);
			value = PQgetvalue (pg_ref, i, 0);
			arr = g_strsplit (value, "\\000", 0);
			if (!strncmp (tblname, arr[1], lentblname)) {
				ref_data->colname = g_strdup (arr[4]);
				ref_data->reference = g_strdup_printf ("%s.%s", arr[2], arr[5]);
				ref_list = g_list_append (ref_list, ref_data);
			}
			
			g_strfreev (arr);
		}
		else {
			gchar *value;
			GSList *itemf, *listf;
			GSList *itemp, *listp;
			
			value = PQgetvalue (pg_ref, i, 0);
			listf = gda_postgres_itemize_simple_array (value);
			itemf = listf;

			value = PQgetvalue (pg_ref, i, 1);
			listp = gda_postgres_itemize_simple_array (value);
			itemp = listp;

			g_assert (g_slist_length (listf) == g_slist_length (listp));
			while (itemp && itemf) {
				PGresult *pg_res;
				query = g_strdup_printf ("SELECT a.attname FROM pg_catalog.pg_class c "
							 "LEFT JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid) "
							 "WHERE c.relname = '%s' AND pg_catalog.pg_table_is_visible (c.oid) "
							 "AND a.attnum = %d AND NOT a.attisdropped", 
							 PQgetvalue (pg_ref, i, 2), atoi (itemp->data));
				pg_res = gda_postgres_PQexec_wrap (priv_data->cnc, priv_data->pconn, query);
				/*g_print ("Query: %s\n", query);*/
				g_free (query);
				if (pg_res == NULL)
					return NULL;
				
				g_assert (PQntuples (pg_res) == 1);

				ref_data = g_new0 (GdaPostgresRefData, 1);
				ref_data->colname = NULL;
				ref_data->colnum = atoi (itemf->data);
				ref_data->reference = g_strdup_printf ("%s.%s", 
								       PQgetvalue (pg_ref, i, 2), PQgetvalue (pg_res, 0, 0));
				PQclear (pg_res);
				ref_list = g_list_append (ref_list, ref_data);
				itemp = g_slist_next (itemp);
				itemf = g_slist_next (itemf);
			}
			gda_postgres_itemize_simple_array_free (listf);
			gda_postgres_itemize_simple_array_free (listp);
		}
	}
	PQclear (pg_ref);
		
	return ref_list;
}

static void
free_idx_data (gpointer data, gpointer user_data)
{
	GdaPostgresIdxData *idx_data = (GdaPostgresIdxData *) data;

	g_free (idx_data->columns);
}

static void
free_ref_data (gpointer data, gpointer user_data)
{
	GdaPostgresRefData *ref_data = (GdaPostgresRefData *) data;

	if (ref_data->colname)
		g_free (ref_data->colname);
	g_free (ref_data->reference);
	g_free (ref_data);
}

/* FIXME: a and b are of the same type! */
static gint
ref_custom_compare (gconstpointer a, gconstpointer b)
{
	GdaPostgresRefData *ref_data = (GdaPostgresRefData *) a;
	gchar *colname = (gchar *) b;

	if (!strcmp (ref_data->colname, colname))
		return 0;

	return 1;
}

static GList *
gda_postgres_fill_md_data (const gchar *tblname, GdaDataModelArray *recset,
			   GdaPostgresConnectionData *priv_data)
{
	gchar *query;
	PGresult *pg_res;
	gint row_count, i;
	GList *list = NULL;
	GList *idx_list;
	GList *ref_list;

	if (priv_data->version_float < 7.3)
		query = g_strdup_printf (
			"(SELECT a.attname, b.typname, a.atttypmod, b.typlen, a.attnotnull, d.adsrc, "
			"a.attnum FROM pg_class c, pg_attribute a, pg_type b, pg_attrdef d "
			"WHERE c.relname = '%s' AND a.attnum > 0 AND "
			"a.attrelid = c.oid and b.oid = a.atttypid AND "
			"a.atthasdef = 't' and d.adrelid=c.oid and d.adnum=a.attnum) "
			"UNION (SELECT a.attname, b.typname, a.atttypmod, b.typlen, a.attnotnull, NULL, "
			"a.attnum FROM pg_class c, pg_attribute a, pg_type b "
			  "WHERE c.relname = '%s' AND a.attnum > 0 AND "
			"a.attrelid = c.oid and b.oid = a.atttypid AND a.atthasdef = 'f') ORDER BY 7",
			tblname, tblname);
	else 
		query = g_strdup_printf (
			"SELECT a.attname, t.typname, a.atttypmod, t.typlen, a.attnotnull, pg_get_expr (d.adbin, c.oid), a.attnum "
			"FROM pg_catalog.pg_class c "
			"LEFT JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid) "
			"FULL JOIN pg_catalog.pg_attrdef d ON (a.attnum = d.adnum AND d.adrelid=c.oid) "
			"LEFT JOIN pg_catalog.pg_type t ON (t.oid = a.atttypid) "
			"WHERE c.relname = '%s' AND pg_catalog.pg_table_is_visible (c.oid) AND a.attnum > 0 "
			"AND NOT a.attisdropped ORDER BY 7", tblname);

	pg_res = gda_postgres_PQexec_wrap (priv_data->cnc, priv_data->pconn, query);
	g_free (query);
	if (pg_res == NULL)
		return NULL;

	idx_list = gda_postgres_get_idx_data (priv_data, tblname);
	ref_list = gda_postgres_get_ref_data (priv_data, tblname);

	row_count = PQntuples (pg_res);
	for (i = 0; i < row_count; i++) {
		GValue *value;
		gchar *thevalue, *colname, *ref = NULL;
		GType type;
		gint32 integer;
		GList *rowlist = NULL;
		GList *rlist;

		/* Field name */
		colname = thevalue = PQgetvalue (pg_res, i, 0);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Data type */
		thevalue = PQgetvalue(pg_res, i, 1);
		type = gda_postgres_type_name_to_gda (priv_data->h_table, 
						      thevalue);
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Defined size */
		thevalue = PQgetvalue(pg_res, i, 3);
		integer = atoi (thevalue);
		if (integer == -1 && type == G_TYPE_STRING) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = atoi (thevalue) - 4; /* don't know where the -4 comes from! */
		}
		if (integer == -1 && type == GDA_TYPE_NUMERIC) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = atoi(thevalue) / 65536;
		}
			
		g_value_set_int (value = gda_value_new (G_TYPE_INT), (integer != -1) ? integer : 0);
		rowlist = g_list_append (rowlist, value);

		/* Scale */ 
		integer = 0;
		if (type == GDA_TYPE_NUMERIC) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = (atoi(thevalue) % 65536) - 4;
		}
		g_value_set_int (value = gda_value_new (G_TYPE_INT), integer);
		rowlist = g_list_append (rowlist, value);

		/* Not null? */
		thevalue = PQgetvalue(pg_res, i, 4);
		g_value_set_boolean (value = gda_value_new (G_TYPE_BOOLEAN), (*thevalue == 't' ? TRUE : FALSE));
		rowlist = g_list_append (rowlist, value);

		/* Primary key? */
		g_value_set_boolean (value = gda_value_new (G_TYPE_BOOLEAN),
				     gda_postgres_index_type (i, idx_list, IDX_PRIMARY));
		rowlist = g_list_append (rowlist, value);

		/* Unique index? */
		g_value_set_boolean (value = gda_value_new (G_TYPE_BOOLEAN),
				     gda_postgres_index_type (i, idx_list, IDX_UNIQUE));
		rowlist = g_list_append (rowlist, value);

		/* References */
		if (priv_data->version_float < 7.3) {
			rlist = g_list_find_custom (ref_list, colname, ref_custom_compare); 
			if (rlist)
				ref = ((GdaPostgresRefData *) rlist->data)->reference;
			else
				ref = "";
		}
		else {
			GList *plist = ref_list;
			ref = NULL;
			
			while (plist && !ref) {
				if (((GdaPostgresRefData *) plist->data)->colnum == atoi (PQgetvalue(pg_res, i, 6)))
					ref = ((GdaPostgresRefData *) plist->data)->reference;
				plist = g_list_next (plist);
			}
			if (!ref)
				ref = "";
		}
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), ref);
		rowlist = g_list_append (rowlist, value);

		/* Default value */
		if (!PQgetisnull (pg_res, i, 5)) {
			thevalue = PQgetvalue (pg_res, i, 5);
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), thevalue);
		}
		else
			value = gda_value_new_null ();
	        rowlist = g_list_append (rowlist, value);

		/* Extra attributes */
		if (strstr (PQgetvalue (pg_res, i, 5), "nextval"))
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), "AUTO_INCREMENT");
		else
			g_value_set_string (value = gda_value_new (G_TYPE_STRING), "");
	        rowlist = g_list_append (rowlist, value);			

		list = g_list_append (list, rowlist);
		rowlist = NULL;
	}

	PQclear (pg_res);

	if (idx_list && idx_list->data) {
		g_list_foreach (idx_list, free_idx_data, NULL);
		g_free (idx_list->data);
	}

	if (ref_list && ref_list->data) 
		g_list_foreach (ref_list, free_ref_data, NULL);

	g_list_free (ref_list);
	g_list_free (idx_list);

	return list;
}

static void
add_g_list_row (gpointer data, gpointer user_data)
{
	GList *rowlist = data;
	GdaDataModelArray *recset = user_data;

	gda_data_model_append_values (GDA_DATA_MODEL (recset), rowlist, NULL);
	g_list_foreach (rowlist, (GFunc) gda_value_free, NULL);
	g_list_free (rowlist);
}

static GdaDataModel *
get_postgres_fields_metadata (GdaConnection *cnc, GdaParameterList *params)
{
	GList *list;
	GdaPostgresConnectionData *priv_data;
	GdaDataModelArray *recset;
	GdaParameter *par;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);
	
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS);
	list = gda_postgres_fill_md_data (tblname, recset, priv_data);
	g_list_foreach (list, add_g_list_row, recset);
	g_list_free (list);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_postgres_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaPostgresConnectionData *priv_data;
	GdaDataModel *recset;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	if (priv_data->version_float < 7.3)
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT datname "
						"FROM pg_database "
						"ORDER BY 1",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT datname "
						"FROM pg_catalog.pg_database "
						"ORDER BY 1",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_DATABASES);

	return recset;
}

static GdaDataModel *
get_postgres_users (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	
	if (priv_data->version_float < 7.3)
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT usename "
						"FROM pg_user "
						"ORDER BY usename",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		reclist = process_sql_commands (NULL, cnc, 
						"SELECT u.usename "
						"FROM pg_catalog.pg_user u"
						"ORDER BY u.usename",
						GDA_COMMAND_OPTION_STOP_ON_ERRORS);	

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_USERS);

	return recset;
}

static GdaDataModel *
get_postgres_sequences (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	
	if (priv_data->version_float < 7.3)
		reclist = process_sql_commands (
                            NULL, cnc, 
			    "SELECT relname, usename, obj_description(pg_class.oid), NULL "
			    "FROM pg_class, pg_user "
			    "WHERE usesysid=relowner AND relkind = 'S' AND relname !~ '^pg_' ORDER BY relname",
			    GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	else
		reclist = process_sql_commands (
                            NULL, cnc, 
			    "SELECT c.relname, u.usename, obj_description (c.oid), NULL "
			    "FROM pg_catalog.pg_class c, pg_catalog.pg_user u "
			    "WHERE u.usesysid = c.relowner "
			    "AND c.relkind = 'S' "
			    "AND pg_catalog.pg_table_is_visible(c.oid) "
			    "ORDER BY relname",
			    GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_SEQUENCES);

	return recset;
}

static GdaDataModel *
get_postgres_parent_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;
	gchar *query;
	const gchar *tblname;
	GdaParameter *par;
	GdaPostgresConnectionData *priv_data;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	if (priv_data->version_float < 7.3)
		query = g_strdup_printf ("SELECT a.relname, b.inhseqno "
					 "FROM pg_inherits b, pg_class a, pg_class c "
					 "WHERE a.oid=b.inhparent AND b.inhrelid = c.oid "
					 "AND c.relname = '%s' ORDER BY b.inhseqno",
					 tblname);
	else
		query = g_strdup_printf ("SELECT p.relname, h.inhseqno "
					 "FROM pg_catalog.pg_inherits h, pg_catalog.pg_class p, pg_catalog.pg_class c "
					 "WHERE pg_catalog.pg_table_is_visible(c.oid) "
					 "AND pg_catalog.pg_table_is_visible(p.oid) "
					 "AND p.oid = h.inhparent "
					 "AND h.inhrelid = c.oid "
					 "AND c.relname = '%s' "
					 "ORDER BY h.inhseqno", 
					 tblname);
		

	reclist = process_sql_commands (NULL, cnc, query,
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	g_free (query);
	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	/* Set it here instead of the SQL query to allow i18n */
	gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_PARENT_TABLES);

	return recset;
}

static GdaDataModel *
get_postgres_constraints (GdaConnection *cnc, GdaParameterList *params)
{
	GdaPostgresConnectionData *priv_data;
	GdaDataModelArray *recset;
	GdaParameter *par;
	const gchar *tblname;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	par = gda_parameter_list_find_param (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);
	
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new 
				       (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_CONSTRAINTS)));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_CONSTRAINTS);

	TO_IMPLEMENT;

	return GDA_DATA_MODEL (recset);
}


/* get_schema handler for the GdaPostgresProvider class */
static GdaDataModel *
gda_postgres_provider_get_schema (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaConnectionSchema schema,
				  GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_postgres_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_postgres_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_INDEXES :
		return get_postgres_indexes (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_postgres_fields_metadata (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES :
		return get_postgres_procedures (cnc, params);
	case GDA_CONNECTION_SCHEMA_SEQUENCES :
		return get_postgres_sequences (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_postgres_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TRIGGERS :
		return get_postgres_triggers (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_postgres_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_USERS :
		return get_postgres_users (cnc, params);
	case GDA_CONNECTION_SCHEMA_VIEWS :
		return get_postgres_views (cnc, params);
	case GDA_CONNECTION_SCHEMA_PARENT_TABLES :
		return get_postgres_parent_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS :
		return get_postgres_constraints (cnc, params);
	default:
		break;
	}

	return NULL;
}

static GdaDataHandler *
gda_postgres_provider_get_data_handler (GdaServerProvider *provider,
					GdaConnection *cnc,
					GType type,
					const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;
	
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

        if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_UINT)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_FLOAT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_SHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_USHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_CHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UCHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT, NULL);
			g_object_unref (dh);
		}
	}
        else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)){ 
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = gda_postgres_handler_bin_new (cnc);
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BLOB, NULL);
				g_object_unref (dh);
			}
		}
	}
        else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == G_TYPE_DATE) ||
		 (type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_time_new ();
			gda_handler_time_set_sql_spec   ((GdaHandlerTime *) dh, G_DATE_YEAR,
							 G_DATE_MONTH, G_DATE_DAY, '-', FALSE);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIMESTAMP, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new_with_provider (provider, cnc);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_ULONG) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_ULONG, NULL);
			g_object_unref (dh);
		}
	}
	else {
		if (dbms_type) {
			GdaPostgresConnectionData *priv_data = NULL;

			if (cnc) {
				g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
				priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
			}
			if (priv_data) {
				gint i;
				GdaPostgresTypeOid *td = NULL;
				
				for (i = 0; i < priv_data->ntypes; i++) {
					if (!strcmp (priv_data->type_data[i].name, dbms_type))
						td = &(priv_data->type_data[i]);
				}
				
				if (td) {
					dh = gda_postgres_provider_get_data_handler (provider, cnc, td->type, NULL);
					gda_server_provider_handler_declare (provider, dh, cnc, 
									     G_TYPE_INVALID, dbms_type);
				}
			}
			else {
				dh = gda_postgres_provider_get_data_handler (provider, cnc, 
									     postgres_name_to_g_type (dbms_type, NULL), 
									     NULL);
				gda_server_provider_handler_declare (provider, dh, cnc, 
								     G_TYPE_INVALID, dbms_type);
			}
		}
	}

	return dh;
}
	
static const gchar* 
gda_postgres_provider_get_default_dbms_type (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (type == G_TYPE_INT64)
		return "int8";
	if (type == G_TYPE_UINT64)
		return "int8";
	if (type == GDA_TYPE_BINARY)
		return "bytea";
	if (type == GDA_TYPE_BLOB)
		return "oid";
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
	if (type == GDA_TYPE_LIST)
		return "text";
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
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	if (type == G_TYPE_CHAR)
		return "smallint";
	if (type == G_TYPE_UCHAR)
		return "smallint";
	if (type == G_TYPE_ULONG)
		return "int8";
        if (type == G_TYPE_UINT)
		return "int4";
	if (type == G_TYPE_INVALID)
		return "text";

	return "text";
}

static gchar *
gda_postgres_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	GdaPostgresConnectionData *priv_data = NULL;
	gchar *dest;
	gint length;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		
		priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
		if (!priv_data) {
			gda_connection_add_event_string (cnc, _("Invalid PostgreSQL handle"));
			return NULL;
		}
	}

	length = strlen (str);
	dest = g_new (gchar, 2*length + 1);
	if (0 && priv_data) {
		/* FIXME: use this call but it's only defined for Postgres >= 8.1 */
		/*PQescapeStringConn (priv_data->pconnpconn, dest, str, length, NULL);*/
	}
	else
		PQescapeString (dest, str, length);

	return dest;
}

static gchar *
gda_postgres_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	return gda_default_unescape_string (str);
}

static GdaSqlParser *
gda_postgres_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
        g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
        return (GdaSqlParser*) g_object_new (GDA_TYPE_POSTGRES_PARSER, "tokenizer-flavour", 
					     GDA_SQL_PARSER_FLAVOUR_POSTGRESQL, NULL);
}
