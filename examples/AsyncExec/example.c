/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <unistd.h>

static GdaStatement *create_statement (const gchar *sql);
static void fetch_execute_result (GdaConnection *cnc, guint task_id);
static guint request_execution (GdaConnection *cnc, const gchar *sql);
static guint request_execution_with_params (GdaConnection *cnc, const gchar *sql, ...);

int
main (int argc, char** argv)
{
	GdaConnection *cnc;
	guint id1, id2, id3;
	GError *error = NULL;

	gda_init ();

	/* REM: the GDA_CONNECTION_OPTIONS_THREAD_ISOLATED flag is necessary
	 * to make sure the connection is opened in a separate thread from the main thread in order
	 * for the asynchronous method to work. Failing to do so will generally (depending on the
	 * database provider's implementation) lead to asynchronous execution failure.
	 */
	cnc = gda_connection_open_from_dsn ("SalesTest", NULL, GDA_CONNECTION_OPTIONS_THREAD_ISOLATED, NULL);
	if (!cnc) {
		g_print ("Can't open connection: %s\n", error && error->message ? error->message : "No detail");
		return 1;
	}

	/* execute */
	id1 = request_execution (cnc, "SELECT * from customers");
	id2 = request_execution (cnc, "SELECT * from products LIMIT 2");
	id3 = request_execution_with_params (cnc, "SELECT * from customers WHERE id>=##theid::int", "theid", "6", NULL);

	/* fetch result */
	fetch_execute_result (cnc, id2);
	fetch_execute_result (cnc, 10);
	fetch_execute_result (cnc, id1);
	fetch_execute_result (cnc, id2);
	fetch_execute_result (cnc, id3);

	/*gda_statement_get_parameters (stmt, &parameters, NULL);*/
  
	g_object_unref (cnc);

	return 0;
}

static GdaStatement *
create_statement (const gchar *sql)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser,  sql, NULL, NULL);
	g_object_unref (parser);
	return stmt;
}

static guint
request_execution (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
	guint id;
	GError *error = NULL;

	g_print ("=========== Async. execution request:\n%s\n", sql);
	stmt = create_statement (sql);
	id = gda_connection_async_statement_execute (cnc, stmt, NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, FALSE,
						     &error);
	g_object_unref (stmt);
	if (id == 0) {
		g_print ("Can't execute ASYNC: %s\n", error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("Assigned task id: %u\n", id);
	return id;
}

/*
 * ... is a list of (const gchar *name, const gchar *value) couples, terminated by %NULL
 */
static guint
request_execution_with_params (GdaConnection *cnc, const gchar *sql, ...)
{
	GdaStatement *stmt;
	guint id;
	GError *error = NULL;
	GdaSet *params;
	const gchar *pname;

	g_print ("=========== Async. exececution request:\n%s\n", sql);
	stmt = create_statement (sql);
	if (! gda_statement_get_parameters (stmt, &params, &error)) {
		g_print ("Can't get statement's parameters: %s\n", error && error->message ? error->message : "No detail");
		exit (1);
	}

	if (params) {
		va_list ap;
		va_start (ap, sql);
		for (pname = va_arg (ap, const gchar *); pname; pname = va_arg (ap, const gchar *)) {
			const gchar *value;
			GdaHolder *holder;
			holder = gda_set_get_holder (params, pname);
			value = va_arg (ap, const gchar *);
			if (holder)
				g_assert (gda_holder_set_value_str (holder, NULL, value, NULL));
			g_print ("\t%s => %s\n", pname, value);
			va_end (ap);
		}
	}
	
	id = gda_connection_async_statement_execute (cnc, stmt, params, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, FALSE,
						     &error);
	if (params)
		g_object_unref (params);
	g_object_unref (stmt);
	if (id == 0) {
		g_print ("Can't execute ASYNC: %s\n", error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("Assigned task id: %u\n", id);
	return id;
}

static void
fetch_execute_result (GdaConnection *cnc, guint task_id)
{
	GObject *result;
	GError *error = NULL;
	gint i;
	for (i = 0; i < 10; i++) {
		g_print ("=========== Waiting for task %u\n", task_id);
		result = gda_connection_async_fetch_result (cnc, task_id, NULL, &error);
		if (result) {
			if (GDA_IS_DATA_MODEL (result))
				gda_data_model_dump (GDA_DATA_MODEL (result), NULL);
			else
				g_print ("Unknown result: %s\n", G_OBJECT_TYPE_NAME (result));
			g_object_unref (result);
			return;
		}
		else if (error) {
			if (error && (error->domain == GDA_CONNECTION_ERROR) &&
			    (error->code == GDA_CONNECTION_TASK_NOT_FOUND_ERROR))
				g_print ("Task not found error: %s\n", error->message);
			else
				g_print ("Execution failed: %s\n", error->message ? error->message : "No detail");
			return;
		}
		else {
			g_print ("Not yet executed...\n");
			g_usleep (100000);
		}
	}
}
