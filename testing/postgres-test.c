/* GDA - Test suite
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include "postgres-test.h"
#include <string.h>

#define IGNORE_ERR	GDA_COMMAND_OPTION_IGNORE_ERRORS
#define STOP_ON_ERR	GDA_COMMAND_OPTION_STOP_ON_ERRORS

static gboolean
execute_non_query (GdaConnection *cnc, const gchar *query)
{
	GdaCommand *command;
	gboolean retval;
	GList *list;

	command = gda_command_new (query, 
				   GDA_COMMAND_TYPE_SQL,
				   STOP_ON_ERR);

	list = gda_connection_execute_command (cnc, command, NULL);
	retval = (list == NULL) ? FALSE : TRUE;
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	gda_command_free (command);

	return retval;
}

static gboolean
create_table (GdaConnection *cnc)
{
	return execute_non_query (cnc,
				"create table gda_postgres_test ("
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
				"time_value time, "
				"date_value date, "
				"timestamp_value timestamp, "
				"null_value char(1), "
				"bytea_value bytea "
				")");
}

static gboolean
drop_table (GdaConnection *cnc)
{
	return execute_non_query (cnc, "drop table gda_postgres_test");
}

static gboolean
insert_data (GdaConnection *cnc)
{
	return execute_non_query (cnc, 
			"insert into gda_postgres_test ("
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
				"time_value, "
				"date_value, "
				"timestamp_value, "
				"point_value, "
				"bytea_value"
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
				"'21:13:14', "
				"'2000-02-29', "
				"'2004-02-29 14:00:11.31', "
				"'(1,0)', "
				"'This is a bytea value' "
				")");
}

static GList *
select_data (GdaConnection *cnc)
{
	GdaCommand *select_command;
	GList *list;

	select_command = gda_command_new ( "select * from gda_postgres_test",
					GDA_COMMAND_TYPE_SQL, STOP_ON_ERR);

	list = gda_connection_execute_command (cnc, select_command,
						NULL);
	gda_command_free (select_command);

	return list;
}

static void
test_parent_tables (GdaConnection *cnc)
{
	GdaParameter *param;
	GdaParameterList *params;
	gchar *query;
	GdaDataModel *model;

	g_print ("Testing schema parent tables...\n");
	query ="create table gda_postgres_parent (a1 integer)";
	if (execute_non_query (cnc, query) == FALSE){
		g_print ("ERROR creating parent table.\n");
		return;
	}
	g_print ("Parent table created.\n");
	query ="create table gda_postgres_child (a2 integer) inherits (gda_postgres_parent)";
	if (execute_non_query (cnc, query) == FALSE){
		g_print ("ERROR creating child table.\n");
		return;
	}
	g_print ("Parent and child created.\n");

	param = gda_parameter_new_string ("name", "gda_postgres_child");
	params = gda_parameter_list_new ();
	gda_parameter_list_add_parameter (params, param);
	model = gda_connection_get_schema (cnc, 
					   GDA_CONNECTION_SCHEMA_PARENT_TABLES, 
					   params);
	if (model != NULL){
		display_recordset_data (model);
		g_object_unref (model);
	}
	else
		g_print ("ERROR!: No parent table found for child\n");

	gda_parameter_list_free (params);
}

static void
print_errors (const GList *list)
{
	GList *tmp;

	tmp = (GList *) list;
	for (tmp = (GList *) list; tmp; tmp = tmp->next) {
		GdaError *error = GDA_ERROR (tmp->data);
		g_print ("\t\t\t%s\n", gda_error_get_description (error));
	}
}

static void
blob_tests (GdaConnection *cnc)
{
	GdaBlob *blob;
	GdaTransaction *xaction;
	gchar *str = "The string";
	gchar copy_str [10];
	gint nbytes;

	xaction = gda_transaction_new (NULL);

	gda_connection_begin_transaction (cnc, xaction);
	blob = g_new0 (GdaBlob, 1);
	g_print ("\t\tCreating a BLOB: ");
	if (!gda_connection_create_blob (cnc, blob)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	g_print ("\t\tOpening a BLOB (read/write): ");
	if (gda_blob_open (blob, GDA_BLOB_MODE_RDWR)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	g_print ("\t\tWriting to BLOB: ");
	if (gda_blob_write (blob, str, strlen (str), &nbytes)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK %d\n", nbytes);
	}

	g_print ("\t\tSeeking: ");
	if (gda_blob_lseek (blob, 2, SEEK_SET) < 0) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	g_print ("\t\tReading from BLOB: ");
	if (gda_blob_read (blob, copy_str, strlen (str) - 2, &nbytes)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		copy_str [nbytes] = '\0';
		if (strcmp (copy_str, str + 2)) {
			g_print ("FAILED\n\t\tExpecting %s but was %s\n", str + 2, copy_str);
		} else {
			g_print ("OK %d\n", nbytes);
		}
	}

	g_print ("\t\tClosing the BLOB: ");
	if (gda_blob_close (blob)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	gda_blob_free_data (blob);
	gda_connection_rollback_transaction (cnc, xaction);
	
	gda_connection_begin_transaction (cnc, xaction);
	g_print ("\t\tCreating another BLOB: ");
	if (!gda_connection_create_blob (cnc, blob)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	g_print ("\t\tRemoving the BLOB: ");
	if (gda_blob_remove (blob)) {
		g_print ("FAILED\n");
		print_errors (gda_connection_get_errors (cnc));
	} else {
		g_print ("OK\n");
	}

	gda_connection_rollback_transaction (cnc, xaction);

	gda_blob_free_data (blob);
	g_free (blob);
	g_object_unref (G_OBJECT (xaction));
}

/* Postgres provider tests */
void
do_postgres_test (GdaConnection *cnc)
{
	GList *list;
	gboolean success;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	g_print ("\tPostgres provider specific tests...\n");

	/* Drops the gda_postgres_test table. Fail if not exists. */
	g_print ("\t\tDrop table: %s\n",
		 drop_table (cnc) ? "OK" : "Error (don't worry about this one)");

	/* Creates a table with all supported data types */
	success = create_table (cnc);
	g_print ("\t\tCreate table with all supported types: %s\n",
			success ? "OK" : "Error");

	if (!success)
		print_errors (gda_connection_get_errors (cnc));

	/* Inserts values */
	success = insert_data (cnc);
	g_print ("\t\tInsert values for all known types: %s\n",
				 success ? "OK" : "Error");

	if (!success)
		print_errors (gda_connection_get_errors (cnc));

	/* Selects values */
	list = select_data (cnc);
	success = (list != NULL);
	g_print ("\t\tSelecting values for all known types: %s\n",
			 list ? "OK" : "Error");

	g_print ("-----------------\n");
	g_list_foreach (list, (GFunc) display_recordset_data, NULL);
	g_print ("-----------------\n");

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	if (!success)
		print_errors (gda_connection_get_errors (cnc));

	/* Parent tables */
	execute_non_query (cnc, "drop table gda_postgres_parent");
	execute_non_query (cnc, "drop table gda_postgres_child");

	test_parent_tables (cnc);

	success = execute_non_query (cnc, "drop table gda_postgres_child");
	if (!success)
		print_errors (gda_connection_get_errors (cnc));

	success = execute_non_query (cnc, "drop table gda_postgres_parent");
	if (!success)
		print_errors (gda_connection_get_errors (cnc));

	g_print ("-----------------\n");

	/* Test random access speed */
	list = select_data (cnc);

	g_list_foreach (list, (GFunc) test_speed_random, NULL);
	g_print ("-----------------\n");

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	/* Clean up */
	g_print ("\t\tDrop table: %s\n", drop_table (cnc) ? "OK" : "Error");

	blob_tests (cnc);
}

