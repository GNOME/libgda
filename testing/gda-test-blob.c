/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <libgda/gda-connection-private.h>

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

static gboolean clear_blobs (GdaConnection *cnc, GError **error);
static gboolean insert_blob (GdaConnection *cnc, gint id, const gchar *data, glong binary_length, GError **error);
static gboolean update_blob (GdaConnection *cnc, gint id, const gchar *data, glong binary_length, GError **error);
static gboolean update_multiple_blobs (GdaConnection *cnc, const gchar *data, glong binary_length, GError **error);
static gboolean display_blobs (GdaConnection *cnc, GError **error);
static gboolean exec_statement (GdaConnection *cnc, GdaStatement *stmt, GdaSet *plist, GError **error);

GdaSqlParser *parser;

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GOptionContext *context;

	GdaConnection *cnc;
	gchar *auth_string = NULL;
	gchar *blob_data;

	/* command line parsing */
	context = g_option_context_new ("Tests opening a connection");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	if (direct && dsn) {
		g_print ("DSN and connection string are exclusive\n");
		exit (1);
	}

	if (!direct && !dsn) {
		g_print ("You must specify a connection to open either as a DSN or a connection string\n");
		exit (1);
	}

	if (direct && !prov) {
		g_print ("You must specify a provider when using a connection string\n");
		exit (1);
	}

	gda_init ();

	/* open connection */
	if (user) {
		if (pass)
			auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", user, pass);
		else
			auth_string = g_strdup_printf ("USERNAME=%s", user);
	}
	if (dsn) {
		GdaDsnInfo *info = NULL;
		info = gda_config_get_dsn_info (dsn);
		if (!info)
			g_error (_("DSN '%s' is not declared"), dsn);
		else {
			cnc = gda_connection_open_from_dsn_name (info->name, auth_string ? auth_string : info->auth_string,
							    0, &error);
			if (!cnc) {
				g_warning (_("Can't open connection to DSN %s: %s\n"), info->name,
				   error && error->message ? error->message : "???");
				exit (1);
			}
			prov = info->provider;
		}
	}
	else {
		
		cnc = gda_connection_open_from_string (prov, direct, auth_string, 0, &error);
		if (!cnc) {
			g_warning (_("Can't open specified connection: %s\n"),
				   error && error->message ? error->message : "???");
			exit (1);
		}
	}
	g_free (auth_string);

	g_print (_("Connection successfully opened!\n"));

	parser = gda_connection_create_parser (cnc);
	gda_connection_begin_transaction (cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL);

	/* 
	 * clear all blobs 
	 */
	if (!clear_blobs (cnc, &error))
		g_error ("Blobs clear error: %s", error && error->message ? error->message : "No detail");

	/* insert a blob */
	blob_data = "Blob Data 1";
	if (!insert_blob (cnc, 1, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob insert error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* insert a blob */
	blob_data = "Blob Data 2";
	if (!insert_blob (cnc, 2, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob insert error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (cnc, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");


	/* update blob */
	blob_data = "New blob 1 contents is now this one...";
	if (!update_blob (cnc, 1, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob update error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (cnc, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");

	/* update blob */
	blob_data = "After several blobs updated";
	if (!update_multiple_blobs (cnc, blob_data, strlen (blob_data), &error)) 
		g_error ("Multiple blob update error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (cnc, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");


	/* SQL Postgres:
	   create table blobs (id serial not null primary key, name varchar (50), data oid);
	   SQL Oracle:
	   CREATE TABLE blobs (id number primary key, name varchar2 (50), data BLOB);
	*/

	gda_connection_commit_transaction (cnc, NULL, NULL);
	if (! gda_connection_close (cnc, &error))
		g_error ("Can't close connection: %s", error && error->message ? error->message : "No detail");

	return 0;
}

static void
show_header (const gchar *str)
{
	g_print ("\n\n******** %s ********\n", str);
}

static gboolean
clear_blobs (GdaConnection *cnc, GError **error)
{
	GdaStatement *stmt;
	gboolean retval;
#define SQL_DELETE "DELETE FROM blobs"
	show_header ("Clear blobs");
	stmt = gda_sql_parser_parse_string (parser, SQL_DELETE, NULL, error);
	if (!stmt)
		return FALSE;
	
	retval = exec_statement (cnc, stmt, NULL, error);
	g_object_unref (stmt);

	return retval;
}

static gboolean
insert_blob (GdaConnection *cnc, gint id, const gchar *data, glong binary_length, GError **error)
{
	GdaStatement *stmt;
	GdaSet *plist;
	GdaHolder *param;
	GValue *value;
	gchar *str;
	gboolean retval;
	#define SQL_INSERT "INSERT INTO blobs (id, name, data) VALUES (##/*name:'id' type:gint*/, ##/*name:'name' type:gchararray*/, ##/*name:'theblob' type:'GdaBlob'*/)"

	show_header ("Insert a blob");
	stmt = gda_sql_parser_parse_string (parser, SQL_INSERT, NULL, error);
	if (!stmt)
		return FALSE;
	if (!gda_statement_get_parameters (stmt, &plist, NULL))
		return FALSE;

	/* blob id */
	param = gda_set_get_holder (plist, "id");
	str = g_strdup_printf ("%d", id);
	if (! gda_holder_set_value_str (param, NULL, str, error))
		return FALSE;
	g_free (str);

	/* blob name */
	param = gda_set_get_holder (plist, "name");
	str = g_strdup_printf ("BLOB_%d", id);
	if (! gda_holder_set_value_str (param, NULL, str, error))
		return FALSE;
	g_free (str);

	/* blob data */
	param = gda_set_get_holder (plist, "theblob");
	value = gda_value_new_blob ((guchar*) data, binary_length);
	if (! gda_holder_set_value (param, value, error))
		return FALSE;
	gda_value_free (value);

	gda_connection_clear_events_list (cnc);
	retval = exec_statement (cnc, stmt, plist, error);
	g_object_unref (stmt);
	g_object_unref (plist);

	return retval;
}

static gboolean
update_blob (GdaConnection *cnc, gint id, const gchar *data, glong binary_length, GError **error)
{
	GdaStatement *stmt;
	GdaSet *plist;
	GdaHolder *param;
	GValue *value;
	gchar *str;
	gboolean retval;
	const gchar* SQL_UPDATE = "UPDATE blobs set name = ##/*name:'name' type:gchararray*/, data = ##/*name:'theblob' type:'GdaBlob'*/ WHERE id= ##/*name:'id' type:gint*/";

	show_header ("Update a blob");
	stmt = gda_sql_parser_parse_string (parser, SQL_UPDATE, NULL, error);
	if (!stmt)
		return FALSE;
	if (!gda_statement_get_parameters (stmt, &plist, NULL))
		return FALSE;

	/* blob id */
	param = gda_set_get_holder (plist, "id");
	str = g_strdup_printf ("%d", id);
	if (! gda_holder_set_value_str (param, NULL, str, error))
		return FALSE;
	g_free (str);

	/* blob name */
	param = gda_set_get_holder (plist, "name");
	str = g_strdup_printf ("BLOB_%d", id);
	if (! gda_holder_set_value_str (param, NULL, str, error))
		return FALSE;
	g_free (str);

	/* blob data */
	param = gda_set_get_holder (plist, "theblob");
	value = gda_value_new_blob ((guchar*) data, binary_length);
	if (! gda_holder_set_value (param, value, error))
		return FALSE;
	gda_value_free (value);

	gda_connection_clear_events_list (cnc);
	retval = exec_statement (cnc, stmt, plist, error);
	g_object_unref (stmt);
	g_object_unref (plist);

	return retval;
}

static gboolean
update_multiple_blobs (GdaConnection *cnc, const gchar *data, glong binary_length, GError **error)
{
	GdaStatement *stmt;
	GdaSet *plist;
	GdaHolder *param;
	GValue *value;
	gboolean retval;
	const gchar* SQL_UPDATE = "UPDATE blobs set name = ##/*name:'name' type:gchararray*/, data = ##/*name:'theblob' type:'GdaBlob'*/";

	show_header ("Update several blobs at once");
	stmt = gda_sql_parser_parse_string (parser, SQL_UPDATE, NULL, error);
	if (!stmt)
		return FALSE;
	if (!gda_statement_get_parameters (stmt, &plist, NULL))
		return FALSE;

	/* blob name */
	param = gda_set_get_holder (plist, "name");
	if (! gda_holder_set_value_str (param, NULL, "---", error))
		return FALSE;

	/* blob data */
	param = gda_set_get_holder (plist, "theblob");
	value = gda_value_new_blob ((guchar*) data, binary_length);
	if (! gda_holder_set_value (param, value, error))
		return FALSE;
	gda_value_free (value);

	gda_connection_clear_events_list (cnc);
	retval = exec_statement (cnc, stmt, plist, error);
	g_object_unref (stmt);
	g_object_unref (plist);

	return retval;
}

static gboolean
exec_statement (GdaConnection *cnc, GdaStatement *stmt, GdaSet *plist, GError **error)
{
	GObject *exec_res;
	exec_res = gda_connection_statement_execute (cnc, stmt, plist, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!exec_res)
		return FALSE;
	
	if (GDA_IS_DATA_MODEL (exec_res)) {
		g_print ("Query returned a GdaDataModel...\n");
		gda_data_model_dump ((GdaDataModel*) exec_res, stdout);
	}
	else {
		if (GDA_IS_SET (exec_res)) {
			GSList *list;

			g_print ("Query returned a GdaSet:\n");
			for (list = gda_set_get_holders (GDA_SET (exec_res)); list; list = list->next) {
				gchar *str;
				str = gda_holder_get_value_str (GDA_HOLDER (list->data), NULL);
				g_print (" %s => %s\n", gda_holder_get_id (GDA_HOLDER (list->data)), str);
				g_free (str);
			}
					 
		}
		else
			g_print ("Query returned a %s object\n", G_OBJECT_TYPE_NAME (exec_res));
	}

	return TRUE;
}

static gboolean
display_blobs (GdaConnection *cnc, GError **error)
{
	GdaStatement *stmt;
	gboolean retval;
	GdaDataModel *model;

	stmt = gda_sql_parser_parse_string (parser, "SELECT * FROM blobs", NULL, error);
	if (!stmt)
		return FALSE;

	model = gda_connection_statement_execute_select (cnc, stmt, NULL, error);

	retval = model ? TRUE : FALSE;
	if (model) {
		gda_data_model_dump (model, stdout);
		g_object_unref (model);
	}
	return retval;
}
