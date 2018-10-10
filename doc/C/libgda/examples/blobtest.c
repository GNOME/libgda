<?xml version="1.0"?>
<!DOCTYPE refentry PUBLIC "-//OASIS//DTD DocBook XML V4.1.2//EN"
          "http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd">
<programlisting>
<![CDATA[#include <libgda/libgda.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/gda-blob-op.h>

GdaConnection *open_connection (void);
static gboolean do_store (GdaConnection *cnc, const gchar *filename, GError **error);
static gboolean do_fetch (GdaConnection *cnc, gint id, GError **error);

int
main (int argc, char *argv[])
{
        GdaConnection *cnc;
	const gchar *filename = NULL;
	gint id = 0;
	gboolean store;
	GError *error = NULL;
	gboolean result;

	/* parse arguments */
	if (argc != 3)
		goto help;
	if (! g_ascii_strcasecmp (argv[1], "store")) {
		filename = argv[2];
		store = TRUE;
	}
	else if (! g_ascii_strcasecmp (argv[1], "fetch")) {
		id = atoi (argv[2]);
		store = FALSE;
	}
	else
		goto help;

	/* do the job */
        gda_init ();
	cnc = open_connection ();
	if (store)
		result = do_store (cnc, filename, &error);
	else
		result = do_fetch (cnc, id, &error);

	if (gda_connection_get_transaction_status (cnc))
		g_print ("Still in a transaction, all modifications will be lost when connection is closed\n");
        gda_connection_close (cnc);

	if (!result) {
		g_print ("ERROR: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
	}
	else
		g_print ("Ok.\n");

        return result ? 0 : 1;

 help:
	g_print ("%s [store <filename> | fetch <ID>]\n", argv[0]);
	return 0;
}

/*
 * Open a connection to the example.db file
 */
GdaConnection *
open_connection (void)
{
        GdaConnection *cnc;
        GError *error = NULL;
        cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=testblob", NULL,
					       GDA_CONNECTION_OPTIONS_NONE,
					       &error);
        if (!cnc) {
                g_print ("Could not open connection to SQLite database in testblob.db file: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        return cnc;
}

static gboolean
do_store (GdaConnection *cnc, const gchar *filename, GError **error)
{
	if (! g_file_test (filename, G_FILE_TEST_EXISTS) ||
	    g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		g_set_error (error, GDA_DIR_BLOB_OP_ERROR, GDA_DIR_BLOB_OP_GENERAL_ERROR,
			     "%s", "File does not exist or is a directory");
		return FALSE;
	}

	GdaSqlParser *parser;
	GdaStatement *stmt;
	GdaSet *params, *newrow;
	GdaHolder *holder;
	GValue *value;
	GdaBlob *blob;
	gint res;

	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser,
					    "INSERT INTO blobstable (data) VALUES (##blob::blob)",
					    NULL, error);
	g_object_unref (parser);
	if (!stmt)
		return FALSE;

	if (! gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return FALSE;
	}

	holder = gda_set_get_holder (params, "blob");
	value = gda_value_new_blob_from_file (filename);
	blob = (GdaBlob*) gda_value_get_blob (value);
	g_assert (gda_holder_take_value (holder, value, NULL));

	g_print ("Before writing BLOB: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	g_print ("STORING file '%s' to database BLOB\n", filename);
	res = gda_connection_statement_execute_non_select (cnc, stmt, params, &newrow, error);
	g_object_unref (params);
	g_object_unref (stmt);

	g_print ("After writing BLOB: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	if (newrow) {
		GSList *list;
		g_print ("Inserted row is (for each numbered column in the table):\n");
		for (list = newrow->holders; list; list = list->next) {
			const GValue *value;
			gchar *tmp;
			value = gda_holder_get_value (GDA_HOLDER (list->data));
			tmp = gda_value_stringify (value);
			g_print ("  [%s] = [%s]\n", gda_holder_get_id (GDA_HOLDER (list->data)), tmp);
			g_free (tmp);
		}
		g_object_unref (newrow);
	}
	else
		g_print ("Provider did not return the inserted row\n");

	gda_connection_rollback_transaction (cnc, NULL, NULL);
	g_print ("After rolling back blob READ transaction: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	return (res == -1) ? FALSE : TRUE;
}

static gboolean
do_fetch (GdaConnection *cnc, gint id, GError **error)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	GdaSet *params;
	GdaDataModel *model;
	const GValue *value;
	GdaBlob *blob;
	gboolean result = TRUE;

	g_print ("Before reading BLOB: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	gchar *filename;
	filename = g_strdup_printf ("fetched_%d", id);
	g_print ("FETCHING BLOB with ID %d to file '%s'\n", id, filename);

	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser,
					    "SELECT data FROM blobstable WHERE id=##id::int",
					    NULL, error);
	g_object_unref (parser);
	if (!stmt)
		return FALSE;

	if (! gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return FALSE;
	}
	g_assert (gda_set_set_holder_value (params, NULL, "id", id));
	model = gda_connection_statement_execute_select (cnc, stmt, params, error);
	g_object_unref (params);
	g_object_unref (stmt);
	if (! model)
		return FALSE;

	g_print ("After reading BLOB: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	value = gda_data_model_get_value_at (model, 0, 0, error);
	if (!value) {
		g_object_unref (model);
		return FALSE;
	}
	g_assert (G_VALUE_TYPE (value) == GDA_TYPE_BLOB);
	
	blob = (GdaBlob*) gda_value_get_blob (value);
	if (blob->op) {
		GValue *dest_value;
		GdaBlob *dest_blob;
		
		dest_value = gda_value_new_blob_from_file (filename);
		dest_blob = (GdaBlob*) gda_value_get_blob (dest_value);
		result = gda_blob_op_write_all (dest_blob->op, (GdaBlob*) blob);
		gda_value_free (dest_value);
	}
	else
		result = g_file_set_contents (filename, (gchar *) ((GdaBinary*)blob)->data,
					     ((GdaBinary*)blob)->binary_length, error);
	g_free (filename);
	g_object_unref (model);

	gda_connection_rollback_transaction (cnc, NULL, NULL);
	g_print ("After rolling back blob READ transaction: %s\n", gda_connection_get_transaction_status (cnc) ?
		 "Transaction started" : "No transaction started");

	return result;
}
]]>
</programlisting>
