/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <libgda/gda-data-model-array.h>
#include "gda-postgres.h"
#include "gda-postgres-provider.h"

static void gda_postgres_provider_class_init (GdaPostgresProviderClass *klass);
static void gda_postgres_provider_init       (GdaPostgresProvider *provider,
					      GdaPostgresProviderClass *klass);
static void gda_postgres_provider_finalize   (GObject *object);

static gboolean gda_postgres_provider_open_connection (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaQuarkList *params,
						       const gchar *username,
						       const gchar *password);

static gboolean gda_postgres_provider_close_connection (GdaServerProvider *provider,
							GdaConnection *cnc);

static gboolean gda_postgres_provider_create_database (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       const gchar *name);
static gboolean gda_postgres_provider_drop_database (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     const gchar *name);

static GList *gda_postgres_provider_execute_command (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaCommand *cmd,
						     GdaParameterList *params);

static gboolean gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 const gchar *trans_id);

static gboolean gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  const gchar *trans_id);

static gboolean gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
							    GdaConnection *cnc,
							    const gchar *trans_id);

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
	gchar *colname;
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
	provider_class->open_connection = gda_postgres_provider_open_connection;
	provider_class->close_connection = gda_postgres_provider_close_connection;
	provider_class->create_database = gda_postgres_provider_create_database;
	provider_class->drop_database = gda_postgres_provider_drop_database;
	provider_class->execute_command = gda_postgres_provider_execute_command;
	provider_class->begin_transaction = gda_postgres_provider_begin_transaction;
	provider_class->commit_transaction = gda_postgres_provider_commit_transaction;
	provider_class->rollback_transaction = gda_postgres_provider_rollback_transaction;
	provider_class->supports = gda_postgres_provider_supports;
	provider_class->get_schema = gda_postgres_provider_get_schema;
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
	/*TODO: by now, this one not supported
	if (!strncmp (name, "bit", 3))
		return GDA_VALUE_TYPE_BINARY;
	*/

	return GDA_VALUE_TYPE_STRING;
}

static int
get_connection_type_list (GdaPostgresConnectionData *priv_td)
{
	GHashTable *h_table;
	GdaPostgresTypeOid *td;
	PGresult *pg_res;
	gint nrows, i;

	pg_res = PQexec (priv_td->pconn,
			 "SELECT oid, typname FROM pg_type "
			 "WHERE typrelid = 0 AND typname !~ '^_' "
			 " AND  typname not in ('SET', 'cid', "
			 "'int2vector', 'oidvector', 'regproc', "
			 "'smgr', 'tid', 'unknown', 'xid') "
			 "ORDER BY typname");

	if (pg_res == NULL || PQresultStatus (pg_res) != PGRES_TUPLES_OK) {
		if (pg_res) PQclear (pg_res);
		return -1;
	}

	nrows = PQntuples (pg_res);
	td = g_new (GdaPostgresTypeOid, nrows);
	h_table = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < nrows; i++) {
		td[i].name = g_strdup (PQgetvalue (pg_res, i, 1));
		td[i].oid = atoi (PQgetvalue (pg_res, i, 0));
		td[i].type = postgres_name_to_gda_type (td[i].name);
		g_hash_table_insert (h_table, td[i].name, &td[i].type);
	}

	PQclear (pg_res);
	priv_td->ntypes = nrows;
	priv_td->type_data = td;
	priv_td->h_table = h_table;

	return 0;
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

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* parse connection string */
	pq_host = gda_quark_list_find (params, "HOST");
	pq_hostaddr = gda_quark_list_find (params, "HOSTADDR");
	pq_db = gda_quark_list_find (params, "DATABASE");
	pq_port = gda_quark_list_find (params, "PORT");
	pq_options = gda_quark_list_find (params, "OPTIONS");
	pq_tty = gda_quark_list_find (params, "TTY");
	pq_user = gda_quark_list_find (params, "USER");
	pq_pwd = gda_quark_list_find (params, "PASSWORD");
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
				   pq_user ? " user=" : "",
				   pq_user ? pq_user : "",
				   pq_pwd ? " password=" : "",
				   pq_pwd ? pq_pwd : "",
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

	priv_data = g_new (GdaPostgresConnectionData, 1);
	priv_data->pconn = pconn;
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

	for (i = 0; i < priv_data->ntypes; i++)
		g_free (priv_data->type_data[i].name);
	
	g_hash_table_destroy (priv_data->h_table);
	g_free (priv_data->type_data);
	g_free (priv_data);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, NULL);
	return TRUE;
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
	}

	return reclist;
}

static gboolean 
gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
				         GdaConnection *cnc,
					 const gchar *trans_id)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "BEGIN"); 
}

static gboolean 
gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *trans_id)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "COMMIT"); 
}

static gboolean 
gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaConnection *cnc,
					    const gchar *trans_id)
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
		return TRUE;
	default :
	}
 
	return FALSE;
}

static GdaDataModel *
get_postgres_procedures (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
					"SELECT proname FROM pg_proc ORDER BY proname",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);

	return recset;
}

static GdaDataModel *
get_postgres_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (
		NULL, cnc,
		"SELECT relname FROM pg_class WHERE relkind = 'r' AND "
		"relname !~ '^pg_' ORDER BY relname",
		GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Tables"));

	return recset;
}

static void
add_string_row (gpointer data, gpointer user_data)
{
	GdaValue *value;
	GList list;
	GdaDataModelArray *recset;
	GdaPostgresTypeOid *td;
	
	td = data;
	recset = user_data;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	value = gda_value_new_string (td->name);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_data_model_append_row (GDA_DATA_MODEL (recset), &list);

	gda_value_free (value);
}

static GdaDataModel *
get_postgres_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	GdaPostgresConnectionData *priv_data;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (1));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Types"));

	/* fill the recordset */
	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	for (i = 0; i < priv_data->ntypes; i++)
		add_string_row (&priv_data->type_data[i], recset);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_postgres_views (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (
		NULL, cnc, 
		"SELECT relname FROM pg_class WHERE relkind = 'v' AND "
		"relname !~ '^pg_' ORDER BY relname",
		GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Views"));

	return recset;
}

static GdaDataModel *
get_postgres_indexes (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (
		NULL, cnc, 
		"SELECT relname FROM pg_class WHERE relkind = 'i' AND "
		"relname !~ '^pg_' ORDER BY relname",
		GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Indexes"));

	return recset;
}

static GdaDataModel *
get_postgres_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (
		NULL, cnc, 
		"SELECT a.aggname || '(' || t.typname || ')' AS \"Name\" "
		" FROM pg_aggregate a, pg_type t  WHERE a.aggbasetype = t.oid "
		" ORDER BY a.aggname",
		GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Aggregates"));

	return recset;
}

static GdaDataModel *
get_postgres_triggers (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
					"SELECT tgname FROM pg_trigger "
					"WHERE tgisconstraint = false "
					"ORDER BY tgname ",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Triggers"));

	return recset;
}

static GdaDataModelArray *
gda_postgres_init_md_recset (GdaConnection *cnc)
{
	GdaDataModelArray *recset;
	gint i;
	GdaPostgresColData cols[8] = {
		{ N_("Field name")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Data type")	, GDA_VALUE_TYPE_STRING  },
		{ N_("Size")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Scale")		, GDA_VALUE_TYPE_INTEGER },
		{ N_("Not null?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Primary key?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("Unique index?")	, GDA_VALUE_TYPE_BOOLEAN },
		{ N_("References")	, GDA_VALUE_TYPE_STRING  }
		};

	recset = GDA_DATA_MODEL_ARRAY (gda_data_model_array_new (sizeof cols / sizeof cols[0]));
	for (i = 0; i < sizeof cols / sizeof cols[0]; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), i, _(cols[i].col_name));

	return recset;
}

static GList *
gda_postgres_get_idx_data (PGconn *pconn, const gchar *tblname)
{
	gint nidx, i;
	GList *idx_list = NULL;
	GdaPostgresIdxData *idx_data;
	gchar *query;
	PGresult *pg_idx;

	query = g_strdup_printf (
			"SELECT i.indkey, i.indisprimary, i.indisunique "
			"FROM pg_class c, pg_class c2, pg_index i "
			"WHERE c.relname = '%s' AND c.oid = i.indrelid "
			"AND i.indexrelid = c2.oid", tblname);

	pg_idx = PQexec(pconn, query);
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

static GList *
gda_postgres_get_ref_data (PGconn *pconn, const gchar *tblname)
{
	gint nref, i;
	GList *ref_list = NULL;
	GdaPostgresRefData *ref_data;
	gchar *query;
	PGresult *pg_ref;
	gint lentblname;

	// WARNING: don't know a better way to get references :-(
	lentblname = strlen (tblname);
	query = g_strdup_printf ("SELECT tr.tgargs "
				 "FROM pg_class c, pg_trigger tr "
				 "WHERE c.relname = '%s' AND c.oid = tr.tgrelid AND "
				 "tr.tgisconstraint = true AND tr.tgnargs = 6", tblname);

	pg_ref = PQexec(pconn, query);
	g_free (query);
	if (pg_ref == NULL)
		return NULL;

	nref = PQntuples (pg_ref);

	ref_data = g_new (GdaPostgresRefData, nref);
	for (i = 0; i < nref; i++){
		gchar **arr;
		gchar *value;

		value = PQgetvalue (pg_ref, i, 0);
		arr = g_strsplit (value, "\\000", 0);
		if (!strncmp (tblname, arr[1], lentblname)) {
			ref_data[i].colname = g_strdup (arr[4]);
			ref_data[i].reference = g_strdup_printf ("%s.%s", arr[2], arr[5]);
			ref_list = g_list_append (ref_list, &ref_data[i]);
		}

		g_strfreev (arr);
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

	g_free (ref_data->colname);
	g_free (ref_data->reference);
}

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

	query = g_strdup_printf (
			"SELECT a.attname, b.typname, a.atttypmod, b.typlen, a.attnotnull "
			"FROM pg_class c, pg_attribute a, pg_type b "
			"WHERE c.relname = '%s' AND a.attnum > 0 AND "
			"      a.attrelid = c.oid and b.oid = a.atttypid "
			"ORDER BY a.attnum", tblname);

	pg_res = PQexec(priv_data->pconn, query);
	g_free (query);
	if (pg_res == NULL)
		return NULL;

	idx_list = gda_postgres_get_idx_data (priv_data->pconn, tblname);
	ref_list = gda_postgres_get_ref_data (priv_data->pconn, tblname);

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
			integer = atoi (thevalue);
		}
			
		value = gda_value_new_integer ((integer != -1) ? integer : 0);
		rowlist = g_list_append (rowlist, value);

		/* Scale */ 
		value = gda_value_new_integer (0); // TODO
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
		rlist = g_list_find_custom (ref_list, colname, ref_custom_compare); 
		if (rlist)
			ref = ((GdaPostgresRefData *) rlist->data)->reference;
		else
			ref = "";

		value = gda_value_new_string (ref);
		rowlist = g_list_append (rowlist, value);

		list = g_list_append (list, rowlist);
		rowlist = NULL;
	}

	PQclear (pg_res);

	if (idx_list && idx_list->data) {
		g_list_foreach (idx_list, free_idx_data, NULL);
		g_free (idx_list->data);
	}

	if (ref_list && ref_list->data) {
		g_list_foreach (ref_list, free_ref_data, NULL);
		g_free (ref_list->data);
	}

	g_list_free (ref_list);
	g_list_free (idx_list);

	return list;
}

static void
add_g_list_row (gpointer data, gpointer user_data)
{
	GList *rowlist = data;
	GdaDataModelArray *recset = user_data;

	gda_data_model_append_row (GDA_DATA_MODEL (recset), rowlist);
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
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
					"SELECT datname "
					"FROM pg_database "
					"ORDER BY 1",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Databases"));

	return recset;
}

static GdaDataModel *
get_postgres_users (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
					"SELECT usename "
					"FROM pg_user "
					"ORDER BY usename",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Users"));

	return recset;
}

static GdaDataModel *
get_postgres_sequences (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);


	reclist = process_sql_commands (NULL, cnc, 
					"SELECT relname "
					"FROM pg_class "
					"WHERE relkind IN ('S','') "
					"      AND relname !~ '^pg_' "
					"ORDER BY 1",
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);

	if (!reclist)
		return NULL;

	recset = GDA_DATA_MODEL (reclist->data);
	g_list_free (reclist);
	// Set it here instead of the SQL query to allow i18n
	gda_data_model_set_column_title (recset, 0, _("Sequences"));

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

	}

	return NULL;
}

