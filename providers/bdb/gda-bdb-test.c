#include <db.h>
#include <libgda/libgda.h>
#include <string.h>

#define DB_FILE		"/tmp/bdb_test.db"

static struct number {
	gchar *english;
	gchar *french;
} numbers [] = {
	{ "one", 	"un"		},
	{ "two", 	"deux"		},
	{ "three", 	"trois"	 	},
	{ "four", 	"quatre" 	},
	{ "five", 	"cinq"		},
	{ "six", 	"six" 		},
	{ "seven",	"sept" 		},
	{ "eight", 	"huit" 		},
	{ "nine", 	"neuf" 		},
	{ "ten", 	"dix" 		}
};

#define MAXNUMBERS	(sizeof (numbers) / sizeof (struct number))
#define BDB_DSN		"bdbtest"

static void
create_db (const gchar *filename)
{
	DB *dbp;
	int ret, i;
	
	g_assert (db_create (&dbp, NULL, 0) == 0);
	g_assert (dbp->set_flags (dbp, DB_RECNUM) == 0);
	ret = dbp->open (dbp, 
#if DB_VERSION_MAJOR >= 4
			 NULL,
#endif
			 filename, NULL, DB_BTREE,
			 DB_CREATE|DB_TRUNCATE, 0);
	g_assert (ret == 0);

	for (i = 0; i < MAXNUMBERS; i++) {
		DBT key, data;
	
		memset (&key, 0, sizeof key);
		memset (&data, 0, sizeof data);
	
		key.data = numbers[i].english;
		key.size = strlen (key.data);
		
		data.data = numbers[i].french;
		data.size = strlen (data.data);

		g_assert (dbp->put (dbp, NULL, &key, &data, 0) == 0);
	}
	
	g_assert (dbp->close (dbp, 0) == 0);
}

static void
gda_stuff (gpointer filename)
{
	gchar *cnc;
	GdaClient *client;
	GdaConnection *conn;
	GdaDataModel *schema;
	int i, j;
	
	/* save dsn */
	cnc = g_strdup_printf ("FILE=%s", (gchar *) filename);
	gda_config_save_data_source (BDB_DSN, "Berkeley-DB", cnc,
				     "test bdb", NULL, NULL);
	g_free (cnc);

	/* connect to the db */
	client = gda_client_new ();
	g_assert (client != NULL);
	conn = gda_client_open_connection (client, BDB_DSN, NULL, NULL, 0);
	g_assert (conn != NULL);
	g_assert (gda_connection_is_open (conn));

	/* get schema */
	schema = gda_connection_get_schema (conn,
					    GDA_CONNECTION_SCHEMA_TABLES,
					    NULL);
	g_assert (schema != NULL);
	g_assert (gda_data_model_get_n_columns (schema) == 2);
	g_assert (gda_data_model_get_n_rows (schema) == MAXNUMBERS);

	for (i = 0; i < MAXNUMBERS; i++) {
		GdaValue *val;
		GdaRow *row;
		gchar *tmp;
		gboolean ok;

		/* get row */
		row = (GdaRow *) gda_data_model_get_row (schema, i);
		g_assert (row != NULL);
		
		/* check first val (english) */
		val = (GdaValue *) gda_row_get_value (row, 0);
		g_assert (val != NULL);
		tmp = gda_value_stringify (val);
		for (j = 0, ok = FALSE; j < MAXNUMBERS; j++) {
			if (strcmp (numbers[j].english, tmp) == 0) {
				g_free (tmp);
					
				/* check second val (french) */
				val = (GdaValue *) gda_row_get_value (row, 1);
				g_assert (val != NULL);
				tmp = gda_value_stringify (val);
				g_assert (strcmp (numbers[j].french, tmp) == 0);
				ok = TRUE;
				break;
			}
		}
		g_free (tmp);
		g_assert (ok);	
	}

	/* disconnect, remove dsn & quit */
	g_assert (gda_connection_close (conn));
	gda_config_remove_data_source (BDB_DSN);
	gda_main_quit ();
}

int main (int argc, char **argv)
{
	gda_init ("gda_bdb_test", "0.0.0", argc, argv);
	create_db (DB_FILE);
	gda_main_run (gda_stuff, DB_FILE);
	g_print ("OK\n");
	return 0;
}
