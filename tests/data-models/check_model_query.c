#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
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
static gboolean check_set_value_at_ext (GdaDataModel *model, gint col, gint row, 
					const GValue *set_value, 
					GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params, GError **error);
static gboolean check_set_values (GdaDataModel *model, gint row, GList *set_values,
				  GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params);
static gint check_append_values (GdaDataModel *model, GList *set_values,
				 GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params);
static gboolean compare_data_models (GdaDataModel *model1, GdaDataModel *model2, GError **error);

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

	/* set up test environment */
        g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, 0);
	g_setenv ("GDA_TOP_SRC_DIR", TOP_SRC_DIR, TRUE);
	gda_init ();

	g_unlink ("modelquery.db");
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
        cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=modelquery", NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
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

	/* create GdaDataModelQuery */
	stmt = stmt_from_string ("SELECT * FROM customers WHERE id= ##theid::gint");
	model = (GdaDataModel*) gda_data_model_query_new (cnc, stmt, NULL);
	g_assert (model);
	if (gda_data_model_get_n_rows (model) >= 0) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Returned n_rows should be -1\n");
#endif
		goto out;
	}

	if (gda_data_model_get_value_at (model, 0, 0, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_get_value_at() should return NULL\n");
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
 * check gda_data_model_set_value_at()
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
	GValue *value;

	clear_signals ();

	/* create GdaDataModelQuery */
	stmt = stmt_from_string ("SELECT * FROM customers");
	model = (GdaDataModel*) gda_data_model_query_new (cnc, stmt, NULL);

	/* test INSERT with undefined params */
	mod_stmt = stmt_from_string ("UPDATE customers SET name = ##+1::string, last_update = ##+2::timestamp WHERE id = ##-0::gint");
	if (!gda_data_model_query_set_modification_statement (GDA_DATA_MODEL_QUERY (model), mod_stmt, &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_query_set_modification_statement() should have succedded, error: %s\n",
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
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "Jack");
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
	error = NULL;

	GdaDataModel *copy;
	copy = (GdaDataModel *) gda_data_model_array_copy_model (model, &error);
	if (!copy) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not copy data model: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	if (! gda_data_model_query_refresh (GDA_DATA_MODEL_QUERY (model), &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_query_refresh() failed: %s\n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	if (! compare_data_models (model, copy, NULL)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Data model differs after a refresh\n");
#endif
		goto out;
	}
	g_object_unref (copy);

 out:
	g_object_unref (model);
	g_object_unref (stmt);

	return nfailed;
}

/*
 * - Create a GdaDataModelQuery with a missing parameter
 * - Set modification statements
 * - Refresh data model and compare with direct SELECT.
 * 
 * Returns the number of failures 
 */
static gint
test3 (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaDataModel *model;
	GdaStatement *stmt;
	GdaSet *params;
	gint nfailed = 0;

	/* create GdaDataModelQuery */
	stmt = stmt_from_string ("SELECT * FROM customers WHERE id <= ##theid::gint");
	g_assert (gda_statement_get_parameters (stmt, &params, NULL));
	model = (GdaDataModel*) gda_data_model_query_new (cnc, stmt, params);
	g_assert (model);
	if (gda_data_model_get_n_rows (model) >= 0) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Returned n_rows should be -1\n");
#endif
		goto out;
	}

	gda_data_model_dump (model, stdout);
	if (! gda_set_set_holder_value (params, &error, "theid", 9)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Can't set 'theid' value: %s \n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	gda_data_model_dump (model, stdout);

	if (! gda_set_set_holder_value (params, &error, "theid", 4)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("Can't set 'theid' value: %s \n",
			 error && error->message ? error->message : "No detail");
#endif
		goto out;
	}
	gda_data_model_dump (model, stdout);

	if (! gda_data_model_query_compute_modification_statements (GDA_DATA_MODEL_QUERY (model), &error)) {
		nfailed++;
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_query_compute_modification_statements() failed: %s\n",
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
 * Checking value function:
 *  - reads the value of @model at the provided column and row and compares with the expected @set_value
 *  - if @stmt is not NULL, then re-run the statement and compares with @model
 */
static gboolean
check_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *set_value, 
		    GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params)
{
	GError *error = NULL;
	gboolean retval;
	retval = check_set_value_at_ext (model, col, row, set_value, cnc, stmt, stmt_params, &error);
	if (error)
		g_error_free (error);
	return retval;
}

static gboolean
check_set_value_at_ext (GdaDataModel *model, gint col, gint row, 
			const GValue *set_value, 
			GdaConnection *cnc, GdaStatement *stmt, GdaSet *stmt_params, GError **error)
{
	const GValue *get_value;

	if (! gda_data_model_set_value_at (model, col, row, set_value, error)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("gda_data_model_set_value_at(%d,%d) failed: %s\n",
			 col, row, 
			 error && *error && (*error)->message ? (*error)->message : "No detail");
#endif
		return FALSE;
	}
	get_value = gda_data_model_get_value_at (model, col, row, error);
	if (!get_value) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Can't get data model's value: %s\n",
			 error && *error && (*error)->message ? (*error)->message : "No detail");
#endif
		return FALSE;
	}
	if (gda_value_compare (get_value, set_value)) {
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
		gboolean cmpres = TRUE;
		rerun = gda_connection_statement_execute_select (cnc, stmt, stmt_params, error);
		if (!rerun) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not re-run the SELECT statement: %s\n",
				 error && *error && (*error)->message ? (*error)->message : "No detail");
#endif
			return FALSE;
		}

		cmp = (GdaDataComparator*) gda_data_comparator_new (model, rerun);
		if (! gda_data_comparator_compute_diff (cmp, error)) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Could not compute the data model differences: %s\n",
				 error && *error && (*error)->message ? (*error)->message : "No detail");
#endif
			cmpres = FALSE;
		}
		if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
			g_print ("There are some differences when re-running the SELECT statement...\n");
#endif
			cmpres = FALSE;
			gda_data_model_dump (model, stdout);
			gda_data_model_dump (rerun, stdout);
			if (error) {
				g_set_error (error, 0, -1,
					     "There are some differences when re-running the SELECT statement...");
			}
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
		
		get_value = gda_data_model_get_value_at (model, i, row, &error);
		if (!get_value) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Can't get data model's value: %s",
				 error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}
		if (gda_value_compare (get_value, (GValue*) list->data)) {
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
		
		get_value = gda_data_model_get_value_at (model, i, newrow, &error);
		if (!get_value) {
#ifdef CHECK_EXTRA_INFO
			g_print ("Can't get data model's value: %s",
				 error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}
		if (gda_value_compare (get_value, (GValue*) list->data)) {
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

static gboolean
compare_data_models (GdaDataModel *model1, GdaDataModel *model2, GError **error)
{
	GdaDataComparator *cmp;
	GError *lerror = NULL;
	cmp = (GdaDataComparator*) gda_data_comparator_new (model1, model2);
	if (! gda_data_comparator_compute_diff (cmp, &lerror)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("Could not compute the data model differences: %s\n",
			 lerror && lerror->message ? lerror->message : "No detail");
#endif
		goto onerror;
	}
	if (gda_data_comparator_get_n_diffs (cmp) != 0) {
#ifdef CHECK_EXTRA_INFO
		g_print ("There are some differences when comparing data models...\n");
		g_print ("Model1 is:\n");
		gda_data_model_dump (model1, stdout);
		g_print ("Model2 is:\n");
		gda_data_model_dump (model2, stdout);
#endif
		goto onerror;
	}
	g_object_unref (cmp);

	return TRUE;

 onerror:
	g_propagate_error (error, lerror);
	return FALSE;
}
