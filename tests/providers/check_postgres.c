/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Daniel Espinosa <despinosa@src.gnome.org>
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
#include <stdlib.h>
#include <string.h>

#define PROVIDER "PostgreSQL"
#include "prov-test-common.h"
#include <sql-parser/gda-sql-parser.h>
#include "../test-errors.h"

#define CHECK_EXTRA_INFO 1

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;
extern gboolean         fork_tests;

//static int test_timestamp_change_format (void);

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	int number_failed = 0;
	fork_tests = FALSE;
	gchar **env;
	const gchar *cnc_string;

	env = g_get_environ ();
	cnc_string = g_environ_getenv (env, "POSTGRESQL_DBCREATE_PARAMS");
	if (cnc_string == NULL) {
		g_message ("No enviroment variable POSTGRESQL_DBCREATE_PARAMS was set. No PostgreSQL provider tests will be performed."
		          "Set this environment variable in order to get access to your server. Example: export POSTGRESQL_DBCREATE_PARAMS=\"HOST=postgres;ADM_LOGIN=$POSTGRES_USER;ADM_PASSWORD=$POSTGRES_PASSWORD\"");
		g_strfreev (env);
		return EXIT_SUCCESS;
	}

	gda_init ();

	pinfo = gda_config_get_provider_info (PROVIDER);
	if (!pinfo) {
		g_warning ("Could not find provider information for %s", PROVIDER);
		g_strfreev (env);
		return EXIT_SUCCESS;
	}
	g_print ("============= Provider now testing: %s =============\n", pinfo->id);
	
	number_failed = prov_test_common_setup ();

	if (cnc) {
		number_failed += prov_test_common_check_timestamp ();
		number_failed += prov_test_common_check_date ();
		// Timestamp format can't be changed by provider because it doesn't render the text
    // once the value has been retorned
    // number_failed += test_timestamp_change_format ();
		number_failed += prov_test_common_check_meta_full ();
		// number_failed += prov_test_common_check_meta_partial (); FIXME: Disable because timeouts
		number_failed += prov_test_common_check_meta_identifiers (TRUE, TRUE);
		number_failed += prov_test_common_check_meta_identifiers (TRUE, FALSE);
		number_failed += prov_test_common_check_meta_identifiers (FALSE, TRUE);
		number_failed += prov_test_common_check_meta_identifiers (FALSE, FALSE);
		number_failed += prov_test_common_load_data ();
		number_failed += prov_test_common_check_cursor_models ();
		number_failed += prov_test_common_check_data_select ();
    number_failed += prov_test_common_values ();
		number_failed += prov_test_common_clean ();
    number_failed += priv_test_common_simultaneos_connections ();
	}

	g_strfreev (env);
	g_print ("Test %s\n", (number_failed == 0) ? "Ok" : "failed");
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* static int */
/* test_timestamp_change_format (void) */
/* { */
/* 	GdaSqlParser *parser = NULL; */
/* 	GdaStatement *stmt = NULL; */
/* 	GError *error = NULL; */
/* 	int number_failed = 0; */
/* 	GValue *value = NULL; */

/* #ifdef CHECK_EXTRA_INFO */
/* 	g_print ("\n============= %s() =============\n", __FUNCTION__); */
/* #endif */

/* 	parser = gda_connection_create_parser (cnc); */
/* 	if (!parser) */
/* 		parser = gda_sql_parser_new (); */

	/* change date style */
/* 	stmt = gda_sql_parser_parse_string (parser, "SET datestyle to 'SQL'", */
/* 					    NULL, &error); */
/* 	if (!stmt || */
/* 	    (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error) == -1)) { */
/* 		number_failed ++; */
/* 		goto out; */
/* 	} */
/* 	g_print ("Changed datestyle to SQL\n"); */

	/* Check that date format has changed */
/* 	GdaDataHandler *dh; */
/* 	gchar *str, *estr; */
/* 	GDate *date = NULL; */
/* 	dh = gda_server_provider_get_data_handler_g_type (gda_connection_get_provider (cnc), cnc, G_TYPE_DATE); */
/* 	date = g_date_new_dmy (28, G_DATE_SEPTEMBER, 2013); */
/* 	g_value_set_boxed ((value = gda_value_new (G_TYPE_DATE)), date); */
/* 	g_date_free (date); */
/* 	str = gda_data_handler_get_str_from_value (dh, value); */
/* 	estr = "09/28/2013"; */
/* 	if (strcmp (str, estr)) { */
/* 		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC, */
/* 			     "GdaDataHandler converted date to STR in an unexpected way: got '%s' and expected '%s'", str, estr); */
/* 		number_failed ++; */
/* 		goto out; */
/* 	} */
/* 	g_free (str); */

/* 	str = gda_data_handler_get_sql_from_value (dh, value); */
/* 	estr = "'09/28/2013'"; */
/* 	if (strcmp (str, estr)) { */
/* 		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC, */
/* 			     "GdaDataHandler converted date to SQL in an unexpected way: got '%s' and expected '%s'", str, estr); */
/* 		number_failed ++; */
/* 		goto out; */
/* 	} */
/* 	g_free (str); */

	/* change date style */
/* 	stmt = gda_sql_parser_parse_string (parser, "SET datestyle to 'ISO'", */
/* 					    NULL, &error); */
/* 	if (!stmt || */
/* 	    (gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error) == -1)) { */
/* 		number_failed ++; */
/* 		goto out; */
/* 	} */
/* 	g_print ("Changed datestyle to ISO\n"); */

	/* Check that date format has changed */
/* 	str = gda_data_handler_get_str_from_value (dh, value); */
/* 	estr = "2013-09-28"; */
/* 	if (strcmp (str, estr)) { */
/* 		g_set_error (&error, TEST_ERROR, TEST_ERROR_GENERIC, */
/* 			     "GdaDataHandler converted date in an unexpected way: got '%s' and expected '%s'", str, estr); */
/* 		number_failed ++; */
/* 		goto out; */
/* 	} */
/* out: */
/* 	if (value) */
/* 		gda_value_free (value); */
/* 	if (stmt) */
/* 		g_object_unref (stmt); */
/* 	g_object_unref (parser); */

/* #ifdef CHECK_EXTRA_INFO */
/* 	g_print ("Date format test resulted in %d error(s)\n", number_failed); */
/* 	if (number_failed != 0) */
/* 		g_print ("error: %s\n", error && error->message ? error->message : "No detail"); */
/* 	if (error) */
/* 		g_error_free (error); */
/* #endif */

/* 	return number_failed; */
/* } */
