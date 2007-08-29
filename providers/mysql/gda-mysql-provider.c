
/* GDA MySQL provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-column-index.h>
#include <libgda/gda-util.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-parameter-list.h>
#include <libgda/gda-server-provider-extra.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"
#include "gda-mysql-ddl.h"

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-bin.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>

#include <libgda/sql-delimiter/gda-sql-delimiter.h>
#include <libgda/gda-connection-private.h>
#include <libgda/binreloc/gda-binreloc.h>

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

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

static gboolean gda_mysql_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							  GdaServerOperationType type, GdaParameterList *options);
static GdaServerOperation *gda_mysql_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
								   GdaServerOperationType type, 
								   GdaParameterList *options, GError **error);
static gchar *gda_mysql_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
						      GdaServerOperation *op, GError **error);

static gboolean gda_mysql_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
						      GdaServerOperation *op, GError **error);

static GList *gda_mysql_provider_execute_command (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);

static gchar *gda_mysql_provider_get_last_insert_id (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GdaDataModel *recset);

static gboolean gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      const gchar *name, GdaTransactionIsolation level,
						      GError **error);

static gboolean gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       const gchar *name, GError **error);

static gboolean gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 const gchar *name, GError **error);

static gboolean gda_mysql_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);

static GdaServerProviderInfo *gda_mysql_provider_get_info (GdaServerProvider *provider,
							      GdaConnection *cnc);

static GdaDataModel *gda_mysql_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);
static GdaDataHandler *gda_mysql_provider_get_data_handler (GdaServerProvider *provider,
						     GdaConnection *cnc,
						     GType g_type,
						     const gchar *dbms_type);
static const gchar* gda_mysql_provider_get_default_dbms_type (GdaServerProvider *provider,
							      GdaConnection *cnc,
							      GType type);


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
	provider_class->get_server_version = gda_mysql_provider_get_server_version;
	provider_class->get_info = gda_mysql_provider_get_info;
	provider_class->supports_feature = gda_mysql_provider_supports;
	provider_class->get_schema = gda_mysql_provider_get_schema;

	provider_class->get_data_handler = gda_mysql_provider_get_data_handler;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = gda_mysql_provider_get_default_dbms_type;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_mysql_provider_open_connection;
	provider_class->close_connection = gda_mysql_provider_close_connection;
	provider_class->get_database = gda_mysql_provider_get_database;
	provider_class->change_database = gda_mysql_provider_change_database;

	provider_class->supports_operation = gda_mysql_provider_supports_operation;
	provider_class->create_operation = gda_mysql_provider_create_operation;
	provider_class->render_operation = gda_mysql_provider_render_operation;
	provider_class->perform_operation = gda_mysql_provider_perform_operation;

	provider_class->execute_command = gda_mysql_provider_execute_command;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = gda_mysql_provider_get_last_insert_id;

	provider_class->begin_transaction = gda_mysql_provider_begin_transaction;
	provider_class->commit_transaction = gda_mysql_provider_commit_transaction;
	provider_class->rollback_transaction = gda_mysql_provider_rollback_transaction;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
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

	if (G_UNLIKELY (type == 0)) {
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

	return GDA_PACKAGE_VERSION;
}

/* generic function to open a MYSQL connection */
static MYSQL *
real_open_connection (const gchar *host, gint port, const gchar *socket,
		      const gchar *db,
		      const gchar *login, const gchar *password, 
		      gboolean usessl, GError **error)
{
	MYSQL *mysql;
	MYSQL *mysql_ret;
	unsigned int mysqlflags = 0;
#if MYSQL_VERSION_ID < 32200
        gint err;
#endif
	
	/* we can't have both a host/pair AND a unix_socket */
        if ((host || (port >= 0)) && socket) {
                g_set_error (error, 0, 0,
			     _("You cannot provide a UNIX SOCKET if you also provide"
			       " either a HOST or a PORT."));
                return NULL;
        }

	/* provide the default of localhost:3306 if neither is provided */
	if (!socket) {
		if (!host)
			host = "localhost";
		else if (port <= 0)
			port = 3306;
	}

	if (usessl)
		 mysqlflags |= CLIENT_SSL;

	mysql = g_new0 (MYSQL, 1);
        mysql_init (mysql);
        mysql_ret = mysql_real_connect (mysql, host, login, password,
#if MYSQL_VERSION_ID >= 32200
					db,
#endif
					port > 0? port : 0,
					socket,
					mysqlflags);
	
	
	if (!mysql_ret) {
		g_set_error (error, 0, 0, mysql_error (mysql));
		g_free (mysql);
		return NULL;
	}
	
#if MYSQL_VERSION_ID < 32200
	err = mysql_select_db (mysql, db);
	if (err != 0) {
		g_set_error (error, 0, 0, mysql_error (mysql));
		mysql_close (mysql);
		
		return NULL;
	}
#endif

#if MYSQL_VERSION_ID >= 50007
	if (mysql_set_character_set (mysql, "utf8"))
		g_warning ("Could not set client character set to UTF8 (using %s), expect problems with non UTF-8 characters\n", 
			   mysql_character_set_name (mysql));
#endif
	
	return mysql;
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

	MYSQL *mysql;
	GdaConnectionEvent *error;
	GError *gerror = NULL;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_host = gda_quark_list_find (params, "HOST");
	t_db = gda_quark_list_find (params, "DB_NAME");
	if (!t_db) {
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
			t_db = str;
		}
	}

	t_user = gda_quark_list_find (params, "USER");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_port = gda_quark_list_find (params, "PORT");
	t_unix_socket = gda_quark_list_find (params, "UNIX_SOCKET");
	t_use_ssl = gda_quark_list_find (params, "USE_SSL");

	if (username && *username != '\0')
		t_user = username;
	if (password && *password != '\0')
		t_password = password;

	mysql = real_open_connection (t_host, t_port ? atoi (t_port) : 0, t_unix_socket,
				      t_db, t_user, t_password, t_use_ssl ? TRUE : FALSE, &gerror);
	if (!mysql) {
		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (error, gerror && gerror->message ? 
					   gerror->message : "NO DESCRIPTION");
                gda_connection_event_set_code (error, gerror ? gerror->code : -1);
		if (gerror)
			g_error_free (gerror);
		gda_connection_add_event (cnc, error);

		return FALSE;
	}
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
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;
	
	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MySQL handle"));
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
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	options = gda_connection_get_options (cnc);

	/* parse SQL string, which can contain several commands, separated by ';' */
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		gint n = 0;
		gboolean allok = TRUE;

		while (arr[n] && allok) {
			gint rc;
			MYSQL_RES *mysql_res;
			GdaMysqlRecordset *recset;
			gchar *tststr;
			GdaConnectionEvent *error = NULL;

			/* if the connection is in read-only mode, just allow SELECT,
			   SHOW commands */
			if (options & GDA_CONNECTION_OPTIONS_READ_ONLY) {
				gchar *s;

				/* FIXME: maybe there's a better way of doing this? */
				s = g_strstrip (g_strdup (arr[n]));
				if (g_ascii_strncasecmp (s, "select", strlen ("select")) &&
				    g_ascii_strncasecmp (s, "show", strlen ("show"))) {
					gda_connection_add_event_string (
						cnc, "Command '%s' cannot be executed in read-only mode", arr[n]);
					reclist = g_list_append (reclist, NULL);
					allok = FALSE;
					break;
				}

				g_free (s);
			}

			/* execute the command on the server */
			rc = gda_mysql_real_query_wrap (cnc, mysql, arr[n], strlen (arr[n]));
			if (rc != 0) {
				error = gda_mysql_make_error (mysql);
				gda_connection_add_event (cnc, error);
				reclist = g_list_append (reclist, NULL);
				allok = FALSE;
			}
			else {
				/* command was executed OK */
				g_strchug (arr[n]);
				tststr = arr[n];
				if (! g_ascii_strncasecmp (tststr, "SELECT", 6) ||
				    ! g_ascii_strncasecmp (tststr, "SHOW", 4) ||
				    ! g_ascii_strncasecmp (tststr, "DESCRIBE", 6) ||
				    ! g_ascii_strncasecmp (tststr, "EXPLAIN", 7)) {
					mysql_res = mysql_store_result (mysql);
					recset = gda_mysql_recordset_new (cnc, mysql_res, mysql);
					if (GDA_IS_MYSQL_RECORDSET (recset)) {
						g_object_set (G_OBJECT (recset), 
							      "command_text", arr[n],
							      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
						reclist = g_list_append (reclist, recset);
					}
					else
						reclist = g_list_append (reclist, NULL);
				}
				else {
					int changes;
					GdaConnectionEvent *event;
					gchar *str, *tmp, *ptr;
					
					changes = mysql_affected_rows (mysql);
					reclist = g_list_append (reclist, 
								 gda_parameter_list_new_inline (NULL, 
												"IMPACTED_ROWS", G_TYPE_INT, changes,
												NULL));
					
					/* generate a notice about changes */
					event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
					ptr = tststr;
					while (*ptr && (*ptr != ' ') && (*ptr != '\t') &&
					       (*ptr != '\n'))
						ptr++;
					*ptr = 0;
					tmp = g_ascii_strup (tststr, -1);
					if (!strcmp (tmp, "INSERT")) {
						if (mysql_insert_id (mysql) != 0)
							str = g_strdup_printf ("%s %lld %d", tmp, 
									       mysql_insert_id (mysql),
									       changes);
						else
							str = g_strdup_printf ("%s NOID %d", tmp, changes);
					}
					else
						str = g_strdup_printf ("%s %d", tmp, changes);
					gda_connection_event_set_description (event, str);
					g_free (str);
					g_free (tmp);
					gda_connection_add_event (cnc, event);
				}
			}

			gda_connection_internal_treat_sql (cnc, arr[n], error);
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
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
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
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	rc = mysql_select_db (mysql, name);
	if (rc != 0) {
		gda_connection_add_event (cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	return TRUE;
}

static gboolean
gda_mysql_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
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
gda_mysql_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				     GdaServerOperationType type, 
				     GdaParameterList *options, GError **error)
{
	gchar *file;
	GdaServerOperation *op;
	gchar *str;
	gchar *dir;
	
	file = g_utf8_strdown (gda_server_operation_op_type_to_string (type), -1);
	str = g_strdup_printf ("mysql_specs_%s.xml", file);
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
gda_mysql_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				     GdaServerOperation *op, GError **error)
{
	gchar *sql = NULL;
	gchar *file;
	gchar *str;
	gchar *dir;

	file = g_utf8_strdown (gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)), -1);
	str = g_strdup_printf ("mysql_specs_%s.xml", file);
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
		sql = gda_mysql_render_CREATE_DB (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_DROP_DB:
		sql = gda_mysql_render_DROP_DB (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_CREATE_TABLE:
		sql = gda_mysql_render_CREATE_TABLE (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_DROP_TABLE:
		sql = gda_mysql_render_DROP_TABLE (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_RENAME_TABLE:
		sql = gda_mysql_render_RENAME_TABLE (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_ADD_COLUMN:
		sql = gda_mysql_render_ADD_COLUMN (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_DROP_COLUMN:
		sql = gda_mysql_render_DROP_COLUMN (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_CREATE_INDEX:
		sql = gda_mysql_render_CREATE_INDEX (provider, cnc, op, error);
		break;
	case GDA_SERVER_OPERATION_DROP_INDEX:
		sql = gda_mysql_render_DROP_INDEX (provider, cnc, op, error);
		break;
	default:
		g_assert_not_reached ();
	}
	return sql;
}

static gboolean
gda_mysql_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error)
{
	GdaServerOperationType optype;

	optype = gda_server_operation_get_op_type (op);
	if (!cnc && 
	    ((optype == GDA_SERVER_OPERATION_CREATE_DB) ||
	     (optype == GDA_SERVER_OPERATION_DROP_DB))) {
		const GValue *value;
		MYSQL *mysql;
		const gchar *login = NULL;
		const gchar *password = NULL;
		const gchar *host = NULL;
		gint         port = -1;
		const gchar *socket = NULL;
		gboolean     usessl = FALSE;

		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/HOST");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			host = g_value_get_string (value);
		
		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/PORT");
		if (value && G_VALUE_HOLDS (value, G_TYPE_INT) && (g_value_get_int (value) > 0))
			port = g_value_get_int (value);

		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/UNIX_SOCKET");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			socket = g_value_get_string (value);

		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/USE_SSL");
		if (value && G_VALUE_HOLDS (value, G_TYPE_BOOLEAN) && g_value_get_boolean (value))
			usessl = TRUE;

		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/ADM_LOGIN");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			login = g_value_get_string (value);

		value = gda_server_operation_get_value_at (op, "/SERVER_CNX_P/ADM_PASSWORD");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
			password = g_value_get_string (value);

		mysql = real_open_connection (host, port, socket,
                                              "mysql", login, password, usessl, error);
                if (!mysql)
                        return FALSE;
		else {
			gchar *sql;
			int res;

			sql = gda_server_provider_render_operation (provider, cnc, op, error);
			if (!sql)
				return FALSE;

			GdaConnectionEvent *event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
			gda_connection_event_set_description (event, sql);
			gda_connection_add_event (cnc, event);

			res = mysql_query (mysql, sql);
			g_free (sql);
			
			if (res) {
				g_set_error (error, 0, 0, mysql_error (mysql));
				mysql_close (mysql);
				return FALSE;
			}
			else {
				mysql_close (mysql);
				return TRUE;
			}
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
		if (reclist && GDA_IS_DATA_MODEL (reclist->data))
			g_object_set (G_OBJECT (reclist->data), 
				      "command_text", gda_command_get_text (cmd),
				      "command_type", GDA_COMMAND_TYPE_TABLE, NULL);

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

	if (recset) {
		g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
		TO_IMPLEMENT;
		return NULL;
	}

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return NULL;
	}

	return g_strdup_printf ("%ld", (unsigned long int ) mysql_insert_id (mysql));
}

/* begin_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_begin_transaction (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name, GdaTransactionIsolation level,
				      GError **error)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;
	GdaConnectionEvent *event = NULL;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	/* set isolation level */
	switch (level) {
	case GDA_TRANSACTION_ISOLATION_READ_COMMITTED :
		rc = gda_mysql_real_query_wrap (cnc, mysql, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", 46);
		break;
	case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED :
		rc = gda_mysql_real_query_wrap (cnc, mysql, "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", 48);
		break;
	case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ :
		rc = gda_mysql_real_query_wrap (cnc, mysql, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", 47);
		break;
	case GDA_TRANSACTION_ISOLATION_SERIALIZABLE :
		rc = gda_mysql_real_query_wrap (cnc, mysql, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", 44);
		break;
	default :
		rc = 0;
	}

	if (rc != 0) {
		event = gda_mysql_make_error (mysql);
		gda_connection_add_event (cnc, event);
	}
	else {
		/* start the transaction */
		rc = gda_mysql_real_query_wrap (cnc, mysql, "BEGIN", 5);
		if (rc != 0) {
			event = gda_mysql_make_error (mysql);
			gda_connection_add_event (cnc, event);
		}
	}
	
	gda_connection_internal_treat_sql (cnc, "BEGIN", event);
	return event ? FALSE : TRUE;
}

/* commit_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_commit_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       const gchar *name, GError **error)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;
	GdaConnectionEvent *event = NULL;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	rc = gda_mysql_real_query_wrap (cnc, mysql, "COMMIT", 6);
	if (rc != 0) {
		event = gda_mysql_make_error (mysql);
		gda_connection_add_event (cnc, event);
	}

	gda_connection_internal_treat_sql (cnc, "COMMIT", event);
	return event ? FALSE : TRUE;
}

/* rollback_transaction handler for the GdaMysqlProvider class */
static gboolean
gda_mysql_provider_rollback_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *name, GError **error)
{
	MYSQL *mysql;
	gint rc;
	GdaMysqlProvider *myprv = (GdaMysqlProvider *) provider;
	GdaConnectionEvent *event = NULL;

	g_return_val_if_fail (GDA_IS_MYSQL_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
		return FALSE;
	}

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	rc = gda_mysql_real_query_wrap (cnc, mysql, "ROLLBACK", 8);
	if (rc != 0) {
		event = gda_mysql_make_error (mysql);
		gda_connection_add_event (cnc, event);
	}

	gda_connection_internal_treat_sql (cnc, "ROLLBACK", event);
	return event ? FALSE : TRUE;
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

static GdaServerProviderInfo *
gda_mysql_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	static GdaServerProviderInfo info = {
		"MySQL",
		TRUE, 
		TRUE,
		TRUE,
		TRUE,
		TRUE,
		FALSE
	};
	
	return &info;
}

static void
add_aggregate_row (GdaDataModelArray *recset, const gchar *str, const gchar *comments)
{
	GList *list;
	GValue *tmpval;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	/* 1st the name */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
	list = g_list_append (NULL, tmpval);
	/* 2nd the unique id */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), str);
	list = g_list_append (list, tmpval);
	/* 3rd the owner */
	list = g_list_append (list, gda_value_new_null ());
	/* 4th the comments */ 
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), comments);
	list = g_list_append (list, tmpval);
	/* 5th the return type */
	list = g_list_append (list, gda_value_new_null ());
	/* 6th the argument type */
	list = g_list_append (list, gda_value_new_null ());
	/* 7th the SQL definition */
	list = g_list_append (list, gda_value_new_null ());

	gda_data_model_append_values (GDA_DATA_MODEL (recset), list, NULL);

	g_list_foreach (list, (GFunc) gda_value_free, NULL);
	g_list_free (list);
}

static GdaDataModel *
get_mysql_aggregates (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_AGGREGATES));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_AGGREGATES);

	/* fill the recordset */
	add_aggregate_row (recset, "abs", "");
	add_aggregate_row (recset, "acos", "");
	add_aggregate_row (recset, "adddate", "");
	add_aggregate_row (recset, "ascii", "");
	add_aggregate_row (recset, "asin", "");
	add_aggregate_row (recset, "atan", "");
	add_aggregate_row (recset, "atan2", "");
	add_aggregate_row (recset, "benchmark", "");
	add_aggregate_row (recset, "bin", "");
	add_aggregate_row (recset, "bit_count", "");
	add_aggregate_row (recset, "ceiling", "");
	add_aggregate_row (recset, "char", "");
	add_aggregate_row (recset, "char_length", "");
	add_aggregate_row (recset, "character_length", "");
	add_aggregate_row (recset, "coalesce", "");
	add_aggregate_row (recset, "concat", "");
	add_aggregate_row (recset, "concat_ws", "");
	add_aggregate_row (recset, "connection_id", "");
	add_aggregate_row (recset, "conv", "");
	add_aggregate_row (recset, "cos", "");
	add_aggregate_row (recset, "cot", "");
	add_aggregate_row (recset, "count", "");
	add_aggregate_row (recset, "curdate", "");
	add_aggregate_row (recset, "current_date", "");
	add_aggregate_row (recset, "current_time", "");
	add_aggregate_row (recset, "current_timestamp", "");
	add_aggregate_row (recset, "curtime", "");
	add_aggregate_row (recset, "database", "");
	add_aggregate_row (recset, "date_add", "");
	add_aggregate_row (recset, "date_format", "");
	add_aggregate_row (recset, "date_sub", "");
	add_aggregate_row (recset, "dayname", "");
	add_aggregate_row (recset, "dayofmonth", "");
	add_aggregate_row (recset, "dayofweek", "");
	add_aggregate_row (recset, "dayofyear", "");
	add_aggregate_row (recset, "decode", "");
	add_aggregate_row (recset, "degrees", "");
	add_aggregate_row (recset, "elt", "");
	add_aggregate_row (recset, "encode", "");
	add_aggregate_row (recset, "encrypt", "");
	add_aggregate_row (recset, "exp", "");
	add_aggregate_row (recset, "export_set", "");
	add_aggregate_row (recset, "extract", "");
	add_aggregate_row (recset, "field", "");
	add_aggregate_row (recset, "find_in_set", "");
	add_aggregate_row (recset, "floor", "");
	add_aggregate_row (recset, "format", "");
	add_aggregate_row (recset, "from_days", "");
	add_aggregate_row (recset, "from_unixtime", "");
	add_aggregate_row (recset, "get_lock", "");
	add_aggregate_row (recset, "greatest", "");
	add_aggregate_row (recset, "hex", "");
	add_aggregate_row (recset, "hour", "");
	add_aggregate_row (recset, "if", "");
	add_aggregate_row (recset, "ifnull", "");
	add_aggregate_row (recset, "inet_aton", "");
	add_aggregate_row (recset, "inet_ntoa", "");
	add_aggregate_row (recset, "insert", "");
	add_aggregate_row (recset, "instr", "");
	add_aggregate_row (recset, "interval", "");
	add_aggregate_row (recset, "isnull", "");
	add_aggregate_row (recset, "last_insert_id", "");
	add_aggregate_row (recset, "lcase", "");
	add_aggregate_row (recset, "least", "");
	add_aggregate_row (recset, "left", "");
	add_aggregate_row (recset, "length", "");
	add_aggregate_row (recset, "load_file", "");
	add_aggregate_row (recset, "locate", "");
	add_aggregate_row (recset, "log", "");
	add_aggregate_row (recset, "log10", "");
	add_aggregate_row (recset, "lower", "");
	add_aggregate_row (recset, "lpad", "");
	add_aggregate_row (recset, "ltrim", "");
	add_aggregate_row (recset, "make_set", "");
	add_aggregate_row (recset, "master_pos_wait", "");
	add_aggregate_row (recset, "match", "");
	add_aggregate_row (recset, "max", "");
	add_aggregate_row (recset, "md5", "");
	add_aggregate_row (recset, "mid", "");
	add_aggregate_row (recset, "min", "");
	add_aggregate_row (recset, "minute", "");
	add_aggregate_row (recset, "mod", "");
	add_aggregate_row (recset, "month", "");
	add_aggregate_row (recset, "monthname", "");
	add_aggregate_row (recset, "now", "");
	add_aggregate_row (recset, "nullif", "");
	add_aggregate_row (recset, "oct", "");
	add_aggregate_row (recset, "octet_length", "");
	add_aggregate_row (recset, "ord", "");
	add_aggregate_row (recset, "password", "");
	add_aggregate_row (recset, "period_add", "");
	add_aggregate_row (recset, "period_diff", "");
	add_aggregate_row (recset, "pi", "");
	add_aggregate_row (recset, "position", "");	
	add_aggregate_row (recset, "pow", "");
	add_aggregate_row (recset, "power", "");
	add_aggregate_row (recset, "quarter", "");
	add_aggregate_row (recset, "radians", "");
	add_aggregate_row (recset, "rand", "");
	add_aggregate_row (recset, "release_lock", "");
	add_aggregate_row (recset, "repeat", "");
	add_aggregate_row (recset, "replace", "");	
	add_aggregate_row (recset, "reverse", "");
	add_aggregate_row (recset, "right", "");
	add_aggregate_row (recset, "round", "");
	add_aggregate_row (recset, "rpad", "");
	add_aggregate_row (recset, "rtrim", "");
	add_aggregate_row (recset, "second", "");
	add_aggregate_row (recset, "sec_to_time", "");
	add_aggregate_row (recset, "session_user", "");
	add_aggregate_row (recset, "sign", "");
	add_aggregate_row (recset, "sin", "");
	add_aggregate_row (recset, "soundex", "");
	add_aggregate_row (recset, "space", "");
	add_aggregate_row (recset, "sqrt", "");
	add_aggregate_row (recset, "strcmp", "");
	add_aggregate_row (recset, "subdate", "");
	add_aggregate_row (recset, "substring", "");
	add_aggregate_row (recset, "substring_index", "");
	add_aggregate_row (recset, "sysdate", "");
	add_aggregate_row (recset, "system_user", "");
	add_aggregate_row (recset, "tan", "");
	add_aggregate_row (recset, "time_format", "");
	add_aggregate_row (recset, "time_to_sec", "");
	add_aggregate_row (recset, "to_days", "");
	add_aggregate_row (recset, "trim", "");
	add_aggregate_row (recset, "truncate", "");
	add_aggregate_row (recset, "ucase", "");
	add_aggregate_row (recset, "unix_timestamp", "");
	add_aggregate_row (recset, "upper", "");
	add_aggregate_row (recset, "user", "");
	add_aggregate_row (recset, "version", "");
	add_aggregate_row (recset, "week", "");
	add_aggregate_row (recset, "weekday", "");
	add_aggregate_row (recset, "year", "");
	add_aggregate_row (recset, "yearweek", "");

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
	GdaParameter *par = NULL;
	const gchar *tablename = NULL;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (params) {
		par = gda_parameter_list_find_param (params, "name");
		if (par)
			tablename = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	}

	reclist = process_sql_commands (NULL, cnc, "show table status");
	if (!reclist)
		return NULL;

	recset = (GdaMysqlRecordset *) reclist->data;
	g_list_free (reclist);
	if (!GDA_IS_MYSQL_RECORDSET (recset))
		return NULL;

	/* add the extra information */
	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_TABLES);

	rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (recset));
	for (r = 0; r < rows; r++) {
		GList *value_list = NULL;
		const gchar *name;
		gchar *str;
		GValue *tmpval;

		if (tablename) {
			const GValue *cvalue;
			
			cvalue = gda_data_model_get_value_at (GDA_DATA_MODEL (recset), 0, r);
			if (strcmp (tablename, g_value_get_string (cvalue)))
				continue;
		}

		/* 1st, the name of the table */
		tmpval = gda_value_copy ((GValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (recset), 0, r));
		name = g_value_get_string (tmpval);
		value_list = g_list_append (value_list, tmpval);

		/* 2nd, the owner */
		value_list = g_list_append (value_list, gda_value_new_null ());

		/* 3rd, the comments */
		tmpval = gda_value_copy ((GValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (recset), 14, r));
		value_list = g_list_append (value_list, tmpval);

		/* 4th, the SQL command */
		str = g_strdup_printf ("SHOW CREATE TABLE %s", name);
		reclist = process_sql_commands (NULL, cnc, str);
		g_free (str);
		if (reclist && GDA_IS_DATA_MODEL (reclist->data)) {
			tmpval = gda_value_copy ((GValue *) gda_data_model_get_value_at (GDA_DATA_MODEL (reclist->data), 1, 0));
			value_list = g_list_append (value_list, tmpval);

			g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
			g_list_free (reclist);
		}
		else
			value_list = g_list_append (value_list, gda_value_new_null ());

		gda_data_model_append_values (model, value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return model;
}

static GdaDataModel *
get_mysql_views (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* add the extra information */
	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_VIEWS));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_VIEWS);

	/* views are not yet supported in MySql => return an empty set */

	return model;
}

static GdaDataModel *
get_mysql_procedures (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *model;
	gint i;

	struct {
		const gchar *name;
		const gchar *id;
		/*const gchar *owner; "" */
		const gchar *comments;
		const gchar *rettype;
		gint         nb_args;
		const gchar *argtypes;
		/*const gchar *definition; NULL */
	} procs[] = {
		{ "ascii", "ascii", "", "smallint", 1, "text"},
		{ "bin", "bin", "", "text", 1, "bigint"},
		{ "bit_length", "bit_length", "", "int", 1, "text"},
		{ "char", "char", "", "text", -1, "-"}
	};


	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* add the extra information */
	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_PROCEDURES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_PROCEDURES);

	/* fill the recordset */
	for (i = 0; i < sizeof (procs) / sizeof (procs[0]); i++) {
		GList *value_list = NULL;
		GValue *tmpval;
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), procs[i].name);
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), procs[i].id);
		value_list = g_list_append (value_list, tmpval);
		
		value_list = g_list_append (value_list, gda_value_new_null ());
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), procs[i].comments);
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), procs[i].rettype);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), procs[i].nb_args);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), procs[i].argtypes);
		value_list = g_list_append (value_list, tmpval);

		value_list = g_list_append (value_list, gda_value_new_null ());

		gda_data_model_append_values (GDA_DATA_MODEL (model), value_list, NULL);

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
		GType type;
		const gchar *synonyms;
	} types[] = {
		{ "bool", "", "Boolean type", G_TYPE_BOOLEAN, "boolean" },
		{ "blob", "", "Binary blob (up to 65535 bytes)", GDA_TYPE_BINARY, NULL },
		{ "bigint", "", "Big integer, range is -9223372036854775808 to 9223372036854775807", G_TYPE_INT64, NULL  },
		{ "bigint unsigned", "", "Big unsigned integer, range is 0 to 18446744073709551615", G_TYPE_UINT64, NULL  },
		{ "char", "", "Char", G_TYPE_STRING, "binary"  },
		{ "date", "", "Date", G_TYPE_DATE, NULL  },
		{ "datetime", "", "Date and time", GDA_TYPE_TIMESTAMP, NULL  },
		{ "decimal", "", "Decimal number", GDA_TYPE_NUMERIC, "dec,numeric,fixed"  },
		{ "double", "", "Double precision number", G_TYPE_DOUBLE, "double precision,real"  },
		{ "double unsigned", "", "Unsigned double precision number", G_TYPE_DOUBLE, "double precision unsigned,real unsigned"  },
		{ "enum", "", "Enumeration: a string object that can have only one value, chosen from the list of values 'value1', 'value2', ..., NULL or the special '' error value. An ENUM column can have a maximum of 65,535 distinct values", G_TYPE_STRING, NULL  },
		{ "float", "", "Floating point number", G_TYPE_FLOAT , NULL },
		{ "float unsigned", "", "Unsigned floating point number", G_TYPE_FLOAT , NULL },
		{ "int", "", "Integer, range is -2147483648 to 2147483647", G_TYPE_INT, "integer"  },
		{ "int unsigned", "", "Unsigned integer, range is 0 to 4294967295", G_TYPE_UINT, "integer unsigned"  },
		{ "long", "", "Long integer", G_TYPE_LONG, NULL  },
		{ "long unsigned", "", "Long unsigned integer", G_TYPE_ULONG, NULL  },
		{ "longblob", "", "Long blob (up to 4Gb)", GDA_TYPE_BINARY, NULL  },
		{ "longtext", "", "Long text (up to 4Gb characters)", GDA_TYPE_BINARY, NULL  },
		{ "mediumint", "", "Medium integer, range is -8388608 to 8388607", G_TYPE_INT, NULL  },
		{ "mediumint unsigned", "", "Medium unsigned integer, range is 0 to 16777215", G_TYPE_INT, NULL  },
		{ "mediumblob", "", "Medium blob (up to 16777215 bytes)", GDA_TYPE_BINARY, NULL  },
		{ "mediumtext", "", "Medium text (up to 16777215 characters)", GDA_TYPE_BINARY, NULL  },				
		{ "set", "", "Set: a string object that can have zero or more values, each of which must be chosen from the list of values 'value1', 'value2', ... A SET column can have a maximum of 64 members", G_TYPE_STRING, NULL  },
		{ "smallint", "", "Small integer, range is -32768 to 32767", GDA_TYPE_SHORT, NULL  },
		{ "smallint unsigned", "", "Small unsigned integer, range is 0 to 65535", GDA_TYPE_USHORT, NULL  },
		{ "text", "", "Text (up to 65535 characters)", GDA_TYPE_BINARY, NULL  },
		{ "tinyint", "", "Tiny integer, range is -128 to 127", G_TYPE_CHAR, "bit" },
		{ "tinyint unsigned", "", "Tiny unsigned integer, range is 0 to 255", G_TYPE_UCHAR, NULL  },
		{ "tinyblob", "", "Tiny blob (up to 255 bytes)", GDA_TYPE_BINARY, NULL  },
		{ "tinytext", "", "Tiny text (up to 255 characters)", GDA_TYPE_BINARY, NULL  },		
		{ "time", "", "Time", GDA_TYPE_TIME, NULL  },
		{ "timestamp", "", "Time stamp", GDA_TYPE_TIMESTAMP, NULL  },
		{ "varchar", "", "Variable Length Char", G_TYPE_STRING, "varbinary"  },
		{ "year", "", "Year", G_TYPE_INT, NULL  }
	};

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES);
	
	/* fill the recordset */
	for (i = 0; i < sizeof (types) / sizeof (types[0]); i++) {
		GList *value_list = NULL;
		GValue *tmpval;
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].name);		
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].owner);		
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].comments);		
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), types[i].type);		
		value_list = g_list_append (value_list, tmpval);
		
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].synonyms);		
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	return GDA_DATA_MODEL (recset);
}

static GList *
field_row_to_value_list (MYSQL *mysql, MYSQL_ROW mysql_row)
{
	gchar **arr;
	GList *value_list = NULL;
	GValue *tmpval;
	
	g_return_val_if_fail (mysql_row != NULL, NULL);

	/* field name */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mysql_row[0]);
	value_list = g_list_append (value_list, tmpval);

	/* type and size */
	arr = g_strsplit_set ((const gchar *) mysql_row[1], "() ", 0);
	if (!arr) {
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
		value_list = g_list_append (value_list, tmpval);

		g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), -1);
		value_list = g_list_append (value_list, tmpval);
	}
	else {
		if (!strcmp (arr[0], "enum") ||
		    !strcmp (arr[0], "set")) {
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), arr[0]);
			value_list = g_list_append (value_list, tmpval);
			
			g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), -1);
			value_list = g_list_append (value_list, tmpval);
		}
		else {
			if (arr[0] && arr[1]) {
				gint i, len;
				len = g_strv_length (arr);
				for (i = 2; (i < len) && strcmp (arr[i], "unsigned"); i++);
				if (i < len) {
					gchar *type;

					type = g_strdup_printf ("%s unsigned", arr[0]);
					g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), type);
				}
				else
					g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), arr[0]);
				value_list = g_list_append (value_list, tmpval);

				/* if arr[0] is an int type, hard code its size instead of
				 * using arr[1] which is an indication of display padding */
				if (!strcmp (arr[0], "int"))
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 4);
				else if (!strcmp (arr[0], "tinyint"))
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 1);
				else if (!strcmp (arr[0], "smallint"))
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 2);
				else if (!strcmp (arr[0], "mediumint"))
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 3);
				else if (!strcmp (arr[0], "bigint"))
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 8);
				else
					g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), atoi (arr[1]));

				value_list = g_list_append (value_list, tmpval);
			}
			else {
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), arr[0]);
				value_list = g_list_append (value_list, tmpval);
				
				g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), -1);
				value_list = g_list_append (value_list, tmpval);
			}
		}

		g_strfreev (arr);
	}

	/* scale */
	g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 0);
	value_list = g_list_append (value_list, tmpval);

	/* Not null? */
	if (mysql_row[2] && !strcmp (mysql_row[2], "YES"))
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	else
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), TRUE);
	value_list = g_list_append (value_list, tmpval);

	/* Primary key? */
	if (mysql_row[3] && !strcmp (mysql_row[3], "PRI"))
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), TRUE);
	else
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append (value_list, tmpval);

	/* Unique index? */
	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append (value_list, tmpval);

	/* references */
	if (atoi (mysql->server_version) < 5) {
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
		value_list = g_list_append (value_list, tmpval);
	}
	else {
		if (mysql_row[6] && mysql_row[7]) {
			gchar *str = g_strdup_printf ("%s.%s", mysql_row[6], mysql_row[7]);
			g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), str);
			value_list = g_list_append (value_list, tmpval);
		}
		else {
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
			value_list = g_list_append (value_list, tmpval);
		}
	}

	/* default value */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mysql_row[4]);
	value_list = g_list_append (value_list, tmpval);

	/* Extra column */
	if (!strcmp (mysql_row[5], "auto_increment"))
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "AUTO_INCREMENT");
	else
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
	value_list = g_list_append (value_list, tmpval);

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

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* get parameters sent by client */
	par = gda_parameter_list_find_param (params, "name");
	if (!par) {
		gda_connection_add_event_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	table_name = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_event_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	/* execute command on server */	
	if (atoi (mysql->server_version) < 5)
		cmd_str = g_strdup_printf ("SHOW COLUMNS FROM %s", table_name);
	else {
		/* with MySQL >= 5.0 there are foreign keys: add support for those */
		cmd_str = g_strdup_printf ("SELECT c.COLUMN_NAME, c.COLUMN_TYPE, c.IS_NULLABLE, c.COLUMN_KEY, "
					   "c.COLUMN_DEFAULT, c.EXTRA, "
					   "u.REFERENCED_TABLE_NAME, u.REFERENCED_COLUMN_NAME "
					   "FROM INFORMATION_SCHEMA.COLUMNS c "
					   "LEFT OUTER JOIN "
					   "(SELECT sub.TABLE_SCHEMA, sub.COLUMN_NAME, sub.TABLE_NAME, "
					   "sub.REFERENCED_TABLE_NAME, sub.REFERENCED_COLUMN_NAME "
					   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE sub where sub.TABLE_NAME='%s' "
					   "AND sub.REFERENCED_TABLE_NAME IS NOT NULL) u "
					   "ON (c.TABLE_NAME = u.TABLE_NAME AND c.TABLE_SCHEMA = u.TABLE_SCHEMA "
					   "AND c.COLUMN_NAME = u.COLUMN_NAME) "
					   "WHERE c.TABLE_NAME = '%s' AND c.TABLE_SCHEMA = DATABASE() "
					   "ORDER BY c.ORDINAL_POSITION",
					   table_name, table_name);
	}

	rc = gda_mysql_real_query_wrap (cnc, mysql, cmd_str, strlen (cmd_str));
	g_free (cmd_str);
	if (rc != 0) {
		gda_connection_add_event (cnc, gda_mysql_make_error (mysql));
		return NULL;
	}

	mysql_res = mysql_store_result (mysql);
	rows = mysql_num_rows (mysql_res);

	/* fill in the recordset to be returned */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_FIELDS));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_FIELDS);
	
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

		value_list = field_row_to_value_list (mysql, mysql_row);
		if (!value_list) {
			mysql_free_result (mysql_res);
			g_object_unref (G_OBJECT (recset));

			return NULL;
		}

		gda_data_model_append_values (GDA_DATA_MODEL (recset),
					      (const GList *) value_list, NULL);

		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	mysql_free_result (mysql_res);
	
	return GDA_DATA_MODEL (recset);
}

static void
mysql_get_constraints_form_create_table_line (GdaDataModelArray *recset, gchar *pos)
{
	/* try to parse a line like this:
	 * "  CONSTRAINT `name` FOREIGN KEY (`fid1`,`fid2`) REFERENCES othertable(`id1`,`id2`)"
	 */
	gchar *cname = 0, *start, *stop;
	GString *kname;
	gchar *tname = 0;
	GString *fname;
	GList *value_list;
	gchar *temp;
	GValue *tmpval;

	/* remove prefix spaces */
	while ( *pos==' ')
		pos++;

	/* "CONSTRAINT "? */
	if ( strncmp (pos, "CONSTRAINT ", 11) )
		return;

	pos += 11; /* strlen("CONSTRAINT ") */ 

	/* find the start of the constraint name.. it should be a ` char */
	if ( ! ( start = strchr (pos, '`') ) )
		return;
	/* we want the first character from the name */
	start++;

	/* find the ending constraint name.. it should be a ` char */
	if ( ! ( stop = strchr (start, '`') ) )
		return;

	/* allocate cname, and store the constraint name */
	cname = g_malloc ( stop - start  + 1);
	g_strlcpy (cname, start, stop - start + 1);

	pos = stop + 1;

	/* remove spaces again */
	while ( *pos == ' ' )
		pos++;

	/* "FOREIGN KEY "? */
	if ( strncmp (pos, "FOREIGN KEY ", 12) )
		goto cname_out;
	/* this is a foreign key constraint */
	pos += 12; /* strlen("FOREIGN KEY ") */

	/* we should be able to locate a ( inside our string */
	if ( ! ( pos = strchr (pos, '(') ) )
		goto cname_out;
	pos++;

	/* we should now be at the character behind the ( */

	kname = g_string_new (NULL);

	while (1) {
		/* find all the entries.. they are in this format "`a`, `b`, `c`".
		 * The list is terminated by a ) character */
		if ( ! ( start = strchr (pos, '`') ) )
			goto kname_out;
		start++;
		if ( ! ( stop = strchr (start, '`') ) )
			goto kname_out;
		if ( kname->len )
			g_string_append_c (kname, ',');
		g_string_append_len (kname, start, stop - start);

		pos = stop + 1;

		while ( *pos == ' ')
			pos++;
		if ( *pos == ',' )
			pos++;
		else /* if (*pos==')')*/
			break;
	}
	/* pass by the ) character.. but if we had an unexpected \0 character, don't crash */
	if (*pos)
		pos++;

	/* remove spaces again */
	while ( *pos == ' ' )
		pos++;

	/* we now expected the REFERENCES text */
	if ( strncmp (pos, "REFERENCES ", 11) )
		goto kname_out;

	pos += 11;

	/* and yeah... remove spaces */
	while ( *pos == ' ' )
		pos++;

	/* we should now hit the name of the foreign table. It is embedded inside ` characters */
	if ( ! ( start = strchr (pos, '`') ) )
		goto kname_out;
	start++;

	if ( ! ( stop = strchr (start, '`') ) )
		goto kname_out;
	/* store it in tname */
	tname = g_malloc ( stop - start + 1);
	g_strlcpy (tname, start, stop - start + 1);

	pos = stop + 1;

	/* we now should find a ( character */
	if ( ! ( start = strchr (pos, '(') ) )
		goto tname_out;
	pos = start + 1;

	fname = g_string_new (NULL);

	while (1) {
		/* and after this character we find a list like above with keys */
		if ( ! ( start = strchr (pos, '`') ) )
			goto kname_out;
		start++;
		if ( ! ( stop = strchr (start, '`') ) )
			goto kname_out;
		if (fname->len)
			g_string_append_c (fname, ',');
		g_string_append_len (fname, start, stop - start);

		pos = stop + 1;
		while ( *pos == ' ')
			pos++;
		if ( *pos == ',' )
			pos++;
		else /* if (*pos==')')*/
			break;
	}
	/* fill in the result into the result table */
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), cname);
	value_list = g_list_append (NULL, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "FOREIGN_KEY");
	value_list = g_list_append (value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), kname->str);
	value_list = g_list_append (value_list, tmpval);
	
	temp = g_strdup_printf ("%s(%s)", tname, fname->str);
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), temp);
	value_list = g_list_append (value_list, tmpval);
	
	value_list = g_list_append (value_list, gda_value_new_null ());

	gda_data_model_append_values (GDA_DATA_MODEL (recset),
	                              (const GList *) value_list, NULL);
		
	g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
	g_list_free (value_list);

fname_out:
	g_string_free (fname, TRUE);
tname_out:
	g_free (tname);
kname_out:
	g_string_free (kname, TRUE);
cname_out:
	g_free (cname);
}

static void
mysql_get_constraints_from_create_table (GdaDataModelArray *recset, gchar *pos)
{
	/* skip the first line. nothing of interrest there */
	if ((pos=strchr(pos, '\n')))
		pos++;
	else
		return;
	while (*pos)
	{
		gchar *next;
		if ((next = strchr (pos, '\n'))) {
			*next=0;
			next++;
		} else
			next = pos + strlen (pos);
		mysql_get_constraints_form_create_table_line (recset, pos);
		pos=next;
	}
}

static GdaDataModel *
get_mysql_constraints (GdaConnection *cnc, GdaParameterList *params)
{
	const gchar *table_name;
	GdaParameter *par;
	gchar *cmd_str;
	GdaDataModelArray *recset;
	gint rows, r;
	gint rc;
	MYSQL *mysql;
	MYSQL_RES *mysql_res;
	GString *pkey_fields = NULL;
	GValue *tmpval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (params != NULL, NULL);

	mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
	if (!mysql) {
		gda_connection_add_event_string (cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* get parameters sent by client */
	par = gda_parameter_list_find_param (params, "name");
	if (!par) {
		gda_connection_add_event_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	table_name = g_value_get_string ((GValue *) gda_parameter_get_value (par));
	if (!table_name) {
		gda_connection_add_event_string (
			cnc,
			_("Table name is needed but none specified in parameter list"));
		return NULL;
	}

	/* create and initialize the recordset to be returned */
	recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_CONSTRAINTS));
	gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_CONSTRAINTS);

	/* 
	 * Obtaining list of columns 
	 */
	cmd_str = g_strdup_printf ("SHOW COLUMNS FROM %s", table_name);
	rc = gda_mysql_real_query_wrap (cnc, mysql, cmd_str, strlen (cmd_str));
	g_free (cmd_str);
	if (rc != 0) {
		gda_connection_add_event (cnc, gda_mysql_make_error (mysql));
		return NULL;
	}

	mysql_res = mysql_store_result (mysql);
	rows = mysql_num_rows (mysql_res);

	for (r = 0; r < rows; r++) {
		GList *value_list = NULL;
		MYSQL_ROW mysql_row;
		gchar **arr;

		mysql_data_seek (mysql_res, r);
		mysql_row = mysql_fetch_row (mysql_res);
		if (!mysql_row) {
			mysql_free_result (mysql_res);
			g_object_unref (G_OBJECT (recset));

			return NULL;
		}

		/* treating ENUM and SET types into CHECK constraints */
		arr = g_strsplit ((const gchar *) mysql_row[1], "(", 0);
		if (arr) {
			gchar *str;
			if (!strcmp (arr[0], "enum")) {
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
				value_list = g_list_append (value_list, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "CHECK");
				value_list = g_list_append (value_list, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mysql_row[0]);
				value_list = g_list_append (value_list, tmpval);

				str = g_strdup_printf ("IN %s", mysql_row[1]+4);
				g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), str);
				value_list = g_list_append (value_list, tmpval);

				value_list = g_list_append (value_list, gda_value_new_null());
			}
			else if (!strcmp (arr[0], "set")) {
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
				value_list = g_list_append (value_list, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "CHECK");
				value_list = g_list_append (value_list, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mysql_row[0]);
				value_list = g_list_append (value_list, tmpval);

				str = g_strdup_printf ("SETOF %s", mysql_row[1]+3);
				g_value_take_string (tmpval = gda_value_new (G_TYPE_STRING), str);
				value_list = g_list_append (value_list, tmpval);

				value_list = g_list_append (value_list, gda_value_new_null());
			}
			g_strfreev (arr);
		}
		if (value_list) {
			gda_data_model_append_values (GDA_DATA_MODEL (recset),
						      (const GList *) value_list, NULL);
			
			g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
			g_list_free (value_list);
			value_list = NULL;
		}

		/* treating PRIMARY key constraints */
		if (!strcmp (mysql_row[3], "PRI")) {
			if (!pkey_fields)
				pkey_fields = g_string_new (mysql_row[0]);
			else {
				g_string_append_c (pkey_fields, ',');
				g_string_append (pkey_fields, mysql_row[0]);
			}
		}
	}

	/* creating the PKEY entry if necessary */
	if (pkey_fields) {
		GList *value_list = NULL;
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "");
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "PRIMARY_KEY");
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), pkey_fields->str);
		value_list = g_list_append (value_list, tmpval);

		value_list = g_list_append (value_list, gda_value_new_null());
		value_list = g_list_append (value_list, gda_value_new_null());
		g_string_free (pkey_fields, TRUE);

		gda_data_model_append_values (GDA_DATA_MODEL (recset),
					      (const GList *) value_list, NULL);
		
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
		g_list_free (value_list);
	}

	mysql_free_result (mysql_res);

	/* 
	 * taking care of foreign keys if possible 
	 */
	if (atoi (mysql->server_version) >= 5) {
		gchar *current_cname = NULL;
		GString *ref_string = NULL;
		GString *src_string = NULL;
		GList *value_list = NULL;
		
		cmd_str = g_strdup_printf ("SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, REFERENCED_TABLE_SCHEMA, "
					   "REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
					   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
					   "WHERE TABLE_NAME = '%s' AND TABLE_SCHEMA = DATABASE() AND "
					   "CONSTRAINT_NAME != 'PRIMARY' AND REFERENCED_TABLE_NAME IS NOT NULL "
					   "ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION", 
					   table_name);
		rc = gda_mysql_real_query_wrap (cnc, mysql, cmd_str, strlen (cmd_str));
		g_free (cmd_str);
		if (rc != 0) {
			gda_connection_add_event (cnc, gda_mysql_make_error (mysql));
			return NULL;
		}

		mysql_res = mysql_store_result (mysql);
		rows = mysql_num_rows (mysql_res);

		for (r = 0; r < rows; r++) {
			MYSQL_ROW mysql_row;

			mysql_data_seek (mysql_res, r);
			mysql_row = mysql_fetch_row (mysql_res);
			if (!mysql_row) {
				mysql_free_result (mysql_res);
				g_object_unref (G_OBJECT (recset));

				return NULL;
			}


			if ((! current_cname) || strcmp (current_cname, mysql_row[0])) {
				/* new constraint */
				if (value_list) {
					/* complete and store current row */
					g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), src_string->str);
					value_list = g_list_append (value_list, tmpval);
					g_string_free (src_string, TRUE);

					g_string_append_c (ref_string, ')');
					g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), ref_string->str);
					value_list = g_list_append (value_list, tmpval);
					g_string_free (ref_string, TRUE);

					value_list = g_list_append (value_list, gda_value_new_null ());

					gda_data_model_append_values (GDA_DATA_MODEL (recset),
								      (const GList *) value_list, NULL);

					g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
					g_list_free (value_list);
				}
				if (current_cname) 
					g_free (current_cname);

				/* prepare new constraint */
				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), mysql_row[0]);
				value_list = g_list_append (NULL, tmpval);

				g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "FOREIGN_KEY");
				value_list = g_list_append (value_list, tmpval);

				src_string = g_string_new (mysql_row[1]);

				ref_string = g_string_new (mysql_row[4]);
				g_string_append_c (ref_string, '(');
				g_string_append (ref_string, mysql_row[5]);
				current_cname = g_strdup (mysql_row[0]);
			}
			else {
				g_string_append_c (ref_string, ',');
				g_string_append (ref_string, mysql_row[5]);
				g_string_append_c (src_string, ',');
				g_string_append (src_string, mysql_row[1]);
			}
		}

		if (value_list) {
			/* complete and store current row */
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), src_string->str);
			value_list = g_list_append (value_list, tmpval);
			g_string_free (src_string, TRUE);

			g_string_append_c (ref_string, ')');
			g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), ref_string->str);
			value_list = g_list_append (value_list, tmpval);
			g_string_free (ref_string, TRUE);

			value_list = g_list_append (value_list, gda_value_new_null ());
		
			gda_data_model_append_values (GDA_DATA_MODEL (recset),
						      (const GList *) value_list, NULL);
		
			g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
			g_list_free (value_list);
		}
		if (current_cname)
			g_free(current_cname);
		mysql_free_result (mysql_res);
	} else {
		gchar *temp;
		MYSQL_ROW mysql_row;
		cmd_str = g_strdup_printf ("SHOW CREATE TABLE %s",
					   table_name);
		rc = gda_mysql_real_query_wrap (cnc, mysql, cmd_str, strlen (cmd_str));
		g_free (cmd_str);
		if (rc != 0) {
			gda_connection_add_event (cnc, gda_mysql_make_error (mysql));
			return NULL;
		}
		mysql_res = mysql_store_result (mysql);
		
		mysql_row = mysql_fetch_row (mysql_res);
		if (!mysql_row) {
			mysql_free_result (mysql_res);
			g_object_unref (G_OBJECT (recset));

			return NULL;
		}
		temp = g_strdup ( mysql_row[1] );
		mysql_get_constraints_from_create_table (recset, mysql_row[1]);
		g_free (temp);
		mysql_free_result (mysql_res);
	}

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
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_mysql_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_mysql_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, params);
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_mysql_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_VIEWS:
		return get_mysql_views (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_mysql_types (cnc, params);
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		return get_mysql_procedures (cnc, params);
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS:
		return get_mysql_constraints (cnc, params);
	default : break;
	}

	return NULL;
}

gchar *
gda_mysql_provider_value_to_sql_string (GdaServerProvider *provider, /* we dont actually use this!*/
					GdaConnection *cnc,
					GValue *from)
{
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	
	val_str = gda_value_stringify (from);
	if (!val_str)
		return NULL;

	type = G_VALUE_TYPE (from);
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == G_TYPE_CHAR))
		ret = g_strdup (val_str);
	else if ((type == GDA_TYPE_TIMESTAMP) ||
		 (type == G_TYPE_DATE) ||
		 (type == GDA_TYPE_TIME))
		ret = g_strdup_printf ("\"%s\"", val_str);
	else {
		MYSQL *mysql;
		char *quoted;
		mysql = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_MYSQL_HANDLE);
		
		if (!mysql) {
			gda_connection_add_event_string (cnc, _("Invalid MYSQL handle"));
			return NULL;
		}
		quoted = ret = g_malloc(strlen(val_str) * 2 + 3);
		*quoted++ 	= '\'';
		quoted += mysql_real_escape_string (
						    mysql, quoted, val_str, strlen (val_str));
		
		*quoted++     	= '\'';
		*quoted++     	= '\0';
		ret = g_realloc(ret, quoted - ret + 1);
	}
	
	g_free (val_str);

	return ret;
}

static GdaDataHandler *
gda_mysql_provider_get_data_handler (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GType type, const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) 
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
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
        else if (type == GDA_TYPE_BINARY) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = gda_handler_bin_new ();
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
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
			gda_handler_time_set_sql_spec (GDA_HANDLER_TIME (dh), 
						       G_DATE_YEAR, G_DATE_MONTH, G_DATE_DAY,
						       '-', FALSE);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIMESTAMP, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
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
		/* special case: we take into account the dbms_type argument */
		if (dbms_type)
			TO_IMPLEMENT;
	}

	return dh;
}

static const gchar*
gda_mysql_provider_get_default_dbms_type (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (type == G_TYPE_INT64)
		return "bigint";
	if (type == G_TYPE_UINT64)
		return "bigint unsigned";
	if (type == GDA_TYPE_BINARY)
		return "longtext";
	if (type == G_TYPE_BOOLEAN)
		return "bool";
	if (type == G_TYPE_DATE)
		return "date";
	if (type == G_TYPE_DOUBLE)
		return "double";
	if (type == GDA_TYPE_GEOMETRIC_POINT)
		return "varchar";
	if (type == G_TYPE_OBJECT)
		return "text";
	if (type == G_TYPE_INT)
		return "int";
	if (type == GDA_TYPE_LIST)
		return "text";
	if (type == GDA_TYPE_NUMERIC)
		return "decimal";
	if (type == G_TYPE_FLOAT)
		return "float";
	if (type == GDA_TYPE_SHORT)
		return "smallint";
	if (type == GDA_TYPE_USHORT)
		return "smallint unsigned";
	if (type == G_TYPE_STRING)
		return "varchar";
	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	if (type == G_TYPE_CHAR)
		return "tinyint";
	if (type == G_TYPE_UCHAR)
		return "tinyint unsigned";
	if (type == G_TYPE_ULONG)
		return "bigint unsigned";
        if (type == G_TYPE_UINT)
		return "int unsigned";
	if (type == G_TYPE_INVALID)
		return "text";

	return "text";
}
