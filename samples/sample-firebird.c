#include <libgda/libgda.h>
#include <libgda/gda-value.h>

/********************************************************
* Set following parameters with your values       	*
* @FIREBIRD_RO_DATABASE database wont be modified 	*
*							*
* Program connects to @FIREBIRD_RO_DATABASE and creates	*
* a new database in /tmp/test_firebird.fdb, and then	*
* do some stuff in it.					*
*							*
* Compile with:						*
* gcc -Wall -o sample-firebird sample-firebird.c \	*
* `pkg-config --libs --cflags libgda`			*
*							*
*********************************************************/
#define FIREBIRD_RO_DATABASE "/opt/bases/test.fdb"
#define FIREBIRD_USER "sysdba"
#define FIREBIRD_PWD "masterkey"


void
show_errors (GdaConnection * cnc)
{
	GList *list, *node;
	GdaConnectionEvent *error;

	list = (GList *) gda_connection_get_events (cnc);
	for (node = g_list_first (list); node != NULL; node = g_list_next (node)) {
		error = (GdaConnectionEvent *) node->data;
		g_print ("\nError no: %ld\n", gda_connection_event_get_code (error));
		g_print ("desc: %s\n", gda_connection_event_get_description (error));
		g_print ("source: %s\n", gda_connection_event_get_source (error));
		g_print ("sqlstate: %s\n\n", gda_connection_event_get_sqlstate (error));
	}
}
								  

void
show_schema (GdaConnection *cnc,
	     GdaConnectionSchema cnc_schema,
	     gboolean show_systables)
{
	GdaDataModel *dm_schema = NULL;
	GdaParameterList *prm_list;
	GdaParameter *prm;
	GdaRow *row;
	GdaValue *value;
	gint n_row;

	prm_list = gda_parameter_list_new ();

	prm = gda_parameter_new_boolean ("systables", show_systables);
	gda_parameter_list_add_parameter (prm_list, prm);

	dm_schema = gda_connection_get_schema (cnc, cnc_schema, prm_list);
	if (dm_schema) {
		for (n_row = 0; n_row < gda_data_model_get_n_rows (dm_schema); n_row++) {
			row = (GdaRow *) gda_data_model_get_row (dm_schema, n_row);
			value = gda_row_get_value (row, 0);	
			g_print (" %s\n", gda_value_get_string (value));
		}
		
		g_print ("\n");
		
	} else { 
		g_print ("* Error getting schema *\n");
	}

	g_object_unref (dm_schema);	
	gda_parameter_list_free (prm_list);
}


gboolean
create_database (GdaConnection *cnc)
{
	return (gda_connection_create_database (cnc, "127.0.0.1:/tmp/test_firebird.fdb"));
}

void
save_datasource (void)
{
	gchar *dsn;
	
	dsn = g_strdup_printf ("DATABASE=127.0.0.1:%s", FIREBIRD_RO_DATABASE);
	gda_config_save_data_source ("test_firebird_dummy", "Firebird", dsn, "Dummy read only database", NULL, NULL);
	gda_config_save_data_source ("test_firebird", "Firebird", "DATABASE=127.0.0.1:/tmp/test_firebird.fdb",
				     "Test database", NULL, NULL);
	g_free (dsn);
}

void
remove_datasource (GdaConnection *cnc)
{
	gda_config_remove_data_source ("test_firebird_dummy");
	gda_config_remove_data_source ("test_firebird");
}

gboolean
create_some_stuff (GdaConnection *cnc)
{
	GdaCommand *command;
	GdaTransaction *transaction;
	gboolean res = FALSE;
	
	g_print ("Creating some tables and a view ... ");
	transaction = gda_transaction_new ("some_stuff");
	if (gda_connection_begin_transaction (cnc, transaction)) {
		command = gda_command_new ("CREATE TABLE BIG(AN_INTEGER INTEGER NOT NULL,"
					   "A_VARCHAR VARCHAR(30), A_SMALLINT SMALLINT NOT NULL,"
					   "A_DATE DATE, A_TIME TIME, A_TS TIMESTAMP,"
					   "A_CHAR CHAR(15), A_DECIMAL DECIMAL(5,1) NOT NULL,"
					   "A_NUMERIC NUMERIC(10,2) NOT NULL, A_FLOAT FLOAT,"
					   "A_DOUBLE DOUBLE PRECISION, A_NULL CHAR(1))",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);
		
		command = gda_command_new ("CREATE TABLE CHICA(EN_ESPANIOL INTEGER NOT NULL)",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);

		command = gda_command_new ("CREATE TABLE KLEINE(AUS_DEUTSCHE INTEGER NOT NULL)",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);
		
		command = gda_command_new ("CREATE VIEW LIST_BIG AS "
					   "SELECT * FROM BIG",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);
		gda_connection_commit_transaction (cnc, transaction);

		res = TRUE;
		g_print ("OK\n");
		
	} else {
		g_print ("ERROR\n");
	}
	
	show_errors (cnc);
	g_object_unref (transaction);
	
	return res;
}

void
populate_table_big (GdaConnection *cnc)
{
	GdaCommand *command;
	GdaTransaction *transaction;
	
	g_print ("Populating table BIG ... ");
	transaction = gda_transaction_new ("populate");
	if (gda_connection_begin_transaction (cnc, transaction)) {
		command = gda_command_new ("INSERT INTO BIG VALUES(11,'Some varying string',0,'8-Nov-1977',"
					   "'17:20','NOW','Some fixed char','134.5','67923.45','1234.5',"
					   "'78123.45',NULL)",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);

		command = gda_command_new ("INSERT INTO BIG VALUES(22,'Some other varying string',2,'8-Dec-1977',"
					   "'12:10','NOW','Some_fixed char','143.5','7123.45','234.5',"
					   "'67891.45',NULL)",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);


		command = gda_command_new ("INSERT INTO BIG VALUES(33,'Again some other varying',5,'8-Nov-2977',"
					   "'22:22','NOW','Some_fixed_char','234.5','127123.45','34.5',"
					   "'6923.45',NULL)",
					   GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		gda_command_set_transaction (command, transaction);
		gda_connection_execute_non_query (cnc, command, NULL);
		gda_command_free (command);
	
		gda_connection_commit_transaction (cnc, transaction);
		g_print ("OK\n");

	} else {
		g_print ("ERROR\n");
		show_errors (cnc);
	}
	
	g_object_unref (transaction);
}

void
select_table_big (GdaConnection *cnc)
{
	GdaCommand *command;
	GdaTransaction *transaction;
	GdaDataModel *dm;
	GList *list, *node;
	GdaValue *valor;
	gint row_n;
	const GdaTime *t;
	const GdaDate *d;
	const GdaTimestamp *ts;
	const GdaNumeric *numeric;
	
	g_print("Selecting data from table BIG ... ");
	transaction = gda_transaction_new ("show_big");
	if (gda_connection_begin_transaction (cnc, transaction)) {
		command = gda_command_new (
					"SELECT * FROM LIST_BIG",
					GDA_COMMAND_TYPE_SQL,
					GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		
		gda_command_set_transaction (command, transaction);
		list = gda_connection_execute_command (cnc, command, NULL);
		if (list != NULL) {
			g_print ("OK\n");
			for (node = g_list_first (list); node != NULL; node = g_list_next (node)) {
				dm = (GdaDataModel *) node->data;
				g_print ("* Selected rows = %d *\n", gda_data_model_get_n_rows (dm));
				g_print ("* Selected columns = %d *\n\n", gda_data_model_get_n_columns (dm));	
		
				for (row_n = 0; row_n < gda_data_model_get_n_rows (dm); row_n++) {
					g_print ("\n- Row nr %d of %d\n", (row_n+1), gda_data_model_get_n_rows (dm));	
				
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 0, row_n);
					g_print("%s: %d\n", gda_data_model_get_column_title (dm, 0),
						gda_value_get_integer (valor));
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 1, row_n);
					g_print("%s: %s\n", gda_data_model_get_column_title (dm, 1),
						gda_value_get_string (valor));
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 2, row_n);
					g_print("%s: %d\n", gda_data_model_get_column_title (dm, 2),
						gda_value_get_smallint (valor));
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 3, row_n);
					d = gda_value_get_date (valor);
					g_print("%s: %02d/%02d/%4d\n", gda_data_model_get_column_title (dm, 3),
						d->month, d->day, d->year);
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 4, row_n);
					t = gda_value_get_time (valor);
					g_print("%s: %02d:%02d\n", gda_data_model_get_column_title (dm, 4),
						t->hour, t->minute);
	
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 5, row_n);
					ts = gda_value_get_timestamp (valor);
					g_print("%s: %02d/%02d/%4d %02d:%02d\n", gda_data_model_get_column_title (dm, 5),
						ts->day, ts->month, ts->year, ts->hour, ts->minute);
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 6, row_n);
					g_print("%s: %s\n", gda_data_model_get_column_title (dm, 6),
						gda_value_get_string (valor));

					valor = (GdaValue *) gda_data_model_get_value_at (dm, 7, row_n);
					numeric = gda_value_get_numeric (valor);
					g_print("%s: %0.2f\n", gda_data_model_get_column_title (dm, 7),
						atof (numeric->number));
	
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 8, row_n);
					numeric = gda_value_get_numeric (valor);
					g_print("%s: %f\n", gda_data_model_get_column_title (dm, 8),
						atof (numeric->number));
	
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 9, row_n);
					g_print("%s: %f\n", gda_data_model_get_column_title (dm, 9),
						gda_value_get_single (valor));
	
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 10, row_n);
					g_print("%s: %f\n", gda_data_model_get_column_title (dm, 10),
						gda_value_get_double (valor));
					
					valor = (GdaValue *) gda_data_model_get_value_at (dm, 11, row_n);
					if (gda_value_get_type (valor) == GDA_VALUE_TYPE_NULL)
					g_print("%s: NULL :-)\n", gda_data_model_get_column_title (dm, 11));

				}
				
				gda_connection_rollback_transaction (cnc, transaction);
				gda_command_free (command);
			}
			
		} else {
			g_print ("ERROR (no data)\n");
		}
		
	} else {
		g_print ("ERROR\n");
	}
	
	g_object_unref (transaction);	
}

void
run_tests (void)
{
	GdaClient *client;
	GdaConnection *connection;

	g_print ("* Creating datasources (test_firebird and dummy_test_firebird) *\n");
	save_datasource ();

	client = gda_client_new ();
	g_print ("Connecting to database a dummy database ... ");
	connection = gda_client_open_connection (
						client,
						"test_firebird_dummy",
						FIREBIRD_USER,
						FIREBIRD_PWD,
						GDA_CONNECTION_OPTIONS_READ_ONLY);
	
	if (connection) {		
		g_print ("OK\nCreating database \"/tmp/test_firebird.fdb\" ...");
		if (!create_database (connection)) {
			g_print ("ERROR\nCan't create test database :-( .Removing datasources, reporting error and exiting.\n");
			show_errors (connection);			
			remove_datasource (connection);

			return;
		}
		
		g_print ("OK\n* Closing dummy connection *\n");
		gda_connection_close (connection);
		connection = NULL;
		
		g_print ("Connecting to test database ... ");
		connection = gda_client_open_connection (client, "test_firebird", FIREBIRD_USER, FIREBIRD_PWD, 0);
	
		if (!connection) {
			g_print ("ERROR\n");
			remove_datasource (connection);
			
			return;
		} else {
			g_print ("OK\n\nProveedor = %s\n", gda_connection_get_provider (connection));
			g_print ("Server Version: %s\n", gda_connection_get_server_version (connection));			
		}
	
		if (gda_connection_supports (connection, GDA_CONNECTION_FEATURE_TRANSACTIONS)) {
			g_print ("Provider support transactions :-)\n");
		} else {
			g_print ("Provider doesn't support transactions :-(\nProgram exit with errors.\n");
			remove_datasource (connection);
			
			return;
		}

		g_print("* Transaction started *\n\n");
		if (create_some_stuff (connection)) {
			populate_table_big (connection);
			select_table_big (connection);
		} else {
			g_print ("* Error creating tables *\n");
		}
		
		g_print ("\n[SHOW TABLES]\n");
		show_schema (connection, GDA_CONNECTION_SCHEMA_TABLES, FALSE);
		g_print ("[SHOW SYSTEM TABLES]\n");
		show_schema (connection, GDA_CONNECTION_SCHEMA_TABLES, TRUE);
		g_print ("[SHOW VIEWS]\n");
		show_schema (connection, GDA_CONNECTION_SCHEMA_VIEWS, FALSE);
	  
		remove_datasource (connection);
	} else {
		g_print ("ERROR\nCan't connect to dummy read-only database :-( .\n");
	}
	
	gda_connection_close (connection);
	g_object_unref (G_OBJECT (client));
	
	gda_main_quit ();
}


int
main(int argc, char* argv[])
{
	g_print ("Running test program ...\n");
	gda_init ("Test db", "0.1", argc, argv);
	gda_main_run ((GdaInitFunc) run_tests, NULL);
	g_print ("Program exit successfully.\n");
	return 0;
}

