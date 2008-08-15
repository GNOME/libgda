#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <providers-support/gda-pmodel.h>
#include <tests/gda-ddl-creator.h>
#include <glib/gstdio.h>
#include "../test-cnc-utils.h"

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

#define CHECK_EXTRA_INFO

static GdaConnection *setup_connection (void);
static GdaStatement *stmt_from_string (const gchar *sql);
static void load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *file);

typedef gboolean (*TestFunc) (GdaConnection *);
static gint test1 (GdaConnection *cnc);
static gint test2 (GdaConnection *cnc);
static gint test3 (GdaConnection *cnc);

TestFunc tests[] = {
        test1,
        test2,
        test3
};

int
main (int argc, char **argv)
{
	gint i, ntests = 0, number_failed = 0;
	GdaConnection *cnc;

	gda_init ();

	g_unlink ("pmodel.db");
	cnc = setup_connection ();
	
	for (i = 0; i < sizeof (tests) / sizeof (TestFunc); i++) {
		g_print ("---------- test %d ----------\n", i);
		gint n = tests[i] (cnc);
		number_failed += n;
		if (n > 0) 
			g_print ("Test %d failed\n", i);
		ntests ++;
	}

	g_object_unref (cnc);
	if (number_failed == 0)
		g_print ("Ok.\n");
	else
		g_print ("%d failed\n", number_failed);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static GdaConnection *
setup_connection (void)
{
	GdaDDLCreator *ddl;
        GError *error = NULL;
        GdaConnection *cnc;
        gchar *str;

        /* open a connection */
        cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=pmodel", NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
        if (!cnc) {
                g_print ("Error opening connection: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
        }

	/* Setup structure */
        ddl = gda_ddl_creator_new ();
        gda_ddl_creator_set_connection (ddl, cnc);
	str = g_build_filename (CHECK_FILES, "tests", "data-models", "pmodel_dbstruct.xml", NULL);
        if (!gda_ddl_creator_set_dest_from_file (ddl, str, &error)) {
                g_print ("Error creating GdaDDLCreator: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
        }
	g_free (str);

#ifdef SHOW_SQL
        str = gda_ddl_creator_get_sql (ddl, &error);
        if (!str) {
                g_print ("Error getting SQL: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
        }
        g_print ("%s\n", str);
        g_free (str);
#endif

        if (!gda_ddl_creator_execute (ddl, &error)) {
                g_print ("Error creating database objects: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
        }

	if (! gda_connection_update_meta_store (cnc, NULL, &error)) {
		g_print ("Error fetching meta data: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
	}

	/* load some data */
	load_data_from_file (cnc, "locations", "pmodel_data_locations.xml");
	load_data_from_file (cnc, "customers", "pmodel_data_customers.xml");

        g_object_unref (ddl);
	return cnc;
}

static void
load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *file)
{
	gchar *str;
	GError *error = NULL;

	str = g_build_filename (CHECK_FILES, "tests", "data-models", file, NULL);
	if (! test_cnc_load_data_from_file (cnc, table, str, &error)) {
		g_print ("Error loading data into table '%s' from file '%s': %s\n", table, str,
			 error && error->message ? error->message : "No detail");
		exit (EXIT_FAILURE);
	}
	g_free (str);
}

static GdaStatement *
stmt_from_string (const gchar *sql)
{
	GdaStatement *stmt;
	GError *error = NULL;

	static GdaSqlParser *parser = NULL;
	if (!parser)
		parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	if (!stmt) {
		g_print ("Cound not parse SQL: %s\nSQL was: %s\n",
			 error && error->message ? error->message : "No detail",
			 sql);
		exit (EXIT_FAILURE);
	}
	return stmt;
}

/* Returns the number of failures */
static gint
test1 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt;
	gint nfailed = 0;

	/* create GdaPModel */
	stmt = stmt_from_string ("SELECT * FROM customers");
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not execute SELECT!\n");
#endif
		goto out;
	}
	if (!GDA_IS_PMODEL (model)) {
		g_print ("Data model should be a GdaPModel!\n");
		exit (EXIT_FAILURE);
	}

	/* test non INSERT, UPDATE or DELETE stmts */
	GdaStatement *mod_stmt;
	
	if (gda_pmodel_set_modification_statement (GDA_PMODEL (model), stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	g_error_free (error);
	error = NULL;

	mod_stmt = stmt_from_string ("BEGIN");
	if (gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	g_error_free (error);
	error = NULL;
	g_object_unref (mod_stmt);

	/* test INSERT with undefined params */
	mod_stmt = stmt_from_string ("INSERT INTO customers (name) VALUES (##aname::string)");
	if (gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	g_error_free (error);
	error = NULL;

 out:
	g_object_unref (model);
	g_object_unref (stmt);
	
	return nfailed;
}

/* Returns the number of failures */
static gint
test2 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;
	GdaSet *params;

	/* create GdaPModel */
	stmt = stmt_from_string ("SELECT * FROM customers WHERE country = ##country::string");
	if (!gda_statement_get_parameters (stmt, &params, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not get SELECT's parameters!\n");
#endif
		goto out;
	}
	gda_set_set_holder_value (params, "country", "SP");
	model = gda_connection_statement_execute_select (cnc, stmt, params, &error);
	g_object_unref (params);
	if (!model) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not execute SELECT!\n");
#endif
		goto out;
	}
	if (!GDA_IS_PMODEL (model)) {
		g_print ("Data model should be a GdaPModel!\n");
		exit (EXIT_FAILURE);
	}


	/* test INSERT with params of the wrong type */
	mod_stmt = stmt_from_string ("INSERT INTO customers (name, country) VALUES (##+1::string, ##country::date)");
	if (gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	g_error_free (error);
	error = NULL;
	g_object_unref (mod_stmt);

	/* test correct INSERT */
	mod_stmt = stmt_from_string ("INSERT INTO customers (name, country) VALUES (##+1::string, ##country::string)");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

 out:	
	g_object_unref (model);
	g_object_unref (stmt);
	
	return nfailed;
}

/* Returns the number of failures */
static gint
test3 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;

	/* create GdaPModel */
	stmt = stmt_from_string ("SELECT * FROM customers");
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not execute SELECT!\n");
#endif
		goto out;
	}
	if (!GDA_IS_PMODEL (model)) {
		g_print ("Data model should be a GdaPModel!\n");
		exit (EXIT_FAILURE);
	}

	/* test INSERT with undefined params */
	mod_stmt = stmt_from_string ("UPDATE customers SET name = ##+1::string WHERE id = ##-0::gint");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	GValue *value;
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "Jack");
	if (! gda_data_model_set_value_at (model, 1, 0, value, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_value_at failed: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	if (gda_data_model_set_value_at (model, 0, 0, value, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_value_at should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	g_error_free (error);
	gda_value_free (value);

 out:
	g_object_unref (model);
	g_object_unref (stmt);
	
	return nfailed;
}
