/* GDA MySQL provider
 * Copyright (C) 1998-2004 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-util.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_MYSQL_HANDLE "GDA_Mysql_MysqlHandle"

static void gda_mysql_provider_class_init (GdaMysqlProviderClass *klass);
static void gda_mysql_provider_init       (GdaMysqlProvider *provider,
					   GdaMysqlProviderClass *klass);
static void gda_mysql_provider_finalize   (GObject *object);

static const gchar *gda_mysql_provider_get_version (GdaServerProvider *provider);
static gboolean gda_mysql_provider_open_connection (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_mysql_provider_close_connection (GdaServerProvider *provider,
						     GdaConnection *cnc);
static const gchar *gda_mysql_provider_get_server_version (GdaServerProvider *provider,
							   GdaConnection *cnc);
static const gchar *gda_mysql_provider_get_database (GdaServerProvider *provider,
						     GdaConnection *cnc);
static gboolean gda_mysql_provider_change_database (GdaServerProvider *provider,
		                                    GdaConnection *cnc,
		                                    const gchar *name);
static gboolean gda_mysql_provider_create_database (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *name);
static gboolean gda_mysql_provider_drop_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
static GList *gda_mysql_provider_execute_command (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);
static gchar *gda_mysql_provider_get_last_insert_id (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaDataModel *recset);
static gboolean gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      GdaTransaction *xaction);
static gboolean gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 GdaTransaction *xaction);
static gboolean gda_mysql_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);
static GdaDataModel *gda_mysql_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);
static gboolean gda_mysql_provider_escape_string (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *from,
						  gchar *to);
				 		 
static GObjectClass *parent_class = NULL;

/*
 * GdaMysqlProvider class implementation
 */

static void
gda_mysql_provider_class_init (GdaMysqlProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_provider_finalize;
	provider_class->get_version = gda_mysql_provider_get_version;
	provider_class->open_connection = gda_mysql_provider_open_connection;
	provider_class->close_connection = gda_mysql_provider_close_connection;
	provider_class->get_server_version = gda_mysql_provider_get_server_version;
	provider_class->get_database = gda_mysql_provider_get_database;
	provider_class->change_database = gda_mysql_provider_change_database;
	provider_class->create_database = gda_mysql_provider_create_database;
	provider_class->drop_database = gda_mysql_provider_drop_database;
	provider_class->execute_command = gda_mysql_provider_execute_command;
	provider_class->get_last_insert_id = gda_mysql_provider_get_last_insert_id;
	provider_class->begin_transaction = gda_mysql_provider_begin_transaction;
	provider_class->commit_transaction = gda_mysql_provider_commit_transaction;
	provider_class->rollback_transaction = gda_mysql_provider_rollback_transaction;
	provider_class->supports = gda_mysql_provider_supports;
	provider_class->get_schema = gda_mysql_provider_get_schema;
	provider_class->escape_string = gda_mysql_provider_escape_string;
}

static void
gda_mysql_provider_init (GdaMysqlProvider *myprv, GdaMysqlProviderClass *klass)
{
}

static void
gda_mysql_provider_finalize (GObject *object)
{
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) object;

	g_return_if_fail (GDA_IS_MYSQL_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_mysql_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaMysqlProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_provider_class_init,
			NULL, NULL,
			sizeof (GdaMysqlProvider),
			0,
			(GInstanceInitFunc) gda_mysql_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaMysqlProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_mysql_provider_new (void)
{
	GdaMysqlProvider *provider;

	provider = g_object_new (gda_mysql_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaMysqlProvider class */
static const gchar *
gda_mysql_provider_get_version (GdaServerProvider *provider)
{
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);

	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_open_connection (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
	const gchar *t_host = NULL;
        const gchar *t_db = NULL;
        const gchar *t_user = NULL;
        const gchar *t_password = NULL;
        const gchar *t_port = NULL;
        const gchar *t_unix_socket = NULL;
        const gchar *t_use_ssl = NULL;
	unsigned int mysqlflags = 0;
	MYSQL *mysql;
#if MYSQL_VERSION_ID < 32200
        gint err;
#endif
	GdaError *error;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_host = gda_quark_list_find (params, "HOST");
	t_db = gda_quark_list_find (params, "DATABASE");
	t_user = gda_quark_list_find (params, "USER");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_port = gda_quark_list_find (params, "PORT");
	t_unix_socket = gda_quark_list_find (params, "UNIX_SOCKET");
	t_use_ssl = gda_quark_list_find (params, "USE_SSL");

	if (username && *username != '\0')
		t_user = username;
	if (password && *password != '\0')
		t_password = password;

	if (t_use_ssl && atoi (t_use_ssl) == 1)
		mysqlflags |= CLIENT_SSL;

	/* we can't have both a host/pair AND a unix_socket */
	if ((t_host || t_port) && t_unix_socket) {
		gda_connection_add_error_string (
			cnc, _("You cannot provide a UNIX_SOCKET if you also provide"
			       " either a HOST or a PORT."));
		return FALSE;
	}

	/* provide the default of localhost:3306 if neither is provided */
	if (!t_unix_socket) {
		if (!t_host)
			t_host = "localhost";
		else if (!t_port)
			t_port = "3306";
	}
	
	mysql = g_new0 (MYSQL, 1);
	mysql_init (mysql);
	mysql = mysql_real_connect (mysql, t_host, t_user, t_password,
#if MYSQL_VERSION_ID >= 32200
				    t_db,
#endif
				    t_port ? atoi (t_port) : 0,
				    t_unix_socket,
				    mysqlflags);
	if (!mysql) {
		error = gda_mysql_make_error (mysql);
		gda_connection_add_error (cnc, error);

		return FALSE;
	}
#if MYSQL_VERSION_ID < 32200
	err = mysql_select_db (mysql, t_db);
	if (err != 0) {
		error = gda_mysql_make_error (mysql);
		mysql_close (mysql);
		gda_connection_add_error (cnc, error);

		return FALSE;
	}
#endif

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE, mysql);

	return TRUE;
}

/* close_connection handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql)
		return FALSE;

	mysql_close (mysql);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaMysqlProvider class */
static const gchar *
gda_mysql_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MySQL handle"));
		return FALSE;
	}

	return (const gchar *) mysql->server_version;
}

static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql)
{
	MYSQL *mysql;
	gchar **arr;
	GdaConnectionOptions options;

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	options = gda_connection_get_options (cnc);

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			gint rc;
			MYSQL_RES *mysql_res;
			GdaMysqlRecordset *recset;

			/* if the connection is in read-only mode, just allow SELECT,
			   SHOW commands */
			if (options & GDA_CONNECTION_OPTIONS_READ_ONLY) {
				gchar *s;

				/* FIXME: maybe there's a better way of doing this? */
				s = g_strstrip (g_strdup (arr[n]));
				if (g_ascii_strncasecmp (s, "select", strlen ("select")) &&
				    g_ascii_strncasecmp (s, "show", strlen ("show"))) {
					gda_connection_add_error_string (
						cnc, "Command '%s' cannot be executed in read-only mode", arr[n]);
					break;
				}

				g_free (s);
			}

			/* execute the command on the server */
			rc = mysql_real_query (mysql, arr[n], strlen (arr[n]));
			if (rc != 0) {
				gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
				break;
			}

			mysql_res = mysql_store_result (mysql);
			recset = gda_mysql_recordset_new (cnc, mysql_res, mysql);
			if (GDA_IS_MYSQL_RECORDSET (recset)) {
				gda_data_model_set_command_text ( (GdaDataModel*) recset, arr[n]);
				gda_data_model_set_command_type ( (GdaDataModel*) recset, GDA_COMMAND_TYPE_SQL);
				reclist = g_list_append (reclist, recset);
			}

			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* get_database handler for the GdaMysqlProvider class */
static const gchar *
gda_mysql_provider_get_database (GdaServerProvider *provider,
				 GdaConnection *cnc)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	return (const gchar *) mysql->db;
}

/* change_database handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_change_database (GdaServerProvider *provider,
		                    GdaConnection *cnc,
				    const gchar *name)
{
	gint rc;
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	rc = mysql_select_db (mysql, name);
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;

}

/* create_database handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_create_database (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *name)
{
	gint rc;
	gchar *sql;
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	sql = g_strdup_printf ("CREATE DATABASE %s", name);
	rc = mysql_query (mysql, sql);
	g_free (sql);
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* drop_database handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_drop_database (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *name)
{
	gint rc;
	gchar *sql;
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	sql = g_strdup_printf ("DROP DATABASE %s", name);
	rc = mysql_query (mysql, sql);
	g_free (sql);
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* execute_command handler for the GdaMysqlProvider class */
static GList *
gda_mysql_provider_execute_command (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaCommand *cmd,
				    GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		reclist = process_sql_commands (reclist, cnc, gda_command_get_text (cmd));
		break;
	case GDA_COMMAND_TYPE_TABLE :
		str = g_strdup_printf ("SELECT * FROM %s", gda_command_get_text (cmd));
		reclist = process_sql_commands (reclist, cnc, str);
		if (reclist && GDA_IS_DATA_MODEL (reclist->data)) {
			gda_data_model_set_command_text (GDA_DATA_MODEL (reclist->data),
							 gda_command_get_text (cmd));
			gda_data_model_set_command_type (GDA_DATA_MODEL (reclist->data),
							 GDA_COMMAND_TYPE_TABLE);
		}

		g_free (str);
		break;
	default: break;
	}

	return reclist;
}

/* get_last_insert_id handler for the GdaMysqlProvider class */
static gchar *
gda_mysql_provider_get_last_insert_id (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaDataModel *recset)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	return g_strdup_printf ("%ld", (unsigned long int ) mysql_insert_id (mysql));
}

/* begin_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaTransaction *xaction)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_error_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	/* set isolation level */
	switch (gda_transaction_get_isolation_level (xaction)) {
	case GDA_TRANSACTION_ISOLATION_READ_COMMITTED :
		rc = mysql_real_query (mysql, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", 46);
		break;
	case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED :
		rc = mysql_real_query (mysql, "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", 48);
		break;
	case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ :
		rc = mysql_real_query (mysql, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", 47);
		break;
	case GDA_TRANSACTION_ISOLATION_SERIALIZABLE :
		rc = mysql_real_query (mysql, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", 44);
		break;
	default :
		rc = 0;
	}

	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	/* start the transaction */
	rc = mysql_real_query (mysql, "BEGIN", strlen ("BEGIN"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* commit_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_error_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	rc = mysql_real_query (mysql, "COMMIT", strlen ("COMMIT"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* rollback_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_error_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	rc = mysql_real_query (mysql, "ROLLBACK", strlen ("ROLLBACK"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* supports handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_supports (GdaServerProvider *provider,
			     GdaConnection *cnc,
			     GdaConnectionFeature feature)
{
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_AGGREGATES :
	case GDA_CONNECTION_FEATURE_SQL :
	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
		return TRUE;
	default : break;
	}

	return FALSE;
}

static void
add_aggregate_row (GdaDataModelArray *recset, const gchar *str, const gchar *comments)
{
	GList *list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	/* 1st the name */
	list = g_list_append (NULL, gda_value_new_string (str));
	/* 2nd the unique id */
	list = g_list_append (list, gda_value_new_string (str));
	/* 3rd the owner */
	list = g_list_append (list, gda_value_new_string (NULL));
	/* 4th the comments */ 
	list = g_list_append (list, gda_value_new_string (comments));
	/* 5th the return type */
	list = g_list_append (list, gda_value_new_string (gda_type_to_string (GDA_VALUE_TYPE_UNKNOWN)));
	/* 6th the argument type */
	list = g_list_append (list, gda_value_new_string (gda_type_to_string (GDA_VALUE_TYPE_UNKNOWN)));
	/* 7th the SQL definition */
	list = g_list_append (list, gda_value_new_string (NULL));

	gda_data_model_append_row (GDA_DATA_MODEL (recset), list);

	g_list_foreach (list, (GFunc) gda_value_free, NULL);
	g_list_free (list);
}

static GdaDataModel *
get_mysql_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (7);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("ID"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 4, _("Return type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 5, _("Args types"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 6, _("SQL"));

	/* fill the recordset */
	add_aggregate_row (recset, "abs", "comments");
	add_aggregate_row (recset, "acos", "comments");
	add_aggregate_row (recset, "adddate", "comments");
	add_aggregate_row (recset, "ascii", "comments");
	add_aggregate_row (recset, "asin", "comments");
	add_aggregate_row (recset, "atan", "comments");
	add_aggregate_row (recset, "atan2", "comments");
	add_aggregate_row (recset, "benchmark", "comments");
	add_aggregate_row (recset, "bin", "comments");
	add_aggregate_row (recset, "bit_count", "comments");
	add_aggregate_row (recset, "ceiling", "comments");
	add_aggregate_row (recset, "char", "comments");
	add_aggregate_row (recset, "char_length", "comments");
	add_aggregate_row (recset, "character_length", "comments");
	add_aggregate_row (recset, "coalesce", "comments");
	add_aggregate_row (recset, "concat", "comments");
	add_aggregate_row (recset, "concat_ws", "comments");
	add_aggregate_row (recset, "connection_id", "comments");
	add_aggregate_row (recset, "conv", "comments");
	add_aggregate_row (recset, "cos", "comments");
	add_aggregate_row (recset, "cot", "comments");
	add_aggregate_row (recset, "count", "comments");
	add_aggregate_row (recset, "curdate", "comments");
	add_aggregate_row (recset, "current_date", "comments");
	add_aggregate_row (recset, "current_time", "comments");
	add_aggregate_row (recset, "current_timestamp", "comments");
	add_aggregate_row (recset, "curtime", "comments");
	add_aggregate_row (recset, "database", "comments");
	add_aggregate_row (recset, "date_add", "comments");
	add_aggregate_row (recset, "date_format", "comments");
	add_aggregate_row (recset, "date_sub", "comments");
	add_aggregate_row (recset, "dayname", "comments");
	add_aggregate_row (recset, "dayofmonth", "comments");
	add_aggregate_row (recset, "dayofweek", "comments");
	add_aggregate_row (recset, "dayofyear", "comments");
	add_aggregate_row (recset, "decode", "comments");
	add_aggregate_row (recset, "degrees", "comments");
	add_aggregate_row (recset, "elt", "comments");
	add_aggregate_row (recset, "encode", "comments");
	add_aggregate_row (recset, "encrypt", "comments");
	add_aggregate_row (recset, "exp", "comments");
	add_aggregate_row (recset, "export_set", "comments");
	add_aggregate_row (recset, "extract", "comments");
	add_aggregate_row (recset, "field", "comments");
	add_aggregate_row (recset, "find_in_set", "comments");
	add_aggregate_row (recset, "floor", "comments");
	add_aggregate_row (recset, "format", "comments");
	add_aggregate_row (recset, "from_days", "comments");
	add_aggregate_row (recset, "from_unixtime", "comments");
	add_aggregate_row (recset, "get_lock", "comments");
	add_aggregate_row (recset, "greatest", "comments");
	add_aggregate_row (recset, "hex", "comments");
	add_aggregate_row (recset, "hour", "comments");
	add_aggregate_row (recset, "if", "comments");
	add_aggregate_row (recset, "ifnull", "comments");
	add_aggregate_row (recset, "inet_aton", "comments");
	add_aggregate_row (recset, "inet_ntoa", "comments");
	add_aggregate_row (recset, "insert", "comments");
	add_aggregate_row (recset, "instr", "comments");
	add_aggregate_row (recset, "interval", "comments");
	add_aggregate_row (recset, "isnull", "comments");
	add_aggregate_row (recset, "last_insert_id", "comments");
	add_aggregate_row (recset, "lcase", "comments");
	add_aggregate_row (recset, "least", "comments");
	add_aggregate_row (recset, "left", "comments");
	add_aggregate_row (recset, "length", "comments");
	add_aggregate_row (recset, "load_file", "comments");
	add_aggregate_row (recset, "locate", "comments");
	add_aggregate_row (recset, "log", "comments");
	add_aggregate_row (recset, "log10", "comments");
	add_aggregate_row (recset, "lower", "comments");
	add_aggregate_row (recset, "lpad", "comments");
	add_aggregate_row (recset, "ltrim", "comments");
	add_aggregate_row (recset, "make_set", "comments");
	add_aggregate_row (recset, "master_pos_wait", "comments");
	add_aggregate_row (recset, "match", "comments");
	add_aggregate_row (recset, "max", "comments");
	add_aggregate_row (recset, "md5", "comments");
	add_aggregate_row (recset, "mid", "comments");
	add_aggregate_row (recset, "min", "comments");
	add_aggregate_row (recset, "minute", "comments");
	add_aggregate_row (recset, "mod", "comments");
	add_aggregate_row (recset, "month", "comments");
	add_aggregate_row (recset, "monthname", "comments");
	add_aggregate_row (recset, "now", "comments");
	add_aggregate_row (recset, "nullif", "comments");
	add_aggregate_row (recset, "oct", "comments");
	add_aggregate_row (recset, "octet_length", "comments");
	add_aggregate_row (recset, "ord", "comments");
	add_aggregate_row (recset, "password", "comments");
	add_aggregate_row (recset, "period_add", "comments");
	add_aggregate_row (recset, "period_diff", "comments");
	add_aggregate_row (recset, "pi", "comments");
	add_aggregate_row (recset, "position", "comments");	
	add_aggregate_row (recset, "pow", "comments");
	add_aggregate_row (recset, "power", "comments");
	add_aggregate_row (recset, "quarter", "comments");
	add_aggregate_row (recset, "radians", "comments");
	add_aggregate_row (recset, "rand", "comments");
	add_aggregate_row (recset, "release_lock", "comments");
	add_aggregate_row (recset, "repeat", "comments");
	add_aggregate_row (recset, "replace", "comments");	
	add_aggregate_row (recset, "reverse", "comments");
	add_aggregate_row (recset, "right", "comments");
	add_aggregate_row (recset, "round", "comments");
	add_aggregate_row (recset, "rpad", "comments");
	add_aggregate_row (recset, "rtrim", "comments");
	add_aggregate_row (recset, "second", "comments");
	add_aggregate_row (recset, "sec_to_time", "comments");
	add_aggregate_row (recset, "session_user", "comments");
	add_aggregate_row (recset, "sign", "comments");
	add_aggregate_row (recset, "sin", "comments");
	add_aggregate_row (recset, "soundex", "comments");
	add_aggregate_row (recset, "space", "comments");
	add_aggregate_row (recset, "sqrt", "comments");
	add_aggregate_row (recset, "strcmp", "comments");
	add_aggregate_row (recset, "subdate", "comments");
	add_aggregate_row (recset, "substring", "comments");
	add_aggregate_row (recset, "substring_index", "comments");
	add_aggregate_row (recset, "sysdate", "comments");
	add_aggregate_row (recset, "system_user", "comments");
	add_aggregate_row (recset, "tan", "comments");
	add_aggregate_row (recset, "time_format", "comments");
	add_aggregate_row (recset, "time_to_sec", "comments");
	add_aggregate_row (recset, "to_days", "comments");
	add_aggregate_row (recset, "trim", "comments");
	add_aggregate_row (recset, "truncate", "comments");
	add_aggregate_row (recset, "ucase", "comments");
	add_aggregate_row (recset, "unix_timestamp", "comments");
	add_aggregate_row (recset, "upper", "comments");
	add_aggregate_row (recset, "user", "comments");
	add_aggregate_row (recset, "version", "comments");
	add_aggregate_row (recset, "week", "comments");
	add_aggregate_row (recset, "weekday", "comments");
	add_aggregate_row (recset, "year", "comments");
	add_aggregate_row (recset, "yearweek", "comments");

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_mysql_databases (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaMysqlRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "show databases");
	if (!reclist)
		return NULL;

	recset = GDA_MYSQL_RECORDSET (reclist->data);
	g_list_free (reclist);

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_mysql_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaMysqlRecordset *recset;
	GdaDataModel *model;
	gint rows, r;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "show table status");
	if (!reclist)
		return NULL;

	recset = (GdaMysqlRecordset *) reclist->data;
	g_list_free (reclist);
	if (!GDA_IS_MYSQL_RECORDSET (recset))
		return NULL;

	/* add the extra information */
	model = gda_data_model_array_new (4);
	gda_data_model_set_column_title (model, 0, _("Name"));
	gda_data_model_set_column_title (model, 1, _("Owner"));
	gda_data_model_set_column_title (model, 2, _("Comments"));
	gda_data_model_set_column_title (model, 3, "SQL");
	rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (recset));
	for (r = 0; r < rows; r++) {
		GList *value_list = NULL;
		gchar *name;
		gchar *str;

		/* 1st, the name of the table */
		name = gda_value_stringify (gda_data_model_get_value_at (GDA_DATA_MODEL (recset), 0, r));
		value_list = g_list_append (value_list, gda_value_new_string (name));

		/* 2nd, the owner */
		value_list = g_list_append (value_list, gda_value_new_string (""));

		/* 3rd, the comments */
		str = gda_value_stringify (gda_data_model_get_value_at (GDA_DATA_MODEL (recset), 14, r));
		value_list = g_list_append (value_list, gda_value_new_string (str));
		g_free (str);

		/* 4th, the SQL command */
		str = g_strdup_printf ("SHOW CREATE TABLE %s", name);
		reclist = process_sql_commands (NULL, cnc, str);
		g_free (str);
		if (reclist && GDA_IS_DATA_MODEL (reclist->data)) {
			str = gda_value_stringify (
				gda_data_model_get_value_at (GDA_DATA_MODEL (reclist->data), 1, 0));
			value_list = g_list_append (value_list, gda_value_new_string (str));

			g_free (str);
			g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
			g_list_free (reclist);
		}
		else
			value_list = g_list_append (value_list, gda_value_new_string (""));

		gda_data_model_append_row (model, value_list);

		g_free (name);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return model;
}

static GdaDataModel *
get_mysql_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	gint i;
	struct {
		const gchar *name;
		const gchar *owner;
		const gchar *comments;
		GdaValueType type;
	} types[] = {
		{ "blob", "", "Binary blob", GDA_VALUE_TYPE_BINARY },
		{ "bigint", "", "Big integer", GDA_VALUE_TYPE_BIGINT },
		{ "char", "", "Char", GDA_VALUE_TYPE_STRING },
		{ "date", "", "Date", GDA_VALUE_TYPE_DATE },
		{ "datetime", "", "Date and time", GDA_VALUE_TYPE_TIMESTAMP },
		{ "decimal", "", "Decimal number", GDA_VALUE_TYPE_DOUBLE },
		{ "double", "", "Double precision number", GDA_VALUE_TYPE_DOUBLE },
		{ "enum", "", "Enumeration", GDA_VALUE_TYPE_STRING },
		{ "float", "", "Single precision number", GDA_VALUE_TYPE_SINGLE },
		{ "int", "", "Integer", GDA_VALUE_TYPE_INTEGER },
		{ "integer", "", "Integer", GDA_VALUE_TYPE_INTEGER },		
		{ "long", "", "Long integer", GDA_VALUE_TYPE_INTEGER },
		{ "longblob", "", "Long blob", GDA_VALUE_TYPE_BINARY },
		{ "longtext", "", "Long text", GDA_VALUE_TYPE_STRING },
		{ "mediumint", "", "Medium integer", GDA_VALUE_TYPE_INTEGER },
		{ "mediumblob", "", "Medium blob", GDA_VALUE_TYPE_BINARY },
		{ "mediumtext", "", "Medium text", GDA_VALUE_TYPE_STRING },				
		{ "set", "", "Set", GDA_VALUE_TYPE_STRING },
		{ "smallint", "", "Small integer", GDA_VALUE_TYPE_INTEGER },
		{ "text", "", "Text", GDA_VALUE_TYPE_STRING },
		{ "tinyint", "", "Tiny integer", GDA_VALUE_TYPE_INTEGER }, /* really a boolean */
		{ "tinyblob", "", "Tiny blob", GDA_VALUE_TYPE_BINARY },
		{ "tinytext", "", "Tiny text", GDA_VALUE_TYPE_STRING },		
		{ "time", "", "Time", GDA_VALUE_TYPE_TIME },
		{ "timestamp", "", "Time stamp", GDA_VALUE_TYPE_TIMESTAMP },
		{ "varchar", "", "Variable Length Char", GDA_VALUE_TYPE_STRING },
		{ "year", "", "Year", GDA_VALUE_TYPE_INTEGER }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (4);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, _("GDA type"));

	/* fill the recordset */
	for (i = 0; i < sizeof (types) / sizeof (types[0]); i++) {
		GList *value_list = NULL;

		value_list = g_list_append (value_list, gda_value_new_string (types[i].name));
		value_list = g_list_append (value_list, gda_value_new_string (types[i].owner));
		value_list = g_list_append (value_list, gda_value_new_string (types[i].comments));
		value_list = g_list_append (value_list, gda_value_new_type (types[i].type));

		gda_data_model_append_row (GDA_DATA_MODEL (recset), value_list);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return GDA_DATA_MODEL (recset);
}

static GList *
field_row_to_value_list (MYSQL_ROW mysql_row)
{
	gchar **arr;
	GList *value_list = NULL;

	g_return_val_if_fail (mysql_row != NULL, NULL);

	/* field name */
	value_list = g_list_append (value_list, gda_value_new_string (mysql_row[0]));

	/* type and size */
	arr = g_strsplit ((const gchar *) mysql_row[1], "(", 0);
	if (!arr) {
		value_list = g_list_append (value_list, gda_value_new_string (""));
		value_list = g_list_append (value_list, gda_value_new_integer (-1));
	}
	else {
		if (arr[0] && arr[1]) {
			value_list = g_list_append (value_list,
						    gda_value_new_string (arr[0]));
			value_list = g_list_append (value_list,
						    gda_value_new_integer (atoi (arr[1])));
		}
		else {
			value_list = g_list_append (value_list,
						    gda_value_new_string (arr[0]));
			value_list = g_list_append (value_list, gda_value_new_integer (-1));
		}

		g_strfreev (arr);
	}

	/* scale */
	value_list = g_list_append (value_list, gda_value_new_integer (0));

	/* Not null? */
	if (mysql_row[2] && !strcmp (mysql_row[2], "YES"))
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));
	else
		value_list = g_list_append (value_list, gda_value_new_boolean (TRUE));

	/* Primary key? */
	if (mysql_row[3] && !strcmp (mysql_row[3], "PRI"))
		value_list = g_list_append (value_list, gda_value_new_boolean (TRUE));
	else
		value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));

	/* Unique index? */
	value_list = g_list_append (value_list, gda_value_new_boolean (FALSE));

	/* references */
	value_list = g_list_append (value_list, gda_value_new_string (mysql_row[5]));

	/* default value */
	value_list = g_list_append (value_list, gda_value_new_string (mysql_row[4]));

	return value_list;
}

static GdaDataModel *
get_table_fields (GdaConnection *cnc, GdaParameterList *params)
{
	const gchar *table_name;
	GdaParameter *par;
	gchar *cmd_str;
	GdaDataModelArray *recset;
	gint rows, r;
	gint rc;
	MYSQL *mysql;
	MYSQL_RES *mysql_res;
	struct {
		const gchar *name;
		GdaValueType type;
	} fields_desc[] = {
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

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* get parameters sent by client */
	par = gda_parameter_list_find (params, "name");
	if (!par) {
		gda_connection_add_error_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	table_name = gda_value_get_string ((GdaValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_error_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	/* execute command on server */	
	cmd_str = g_strdup_printf ("SHOW COLUMNS FROM %s", table_name);
	rc = mysql_real_query (mysql, cmd_str, strlen (cmd_str));
	g_free (cmd_str);
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_mysql_make_error (mysql));
		return NULL;
	}

	mysql_res = mysql_store_result (mysql);
	rows = mysql_num_rows (mysql_res);

	/* fill in the recordset to be returned */
	recset = (GdaDataModelArray *) gda_data_model_array_new (9);
	for (r = 0; r < sizeof (fields_desc) / sizeof (fields_desc[0]); r++) {
/*
		gint defined_size =  (fields_desc[r].type == GDA_VALUE_TYPE_STRING) ? 64 : 
			(fields_desc[r].type == GDA_VALUE_TYPE_INTEGER) ? sizeof (gint) : 1;
*/

		/* gda_server_recordset_model_set_field_defined_size (recset, r, defined_size); */
		gda_data_model_set_column_title (GDA_DATA_MODEL (recset), r,
						_(fields_desc[r].name));
/*
		gda_server_recordset_model_set_field_scale (recset, r, 0);
		gda_server_recordset_model_set_field_gdatype (recset, r, fields_desc[r].type);
*/
	}
	
	for (r = 0; r < rows; r++) {
		GList *value_list;
		MYSQL_ROW mysql_row;

		mysql_data_seek (mysql_res, r);
		mysql_row = mysql_fetch_row (mysql_res);
		if (!mysql_row) {
			mysql_free_result (mysql_res);
			g_object_unref (G_OBJECT (recset));

			return NULL;
		}

		value_list = field_row_to_value_list (mysql_row);
		if (!value_list) {
			mysql_free_result (mysql_res);
			g_object_unref (G_OBJECT (recset));

			return NULL;
		}

		gda_data_model_append_row (GDA_DATA_MODEL (recset),
					   (const GList *) value_list);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	mysql_free_result (mysql_res);

	return GDA_DATA_MODEL (recset);
}

/* get_schema handler for the GdaMysqlProvider class */
static GdaDataModel *
gda_mysql_provider_get_schema (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionSchema schema,
			       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_mysql_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_mysql_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_mysql_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_mysql_types (cnc, params);
	default : break;
	}

	return NULL;
}


gboolean
gda_mysql_provider_escape_string (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *from,
				  gchar *to)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (to != NULL, FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return 0;
	}
	return mysql_real_escape_string (mysql, to, from, strlen (from));
}
