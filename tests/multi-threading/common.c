#include "common.h"
#include <string.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>

/*
 * Creates an SQLite .db file from the definitions in @sqlfile
 */
gboolean
create_sqlite_db (const gchar *dir, const gchar *dbname, const gchar *sqlfile, GError **error)
{
	GdaBatch *batch;
	GdaSqlParser *parser;
	GdaServerProvider *prov;
	GdaConnection *cnc;

	/* create batch */
	prov = gda_config_get_provider ("SQLite", NULL);
	if (!prov) {
		g_set_error (error, 0, 0,
			     "Cannot find the SQLite provider");
		return FALSE;
	}
	parser = gda_server_provider_create_parser (prov, NULL);
	if (!parser)
		parser = gda_sql_parser_new ();
	
	batch = gda_sql_parser_parse_file_as_batch (parser, sqlfile, error);
	g_object_unref (parser);
	if (!batch)
		return FALSE;

	/* clean any previous DB file */
	gchar *fname, *tmp;
	tmp = g_strdup_printf ("%s.db", dbname);
	fname = g_build_filename (dir, tmp, NULL);
	g_free (tmp);
	g_unlink (fname);
	g_free (fname);

	/* open a connection */
	gchar *cnc_string;
	gchar *edir, *edbname;

	edir = gda_rfc1738_encode (dir);
	edbname = gda_rfc1738_encode (dbname);
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=%s", edir, edbname);
	g_free (edir);
	g_free (edbname);
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL, 
					       GDA_CONNECTION_OPTIONS_NONE, error);
	g_free (cnc_string);
	if (!cnc) {
		g_object_unref (batch);
		return FALSE;
	}

	/* execute batch */
	GSList *list;
	const GSList *stmt_list;
	gboolean retval = TRUE;
	list = gda_connection_batch_execute (cnc, batch, NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	stmt_list = gda_batch_get_statements (batch);
	if (g_slist_length (list) != g_slist_length ((GSList *) stmt_list))
		retval = FALSE;

	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);

	gda_connection_close (cnc);
	g_object_unref (cnc);
	g_object_unref (batch);
	return retval;
}

void
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
        g_object_unref (stmt);
        if (nrows == -1)
                g_print ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
}

GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        GdaDataModel *res;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        res = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
        if (!res)
                g_print ("Could not execute query: %s\n",
                         error && error->message ? error->message : "no detail");
        return res;
}
