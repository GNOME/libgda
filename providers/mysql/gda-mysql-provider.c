/* GDA MySQL provider
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <bonobo/bonobo-i18n.h>
#include <libgda/gda-server-recordset-model.h>
#include "gda-mysql.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_MYSQL_HANDLE "GDA_Mysql_MysqlHandle"

static void gda_mysql_provider_class_init (GdaMysqlProviderClass *klass);
static void gda_mysql_provider_init       (GdaMysqlProvider *provider,
					   GdaMysqlProviderClass *klass);
static void gda_mysql_provider_finalize   (GObject *object);

static gboolean gda_mysql_provider_open_connection (GdaServerProvider *provider,
						    GdaServerConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_mysql_provider_close_connection (GdaServerProvider *provider,
						     GdaServerConnection *cnc);
static GList *gda_mysql_provider_execute_command (GdaServerProvider *provider,
						  GdaServerConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);
static gboolean gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
						      GdaServerConnection *cnc,
						      const gchar *trans_id);
static gboolean gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
						       GdaServerConnection *cnc,
						       const gchar *trans_id);
static gboolean gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
							 GdaServerConnection *cnc,
							 const gchar *trans_id);
static gboolean gda_mysql_provider_supports (GdaServerProvider *provider,
					     GdaServerConnection *cnc,
					     GNOME_Database_Feature feature);
static GdaServerRecordset *gda_mysql_provider_get_schema (GdaServerProvider *provider,
							  GdaServerConnection *cnc,
							  GNOME_Database_Connection_Schema schema,
							  GdaParameterList *params);

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
	provider_class->open_connection = gda_mysql_provider_open_connection;
	provider_class->close_connection = gda_mysql_provider_close_connection;
	provider_class->execute_command = gda_mysql_provider_execute_command;
	provider_class->begin_transaction = gda_mysql_provider_begin_transaction;
	provider_class->commit_transaction = gda_mysql_provider_commit_transaction;
	provider_class->rollback_transaction = gda_mysql_provider_rollback_transaction;
	provider_class->supports = gda_mysql_provider_supports;
	provider_class->get_schema = gda_mysql_provider_get_schema;
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
		type = g_type_register_static (PARENT_TYPE,
					       "GdaMysqlProvider",
					       &info, 0);
	}

	return type;
}

/* open_connection handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_open_connection (GdaServerProvider *provider,
				    GdaServerConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
	gchar *t_host = NULL;
        gchar *t_db = NULL;
        gchar *t_user = NULL;
        gchar *t_password = NULL;
        gchar *t_port = NULL;
        gchar *t_unix_socket = NULL;
        gchar *t_flags = NULL;
	MYSQL *mysql;
#if MYSQL_VERSION_ID < 32200
        gint err;
#endif
	GdaError *error;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_host = gda_quark_list_find (params, "HOST");
	t_db = gda_quark_list_find (params, "DATABASE");
	t_user = gda_quark_list_find (params, "USERNAME");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_port = gda_quark_list_find (params, "PORT");
	t_unix_socket = gda_quark_list_find (params, "UNIX_SOCKET");
	t_flags = gda_quark_list_find (params, "FLAGS");

	if (username)
		t_user = username;
	if (password)
		t_password = password;

	/* we can't have both a host/pair AND a unix_socket */
	if ((t_host || t_port) && t_unix_socket) {
		gda_server_connection_add_error_string (
			cnc, _("You cannot provide a UNIX_SOCKET if you also provide"
			       " either a HOST or a PORT."));
		return FALSE;
	}

	/* provide the default of localhost:3306 if neither is provided */
	if (!t_unix_socket) {
		if (!t_port)
			t_port = "3306";
		if (!t_host)
			t_host = "localhost";
	}

	mysql = g_new0 (MYSQL, 1);
	mysql = mysql_real_connect (mysql, t_host, t_user, t_password,
#if MYSQL_VERSION_ID >= 32200
				    t_db,
#endif
				    t_port ? atoi (t_port) : 0,
				    t_unix_socket,
				    t_flags ? atoi (t_flags) : 0);
	if (!mysql) {
		error = gda_mysql_make_error (mysql);
		gda_server_connection_add_error (cnc, error);

		return FALSE;
	}
#if MYSQL_VERSION_ID < 32200
	err = mysql_select_db (mysql, t_db);
	if (err != 0) {
		error = gda_mysql_make_error (mysql);
		mysql_close (mysql);
		gda_server_connection_add_error (cnc, error);

		return FALSE;
	}
#endif

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE, mysql);

	return TRUE;
}

/* close_connection handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	MYSQL *mysql;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql)
		return FALSE;

	mysql_close (mysql);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE, NULL);

	return TRUE;
}

static GList *
process_sql_commands (GList *reclist, GdaServerConnection *cnc, const gchar *sql)
{
	MYSQL *mysql;
	gchar **arr;

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_server_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = g_strsplit (sql, ";", 0);
	if (arr) {
		gint n = 0;

		while (arr[n]) {
			gint rc;
			MYSQL_RES *mysql_res;
			GdaServerRecordset *recset;

			rc = mysql_real_query (mysql, arr[n], strlen (arr[n]));
			if (rc != 0) {
				gda_server_connection_add_error (
					cnc, gda_mysql_make_error (mysql));
				break;
			}

			mysql_res = mysql_store_result (mysql);
			recset = gda_mysql_recordset_new (cnc, mysql_res);
			if (GDA_IS_SERVER_RECORDSET (recset))
				reclist = g_list_append (reclist, recset);

			n++;
		}

		g_strfreev (arr);
	}

	return reclist;
}

/* execute_command handler for the GdaMysqlProvider class */
static GList *
gda_mysql_provider_execute_command (GdaServerProvider *provider,
				    GdaServerConnection *cnc,
				    GdaCommand *cmd,
				    GdaParameterList *params)
{
	GList *reclist = NULL;
	gchar *str;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), NULL);
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

/* begin_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
				      GdaServerConnection *cnc,
				      const gchar *trans_id)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_server_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	rc = mysql_real_query (mysql, "BEGIN", strlen ("BEGIN"));
	if (rc != 0) {
		gda_server_connection_add_error (
			cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* commit_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
				       GdaServerConnection *cnc,
				       const gchar *trans_id)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_server_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	rc = mysql_real_query (mysql, "COMMIT", strlen ("COMMIT"));
	if (rc != 0) {
		gda_server_connection_add_error (
			cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* rollback_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
					 GdaServerConnection *cnc,
					 const gchar *trans_id)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_server_connection_add_error_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	rc = mysql_real_query (mysql, "ROLLBACK", strlen ("ROLLBACK"));
	if (rc != 0) {
		gda_server_connection_add_error (
			cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

/* supports handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_supports (GdaServerProvider *provider,
			     GdaServerConnection *cnc,
			     GNOME_Database_Feature feature)
{
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);

	switch (feature) {
	case GNOME_Database_FEATURE_TRANSACTIONS :
		return TRUE;
	default :
	}

	return FALSE;
}

static GdaServerRecordset *
get_mysql_tables (GdaServerConnection *cnc, GdaParameterList *params)
{
	GList *reclist;
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	reclist = process_sql_commands (NULL, cnc, "show tables");
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
get_mysql_types (GdaServerConnection *cnc, GdaParameterList *params)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = gda_server_recordset_model_new (cnc, 1);
	gda_server_recordset_model_set_field_defined_size (recset, 0, 32);
	gda_server_recordset_model_set_field_name (recset, 0, _("Type"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	/* fill the recordset */
	add_string_row (recset, "blob");
	add_string_row (recset, "date");
	add_string_row (recset, "datetime");
	add_string_row (recset, "decimal");
	add_string_row (recset, "double");
	add_string_row (recset, "enum");
	add_string_row (recset, "float");
	add_string_row (recset, "int24");
	add_string_row (recset, "long");
	add_string_row (recset, "longlong");
	add_string_row (recset, "set");
	add_string_row (recset, "short");
	add_string_row (recset, "string");
	add_string_row (recset, "time");
	add_string_row (recset, "timestamp");
	add_string_row (recset, "tiny");
	add_string_row (recset, "year");

	return recset;
}

/* get_schema handler for the GdaMysqlProvider class */
static GdaServerRecordset *
gda_mysql_provider_get_schema (GdaServerProvider *provider,
			       GdaServerConnection *cnc,
			       GNOME_Database_Connection_Schema schema,
			       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	switch (schema) {
	case GNOME_Database_Connection_SCHEMA_PROCEDURES :
		break; /* FIXME */
	case GNOME_Database_Connection_SCHEMA_TABLES :
		return get_mysql_tables (cnc, params);
	case GNOME_Database_Connection_SCHEMA_TYPES :
		return get_mysql_types (cnc, params);
	case GNOME_Database_Connection_SCHEMA_VIEWS :
		break; /* FIXME */
	}

	return NULL;
}
