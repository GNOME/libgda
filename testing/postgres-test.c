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

#define IGNORE_ERR	GDA_COMMAND_OPTION_IGNORE_ERRORS
#define STOP_ON_ERR	GDA_COMMAND_OPTION_STOP_ON_ERRORS

static gboolean
create_table (GdaConnection *cnc)
{
	GdaCommand *create_command;
	gboolean retval;
	GList *list;

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
				"time_value time, "
				"date_value date, "
				"timestamp_value timestamp, "
				"null_value char(1) "
				")",
				GDA_COMMAND_TYPE_SQL, STOP_ON_ERR);

	list = gda_connection_execute_command (cnc, create_command, 
						NULL);
	retval = list == NULL ? FALSE : TRUE;
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	gda_command_free (create_command);

	return retval;
}

static gboolean
drop_table (GdaConnection *cnc)
{
	GdaCommand *drop_command;
	gboolean retval;
	GList *list;

	drop_command = gda_command_new ( "drop table gda_postgres_test",
					GDA_COMMAND_TYPE_SQL, IGNORE_ERR);

	list = gda_connection_execute_command (cnc, drop_command, NULL);
	retval = list == NULL ? FALSE : TRUE;
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	gda_command_free (drop_command);

	return retval;
}

static GList *
insert_data (GdaConnection *cnc)
{
	GdaCommand *insert_command;
	GList *list;

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
				"time_value, "
				"date_value, "
				"timestamp_value, "
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
				"'21:13:14', "
				"'2000-02-29', "
				"'2004-02-29 14:00:11.31', "
				"'(1,0)' "
				")",
				GDA_COMMAND_TYPE_SQL, STOP_ON_ERR);

	list = gda_connection_execute_command (cnc, insert_command, NULL);
	gda_command_free (insert_command);

	return list;
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

/* Postgres provider tests */
void
do_postgres_test (GdaConnection *cnc)
{
	GList *list;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	g_print ("\tPostgres provider specific tests...\n");

	/* Drops the gda_postgres_test table. Fail if not exists. */
	g_print ("\t\tDrop table: %s\n",
		 drop_table (cnc) ? "OK" : "Error (don't worry about this one)");

	/* Creates a table with all supported data types */
	g_print ("\t\tCreate table with all supported types: %s\n",
			create_table (cnc) ? "OK" : "Error");

	/* Inserts values */
	list = insert_data (cnc);
	g_print ("\t\tInsert values for all known types: %s\n",
				 list ? "OK" : "Error");
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Selects values */
	list = select_data (cnc);
	g_print ("\t\tSelecting values for all known types: %s\n",
			 list ? "OK" : "Error");

	g_print ("-----------------\n");
	g_list_foreach (list, (GFunc) display_recordset_data, NULL);
	g_print ("-----------------\n");

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Clean up */
	g_print ("\t\tDrop table: %s\n", drop_table (cnc) ? "OK" : "Error");
}

