/* GDA MySQL provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
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
#include <stdlib.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"

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
	const gchar *t_host = NULL;
        const gchar *t_db = NULL;
        const gchar *t_user = NULL;
        const gchar *t_password = NULL;
        const gchar *t_port = NULL;
        const gchar *t_unix_socket = NULL;
        const gchar *t_flags = NULL;
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
	default:
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
	case GNOME_Database_FEATURE_SQL :
	case GNOME_Database_FEATURE_TRANSACTIONS :
		return TRUE;
	default :
	}

	return FALSE;
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
get_mysql_aggregates (GdaServerConnection *cnc, GdaParameterList *params)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaServerRecordsetModel *) gda_server_recordset_model_new (cnc, 1);
	gda_server_recordset_model_set_field_defined_size (recset, 0, 32);
	gda_server_recordset_model_set_field_name (recset, 0, _("Name"));
	gda_server_recordset_model_set_field_scale (recset, 0, 0);
	gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING);

	/* fill the recordset */
	add_string_row (recset, "abs");
	add_string_row (recset, "acos");
	add_string_row (recset, "adddate");
	add_string_row (recset, "ascii");
	add_string_row (recset, "asin");
	add_string_row (recset, "atan");
	add_string_row (recset, "atan2");
	add_string_row (recset, "benchmark");
	add_string_row (recset, "bin");
	add_string_row (recset, "bit_count");
	add_string_row (recset, "ceiling");
	add_string_row (recset, "char");
	add_string_row (recset, "char_length");
	add_string_row (recset, "character_length");
	add_string_row (recset, "coalesce");
	add_string_row (recset, "concat");
	add_string_row (recset, "concat_ws");
	add_string_row (recset, "connection_id");
	add_string_row (recset, "conv");
	add_string_row (recset, "cos");
	add_string_row (recset, "cot");
	add_string_row (recset, "count");
	add_string_row (recset, "curdate");
	add_string_row (recset, "current_date");
	add_string_row (recset, "current_time");
	add_string_row (recset, "current_timestamp");
	add_string_row (recset, "curtime");
	add_string_row (recset, "database");
	add_string_row (recset, "date_add");
	add_string_row (recset, "date_format");
	add_string_row (recset, "date_sub");
	add_string_row (recset, "dayname");
	add_string_row (recset, "dayofmonth");
	add_string_row (recset, "dayofweek");
	add_string_row (recset, "dayofyear");
	add_string_row (recset, "decode");
	add_string_row (recset, "degrees");
	add_string_row (recset, "elt");
	add_string_row (recset, "encode");
	add_string_row (recset, "encrypt");
	add_string_row (recset, "exp");
	add_string_row (recset, "export_set");
	add_string_row (recset, "extract");
	add_string_row (recset, "field");
	add_string_row (recset, "find_in_set");
	add_string_row (recset, "floor");
	add_string_row (recset, "format");
	add_string_row (recset, "from_days");
	add_string_row (recset, "from_unixtime");
	add_string_row (recset, "get_lock");
	add_string_row (recset, "greatest");
	add_string_row (recset, "hex");
	add_string_row (recset, "hour");
	add_string_row (recset, "if");
	add_string_row (recset, "ifnull");
	add_string_row (recset, "inet_aton");
	add_string_row (recset, "inet_ntoa");
	add_string_row (recset, "insert");
	add_string_row (recset, "instr");
	add_string_row (recset, "interval");
	add_string_row (recset, "isnull");
	add_string_row (recset, "last_insert_id");
	add_string_row (recset, "lcase");
	add_string_row (recset, "least");
	add_string_row (recset, "left");
	add_string_row (recset, "length");
	add_string_row (recset, "load_file");
	add_string_row (recset, "locate");
	add_string_row (recset, "log");
	add_string_row (recset, "log10");
	add_string_row (recset, "lower");
	add_string_row (recset, "lpad");
	add_string_row (recset, "ltrim");
	add_string_row (recset, "make_set");
	add_string_row (recset, "master_pos_wait");
	add_string_row (recset, "match");
	add_string_row (recset, "max");
	add_string_row (recset, "md5");
	add_string_row (recset, "mid");
	add_string_row (recset, "min");
	add_string_row (recset, "minute");
	add_string_row (recset, "mod");
	add_string_row (recset, "month");
	add_string_row (recset, "monthname");
	add_string_row (recset, "now");
	add_string_row (recset, "nullif");
	add_string_row (recset, "oct");
	add_string_row (recset, "octet_length");
	add_string_row (recset, "ord");
	add_string_row (recset, "password");
	add_string_row (recset, "period_add");
	add_string_row (recset, "period_diff");
	add_string_row (recset, "pi");
	add_string_row (recset, "position");	
	add_string_row (recset, "pow");
	add_string_row (recset, "power");
	add_string_row (recset, "quarter");
	add_string_row (recset, "radians");
	add_string_row (recset, "rand");
	add_string_row (recset, "release_lock");
	add_string_row (recset, "repeat");
	add_string_row (recset, "replace");	
	add_string_row (recset, "reverse");
	add_string_row (recset, "right");
	add_string_row (recset, "round");
	add_string_row (recset, "rpad");
	add_string_row (recset, "rtrim");
	add_string_row (recset, "second");
	add_string_row (recset, "sec_to_time");
	add_string_row (recset, "session_user");
	add_string_row (recset, "sign");
	add_string_row (recset, "sin");
	add_string_row (recset, "soundex");
	add_string_row (recset, "space");
	add_string_row (recset, "sqrt");
	add_string_row (recset, "strcmp");
	add_string_row (recset, "subdate");
	add_string_row (recset, "substring");
	add_string_row (recset, "substring_index");
	add_string_row (recset, "sysdate");
	add_string_row (recset, "system_user");
	add_string_row (recset, "tan");
	add_string_row (recset, "time_format");
	add_string_row (recset, "time_to_sec");
	add_string_row (recset, "to_days");
	add_string_row (recset, "trim");
	add_string_row (recset, "truncate");
	add_string_row (recset, "ucase");
	add_string_row (recset, "unix_timestamp");
	add_string_row (recset, "upper");
	add_string_row (recset, "user");
	add_string_row (recset, "version");
	add_string_row (recset, "week");
	add_string_row (recset, "weekday");
	add_string_row (recset, "year");
	add_string_row (recset, "yearweek");

	return GDA_SERVER_RECORDSET (recset);
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

static GdaServerRecordset *
get_mysql_types (GdaServerConnection *cnc, GdaParameterList *params)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaServerRecordsetModel *) gda_server_recordset_model_new (cnc, 1);
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

	return GDA_SERVER_RECORDSET (recset);
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
	case GNOME_Database_Connection_SCHEMA_TABLES :
		return get_mysql_tables (cnc, params);
	case GNOME_Database_Connection_SCHEMA_TYPES :
		return get_mysql_types (cnc, params);
	case GNOME_Database_Connection_SCHEMA_AGGREGATES :
		return get_mysql_aggregates (cnc, params);
	}

	return NULL;
}
