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
	provider_class->begin_transaction = NULL;
	provider_class->commit_transaction = NULL;
	provider_class->rollback_transaction = NULL;
}

static void
gda_postgres_provider_init (GdaPostgresProvider *myprv, GdaPostgresProviderClass *klass)
{
}

static void
gda_postgres_provider_finalize (GObject *object)
{
	GdaPostgresProvider *myprv = (GdaPostgresProvider *) object;

	g_return_if_fail (GDA_IS_POSTGRES_PROVIDER (myprv));

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
	const gchar *pq_login;
	const gchar *pq_pwd;
	PGconn *pc;
	PGresult *res;
	GdaError *error;

	GdaPostgresProvider *myprv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	/* parse connection string */
	pq_host = gda_quark_list_find (params, "HOST");
	pq_db = gda_quark_list_find (params, "DATABASE");
	pq_port = gda_quark_list_find (params, "PORT");
	pq_options = gda_quark_list_find (params, "OPTIONS");
	pq_tty = gda_quark_list_find (params, "TTY");
	pq_login = gda_quark_list_find (params, "LOGIN");
	pq_pwd = gda_quark_list_find (params, "PASSWORD");


	/* actually establish the connection */
	pc = PQsetdbLogin (pq_host, pq_port, pq_options, pq_tty, pq_db,
		      pq_login, pq_pwd);

	if (PQstatus (pc) != CONNECTION_OK) {
		gda_postgres_make_error (pc);
		return FALSE;
	}

	/*
	 * Sets the DATE format for all the current session to DD-MM-YYYY
	 */
	res = PQexec (pc, "SET DATESTYLE TO 'SQL, US'");
	PQclear (res);
	/*
	 * the TIMEZONE is left to what is the default, without trying to impose
	 * one. Otherwise the command would be:
	 * "SET TIME ZONE '???'" or "SET TIME ZONE DEFAULT"
	 */

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, pc);
	return TRUE;
}

/* close_connection handler for the GdaPostgresProvider class */
static gboolean
gda_postgres_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	PGconn *pconn;

	GdaPostgresProvider *myprv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	pconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!pconn)
		return FALSE;

	/* check connection status */
	if (PQstatus (pconn) == CONNECTION_OK) {
		PQfinish (pconn);
		pconn = NULL;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE, NULL);
	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaServerConnection *cnc, const gchar *sql)
{
	PGconn *pconn;
	gchar *arr;

	pconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!pconn) {
		gda_server_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n=0;

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

			PQclear(pg_res);
			pg_res = NULL;

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
	GdaPostgresProvider *myprv = (GdaPostgresProvider *) provider;

	g_return_val_if_fail (GDA_IS_POSTGRES_PROVIDER (myprv), NULL);
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

