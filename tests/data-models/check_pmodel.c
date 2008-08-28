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

/* signals checking */
typedef struct {
	GdaDataModel *model;
	gchar         type; /* U, I, D */
	gint          row;
} SigEvent;
static GSList *signals = NULL;
static void     monitor_model_signals (GdaDataModel *model);
static void     signal_callback (GdaDataModel *model, gint row, gchar *type);
static void     clear_signals (void);
static gboolean check_expected_signal (GdaDataModel *model, gchar type, gint row);
static gboolean check_no_expected_signal (GdaDataModel *model);

/* utility functions */
static GdaConnection *setup_connection (void);
static GdaStatement *stmt_from_string (const gchar *sql);
static void load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *file);
static gboolean check_set_value_at (GdaDataModel *model, gint col, gint row, 
				    const GValue *set_value, 
				    GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params);
static gboolean check_set_values (GdaDataModel *model, gint row, GList *set_values,
				  GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params);
static gint check_append_values (GdaDataModel *model, GList *set_values,
				 GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params);

typedef gboolean (*TestFunc) (GdaConnection *);
static gint test1 (GdaConnection *cnc);
static gint test2 (GdaConnection *cnc);
static gint test3 (GdaConnection *cnc);
static gint test4 (GdaConnection *cnc);
static gint test5 (GdaConnection *cnc);
static gint test6 (GdaConnection *cnc);
static gint test7 (GdaConnection *cnc);

TestFunc tests[] = {
        test1,
        test2,
        test3,
	test4,
	test5,
	test6,
	test7
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
		g_print ("---------- test %d ----------\n", i+1);
		gint n = tests[i] (cnc);
		number_failed += n;
		if (n > 0) 
			g_print ("Test %d failed\n", i+1);
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

	/* update meta store */
	if (! gda_connection_update_meta_store (cnc, NULL, &error)) {
		g_print ("Error updateing meta store: %s\n", error && error->message ? error->message : "No detail");
                g_error_free (error);
                exit (EXIT_FAILURE);
	}

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

/*
 * check modifications statements' setting
 * 
 * Returns the number of failures 
 */
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

/*
 * check modifications statements' setting, with external parameter
 * 
 * Returns the number of failures 
 */
static gint
test2 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;
	GdaSet *params;

	clear_signals ();

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

/*
 * check gda_data_model_set_at()
 * 
 * Returns the number of failures 
 */
static gint
test3 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;
	GValue *value;

	clear_signals ();

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
	mod_stmt = stmt_from_string ("UPDATE customers SET name = ##+1::string, last_update = ##+2::timestamp WHERE id = ##-0::gint");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

	/****/
	monitor_model_signals (model);
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "Jack");
	if (! check_set_value_at (model, 1, 0, value, cnc, stmt, NULL)) {
		nfailed ++;
		goto out;
	}
	if (! check_expected_signal (model, 'U', 0)) {
		nfailed++;
		goto out;
	}
	gda_value_free (value);
	clear_signals ();

	/****/
	gda_value_set_from_string ((value = gda_value_new (GDA_TYPE_TIMESTAMP)), 
				   "2009-11-30 11:22:33", GDA_TYPE_TIMESTAMP);
	if (! check_set_value_at (model, 2, 1, value, cnc, stmt, NULL)) {
		nfailed ++;
		goto out;
	}
	gda_value_free (value);

	/****/
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "Henry");
	if (! check_set_value_at (model, 1, 0, value, cnc, stmt, NULL)) {
		nfailed ++;
		goto out;
	}
	gda_value_free (value);

	/****/
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

 out:
	g_object_unref (model);
	g_object_unref (stmt);

	return nfailed;
}

/*
 * more check gda_data_model_remove_row()
 * 
 * Returns the number of failures 
 */
static gint
test4 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;

	clear_signals ();

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
	mod_stmt = stmt_from_string ("DELETE FROM customers WHERE id = ##-0::gint");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	
	monitor_model_signals (model);
	if (! gda_data_model_remove_row (model, 0, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_remove_row() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		goto out;
	}
	
	if (! check_expected_signal (model, 'D', 0))
		nfailed++;
	clear_signals ();

 out:
	g_object_unref (model);
	g_object_unref (stmt);
	
	return nfailed;
}

/*
 * check gda_data_model_set_values()
 * 
 * Returns the number of failures 
 */
static gint
test5 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;
	GValue *v1, *v2;
	GList *values;

	clear_signals ();

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
	mod_stmt = stmt_from_string ("UPDATE customers SET name = ##+1::string, last_update = ##+2::timestamp, default_served_by = ##+3::gint WHERE id = ##-0::gint");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

	/****/
	monitor_model_signals (model);
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "Alf");
	g_value_set_int ((v2 = gda_value_new (G_TYPE_INT)), 999);
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, v1);
	values = g_list_append (values, NULL);
	values = g_list_append (values, v2);
	if (! check_set_values (model, 2, values, cnc, stmt, NULL)) {
		nfailed ++;
		goto out;
	}
	if (! check_expected_signal (model, 'U', 2)) {
		nfailed++;
		goto out;
	}
	gda_value_free (v1);
	gda_value_free (v2);
	g_list_free (values);
	clear_signals ();
	
	/****/
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, NULL);
	if (! check_set_values (model, 2, values, cnc, stmt, NULL)) {
		nfailed ++;
		goto out;
	}
	if (! check_no_expected_signal (model)) {
		nfailed++;
		goto out;
	}
	g_list_free (values);

	/****/
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	if (gda_data_model_set_values (model, 2, values, &error)) {
		nfailed ++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_values should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif		
	if (! check_no_expected_signal (model)) {
		nfailed++;
		goto out;
	}
	g_list_free (values);

 out:
	g_object_unref (model);
	g_object_unref (stmt);

	return nfailed;
}

/*
 * check gda_data_model_append_values()
 * 
 * Returns the number of failures 
 */
static gint
test6 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt, *mod_stmt;
	gint nfailed = 0;
	GValue *v1, *v2;
	GList *values;
	gint newrow;

	clear_signals ();

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

	/* set unique row select */
	if (gda_pmodel_set_row_selection_condition_sql (GDA_PMODEL (model), "id > ##-0::gint", &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_row_selection_condition_sql() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	if (error) {
		g_error_free (error);
		error = NULL;
	}

	if (! gda_pmodel_set_row_selection_condition_sql (GDA_PMODEL (model), "id = ##-0::gint", &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_row_selection_condition_sql() have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

	if (gda_pmodel_set_row_selection_condition_sql (GDA_PMODEL (model), "id = ##-0::gint", &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_row_selection_condition_sql() should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	if (error) {
		g_error_free (error);
		error = NULL;
	}

	/* set INSERT statement */
	mod_stmt = stmt_from_string ("INSERT INTO customers (name, last_update, default_served_by) VALUES (##+1::string, CURRENT_TIMESTAMP, ##+3::gint)");
	if (!gda_pmodel_set_modification_statement (GDA_PMODEL (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_set_modification_statement() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

	/****/
	monitor_model_signals (model);
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "George");
	g_value_set_int ((v2 = gda_value_new (G_TYPE_INT)), 21);
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, v1);
	values = g_list_append (values, NULL);
	values = g_list_append (values, v2);
	newrow = check_append_values (model, values, cnc, stmt, NULL);
	if (newrow < 0) {
		nfailed ++;
		goto out;
	}
	if (! check_expected_signal (model, 'I', newrow)) {
		nfailed++;
		goto out;
	}
	gda_value_free (v1);
	gda_value_free (v2);
	g_list_free (values);
	clear_signals ();
	
	/****/
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, NULL);
	if (gda_data_model_append_values (model, values, &error) >= 0) {
		nfailed ++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_values should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	if (error) {
		g_error_free (error);
		error = NULL;
	}
	if (! check_no_expected_signal (model)) {
		nfailed++;
		goto out;
	}
	g_list_free (values);

	/****/
	values = g_list_append (NULL, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	values = g_list_append (values, NULL);
	if (gda_data_model_append_values (model, values, &error) >= 0) {
		nfailed ++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_values should have failed\n");
#endif
		goto out;
	}
#ifdef CHECK_EXTRA_INFO
	g_print ("Got expected error: %s\n", error && error->message ? error->message : "No detail");
#endif
	if (error) {
		g_error_free (error);
		error = NULL;
	}
	if (! check_no_expected_signal (model)) {
		nfailed++;
		goto out;
	}
	g_list_free (values);

 out:
	g_object_unref (model);
	g_object_unref (stmt);

	return nfailed;
}

/*
 * check modifications statements' setting, with external parameter
 * 
 * Returns the number of failures 
 */
static gint
test7 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt;
	gint nfailed = 0;
	GdaSet *params;
	GValue *value;

	clear_signals ();

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

	
	/* gda_pmodel_compute_modification_statements() */
	if (!gda_pmodel_compute_modification_statements (GDA_PMODEL (model), TRUE, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_pmodel_compute_modification_statements() should have succedded, error: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}

	gda_value_set_from_string ((value = gda_value_new (GDA_TYPE_TIMESTAMP)), 
				   "2004-03-22 11:22:44", GDA_TYPE_TIMESTAMP);
	if (! check_set_value_at (model, 2, 1, value, cnc, stmt, params)) {
		nfailed ++;
		goto out;
	}
	gda_value_free (value);

 out:	
	g_object_unref (model);
	g_object_unref (stmt);
	g_object_unref (params);
	
	return nfailed;
}


/*
 * Checking value function:
 *  - reads the value of @model at the provided column and row and compares with the expected @set_value
 *  - if @stmt is not NULL, then re-run the statement and compares with @model
 */
static gboolean
check_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *set_value, 
		    GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params)
{
	const GValue *get_value;
	GError *error = NULL;

	if (! gda_data_model_set_value_at (model, col, row, set_value, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_value_at(%d,%d) failed: %s\n",
			 col, row, 
			 error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}
	get_value = gda_data_model_get_value_at (model, col, row);
	if (gda_value_compare_ext (get_value, set_value)) {
#ifdef CHECK_EXTRA_INFO
		gchar *s1, *s2;
		s1 = gda_value_stringify (get_value);
		s2 = gda_value_stringify (set_value);
		g_print ("gda_data_model_get_value_at(%d,%d) returned '%s' when it should have returned '%s'\n", col, row, s1, s2);
		g_free (s1);
		g_free (s2);
#endif
		return FALSE;
	}

	if (stmt) {
		/* run the statement and compare it with @model */
		GdaDataModel *rerun;
		GdaDataComparator *cmp;
		GError *error = NULL;
		gboolean cmpres = TRUE;
		rerun = gda_connection_statement_execute_select (cnc, stmt, stmt_params, &error);
		if (!rerun) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not re-run the SELECT statement: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}

		cmp = (GdaDataComparator*) gda_data_comparator_new (model, rerun);
		if (! gda_data_comparator_compute_diff (cmp, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not compute the data model differences: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			cmpres = FALSE;
		}
		if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
			g_print ("There are some differences when re-running the SELECT statement...\n");
#endif
			cmpres = FALSE;
		}
		g_object_unref (cmp);
		g_object_unref (rerun);
		return cmpres;
	}

	return TRUE;
}

/*
 * Checking value function:
 *  - reads the value of @model at the provided column and row and compares with the expected @set_value
 *  - if @stmt is not NULL, then re-run the statement and compares with @model
 */
static gboolean
check_set_values (GdaDataModel *model, gint row, GList *set_values, GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params)
{
	GError *error = NULL;
	GList *list;
	gint i;

	if (! gda_data_model_set_values (model, row, set_values, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_values (%d) failed: %s\n",
			 row, 
			 error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}

	for (i = 0, list = set_values; list; i++, list = list->next) {
		const GValue *get_value;
		
		if (!list->data)
			continue;
		
		get_value = gda_data_model_get_value_at (model, i, row);
		if (gda_value_compare_ext (get_value, (GValue*) list->data)) {
#ifdef CHECK_EXTRA_INFO
			gchar *s1, *s2;
			s1 = gda_value_stringify (get_value);
			s2 = gda_value_stringify ((GValue*) list->data);
			g_print ("gda_data_model_get_value_at(%d,%d) returned '%s' when it should have returned '%s'\n", i, row, s1, s2);
			g_free (s1);
			g_free (s2);
#endif
			return FALSE;
		}
	}

	if (stmt) {
		/* run the statement and compare it with @model */
		GdaDataModel *rerun;
		GdaDataComparator *cmp;
		GError *error = NULL;
		gboolean cmpres = TRUE;
		rerun = gda_connection_statement_execute_select (cnc, stmt, stmt_params, &error);
		if (!rerun) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not re-run the SELECT statement: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}

		cmp = (GdaDataComparator*) gda_data_comparator_new (model, rerun);
		if (! gda_data_comparator_compute_diff (cmp, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not compute the data model differences: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			cmpres = FALSE;
		}
		if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
			g_print ("There are some differences when re-running the SELECT statement...\n");
#endif
			cmpres = FALSE;
		}

		g_object_unref (cmp);
		g_object_unref (rerun);
		return cmpres;
	}

	return TRUE;
}

/*
 * Checking value function:
 *  - reads the value of @model at the provided column and row and compares with the expected @set_value
 *  - if @stmt is not NULL, then re-run the statement and compares with @model
 *
 * Returns: -1 if an error occurred, or the new row number
 */
static gint
check_append_values (GdaDataModel *model, GList *set_values, GdaConnection *cnc, 
		     GdaStatement *stmt, GdaSet *stmt_params)
{
	GError *error = NULL;
	GList *list;
	gint i, newrow;

	newrow = gda_data_model_append_values (model, set_values, &error);
	if (newrow < 0) {
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_append_values () failed: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		return -1;
	}

	for (i = 0, list = set_values; list; i++, list = list->next) {
		const GValue *get_value;
		
		if (!list->data)
			continue;
		
		get_value = gda_data_model_get_value_at (model, i, newrow);
		if (gda_value_compare_ext (get_value, (GValue*) list->data)) {
#ifdef CHECK_EXTRA_INFO
			gchar *s1, *s2;
			s1 = gda_value_stringify (get_value);
			s2 = gda_value_stringify ((GValue*) list->data);
			g_print ("gda_data_model_get_value_at(%d,%d) returned '%s' when it should have returned '%s'\n", i, 
				 newrow, s1, s2);
			g_free (s1);
			g_free (s2);
#endif
			return -1;
		}
	}

	if (stmt) {
		/* run the statement and compare it with @model */
		GdaDataModel *rerun;
		GdaDataComparator *cmp;
		GError *error = NULL;
		rerun = gda_connection_statement_execute_select (cnc, stmt, stmt_params, &error);
		if (!rerun) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not re-run the SELECT statement: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			return -1;
		}

		cmp = (GdaDataComparator*) gda_data_comparator_new (model, rerun);
		if (! gda_data_comparator_compute_diff (cmp, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not compute the data model differences: %s\n",
				 error && error->message ? error->message : "No detail");
#endif
			newrow = -1;
		}
		if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
			g_print ("There are some differences when re-running the SELECT statement...\n");
#endif
			newrow = -1;
		}

		g_object_unref (cmp);
		g_object_unref (rerun);
		return newrow;
	}

	return newrow;
}

/* 
 * signals checking 
 */
static void
monitor_model_signals (GdaDataModel *model)
{
	g_signal_connect (model, "row-updated", G_CALLBACK (signal_callback), "U");
	g_signal_connect (model, "row-removed", G_CALLBACK (signal_callback), "D");
	g_signal_connect (model, "row-inserted", G_CALLBACK (signal_callback), "I");
}

static void
signal_callback (GdaDataModel *model, gint row, gchar *type)
{
	SigEvent *se;
	se = g_new0 (SigEvent, 1);
	se->model = model;
	se->type = *type;
	se->row = row;
	signals = g_slist_append (signals, se);
#ifdef CHECK_EXTRA_INFO
	g_print ("Received '%s' signal for row %d from model %p\n",
		 (*type == 'I') ? "row-inserted" : ((*type == 'D') ? "row-removed" : "row-updated"),
		 row, model);
#endif
}

static void
clear_signals (void)
{
	if (signals) {
		GSList *list;
		for (list = signals; list; list = list->next) {
			SigEvent *se = (SigEvent*) list->data;
			g_free (se);
		}
		g_slist_free (signals);
		signals = NULL;
	}
}

static gboolean
check_expected_signal (GdaDataModel *model, gchar type, gint row)
{
	SigEvent *se = NULL;
	if (signals)
		se = (SigEvent*) signals->data;
	if (se && (se->model == model) && (se->row == row) && (se->type == type)) {
		g_free (se);
		signals = g_slist_remove (signals, se);
		return TRUE;
	}
#ifdef CHECK_EXTRA_INFO
	else {
		g_print ("Expected signal '%s' for row %d from model %p and got ",
			 (type == 'I') ? "row-inserted" : ((type == 'D') ? "row-removed" : "row-updated"),
			 row, model);
		if (se)
			g_print ("signal '%s' for row %d from model %p\n",
				 (se->type == 'I') ? "row-inserted" : ((se->type == 'D') ? "row-removed" : "row-updated"),
				 se->row, se->model);
		else
			g_print ("no signal\n");
	}
#endif

	return FALSE;
}

static gboolean
check_no_expected_signal (GdaDataModel *model)
{
	SigEvent *se = NULL;
	if (signals)
		se = (SigEvent*) signals->data;
	if (se && (se->model == model)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("No signal expected and got ");
		if (se)
			g_print ("signal '%s' for row %d from model %p\n",
				 (se->type == 'I') ? "row-inserted" : ((se->type == 'D') ? "row-removed" : "row-updated"),
				 se->row, se->model);
		else
			g_print ("no signal\n");
#endif
		return FALSE;
	}

	return TRUE;
}
