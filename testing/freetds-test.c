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

#include "freetds-test.h"

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
				"create table gda_freetds_test ("
				"boolean_value bit, "
				"int1_value tinyint, "
				"int2_value smallint, "
				"int4_value int, "
				"float_value float, "
				"double_value real, "
				"char_value char(50), "
				"varchar_value varchar(20), "
				"text_value text, "
				"null_value char(1) null"
				")");
}

static gboolean
drop_table (GdaConnection *cnc)
{
	return execute_non_query (cnc, "drop table gda_freetds_test");
}

static gboolean
insert_data (GdaConnection *cnc)
{
	return execute_non_query (cnc, 
			"insert into gda_freetds_test ("
				"boolean_value, "
				"int1_value, "
				"int2_value, "
				"int4_value, "
				"float_value, "
				"double_value, "
				"char_value, "
				"varchar_value, "
				"text_value "
				") values ("
				"1, "
				"1, "
				"-22, "
				"1048000, "
				"3.141592, "
				"3.1415926969696, "
				"'This is a char', "
				"'This is a varchar', "
				"'This is a text' "
				")");
}

static GList *
select_data (GdaConnection *cnc)
{
	GdaCommand *select_command;
	GList *list;

	select_command = gda_command_new ( "select * from gda_freetds_test",
					GDA_COMMAND_TYPE_SQL, STOP_ON_ERR);

	list = gda_connection_execute_command (cnc, select_command,
						NULL);
	gda_command_free (select_command);

	return list;
}

/* freetds provider tests */
void
do_freetds_test (GdaConnection *cnc)
{
	GList *list;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	g_print ("\tFreetTDS provider specific tests...\n");

	/* Drops the gda_freetds_test table. Fail if not exists. */
	g_print ("\t\tDrop table: %s\n",
		 drop_table (cnc) ? "OK" : "Error (don't worry about this one)");

	/* Creates a table with all supported data types */
	g_print ("\t\tCreate table with all supported types: %s\n",
			create_table (cnc) ? "OK" : "Error");

	/* Inserts values */
	g_print ("\t\tInsert values for all known types: %s\n",
				 insert_data (cnc) ? "OK" : "Error");

	/* Selects values */
	list = select_data (cnc);
	g_print ("\t\tSelecting values for all known types: %s\n",
			 list ? "OK" : "Error");

	g_print ("-----------------\n");
	g_list_foreach (list, (GFunc) display_recordset_data, NULL);
	g_print ("-----------------\n");

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	/* Test random access speed */
	list = select_data (cnc);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
	/* Clean up */
//	g_print ("\t\tDrop table: %s\n", drop_table (cnc) ? "OK" : "Error");
}

