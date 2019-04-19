#include<libgda/libgda.h>

int main() {
	GdaConnection *cnc;
  GdaStatement *stmt;
  GError *error = NULL;
	gda_init();
	cnc = gda_connection_open_from_string("SQLite", "DB_DIR=.;DB_NAME=test", NULL, GDA_CONNECTION_OPTIONS_NONE, NULL);
  stmt = gda_connection_parse_sql_string (cnc, "CREATE TABLE t1(id INTEGER PRIMARY KEY, name text);", NULL, NULL);
  gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, NULL);
  g_object_unref (stmt);
  gda_connection_execute_non_select_command (cnc, "INSERT INTO t1 (name) VALUES ('t01')", &error);
  if (error != NULL) {
    g_message ("Error messages: %s", error->message);
  }
	g_object_unref(cnc);
  g_message ("Test finished");
	return EXIT_SUCCESS;
}

