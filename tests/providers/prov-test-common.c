#include <stdlib.h>
#include <string.h>
#include "prov-test-common.h"
#include "prov-test-util.h"
#include <sql-parser/gda-sql-statement.h>

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

GdaProviderInfo *pinfo;
GdaConnection   *cnc;
gboolean         params_provided;
gboolean         db_created;
gboolean         fork_tests = TRUE;

/*
 *
 * SETUP
 *
 */

int
prov_test_common_setup ()
{
	int number_failed = 0;
	cnc = prov_test_setup_connection (pinfo, &params_provided, &db_created);
	if (params_provided)
		fail_if (!cnc, "Could not setup connection");
	return number_failed;
}


/*
 *
 * CLEAN
 *
 */
int
prov_test_common_clean ()
{
	int number_failed = 0;

	if (!prov_test_clean_connection (cnc, db_created))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CREATE_TABLES_SQL
 *
 */
int
prov_test_common_create_tables_sql ()
{
	int number_failed = 0;
	if (!prov_test_create_tables_sql (cnc))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_META
 *
 */

int
prov_test_common_check_meta ()
{
	int number_failed = 0;
	GSList *tables, *list;
	gboolean dump_ok = TRUE;
	GdaMetaStore *store;
	gchar **dump1 = NULL;
	GError *gerror = NULL;
	gint ntables, i;

	/* update meta store */
	if (! gda_connection_update_meta_store (cnc, NULL, &gerror)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't update meta store (1): %s\n",
			   gerror && gerror->message ? gerror->message : "???");
#endif
		g_error_free (gerror);
		number_failed++;
		goto theend;
	}

	/* dump all tables */
	store = gda_connection_get_meta_store (cnc);
	tables = gda_meta_store_schema_get_all_tables (store);
	ntables = g_slist_length (tables);
	dump1 = g_new0 (gchar *, ntables + 1);

	for (i = 0, list = tables; list; i++, list = list->next) {
		GdaDataModel *model;
		gchar *tmp;
		
		tmp = g_strdup_printf ("SELECT * FROM %s", (gchar*) list->data);
		model = gda_meta_store_extract (store, tmp, &gerror, NULL);
		g_free (tmp);
		if (!model) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't execute SELECT statement: %s\n",
				   gerror && gerror->message ? gerror->message : "???");
#endif
			g_error_free (gerror);
			dump_ok = FALSE;
			break;
		}

		dump1 [i] = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							     NULL, 0, NULL, 0, NULL);
		g_object_unref (model);
		if (!dump1 [i]) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't export data model\n");
#endif
			dump_ok = FALSE;
			break;
		}
	}

	if (!dump_ok) {
		number_failed++;
		goto theend;
	}

	/* update meta store */
	if (! gda_connection_update_meta_store (cnc, NULL, &gerror)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't update meta store (2): %s\n",
			   gerror && gerror->message ? gerror->message : "???");
#endif
		g_error_free (gerror);
		number_failed++;
		goto theend;
	}

	for (i = 0, list = tables; list; i++, list = list->next) {
		GdaDataModel *model;
		gchar *tmp;
		GError *gerror = NULL;
		
		tmp = g_strdup_printf ("SELECT * FROM %s", (gchar*) list->data);
		model = gda_meta_store_extract (store, tmp, &gerror, NULL);
		g_free (tmp);
		if (!model) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't execute SELECT statement: %s\n",
				   gerror && gerror->message ? gerror->message : "???");
#endif
			g_error_free (gerror);
			number_failed++;
			continue;
		}

		tmp = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
						       NULL, 0, NULL, 0, NULL);
		g_object_unref (model);
		if (!tmp) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Can't export data model\n");
#endif
			number_failed++;
			continue;
		}
		if (strcmp (tmp, dump1[i])) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Meta data has changed after update for table %s\n", (gchar*) list->data);
#endif
			number_failed++;
			g_free (tmp);
			continue;
		}
#ifdef CHECK_EXTRA_INFO
		else
			g_print ("Meta for table '%s' Ok\n", (gchar*) list->data);
#endif
		g_free (tmp);
	}

 theend:
	/* remove tmp files */
	if (dump1)
		g_strfreev (dump1);
	g_slist_free (tables);

	return number_failed;	
}

/*
 *
 * LOAD_DATA
 *
 */
int
prov_test_common_load_data ()
{
	int number_failed = 0;
	if (!prov_test_load_data (cnc, "actor"))
		number_failed++;
	if (!prov_test_load_data (cnc, "language"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film_actor"))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_CURSOR_MODELS
 *
 */
int
prov_test_common_check_cursor_models ()
{
	int number_failed = 0;
	if (!prov_test_check_table_cursor_model (cnc, "actor"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "language"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film_actor"))
		number_failed++;
	if (!prov_test_check_types_schema (cnc))
		number_failed++;

	return number_failed;	
}
