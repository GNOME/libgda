/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2005 The GNOME Foundation
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
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-column-index.h>
#include "gda-postgres.h"
#include "gda-postgres-provider.h"

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

static gboolean gda_postgres_provider_create_database (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       const gchar *name);

static gboolean gda_postgres_provider_drop_database (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *name);

static gboolean gda_postgres_provider_create_table (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *table_name,
						    const GList *attributes_list,
						    const GList *index_list);

static gboolean gda_postgres_provider_drop_table (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *table_name);

static gboolean gda_postgres_provider_create_index (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const GdaDataModelIndex *index,
						    const gchar *table_name);

static gboolean gda_postgres_provider_drop_index (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *index_name,
						     gboolean primary_key,
						     const gchar *table_name);

static GList *gda_postgres_provider_execute_command (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaCommand *cmd,
						     GdaParameterList *params);

static gchar *gda_postgres_provider_get_last_insert_id (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaDataModel *recset);

static gboolean gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 GdaTransaction *xaction);

static gboolean gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  GdaTransaction *xaction);

static gboolean gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
							    GdaConnection *cnc,
							    GdaTransaction *xaction);

static gboolean gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
						      GdaConnection *cnc,
						      const gchar *command);

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaConnectionFeature feature);

static GdaDataModel *gda_postgres_provider_get_schema (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaConnectionSchema schema,
						       GdaParameterList *params);

static gboolean gda_postgres_provider_create_blob (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaBlob *blob);

static gboolean gda_postgres_provider_escape_string (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *from,
						     gchar *to);
				 		 
typedef struct {
	gchar *col_name;
	GdaValueType data_type;
} GdaPostgresColData;

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
	provider_class->open_connection = gda_postgres_provider_open_connection;
	provider_class->close_connection = gda_postgres_provider_close_connection;
	provider_class->get_server_version = gda_postgres_provider_get_server_version;
	provider_class->get_database = gda_postgres_provider_get_database;
	provider_class->create_database = gda_postgres_provider_create_database;
	provider_class->drop_database = gda_postgres_provider_drop_database;
	provider_class->create_table = gda_postgres_provider_create_table;
	provider_class->drop_table = gda_postgres_provider_drop_table;
	provider_class->create_index = gda_postgres_provider_create_index;
	provider_class->drop_index = gda_postgres_provider_drop_index;
	provider_class->execute_command = gda_postgres_provider_execute_command;
	provider_class->get_last_insert_id = gda_postgres_provider_get_last_insert_id;
	provider_class->begin_transaction = gda_postgres_provider_begin_transaction;
	provider_class->commit_transaction = gda_postgres_provider_commit_transaction;
	provider_class->rollback_transaction = gda_postgres_provider_rollback_transaction;
	provider_class->supports = gda_postgres_provider_supports;
	provider_class->get_schema = gda_postgres_provider_get_schema;
	provider_class->create_blob = gda_postgres_provider_create_blob;
	provider_class->escape_string = gda_postgres_provider_escape_string;
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

	if (!type) {
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

static GdaValueType
postgres_name_to_gda_type (const gchar *name)
{
	if (!strcmp (name, "bool"))
		return GDA_VALUE_TYPE_BOOLEAN;
	else if (!strcmp (name, "int8"))
		return GDA_VALUE_TYPE_BIGINT;
	else if (!strcmp (name, "int4") || !strcmp (name, "abstime") || !strcmp (name, "oid"))
		return GDA_VALUE_TYPE_INTEGER;
	else if (!strcmp (name, "int2"))
		return GDA_VALUE_TYPE_SMALLINT;
	else if (!strcmp (name, "float4"))
		return GDA_VALUE_TYPE_SINGLE;
	else if (!strcmp (name, "float8"))
		return GDA_VALUE_TYPE_DOUBLE;
	else if (!strcmp (name, "numeric"))
		return GDA_VALUE_TYPE_NUMERIC;
	else if (!strncmp (name, "timestamp", 9))
		return GDA_VALUE_TYPE_TIMESTAMP;
	else if (!strcmp (name, "date"))
		return GDA_VALUE_TYPE_DATE;
	else if (!strncmp (name, "time", 4))
		return GDA_VALUE_TYPE_TIME;
	else if (!strcmp (name, "point"))
		return GDA_VALUE_TYPE_GEOMETRIC_POINT;
	else if (!strcmp (name, "oid"))
		return GDA_VALUE_TYPE_BLOB;
	else if (!strcmp (name, "bytea"))
		return GDA_VALUE_TYPE_BINARY;

	return GDA_VALUE_TYPE_STRING;
}

static gchar *
postgres_name_from_gda_type (const GdaValueType type)
{
	switch (type) {
	case GDA_VALUE_TYPE_NULL :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_BIGINT :
		return g_strdup_printf ("int8");
	case GDA_VALUE_TYPE_BIGUINT :
		return g_strdup_printf ("int8");
	case GDA_VALUE_TYPE_BINARY :
		return g_strdup_printf ("bytea");
	case GDA_VALUE_TYPE_BLOB :
		return g_strdup_printf ("oid");
	case GDA_VALUE_TYPE_BOOLEAN :
		return g_strdup_printf ("bool");
	case GDA_VALUE_TYPE_DATE :
		return g_strdup_printf ("date");
	case GDA_VALUE_TYPE_DOUBLE :
		return g_strdup_printf ("float8");
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		return g_strdup_printf ("point");
	case GDA_VALUE_TYPE_GOBJECT :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_INTEGER :
		return g_strdup_printf ("int4");
	case GDA_VALUE_TYPE_LIST :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_MONEY :
		return g_strdup_printf ("money");
	case GDA_VALUE_TYPE_NUMERIC :
		return g_strdup_printf ("numeric");
	case GDA_VALUE_TYPE_SINGLE :
		return g_strdup_printf ("float4");
	case GDA_VALUE_TYPE_SMALLINT :
		return g_strdup_printf ("int2");
	case GDA_VALUE_TYPE_SMALLUINT :
		return g_strdup_printf ("int2");
	case GDA_VALUE_TYPE_STRING :
		return g_strdup_printf ("varchar");
	case GDA_VALUE_TYPE_TIME :
		return g_strdup_printf ("time");
	case GDA_VALUE_TYPE_TIMESTAMP :
		return g_strdup_printf ("timestamp");
	case GDA_VALUE_TYPE_TINYINT :
		return g_strdup_printf ("smallint");
	case GDA_VALUE_TYPE_TINYUINT :
		return g_strdup_printf ("smallint");
	case GDA_VALUE_TYPE_TYPE :
		return g_strdup_printf ("int2");
        case GDA_VALUE_TYPE_UINTEGER :
		return g_strdup_printf ("int4");
	case GDA_VALUE_TYPE_UNKNOWN :
		return g_strdup_printf ("text");
	default :
		return g_strdup_printf ("text");
	}

	return g_strdup_printf ("text");
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
		pg_res = PQexec (priv_td->pconn, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT pg_type.oid FROM pg_type WHERE typname in (%s)", avoid_types);
		pg_res_avoid = PQexec (priv_td->pconn, query);
		g_free (query);
	}
	else {
		gchar *query;
		avoid_types = "'any', 'anyarray', 'anyelement', 'cid', 'cstring', 'int2vector', 'internal', 'language_handler', 'oid', 'oidvector', 'opaque', 'record', 'refcursor', 'regclass', 'regoper', 'regoperator', 'regproc', 'regprocedure', 'regtype', 'SET', 'smgr', 'tid', 'trigger', 'unknown', 'void', 'xid'";

		/* main query to fetch infos about the data types */
		query = g_strdup_printf (
                          "SELECT t.oid, t.typname, u.usename, pg_catalog.obj_description(t.oid) "
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
		pg_res = PQexec (priv_td->pconn, query);
		g_free (query);

		/* query to fetch non returned data types */
		query = g_strdup_printf ("SELECT t.oid FROM pg_catalog.pg_type t WHERE t.typname in (%s)",
					 avoid_types);
		pg_res_avoid = PQexec (priv_td->pconn, query);
		g_free (query);

		/* query to fetch the oid of the 'any' data type */
		pg_res_anyoid = PQexec (priv_td->pconn, 
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
		td[i].name = g_strdup (PQgetvalue (pg_res, i, 1));
		td[i].oid = atoi (PQgetvalue (pg_res, i, 0));
		td[i].type = postgres_name_to_gda_type (td[i].name);
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

/* get the float version og a Postgres version which looks like:
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
	pq_db = gda_quark_list_find (params, "DATABASE");
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

	pq_requiressl = gda_quark_list_find (params, "REQUIRESSL");

	conn_string = g_strconcat ("",
				   pq_host ? "host=" : "",
				   pq_host ? pq_host : "",
				   pq_hostaddr ? " hostaddr=" : "",
				   pq_hostaddr ? pq_hostaddr : "",
				   pq_db ? " dbname=" : "",
				   pq_db ? pq_db : "",
				   pq_port ? " port=" : "",
				   pq_port ? pq_port : "",
				   pq_options ? " options='" : "",
				   pq_options ? pq_options : "",
				   pq_options ? "'" : "",
				   pq_tty ? " tty=" : "",
				   pq_tty ? pq_tty : "",
				   (pq_user && *pq_user) ? " user=" : "",
				   (pq_user && *pq_user)? pq_user : "",
				   (pq_pwd && *pq_pwd) ? " password=" : "",
				   (pq_pwd && *pq_pwd) ? pq_pwd : "",
				   pq_requiressl ? " requiressl=" : "",
				   pq_requiressl ? pq_requiressl : "",
				   NULL);

	/* actually establish the connection */
	pconn = PQconnectdb (conn_string);
	g_free(conn_string);

	if (PQstatus (pconn) != CONNECTION_OK) {
		gda_connection_add_error (cnc, gda_postgres_make_error (pconn, NULL));
		PQfinish(pconn);
		return FALSE;
	}

	/*
	 * Sets the DATE format for all the current session to YYYY-MM-DD
	 */
	pg_res = PQexec (pconn, "SET DATESTYLE TO 'ISO'");
	PQclear (pg_res);

	/*
	 * Unicode is the default character set now
	 */
	pg_res = PQexec (pconn, "SET CLIENT_ENCODING TO 'UNICODE'");
	PQclear (pg_res);

	/*
	 * Get the vesrion as a float
	 */
	pg_res = PQexec (pconn, "SELECT version ()");
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
			pg_res = PQexec (pconn, query);
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
	priv_data->pconn = pconn;
	priv_data->version = version;
	priv_data->version_float = version_float;
	if (get_connection_type_list (priv_data) != 0) {
		gda_connection_add_error (cnc, gda_postgres_make_error (pconn, NULL));
		PQfinish(pconn);
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
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return priv_data->version;
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
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	pconn = priv_data->pconn;
	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			PGresult *pg_res;
			GdaDataModel *recset;
			gint status;
			gint i;

			pg_res = PQexec(pconn, arr[n]);
			if (pg_res == NULL) {
				gda_connection_add_error (cnc, gda_postgres_make_error (pconn, NULL));
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
				reclist = NULL;
				break;
			} 
			
			status = PQresultStatus(pg_res);
			if (options & GDA_COMMAND_OPTION_IGNORE_ERRORS	||
			    status == PGRES_EMPTY_QUERY			||
			    status == PGRES_TUPLES_OK 			||
			    status == PGRES_COMMAND_OK) {
				recset = gda_postgres_recordset_new (cnc, pg_res);
				if (GDA_IS_DATA_MODEL (recset)) {
					gda_data_model_set_command_text (recset, arr[n]);
					gda_data_model_set_command_type (recset, GDA_COMMAND_TYPE_SQL);
					for (i = PQnfields (pg_res) - 1; i >= 0; i--)
						gda_data_model_set_column_title (recset, i, 
									PQfname (pg_res, i));

					reclist = g_list_append (reclist, recset);
				}
			}
			else {
				gda_connection_add_error (cnc, gda_postgres_make_error (pconn, pg_res));
				g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
				g_list_free (reclist);
				reclist = NULL;
				break;
			}

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
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return (const char *) PQdb ((const PGconn *) priv_data->pconn);
}

/* create_database handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_create_database (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       const gchar *name)
{
	gboolean retval;
	gchar *sql;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	sql = g_strdup_printf ("CREATE DATABASE %s", name);
	retval = gda_postgres_provider_single_command (pg_prv, cnc, sql);
	g_free (sql);

	return retval;
}

/* drop_database handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_drop_database (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name)
{
	gboolean retval;
	gchar *sql;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	sql = g_strdup_printf ("DROP DATABASE %s", name);
	retval = gda_postgres_provider_single_command (pg_prv, cnc, sql);
	g_free (sql);

	return retval;
}

/* create_table handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_create_table (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *table_name,
				    const GList *attributes_list,
				    const GList *index_list)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaColumn *dmca;
	GdaDataModelIndex *dmi;
	GString *sql;
	gint i;
	gchar *postgres_data_type, *default_value, *references;
	GdaValueType value_type;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (attributes_list != NULL, FALSE);

	/* prepare the SQL command */
	sql = g_string_new ("CREATE TABLE ");
	g_string_append_printf (sql, "%s (", table_name);

	/* step through list */
	for (i=0; i<g_list_length ((GList *) attributes_list); i++) {

		if (i>0)
			g_string_append_printf (sql, ", ");
			
		dmca = (GdaColumn *) g_list_nth_data ((GList *) attributes_list, i);

		/* name */	
		g_string_append_printf (sql, "\"%s\" ", gda_column_get_name (dmca));

		/* data type; force serial to be used when auto_increment is set */
		if (gda_column_get_auto_increment (dmca) == TRUE) {
			g_string_append_printf (sql, "serial");
			value_type = GDA_VALUE_TYPE_INTEGER;
		} else {
			value_type = gda_column_get_gdatype (dmca);
			postgres_data_type = postgres_name_from_gda_type (value_type);
			g_string_append_printf (sql, "%s", postgres_data_type);
			g_free(postgres_data_type);
		}

		/* size */
		if (value_type == GDA_VALUE_TYPE_STRING)
			g_string_append_printf (sql, "(%ld)", gda_column_get_defined_size (dmca));
		else if (value_type == GDA_VALUE_TYPE_NUMERIC)
			g_string_append_printf (sql, "(%ld,%ld)", 
				gda_column_get_defined_size (dmca),
				gda_column_get_scale (dmca));
		
		/* NULL */
		if (gda_column_get_allow_null (dmca) == TRUE)
			g_string_append_printf (sql, " NULL");
		else
			g_string_append_printf (sql, " NOT NULL");
	
		/* (primary) key */
		if (gda_column_get_primary_key (dmca) == TRUE)
			g_string_append_printf (sql, " PRIMARY KEY");
		else
			if (gda_column_get_unique_key (dmca) == TRUE)
				g_string_append_printf (sql, " UNIQUE");

		/* default value (in case of string, user needs to add "'" around the field) */
		if (gda_column_get_default_value (dmca) != NULL) {
			default_value = gda_value_stringify ((GdaValue *) gda_column_get_default_value (dmca));
			if ((default_value != NULL) && (*default_value != '\0'))
				g_string_append_printf (sql, " DEFAULT %s", default_value);
		}

		/* any additional parameters */			
		if (gda_column_get_references (dmca) != NULL) {
			references = (gchar *) gda_column_get_references (dmca);
			if ((references != NULL) && (*references != '\0'))
				g_string_append_printf (sql, " %s", references);
		}
	}

	/* finish the SQL command */
	g_string_append_printf (sql, ")");

	/* execute sql command */
	retval = gda_postgres_provider_single_command (pg_prv, cnc, sql->str);

	/* clean up */
	g_string_free (sql, TRUE);

	/* Create indexes */
	if (index_list != NULL) {

		/* step through index_list */
		for (i=0; i<g_list_length ((GList *) index_list); i++) {

			/* get index */
			dmi = (GdaDataModelIndex *) g_list_nth_data ((GList *) index_list, i);

			/* create index */
			retval = gda_postgres_provider_create_index (provider, cnc, dmi, table_name);
			if (retval == FALSE)
				return FALSE;
		}
	}

	/* return */
	return retval;
}

/* drop_table handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_drop_table (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *table_name)
{
	gboolean retval;
	gchar *sql;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	sql = g_strdup_printf ("DROP TABLE %s", table_name);

	retval = gda_postgres_provider_single_command (pg_prv, cnc, sql);
	g_free (sql);

	return retval;
}

/* create_index handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_create_index (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const GdaDataModelIndex *index,
				    const gchar *table_name)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaColumnIndex *dmcia;
	GString *sql;
	GList *col_list;
	gchar *index_name, *col_ref, *idx_ref;
	gint i;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (index != NULL, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	/* the 'CREATE INDEX' command in PostgreSQL is limited to
	secondary non-primary indices only. Therefore use 'ALTER TABLE'
	for PRIMARY and UNIQUE */

	/* prepare the SQL command */
	sql = g_string_new (" ");

	/* get index name */
	index_name = (gchar *) gda_data_model_index_get_name ((GdaDataModelIndex *) index);

	/* determine type of index (PRIMARY, UNIQUE or INDEX) */
	if (gda_data_model_index_get_primary_key ((GdaDataModelIndex *) index) == TRUE)
		g_string_append_printf (sql, "ALTER TABLE %s ADD CONSTRAINT %s PRIMARY KEY (", table_name, index_name);
	else if (gda_data_model_index_get_unique_key ((GdaDataModelIndex *) index) == TRUE)
		g_string_append_printf (sql, "ALTER TABLE %s ADD CONSTRAINT %s UNIQUE (", table_name, index_name);
	else
		g_string_append_printf (sql, "CREATE INDEX %s ON %s (", index_name, table_name);

	/* get the list of index columns */
	col_list = gda_data_model_index_get_column_index_list ((GdaDataModelIndex *) index);

	/* step through list */
	for (i = 0; i < g_list_length ((GList *) col_list); i++) {

		if (i > 0)
			g_string_append_printf (sql, ", ");
			
		dmcia = (GdaColumnIndex *) g_list_nth_data ((GList *) col_list, i);

		/* name */	
		g_string_append_printf (sql, "\"%s\" ", gda_column_index_get_column_name (dmcia));

		/* any additional column parameters */
		if (gda_column_index_get_references (dmcia) != NULL) {
			col_ref = (gchar *) gda_column_index_get_references (dmcia);
			if ((col_ref != NULL) && (*col_ref != '\0'))
				g_string_append_printf (sql, " %s ", col_ref);
		}
	}

	/* finish the SQL column(s) section command */
	g_string_append_printf (sql, ")");

	/* any additional index parameters */
	if (gda_data_model_index_get_references ((GdaDataModelIndex *) index) != NULL) {
		idx_ref = (gchar *) gda_data_model_index_get_references ((GdaDataModelIndex *) index);
		if ((idx_ref != NULL) && (*idx_ref != '\0'))
			g_string_append_printf (sql, " %s ", idx_ref);
	}

	/* execute sql command */
	retval = gda_postgres_provider_single_command (pg_prv, cnc, sql->str);

	/* clean up */
	g_string_free (sql, TRUE);

	return retval;
}

/* drop_index handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_drop_index (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *index_name,
				  gboolean primary_key,
				  const gchar *table_name)
{
	gboolean retval = FALSE, retval_temp;
	gchar *sql_alter;
	gchar *sql_drop;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (index_name != NULL, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	/* In PostgreSQL PRIMARY can only and UNIQUE can as well be defined as
	   constraints. Therefore use 2 methods for removal */
	sql_alter = g_strdup_printf ("ALTER TABLE %s DROP CONSTRAINT %s", table_name, index_name);
	sql_drop  = g_strdup_printf ("DROP INDEX %s", index_name);

	retval_temp = gda_postgres_provider_single_command (pg_prv, cnc, sql_alter);
	if (retval_temp == TRUE)
		retval = TRUE;

	retval_temp = gda_postgres_provider_single_command (pg_prv, cnc, sql_drop);
	if (retval_temp == TRUE)
		retval = TRUE;

	/* clean up */
	g_free (sql_alter);
	g_free (sql_drop);

	return retval;
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

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

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

	return reclist;
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
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* get the PQresult from the recordset */
	pgres = gda_postgres_recordset_get_pgresult (GDA_POSTGRES_RECORDSET (recset));
	if (pgres) {
		oid = PQoidValue (pgres);
		if (oid != InvalidOid)
			return g_strdup_printf ("%d", oid);
	}

	return NULL;
}

static gboolean 
gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
				         GdaConnection *cnc,
					 GdaTransaction *xaction)
{
        gboolean result;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;

	gchar * write_option=NULL;
	gchar * isolation_level=NULL;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float >= 6.5){
	        if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
	                if (priv_data->version_float >= 7.4){
		                write_option = "READ ONLY";
           	        } 
                        else {
		                gda_connection_add_error_string (cnc, _("Transactions are not supported in read-only mode"));
				return FALSE;
			}
		}
		switch (gda_transaction_get_isolation_level (xaction)) {
		case GDA_TRANSACTION_ISOLATION_READ_COMMITTED :
		        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL READ COMMITTED ", write_option, NULL);
		        break;
		case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED :
		        gda_connection_add_error_string (cnc, _("Transactions are not supported in read uncommitted isolation level"));
		        return FALSE;
		case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ :
		        gda_connection_add_error_string (cnc, _("Transactions are not supported in repeatable read isolation level"));
		        return FALSE;
		case GDA_TRANSACTION_ISOLATION_SERIALIZABLE :
		        isolation_level = g_strconcat ("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE ", write_option, NULL);
		        break;
		default: 
		        isolation_level = NULL;
		}
	}

	result = gda_postgres_provider_single_command (pg_prv, cnc, "BEGIN"); 
	if (result&&isolation_level != NULL) {
	        result=gda_postgres_provider_single_command (pg_prv, cnc, isolation_level) ;
	} 
	g_free(isolation_level);

	return result;
}

static gboolean 
gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "COMMIT"); 
}

static gboolean 
gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    GdaTransaction *xaction)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "ROLLBACK"); 
}

/* Really does the BEGIN/ROLLBACK/COMMIT */
static gboolean 
gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
				      GdaConnection *cnc,
				      const gchar *command)
{
	GdaPostgresConnectionData *priv_data;
	PGconn *pconn;
	PGresult *pg_res;
	gboolean result;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return FALSE;
	}

	pconn = priv_data->pconn;
	result = FALSE;
	pg_res = PQexec (pconn, command);
	if (pg_res != NULL){
		result = PQresultStatus (pg_res) == PGRES_COMMAND_OK;
		PQclear (pg_res);
	}

	if (result == FALSE)
		gda_connection_add_error (cnc, gda_postgres_make_error (pconn, pg_res));

	return result;
}

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaConnection *cnc,
						GdaConnectionFeature feature)
{
	GdaPostgresProvider *pgprv = (GdaPostgresProvider *) provider;
	GdaPostgresConnectionData *priv_data;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pgprv), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_AGGREGATES :
	case GDA_CONNECTION_FEATURE_INDEXES :
	case GDA_CONNECTION_FEATURE_INHERITANCE :
	case GDA_CONNECTION_FEATURE_PROCEDURES :
	case GDA_CONNECTION_FEATURE_SEQUENCES :
	case GDA_CONNECTION_FEATURE_SQL :
	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
	case GDA_CONNECTION_FEATURE_TRIGGERS :
	case GDA_CONNECTION_FEATURE_USERS :
	case GDA_CONNECTION_FEATURE_VIEWS :
	case GDA_CONNECTION_FEATURE_BLOBS :
		return TRUE;
	case GDA_CONNECTION_FEATURE_NAMESPACES :
		priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
		if (priv_data->version_float >= 7.3)
			return TRUE;
	default :
		break;
	}
 
	return FALSE;
}


static GdaDataModelArray *
gda_postgres_init_procs_recset (GdaConnection *cnc)
{
	GdaDataModelArray *recset;
	gint i;
	GdaPostgresColData cols[8] = {
		{ N_("Procedure")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Id")              , GDA_VALUE_TYPE_STRING  },
		{ N_("Owner")		, GDA_VALUE_TYPE_STRING  },
		{ N_("Comments")       	, GDA_VALUE_TYPE_STRING  },
		{ N_("Return type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Nb args")	        , GDA_VALUE_TYPE_INTEGER },
		{ N_("Args types")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Definition")	, GDA_VALUE_TYPE_STRING  }
		};

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols / sizeof cols[0]));
	for (i = 0; i < sizeof cols / sizeof cols[0]; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols[i].col_name));

	return recset;
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

	pg_res = PQexec(priv_data->pconn, query);
	g_free (query);
	if (pg_res == NULL)
		return NULL;

	row_count = PQntuples (pg_res);
	for (i = 0; i < row_count; i++) {
		GdaValue *value;
		gchar *thevalue, *procname, *instr, *ptr;
		GString *gstr;
		gint nbargs, argcounter;
		GList *rowlist = NULL;
		gboolean insert = TRUE;

		/* Proc name */
		procname = thevalue = PQgetvalue(pg_res, i, 1);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Proc_Id */
		thevalue = PQgetvalue (pg_res, i, 0);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Owner */
		thevalue = PQgetvalue(pg_res, i, 2);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Comments */ 
		thevalue = PQgetvalue(pg_res, i, 3);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Out type */ 
		thevalue = PQgetvalue(pg_res, i, 4);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Number of args */
		thevalue = PQgetvalue(pg_res, i, 5);
		nbargs = atoi (thevalue);
		value = gda_value_new_integer (nbargs);
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
						g_string_append_printf (gstr, " %s", thevalue);
				}
					
				ptr = strtok (NULL, " ");
				argcounter ++;
			}
			g_free (instr);
		}
		else 
			insert = FALSE;

		if (gstr) {
			value = gda_value_new_string (gstr->str);
			g_string_free (gstr, TRUE);
		}
		else
			value = gda_value_new_string (NULL);
		rowlist = g_list_append (rowlist, value);

		
		if (argcounter != nbargs)
			insert = FALSE;
		
		
		
		/* Definition */
		thevalue = PQgetvalue(pg_res, i, 7);
		if (!strcmp (thevalue, procname))
			value = gda_value_new_string (NULL);
		else
			value = gda_value_new_string (thevalue);
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

	recset = gda_postgres_init_procs_recset (cnc);
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
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params)
		par = gda_parameter_list_find (params, "namespace");

	if (par)
		namespace = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));

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
			gchar *query;

			query = g_strdup_printf ("SELECT c.relname, u.usename, pg_catalog.obj_description(c.oid), NULL "
						 "FROM pg_catalog.pg_class c, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
						 "WHERE u.usesysid=c.relowner AND c.relkind = 'r' "
						 "AND c.relnamespace=n.oid "
						 "AND n.nspname ='%s' "
						 "AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
						 "ORDER BY relname", namespace);
			reclist = process_sql_commands (NULL, cnc,
							query,
							GDA_COMMAND_OPTION_STOP_ON_ERRORS);
			g_free (query);
		}
		else
			reclist = process_sql_commands (
				    NULL, cnc,
				    "SELECT c.relname, u.usename, pg_catalog.obj_description(c.oid), NULL "
				    "FROM pg_catalog.pg_class c, pg_catalog.pg_user u, pg_catalog.pg_namespace n "
				    "WHERE u.usesysid=c.relowner AND c.relkind = 'r' "
				    "AND c.relnamespace=n.oid AND pg_catalog.pg_table_is_visible (c.oid) "
				    "AND n.nspname NOT IN ('pg_catalog', 'pg_toast') "
				    "ORDER BY relname",
				    GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	}

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	/* Set it here instead of the SQL query to allow i18n */
	gda_data_model_set_column_title (recset, 0, _("Table"));
	gda_data_model_set_column_title (recset, 1, _("Owner"));
	gda_data_model_set_column_title (recset, 2, _("Description"));
	gda_data_model_set_column_title (recset, 3, _("Definition"));

	return recset;
}

static GdaDataModel *
get_postgres_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaPostgresConnectionData *priv_data;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (4));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	for (i = 0; i < priv_data->ntypes; i++) {
		GList *value_list = NULL;

		value_list = g_list_append (value_list, gda_value_new_string (priv_data->type_data[i].name));
		value_list = g_list_append (value_list, gda_value_new_string (priv_data->type_data[i].owner));
		value_list = g_list_append (value_list, gda_value_new_string (priv_data->type_data[i].comments));
		value_list = g_list_append (value_list, gda_value_new_type (priv_data->type_data[i].type));

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list);

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
	gda_data_model_set_column_title (recset, 0, _("View"));
	gda_data_model_set_column_title (recset, 1, _("Owner"));
	gda_data_model_set_column_title (recset, 2, _("Comments"));
	gda_data_model_set_column_title (recset, 3, _("Definition"));

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
	gda_data_model_set_column_title (recset, 0, _("Indexes"));

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
		par = gda_parameter_list_find (params, "namespace");

	if (par)
		namespace = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (priv_data->version_float < 7.3) {
		reclist = process_sql_commands (
			   NULL, cnc, 
			   "(SELECT a.aggname, a.oid, usename, obj_description (a.oid), t2.typname, t1.typname, NULL "
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
                                  "SELECT p.proname, p.oid, u.usename, pg_catalog.obj_description(p.oid),"
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
	gda_data_model_set_column_title (recset, 0, _("Aggregate"));
	gda_data_model_set_column_title (recset, 1, _("Id"));
	gda_data_model_set_column_title (recset, 2, _("Owner"));
	gda_data_model_set_column_title (recset, 3, _("Comments"));
	gda_data_model_set_column_title (recset, 4, _("OutType"));
	gda_data_model_set_column_title (recset, 5, _("InType"));
	gda_data_model_set_column_title (recset, 6, _("Definition"));

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
	gda_data_model_set_column_title (recset, 0, _("Triggers"));

	return recset;
}

static GdaDataModelArray *
gda_postgres_init_md_recset (GdaConnection *cnc)
{
	GdaDataModelArray *recset;
	gint i;
	GdaPostgresColData cols[9] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Default value")   , GDA_VALUE_TYPE_STRING  }
		};

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols / sizeof cols[0]));
	for (i = 0; i < sizeof cols / sizeof cols[0]; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols[i].col_name));

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
					 "FROM pg_catalog.pg_class c, pg_catalog.pg_class c2, pg_catalog.pg_index i "
					 "WHERE c.relname = '%s' "
					 "AND c.oid = i.indrelid "
					 "AND i.indexrelid = c2.oid "
					 "AND pg_catalog.pg_table_is_visible(c.oid)", tblname);

	pg_idx = PQexec(priv_data->pconn, query);
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

		arr = g_strsplit (PQgetvalue (pg_idx, i, 0), " ", 0);
		if (arr == NULL || arr[0][0] == '\0') {
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

#ifdef LIBGDA_WIN32
/* this is a ming thing.. */
#define strtok_r(a,b,c) strtok(a,b)

#endif

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

	pg_ref = PQexec(priv_data->pconn, query);
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
				pg_res = PQexec(priv_data->pconn, query);
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
			"SELECT a.attname, t.typname, a.atttypmod, t.typlen, a.attnotnull, d.adsrc, a.attnum "
			"FROM pg_catalog.pg_class c "
			"LEFT JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid) "
			"FULL JOIN pg_catalog.pg_attrdef d ON (a.attnum = d.adnum AND d.adrelid=c.oid) "
			"LEFT JOIN pg_catalog.pg_type t ON (t.oid = a.atttypid) "
			"WHERE c.relname = '%s' AND pg_catalog.pg_table_is_visible (c.oid) AND a.attnum > 0 "
			"AND NOT a.attisdropped ORDER BY 7", tblname);

	pg_res = PQexec(priv_data->pconn, query);
	g_free (query);
	if (pg_res == NULL)
		return NULL;

	idx_list = gda_postgres_get_idx_data (priv_data, tblname);
	ref_list = gda_postgres_get_ref_data (priv_data, tblname);

	row_count = PQntuples (pg_res);
	for (i = 0; i < row_count; i++) {
		GdaValue *value;
		gchar *thevalue, *colname, *ref = NULL;
		GdaValueType type;
		gint32 integer;
		GList *rowlist = NULL;
		GList *rlist;

		/* Field name */
		colname = thevalue = PQgetvalue (pg_res, i, 0);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Data type */
		thevalue = PQgetvalue(pg_res, i, 1);
		type = gda_postgres_type_name_to_gda (priv_data->h_table, 
						      thevalue);
		value = gda_value_new_string (thevalue);
		rowlist = g_list_append (rowlist, value);

		/* Defined size */
		thevalue = PQgetvalue(pg_res, i, 3);
		integer = atoi (thevalue);
		if (integer == -1 && type == GDA_VALUE_TYPE_STRING) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = atoi (thevalue) - 4; /* don't know where the -4 comes from! */
		}
		if (integer == -1 && type == GDA_VALUE_TYPE_NUMERIC) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = atoi(thevalue) / 65536;
		}
			
		value = gda_value_new_integer ((integer != -1) ? integer : 0);
		rowlist = g_list_append (rowlist, value);

		/* Scale */ 
		integer = 0;
		if (type == GDA_VALUE_TYPE_NUMERIC) {
			thevalue = PQgetvalue(pg_res, i, 2);
			integer = (atoi(thevalue) % 65536) - 4;
		}
		value = gda_value_new_integer (integer);
		rowlist = g_list_append (rowlist, value);

		/* Not null? */
		thevalue = PQgetvalue(pg_res, i, 4);
		value = gda_value_new_boolean ((*thevalue == 't' ? TRUE : FALSE));
		rowlist = g_list_append (rowlist, value);

		/* Primary key? */
		value = gda_value_new_boolean (
				gda_postgres_index_type (i, idx_list, IDX_PRIMARY));
		rowlist = g_list_append (rowlist, value);

		/* Unique index? */
		value = gda_value_new_boolean (
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
		value = gda_value_new_string (ref);
		rowlist = g_list_append (rowlist, value);

		/* Default value */
		if (!PQgetisnull (pg_res, i, 5)) {
			thevalue = PQgetvalue (pg_res, i, 5);
			value = gda_value_new_string (thevalue);
		}
		else
			value = gda_value_new_null ();
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

	gda_data_model_append_values (GDA_DATA_MODEL (recset), rowlist);
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

	par = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	g_return_val_if_fail (tblname != NULL, NULL);
	
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	recset = gda_postgres_init_md_recset (cnc);
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
	gda_data_model_set_column_title (recset, 0, _("Databases"));

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
	gda_data_model_set_column_title (recset, 0, _("Users"));

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
	gda_data_model_set_column_title (recset, 0, _("Sequence"));
	gda_data_model_set_column_title (recset, 1, _("Owner"));
	gda_data_model_set_column_title (recset, 2, _("Comments"));
        gda_data_model_set_column_title (recset, 3, _("Definition"));

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

	par = gda_parameter_list_find (params, "name");
	g_return_val_if_fail (par != NULL, NULL);

	tblname = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
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
	gda_data_model_set_column_title (recset, 0, _("Table"));
	gda_data_model_set_column_title (recset, 1, _("Sequence"));

	return recset;
}

/* get_schema handler for the GdaPostgresProvider class */
static GdaDataModel *
gda_postgres_provider_get_schema (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaConnectionSchema schema,
				  GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

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
	default:
		break;
	}

	return NULL;
}

static gboolean gda_postgres_provider_create_blob (GdaServerProvider *provider,
						   GdaConnection *cnc,
						   GdaBlob *blob)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_blob_create (blob, cnc);
}

gboolean
gda_postgres_provider_escape_string (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *from,
				     gchar *to)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (to != NULL, FALSE);

	return (unsigned long)  PQescapeString (to, from, (size_t) strlen (from));
}
