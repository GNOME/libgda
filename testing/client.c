/* GDA - Test suite
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-test.h"

/* Shows the object schemas */
static void
show_schema (GdaConnection *cnc, GdaConnectionSchema schema, const gchar *label)
{
	GdaDataModel *model;
	gint r, c;
	gint row_count, col_count;

	model = gda_connection_get_schema (cnc, schema, NULL);
	if (!GDA_IS_DATA_MODEL (model)) {
		g_print ("\t%s\n", label);
		g_print ("\t\tNONE\n");
		return;
	}

	row_count = gda_data_model_get_n_rows (model);
	col_count = gda_data_model_get_n_columns (model);
	g_print ("\t%s %d\n", label, row_count);

	for (r = 0; r < row_count; r++) {
		g_print ("\t");
		for (c = 0; c < col_count; c++) {
			const GdaValue *value;
			gchar *str;

			value = gda_data_model_get_value_at (model, c, r);
			str = gda_value_stringify (value);
			g_print ("\t%s", str);
			g_free (str);
		}
		g_print ("\n");
	}

}

/* Prints the data in a GdaServerRecordset. Called from g_list_foreach() */
static void
display_row_data (gpointer data, gpointer user_data)
{
	GdaServerRecordset *recset = GDA_SERVER_RECORDSET (data);

	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));
}

/* Postgres provider own tests */
static void
do_postgres_test (GdaConnection *cnc)
{
	GdaCommand *create_command;
	GdaCommand *drop_command;
	GdaCommand *insert_command;
	GdaCommand *select_command;
	GList *list;
	gint col_count;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* 
	 * Drops table
	 * Creates table
	 * Inserts
	 * Selects
	 * Clean up
	 */
	g_print ("\tPostgres provider specific tests...\n");

	/* Drops the gda_postgres_test table. Fail if not exists. */
	drop_command = gda_command_new ( "drop table gda_postgres_test",
					GDA_COMMAND_TYPE_SQL);
	list = gda_connection_execute_command (cnc, drop_command, NULL);
	g_print ("\t\tDrop table: %s\n",
			 list ? "OK" : "Error (don't worry about this one)");
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Creates a table with all supported data types */
	create_command = gda_command_new ( "create table gda_postgres_test ("
				"boolean_value boolean, "
				"int2_value smallint, "
				"int4_value integer, "
				"bigint_value bigint, "
				"float_value real, "
				"double_value double precision, "
				"numeric_value numeric(15, 3), "
				"char_value char(50), "
				"varchar_value varchar(20), "
				"text_value text, "
				"point_value point, "
				"null_value char(1) "
				")",
				GDA_COMMAND_TYPE_SQL);
	list = gda_connection_execute_command (cnc, create_command, NULL);
	g_print ("\t\tCreate table with all supported types: %s\n",
			 list ? "OK" : "Error");
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Inserts values */
	insert_command = gda_command_new ( "insert into gda_postgres_test ("
				"boolean_value, "
				"int2_value, "
				"int4_value, "
				"bigint_value, "
				"float_value, "
				"double_value, "
				"numeric_value, "
				"char_value, "
				"varchar_value, "
				"text_value, "
				"point_value "
				") values ("
				"'T', "
				"-22, "
				"1048000, "
				"123456789012345, "
				"3.141592, "
				"3.1415926969696, "
				"123456789012.345, "
				"'This is a char', "
				"'This is a varchar', "
				"'This is a text', "
				"'(1,0)' "
				")",
				GDA_COMMAND_TYPE_SQL);
	list = gda_connection_execute_command (cnc, insert_command, NULL);
	g_print ("\t\tInsert values for all known types: %s\n",
			 list ? "OK" : "Error");
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Selects values */
	select_command = gda_command_new ( "select * from gda_postgres_test",
						GDA_COMMAND_TYPE_SQL);
	list = gda_connection_execute_command (cnc, select_command, NULL);
	g_print ("\t\tSelecting values for all known types: %s\n",
			 list ? "OK" : "Error");
	
	g_list_foreach (list, display_row_data, NULL);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Clean up */
	list = gda_connection_execute_command (cnc, drop_command, NULL);
	g_print ("\t\tDrop table: %s\n",
			 list ? "OK" : "Error");
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	gda_command_free (select_command);
	gda_command_free (insert_command);
	gda_command_free (create_command);
	gda_command_free (drop_command);
}
/* Opens a connection and test basic operations on it */
static void
open_connection (GdaClient *client,
		 const gchar *name,
		 const gchar *username,
		 const gchar *password)
{
	GdaConnection *cnc;
	gboolean res;

	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc = gda_client_open_connection (client, name, username, password);
	if (!GDA_IS_CONNECTION (cnc)) {
		g_print ("** ERROR: could not open connection to %s\n", name);
		return;
	}

	/* show provider features */
	g_print ("\tProvider capabilities...\n");
	g_print ("\t\tTransactions: %s\n",
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_TRANSACTIONS) ?
		 _("Supported") : _("Not supported"));

	/* show connection schemas */
	show_schema (cnc, GDA_CONNECTION_SCHEMA_PROCEDURES, "Stored procedures");
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TABLES, "Connection Tables");
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TYPES, "Available types");
	show_schema (cnc, GDA_CONNECTION_SCHEMA_VIEWS, "Views");

	/* test transactions */
	g_print ("\tStarting transaction...");
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tFinishing transaction...");
	res = gda_connection_commit_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tStarting transaction...");
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tRolling back transaction...");
	res = gda_connection_rollback_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	/* Postgres own tests */
	if (!strcmp (name, "postgres"))
		do_postgres_test (cnc);

	/* close the connection */
	gda_connection_close (cnc);
}

/* Main entry point for the basic client API test */
void
test_client (void)
{
	GdaClient *client;
	GList *dsnlist;
	GList *l;

	DISPLAY_MESSAGE ("Testing GDA client API");

	client = gda_client_new ();

	dsnlist = gda_config_get_data_source_list ();
	for (l = dsnlist; l != NULL; l = l->next) {
		GdaDataSourceInfo *info = (GdaDataSourceInfo *) l->data;

		if (!info) {
			g_print ("** ERROR: gda_config_get_data_source_list returned a NULL item\n");
			gda_main_quit ();
		}

		g_print (" Data source = %s, User = %s\n", info->name, info->username);
		open_connection (client, info->name, info->username, "");
	}

	/* free memory */
	gda_config_free_data_source_list (dsnlist);
	g_object_unref (G_OBJECT (client));
}
