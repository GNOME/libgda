/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#ifdef USING_MINGW
#define _NO_OLDNAMES
#endif
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>

#define PROVNAME "org.postgresql.Driver"
#define CNCSTRING "jdbc:postgresql:sales"
#define USERNAME "vmalerba"

#define H2_PROVNAME "org.h2.Driver"
#define H2_CNCSTRING "jdbc:h2:h2data"
#define H2_USERNAME "h2"

static void test_generic (const gchar *prov_name, const gchar *cnc_string, const gchar *username);
static void test_h2 (void);

int
main (int argc, char **argv)
{
	const gchar *prov_name, *cnc_string, *username;

	/* set up test environment */
	g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, 0);
	gda_init ();

	/* generic test */
	if (argc != 1) {
		if (argc != 4) {
			g_print ("%s [<JDBC driver> <cnc string> <username>]\n", argv [0]);
			return 1;
		}
		prov_name = argv [1];
		cnc_string = argv [2];
		username = argv [3];
	}
	else {
		prov_name = PROVNAME;
		cnc_string = CNCSTRING;
		username = USERNAME;
	}

	test_generic (prov_name, cnc_string, username);
	test_h2 ();
	g_print ("OK\n");
	return 0;
}

static void
run_select (GdaConnection *cnc, const gchar *sql)
{
	static GdaSqlParser *parser = NULL;
	GdaStatement *stmt;
	GdaDataModel *model;
	GError *error = NULL;	

	if (!parser)
		parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Error executing statement: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	gda_data_model_dump (model, stdout);
	g_object_unref (stmt);
	g_object_unref (model);
}

static void
test_h2 (void)
{
	GError *error = NULL;
	GdaConnection *cnc;
	gchar *real_auth, *tmp;

	tmp = gda_rfc1738_encode (H2_USERNAME);
	real_auth = g_strdup_printf ("USERNAME=%s", tmp);
	g_free (tmp);
	cnc = gda_connection_new_from_string (H2_PROVNAME, "URL=" H2_CNCSTRING, real_auth,
					      GDA_CONNECTION_OPTIONS_NONE, &error);
	g_free (real_auth);
	if (!cnc) {
		g_print ("Could create connection with the '%s' provider: %s\n", "org.h2.Driver",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	if (!gda_connection_open (cnc, &error)) {
		g_print ("Could open connection with the '%s' provider: %s\n", "org.h2.Driver",
			 error && error->message ? error->message : "No detail");
		g_object_unref (cnc);
		exit (1);
	}
	run_select (cnc, "SELECT * FROM t_string");
	run_select (cnc, "SELECT * FROM t_numbers");
	run_select (cnc, "SELECT * FROM t_misc");
	run_select (cnc, "SELECT * FROM t_bin");
	run_select (cnc, "SELECT * FROM t_others");
	

	g_object_unref (cnc);
}

static void
test_generic (const gchar *prov_name, const gchar *cnc_string, const gchar *username)
{
	GdaServerProvider *prov;
	GError *error = NULL;
	GdaConnection *cnc;


	/* pick up provider */
	prov = gda_config_get_provider (prov_name, &error);
	if (!prov) {
		g_print ("Could not get the '%s' provider: %s\n", prov_name,
			 error && error->message ? error->message : "No detail");
		return;
	}

	/* open connection - failure */
	g_print ("TEST: Connection creating with failure...\n");
	cnc = gda_connection_new_from_string (prov_name, "URL=hello", NULL,
					      GDA_CONNECTION_OPTIONS_NONE, &error);
	if (cnc) {
		g_print ("Connection creating should have failed...\n");
		exit (1);
	}
	g_print ("Connection creating error: %s\n",
		 error && error->message ? error->message : "No detail");
	if (error) {
		g_error_free (error);
		error = NULL;
	}

	/* open connection - success */
	g_print ("\nTEST: Connection opening with success...\n");
	gchar *real_cnc_string, *tmp;
	tmp = gda_rfc1738_encode (cnc_string);
	real_cnc_string = g_strdup_printf ("URL=%s", tmp);
	g_free (tmp);

	gchar *real_auth;
	tmp = gda_rfc1738_encode (username);
	real_auth = g_strdup_printf ("USERNAME=%s", tmp);
	g_free (tmp);

	g_print ("CNC STRING: %s\n", real_cnc_string);
	cnc = gda_connection_new_from_string (prov_name, real_cnc_string, real_auth,
					      GDA_CONNECTION_OPTIONS_NONE, &error);
	g_free (real_cnc_string);
	g_free (real_auth);
	if (!cnc) {
		g_print ("Could create connection with the '%s' provider: %s\n", prov_name,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	if (! gda_connection_open (cnc, &error)) {
		g_print ("Could open connection with the '%s' provider: %s\n", prov_name,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	if (! gda_connection_close (cnc, &error)) {
		g_print ("Connection closing error: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	if (! gda_connection_open (cnc, &error)) {
		g_print ("Connection re-opening error: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	prov = gda_connection_get_provider (cnc);
	g_print ("Server version: %s\n", gda_server_provider_get_server_version (prov, cnc));
	g_print ("Server name   : %s\n", gda_server_provider_get_name (prov));

	/* prepared statement */
	GdaSqlParser *parser;
	GdaStatement *stmt;
	g_print ("\nTEST: prepared statement\n");
	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser, "SELECT * FROM customers", NULL, NULL);
	g_assert (stmt);
	if (! gda_connection_statement_prepare (cnc, stmt, &error)) {
		g_print ("Error preparing statement: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	g_print ("\nTEST: execute SELECT statement\n");
	GdaDataModel *model;
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Error executing statement: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	gda_data_model_dump (model, stdout);
	g_object_unref (stmt);
	g_object_unref (model);


	g_print ("\nTEST: execute SELECT statement - 2\n");
	stmt = gda_sql_parser_parse_string (parser, "SELECT * FROM orders", NULL, NULL);
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Error executing statement: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	gda_data_model_dump (model, stdout);
	g_object_unref (model);

	g_object_unref (stmt);
	g_object_unref (cnc);
}
