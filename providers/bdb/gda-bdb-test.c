/*
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include <db.h>
#include <gio/gio.h>

#define DB_FILE	"bdb_test.db"

static void
print_events (GdaConnection *cnc) {
  g_return_if_fail (cnc != NULL);
  GList *events = gda_connection_get_events (cnc);
  if (g_list_length (events) == 0) {
    g_print ("No events");
    return;
  }
  for (; events != NULL; events = events->next) {
    GdaConnectionEvent *ev = GDA_CONNECTION_EVENT(events->data);
    g_print ("EVENT: %s\n", gda_connection_event_get_description (ev));
  }
}

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
	gint ret;
	gsize i;
	
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
	cnc = gda_connection_new_from_string ("Berkeley-DB", cncstring, NULL, 0, &error);
	if (!cnc) {
		g_warning ("Could not create connection; %s\n", error && error->message ? error->message : "no detail");
		print_events (cnc);
    g_clear_error (&error);
		exit (1);
	}
  g_print ("Connection's provider: %s\n", gda_server_provider_get_name (gda_connection_get_provider (cnc)));
  g_assert (gda_server_provider_get_name (gda_connection_get_provider (cnc)) == "Berkeley-DB");

	if (! gda_connection_open (cnc, &error)) {
		g_warning ("Could not open connection; %s\n", error && error->message ? error->message : "no detail");
		print_events (cnc);
		g_clear_error (&error);
		g_object_unref (cnc);
		exit (1);
	}

	/* get model */
  g_print ("Runnning a SELECT query\n");
	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, "SELECT * from data", NULL, NULL);
	g_object_unref (parser);
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		print_events (cnc);
		g_warning ("Could not execute command; %s\n", error && error->message ? error->message : "no detail");
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
	gda_connection_close (cnc, NULL);
}

int main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
  GFile *dbp;
  GFile *dbf;
  GString* path;

	gda_init ();
  dbp = g_file_new_for_path (BDB_CURRENT_BUILDDIR);
  if (!g_file_query_exists (dbp, NULL))
    return FALSE;
  path = g_string_new ("");
  g_string_printf (path, "%s/%s", g_file_get_uri (dbp), DB_FILE);
  g_print ("%s\n", path->str);
  dbf = g_file_new_for_uri (path->str);
	create_db (g_file_get_path (dbf));
	gda_stuff (g_file_get_path (dbf));
  g_string_free (path, TRUE);
	g_print ("OK\n");
	return 0;
}
