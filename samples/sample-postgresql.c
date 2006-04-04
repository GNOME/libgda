/* -*- Mode: C; c-style: "K&R"; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 The GNOME Foundation
 *
 * AUTHORS:
 *      Alvaro del Castillo <acs@barrapunto.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the G88NU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* An example on how to use libgda to access PostgreSQL data 
 * You will need:
 *
 * 1. Postgresql running
 * 2. A database called "test" (you can change that in the step 2, DSN creation
 * 3. A table called "test1" in the database test with only a string column
 * 3. A user/password which can access postgresql server database test
 *    using a network socket. (Look /etc/postgresql/pg_hba.conf if you find probs)
 * 
 * Please, verify the three requisites before running the example.
 *
*/

#include <libgda/libgda.h>

/* Show errors from a working connection */
static void
get_errors (GdaConnection *connection)
{
        GList    *list;
        GList    *node;
        GdaConnectionEvent *error;
      
        list = (GList *) gda_connection_get_events (connection);
      
        for (node = g_list_first (list); node != NULL; node = g_list_next (node)) {
		error = (GdaConnectionEvent *) node->data;
		g_print ("Error no: %d\t", gda_connection_event_get_code (error));
		g_print ("desc: %s\t", gda_connection_event_get_description (error));
		g_print ("source: %s\t", gda_connection_event_get_source (error));
		g_print ("sqlstate: %s\n", gda_connection_event_get_sqlstate (error));
	}
}

/* Show results from a query */
static void
show_table2 (GdaDataModel * dm)
{
	gint      row_id;
	gint      column_id;
	GdaValue *value;
	GdaRow   *row;
	gchar    *string;


	g_print ("Data on the sample table:\n");

	for (column_id = 0; column_id < gda_data_model_get_n_columns (dm);
	     column_id++)
		g_print ("%s\t", gda_data_model_get_column_title (dm, column_id));
  

	g_print ("\n---------------------------------------\n");

	for (row_id = 0; row_id < gda_data_model_get_n_rows (dm); row_id++) {
		row = (GdaRow *) gda_data_model_get_row (dm, row_id);
		for (column_id = 0; column_id < gda_data_model_get_n_columns (dm);
		     column_id++) {
			value = gda_row_get_value (row, column_id);
			string = gda_value_stringify (value);
			g_print ("%s\t", string);
			g_free (string);
		}
		g_print ("\n");
	}
}

int
main(int argc, char *argv[])
{
        /* General GDA variables */
        GdaClient     *client;
        GdaConnection *con;
        GdaCommand    *cmd;
        GdaDataModel  *dm;
        
        /* Results from queries come in lists */
        GList         *res_cmd;
        GList         *node;
        gboolean       errors = FALSE;
        gint           rows;

        /* Connection data */
        gchar         *dsn_name = "gda-auto";
        gchar         *username = "acs";
        gchar         *password = "";

	/* 1. The first: init libgda */
        gda_init ("planner-gda", NULL, argc, argv);

        /* 2. Create a test DSN. You could have it already created */
        gda_config_save_data_source (dsn_name, 
                                     "PostgreSQL", 
                                     "DATABASE=test",
                                     "postgresql test", username, password);

        /* 3. Create a gda client */
        client = gda_client_new ();
  
        /* 4. Open the connection */
        con = gda_client_open_connection (client, dsn_name, NULL, NULL, 0);
        if (!GDA_IS_CONNECTION (con)) {
		g_print ("** ERROR: could not open connection to %s\n", dsn_name);
                get_errors (con);
		return 0;
	}

        /* 5. Our first query with results */
        cmd = gda_command_new ("SELECT * from test1", GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
        res_cmd = gda_connection_execute_command_l (con, cmd, NULL);

        if (res_cmd != NULL)
		for (node = g_list_first (res_cmd); node != NULL; node = g_list_next (node)) {
			dm = (GdaDataModel *) node->data;
			if (dm == NULL) {
				errors = TRUE;
				get_errors (con);
			} else {
				show_table2 (dm);
				g_object_unref (dm);
			}
		}
        else {
		errors = TRUE;
		get_errors (con);
        }
        gda_command_free (cmd);

        /* 6. Our first query with no SQL results to analyze */
        cmd = gda_command_new ("INSERT INTO test1 VALUES ('fool_txt')", GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
        (gda_connection_execute_command (con, cmd, NULL, NULL) == GDA_CONNECTION_EXEC_FAILED)
		get_errors (con);
        gda_command_free (cmd);

        /* 7. Clean the test DSN */
        gda_config_remove_data_source ("gda-auto");

	/* 8. Close the connection */
	gda_connection_close (con);

	/* 9. Free memory */
	g_object_unref (G_OBJECT (client));

        return 0;
}


/* gcc -Wall sample-postgresql.c `pkg-config --libs --cflags libgda` -o sample-postgresql */
