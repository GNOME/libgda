/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-parser.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

GdaConnection *cnc;
static gint do_test (const xmlChar *id, const xmlChar *sql, const xmlChar *norm);

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	xmlDocPtr doc;
        xmlNodePtr root, node;
	gint failures = 0;
	gint ntests = 0;
	gchar *fname;

	gda_init ();

	/* open connection */
	gchar *cnc_string;
	fname = g_build_filename (ROOT_DIR, "data", NULL);
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=sales_test", fname);
	g_free (fname);
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL,
					       GDA_CONNECTION_OPTIONS_READ_ONLY, NULL);
	if (!cnc) {
		g_print ("Failed to open connection, cnc_string = %s\n", cnc_string);
		exit (1);
	}
	if (!gda_connection_update_meta_store (cnc, NULL, NULL)) {
		g_print ("Failed to update meta store, cnc_string = %s\n", cnc_string);
		exit (1);
	}
	g_free (cnc_string);

	/* load file */
	fname = g_build_filename (ROOT_DIR, "tests", "parser", "testvalid.xml", NULL);
	if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", fname);
                exit (1);
        }

	/* use test data */
	doc = xmlParseFile (fname);
	g_free (fname);
	g_assert (doc);
	root = xmlDocGetRootElement (doc);
	g_assert (!strcmp ((gchar*) root->name, "testdata"));
	for (node = root->children; node; node = node->next) {
		if (strcmp ((gchar*) node->name, "test"))
			continue;
		xmlNodePtr snode;
		xmlChar *sql = NULL;
		xmlChar *id;
		xmlChar *norm = NULL;

		id = xmlGetProp (node, BAD_CAST "id");
		for (snode = node->children; snode; snode = snode->next) {
			if (!strcmp ((gchar*) snode->name, "sql")) 
				sql = xmlNodeGetContent (snode);
			if (!strcmp ((gchar*) snode->name, "normalized")) 
				norm = xmlNodeGetContent (snode);
		}
		if (sql && norm) {
			if (!do_test (id, sql, norm))
				failures++;
			ntests++;
		}

		/* mem free */
		if (sql) xmlFree (sql);
		if (norm) xmlFree (norm);
		if (id)	xmlFree (id);
	}
	xmlFreeDoc (doc);

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);
  
	return failures != 0 ? 1 : 0;
}

/*
 * Returns: the number of failures
 */
static gint
do_test (const xmlChar *id, const xmlChar *sql, const xmlChar *norm) 
{
	static GdaSqlParser *parser = NULL;
	GdaStatement *stmt;
	GError *error = NULL;
	gchar *str;

	if (!parser) {
		parser = gda_connection_create_parser (cnc);
		if (!parser)
			parser = gda_sql_parser_new ();
	}

#ifdef GDA_DEBUG
	g_print ("===== TEST %s SQL: @%s@\n", id, sql);
#endif

	stmt = gda_sql_parser_parse_string (parser, (const gchar*) sql, NULL, NULL);
	if (!stmt) {
		g_print ("ERROR for test '%s': could not parse statement\n", id);
		return FALSE;
	}
	if (!gda_statement_normalize (stmt, cnc, &error)) {
		g_print ("ERROR for test '%s': statement can't be normalized: %s\n", id,
			 error && error->message ? error->message : "No detail");
		g_object_unref (stmt);
		return FALSE;
	}

	str = gda_statement_serialize (stmt);
	if (g_strcmp0 (str, (const gchar*) norm)) {
		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		g_print ("ERROR for test '%s': \n\tEXP: %s\n\tGOT: %s\n\tSQL: %s\n", id, norm, str, sql);
		g_free (sql);
		g_free (str);
		return FALSE;
	}
	
	g_free (str);
	g_object_unref (stmt);
	return TRUE;
}
