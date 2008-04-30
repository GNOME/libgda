#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <string.h>
#include <db.h>

#define DB_FILE	"bdb_test.db"

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

static void
create_db (const gchar *filename)
{
	DB *dbp;
	int ret, i;
	
	g_print ("Creating BDB database in '%s'\n", filename);
	g_assert (db_create (&dbp, NULL, 0) == 0);

	ret = dbp->open (dbp, NULL, filename, NULL, DB_BTREE, DB_CREATE, 0664);
	g_assert (ret == 0);

	for (i = 0; i < MAXNUMBERS; i++) {
		DBT key, data;
	
		memset (&key, 0, sizeof key);
		memset (&data, 0, sizeof data);
		
		key.data = numbers[i].english;
		key.size = strlen (key.data) + 1;
		
		data.data = numbers[i].french;
		data.size = strlen (data.data) + 1;

		g_print ("key: %s, data: %s\n", (gchar *) key.data, (gchar *) data.data);

		g_assert (dbp->put (dbp, NULL, &key, &data, 0) == 0);
	}
	
	dbp->close (dbp, 0);
	g_print ("End of BDB database creation\n");
}

static void
gda_stuff (gpointer filename)
{
	gchar *cncstring;
	GdaConnection *cnc;
	GdaDataModel *model;
	GError *error = NULL;

	GdaSqlParser *parser;
	GdaStatement *stmt;
	
	/* create dsn */
	cncstring = g_strdup_printf ("DB_NAME=%s", (gchar *) filename);

	/* connect to the db */
	cnc = gda_connection_open_from_string ("Berkeley-DB", cncstring, NULL, 0, &error);
	if (!cnc) {
		g_print ("Could not open connection; %s\n", error && error->message ? error->message : "no detail");
		exit (1);
	}

	/* get model */
	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, "SELECT * from data", NULL, NULL);
	g_object_unref (parser);
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Could execute command; %s\n", error && error->message ? error->message : "no detail");
		exit (1);
	}

	g_object_unref (stmt);
	g_assert (gda_data_model_get_n_rows (model) == MAXNUMBERS);

	gda_data_model_dump (model, stdout);
	g_object_unref (model);

#ifdef NO
	int i, j;
	for (i = 0; i < MAXNUMBERS; i++) {
		GValue *val;
		gchar *tmp;
		gboolean ok;

		/* check first val (english) */
		val = (GValue *) gda_data_model_get_value_at (model, 0, i);
		g_assert (val != NULL);
		tmp = gda_value_stringify (val);
		for (j = 0, ok = FALSE; j < MAXNUMBERS; j++) {
			if (strcmp (numbers[j].english, tmp) == 0) {
				g_free (tmp);
					
				/* check second val (french) */
				val = (GValue *) gda_row_get_value (row, 1);
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
#endif

	/* disconnect, remove dsn & quit */
	gda_connection_close (cnc);
}

int main (int argc, char **argv)
{
	gda_init ();
	create_db (DB_FILE);
	gda_stuff (DB_FILE);
	g_print ("OK\n");
	return 0;
}
