/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include "gda-postgres.h"
#include <libgda/gda-server-recordset-model.h>

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_POSTGRES_HANDLE "GDA_Postgres_PostgresHandle"

static void gda_postgres_provider_class_init (GdaPostgresProviderClass *klass);
static void gda_postgres_provider_init       (GdaPostgresProvider *provider,
					   GdaPostgresProviderClass *klass);
static void gda_postgres_provider_finalize   (GObject *object);

static gboolean gda_postgres_provider_open_connection (GdaServerProvider *provider,
						    GdaServerConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);

static gboolean gda_postgres_provider_close_connection (GdaServerProvider *provider,
						     GdaServerConnection *cnc);

static GList *gda_postgres_provider_execute_command (GdaServerProvider *provider,
						  GdaServerConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);

static gboolean gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
						     GdaServerConnection *cnc,
						     const gchar *trans_id);

static gboolean gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
						     GdaServerConnection *cnc,
						     const gchar *trans_id);

static gboolean gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
						     GdaServerConnection *cnc,
						     const gchar *trans_id);

static gboolean gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
						      GdaServerConnection *cnc,
						      const gchar *command);

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaServerConnection *cnc,
						GNOME_Database_Feature feature);

static GdaServerRecordset *gda_postgres_provider_get_schema (GdaServerProvider *provider,
							  GdaServerConnection *cnc,
							  GNOME_Database_Connection_Schema schema,
							  GdaParameterList *params);

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

/* open_connection handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_open_connection (GdaServerProvider *provider,
				    GdaServerConnection *cnc,
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
	PGconn *pconn;
	PGresult *pg_res;
	GdaError *error;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

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

	conn_string = g_strconcat ( "",
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
		gda_server_connection_add_error (
			cnc, gda_postgres_make_error (pconn));
		PQfinish(pconn);
		return FALSE;
	}

	/*
	 * Sets the DATE format for all the current session to YYYY-MM-DD
	 */
	pg_res = PQexec (pconn, "SET DATESTYLE TO 'ISO'");
	PQclear (pg_res);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, pconn);
	return TRUE;
}

/* close_connection handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	PGconn *pconn;

	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	pconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!pconn)
		return FALSE;

	PQfinish (pconn);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, NULL);
	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaServerConnection *cnc, const gchar *sql)
{
	PGconn *pconn;
	gchar **arr;

	pconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!pconn) {
		gda_server_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			PGresult *pg_res;
			GdaServerRecordset *recset;

			pg_res = PQexec(pconn, arr[n]);
			if (pg_res == NULL) {
				gda_server_connection_add_error (
					cnc, gda_postgres_make_error (pconn));
				break;
			} 
			
			if (PQresultStatus(pg_res) == PGRES_TUPLES_OK){
				recset = gda_postgres_recordset_new (cnc, pg_res);
				if (GDA_IS_SERVER_RECORDSET (recset))
					reclist = g_list_append (reclist, recset);
			}

			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* execute_command handler for the GdaPostgresProvider class */
static GList *
gda_postgres_provider_execute_command (GdaServerProvider *provider,
				       GdaServerConnection *cnc,
				       GdaCommand *cmd,
				       GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd));
		break;
	case GDA_COMMAND_TYPE_TABLE :
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = process_sql_commands (reclist, cnc, str);
		g_free (str);
		break;
	}

	return reclist;
}

static gboolean 
gda_postgres_provider_begin_transaction (GdaServerProvider *provider,
				         GdaServerConnection *cnc,
					 const gchar *trans_id)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "BEGIN"); 
}

static gboolean 
gda_postgres_provider_commit_transaction (GdaServerProvider *provider,
					  GdaServerConnection *cnc,
					  const gchar *trans_id)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "COMMIT"); 
}

static gboolean 
gda_postgres_provider_rollback_transaction (GdaServerProvider *provider,
					    GdaServerConnection *cnc,
					    const gchar *trans_id)
{
	GdaPostgresProvider *pg_prv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pg_prv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	return gda_postgres_provider_single_command (pg_prv, cnc, "ROLLBACK"); 
}

/* Really does the BEGIN/ROLLBACK/COMMIT */
static gboolean 
gda_postgres_provider_single_command (const GdaPostgresProvider *provider,
				      GdaServerConnection *cnc,
				      const gchar *command)
{
	PGconn *pconn;
	PGresult *pg_res;
	gboolean result;

	pconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!pconn) {
		gda_server_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return FALSE;
	}

	result = FALSE;
	pg_res = PQexec(pconn, command);
	if (pg_res != NULL){
		result = PQresultStatus(pg_res) == PGRES_COMMAND_OK;
		PQclear (pg_res);
	}

	if (result == FALSE)
		gda_server_connection_add_error (
			cnc, gda_postgres_make_error (pconn));

	return result;
}

static gboolean gda_postgres_provider_supports (GdaServerProvider *provider,
						GdaServerConnection *cnc,
						GNOME_Database_Feature feature)
{
	GdaPostgresProvider *pgprv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (pgprv), FALSE);

	switch (feature) {
	case GNOME_Database_FEATURE_TRANSACTIONS :
		return TRUE;
	default :
	}
 
	return FALSE;
}

static GdaServerRecordset *
get_postgres_procedures (GdaServerConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
			"SELECT proname FROM pg_proc ORDER BY proname");

	if (!reclist)
		return NULL;

	recset = GDA_SERVER_RECORDSET (reclist->data);
	g_list_free (reclist);

	return recset;
}

static GdaServerRecordset *
get_postgres_tables (GdaServerConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
			"SELECT relname FROM pg_class WHERE relkind = 'r' AND "
			"relname !~ '^pg_' ORDER BY relname");

	if (!reclist)
		return NULL;

	recset = GDA_SERVER_RECORDSET (reclist->data);
	g_list_free (reclist);

	return recset;
}

static void
add_string_row (GdaServerRecordsetModel *recset, const gchar *str)
{
	GdaValue *value;
	GList list;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	value = gda_value_new_string (str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_server_recordset_model_append_row (recset, &list);

	gda_value_free (value);
}

static GdaServerRecordset *
get_postgres_types (GdaServerConnection *cnc, GdaParameterList *params)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = GDA_SERVER_RECORDSET_MODEL (gda_server_recordset_model_new (cnc, 1));
	gda_server_recordset_model_set_field_defined_size (recset, 0, 32);
	gda_server_recordset_model_set_field_name (recset, 0, _("Type"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	/* fill the recordset */
	//TODO: Get it from system tables.
	add_string_row (recset, "abstime");
	add_string_row (recset, "boolean");
	add_string_row (recset, "bpchar");
	add_string_row (recset, "bytea");
	add_string_row (recset, "char");
	add_string_row (recset, "date");
	add_string_row (recset, "datetz");
	add_string_row (recset, "float4");
	add_string_row (recset, "float8");
	add_string_row (recset, "int2");
	add_string_row (recset, "int4");
	add_string_row (recset, "int8");
	add_string_row (recset, "numeric");
	add_string_row (recset, "reltime");
	add_string_row (recset, "time");
	add_string_row (recset, "timetz");
	add_string_row (recset, "timestamp");
	add_string_row (recset, "text");
	add_string_row (recset, "varbit");
	add_string_row (recset, "varchar");

	return GDA_SERVER_RECORDSET (recset);
}

static GdaServerRecordset *
get_postgres_views (GdaServerConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, 
			"SELECT relname FROM pg_class WHERE relkind = 'v' AND "
			"relname !~ '^pg_' ORDER BY relname");

	if (!reclist)
		return NULL;

	recset = GDA_SERVER_RECORDSET (reclist->data);
	g_list_free (reclist);

	return recset;
}

/* get_schema handler for the GdaPostgresProvider class */
static GdaServerRecordset *
gda_postgres_provider_get_schema (GdaServerProvider *provider,
			       GdaServerConnection *cnc,
			       GNOME_Database_Connection_Schema schema,
			       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	switch (schema) {
	case GNOME_Database_Connection_SCHEMA_PROCEDURES :
		return get_postgres_procedures (cnc, params);
	case GNOME_Database_Connection_SCHEMA_TABLES :
		return get_postgres_tables (cnc, params);
	case GNOME_Database_Connection_SCHEMA_TYPES :
		return get_postgres_types (cnc, params);
	case GNOME_Database_Connection_SCHEMA_VIEWS :
		return get_postgres_views (cnc, params);
	}

	return NULL;
}

