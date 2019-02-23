/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <sql-parser/gda-sql-parser.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#define NTHREADS 3

typedef struct {
	GMutex             mutex;
	GPrivate          *thread_priv;
	xmlDocPtr          doc;
	xmlNodePtr         node;
	gboolean           all_done;
	GHashTable        *parsers_hash;
	GdaSqlParserMode   current_tested_mode;
} ThData;

static GPrivate priv_treads[20] = {
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
  G_PRIVATE_INIT (g_free),
};

typedef struct {
	gint ntests;
	gint nfailures;
} ThPrivate;

static GdaSqlParser *create_parser_for_provider (const gchar *prov_name);
static gint do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *sql, const xmlChar *expected, 
		     const xmlChar *error_line, const xmlChar *error_col);

static void set_mode_foreach (gpointer key, gpointer value, gpointer pmode);
static gpointer start_thread (ThData *data);


int 
main (int argc, char** argv)
{
	xmlDocPtr doc;
	GdaSqlParser *parser;
	gint failures = 0;
	gint ntests = 0;
	gchar *fname;
	GHashTable *parsers_hash;
	GdaDataModel *providers_model;
	gint i;

	gda_init ();

	/* load file */
	fname = g_build_filename (ROOT_DIR, "tests", "parser", "testdata.xml", NULL);
	if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", fname);
                exit (1);
        }

	/* create parsers */
	parsers_hash = g_hash_table_new (g_str_hash, g_str_equal);
	providers_model = gda_config_list_providers ();
	for (i = 0; i < gda_data_model_get_n_rows (providers_model); i++) {
		const GValue *pname;
		GError *lerror = NULL;
		pname = gda_data_model_get_value_at (providers_model, 0, i, &lerror);
		if (!pname) {
			g_print ("Can't get data model's value: %s",
				 lerror && lerror->message ? lerror->message : "No detail");
			exit (1);
		}
		parser = create_parser_for_provider (g_value_get_string (pname));
		g_hash_table_insert (parsers_hash, g_strdup (g_value_get_string (pname)), parser);
		g_print ("Created parser for provider %s\n", g_value_get_string (pname));
	}
	g_object_unref (providers_model);
	g_hash_table_insert (parsers_hash, "", gda_sql_parser_new ());
	

	/* use test data */
	ThData data;
	doc = xmlParseFile (fname);
	g_free (fname);
	g_assert (doc);

	GdaSqlParserMode pmode;
	for (pmode = GDA_SQL_PARSER_MODE_PARSE; pmode <= GDA_SQL_PARSER_MODE_DELIMIT; pmode++) {
		gint j;
		g_hash_table_foreach (parsers_hash, (GHFunc) set_mode_foreach, GINT_TO_POINTER (pmode));
		for (j = 0; j < 20; j++) {
			gint i;
			GThread *threads[NTHREADS];
			
			g_mutex_init (& (data.mutex));
			data.thread_priv = &priv_treads[j];
			data.doc = doc;
			data.node = NULL;
			data.all_done = FALSE;
			data.parsers_hash = parsers_hash;
			data.current_tested_mode = pmode;
			
			for (i = 0; i < NTHREADS; i++) {
				//g_print ("Starting thread %d\n", i);
				threads [i] = g_thread_new ("ParserTh", (GThreadFunc) start_thread, &data);
			}
			
			for (i = 0; i < NTHREADS; i++) {
				ThPrivate *priv;
				//g_print ("Joining thread %d... ", i);
				priv = g_thread_join (threads [i]);
				ntests += priv->ntests;
				failures += priv->nfailures;
				//g_print ("%d tests and %d failures\n", priv->ntests, priv->nfailures);
				g_free (priv);
			}
			g_mutex_clear (& (data.mutex));
		}
	}
	xmlFreeDoc (doc);

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);
  
	return failures != 0 ? 1 : 0;
}

static void
set_mode_foreach (gpointer key, gpointer value, gpointer pmode)
{
	g_object_set (G_OBJECT (value), "mode", GPOINTER_TO_INT (pmode), NULL);
}

static gpointer
start_thread (ThData *data)
{
	while (1) {
		xmlNodePtr node; /* node to test */

		/* thread's private data */
		ThPrivate *thpriv = g_private_get (data->thread_priv);
		if (!thpriv) {
			thpriv = g_new (ThPrivate, 1);
			thpriv->ntests = 0;
			thpriv->nfailures = 0;
			g_private_set (data->thread_priv, thpriv);
		}

		/* grep next test to perform, and parser to use */
		xmlChar *prov_name;
		GdaSqlParser *parser;

		g_mutex_lock (& (data->mutex));
			
		if (data->all_done) {
			ThPrivate *copy = g_new (ThPrivate, 1);
			g_mutex_unlock (& (data->mutex));
			*copy = *thpriv;
			return copy;
		}
		if (data->node)
			data->node = data->node->next;
		else {
			xmlNodePtr root = xmlDocGetRootElement (data->doc);
			g_assert (!strcmp ((gchar*) root->name, "testdata"));
			data->node = root->children;
		}
		if (!data->node) {
			ThPrivate *copy = g_new (ThPrivate, 1);
			data->all_done = TRUE;
			g_mutex_unlock (& (data->mutex));
			*copy = *thpriv;
			return copy;
		}
		node = data->node;

		prov_name = xmlGetProp (node, BAD_CAST "provider");
		if (prov_name) {
			parser = g_hash_table_lookup (data->parsers_hash, (gchar *) prov_name);
			xmlFree (prov_name);
		}
		else
			parser = g_hash_table_lookup (data->parsers_hash, "");

		g_mutex_unlock (& (data->mutex));
		g_thread_yield ();

		/* test elected node */
		if (strcmp ((gchar*) node->name, "test"))
			continue;
		if (!parser)
			continue;

		xmlNodePtr snode;
		xmlChar *sql = NULL;
		xmlChar *id;
		
	
		id = xmlGetProp (node, BAD_CAST "id");
		for (snode = node->children; snode; snode = snode->next) {
			if (!strcmp ((gchar*) snode->name, "sql"))
				sql = xmlNodeGetContent (snode);
			else if (!strcmp ((gchar*) snode->name, "expected")) {
				xmlChar *expected;
				xmlChar *mode;

				expected = xmlNodeGetContent (snode);
				mode = xmlGetProp (snode, BAD_CAST "mode");
				if ((mode && !g_strcmp0 ((const gchar*) mode, "delim") &&
				     (data->current_tested_mode == GDA_SQL_PARSER_MODE_DELIMIT)) ||
				    ((!mode || g_strcmp0 ((const gchar*) mode, "delim")) &&
				     (data->current_tested_mode == GDA_SQL_PARSER_MODE_PARSE))) {
					if (sql) {
						thpriv->nfailures += do_test (parser, id, sql, expected, NULL, NULL);
						thpriv->ntests +=1;
					}
					else
						g_print ("===== Test '%s' doe not have any <sql> tag!\n", id);
				}
				if (expected) xmlFree (expected);
				if (mode) xmlFree (mode);
			}
			else if (!strcmp ((gchar*) snode->name, "error")) {
				xmlChar *error_line, *error_col;
				xmlChar *mode;
				mode = xmlGetProp (snode, BAD_CAST "mode");
				error_line = xmlGetProp (snode, BAD_CAST "line");
				error_col = xmlGetProp (snode, BAD_CAST "col");
				if ((mode && !g_strcmp0 ((const gchar*) mode, "delim") &&
				     (data->current_tested_mode == GDA_SQL_PARSER_MODE_DELIMIT)) ||
				    ((!mode || g_strcmp0 ((const gchar*) mode, "delim")) &&
				     (data->current_tested_mode == GDA_SQL_PARSER_MODE_PARSE))) {
					if (sql) {
						thpriv->nfailures += do_test (parser, id, sql, NULL, error_line, error_col);
						thpriv->ntests += 1;
					}
					else
						g_print ("===== Test '%s' doe not have any <sql> tag!\n", id);
				}
				
				if (mode) xmlFree (mode);
				if (error_line)	xmlFree (error_line);
				if (error_col) xmlFree (error_col);
			}
		}

		/* mem free */
		if (sql) xmlFree (sql);
		if (id)	xmlFree (id);
	}
}

/*
 * Returns: the number of failures. This function is called from several threads at the same time
 */
static gint
do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *sql, const xmlChar *expected, 
	 const xmlChar *error_line, const xmlChar *error_col)
{
	gboolean failures = 0;
#ifdef GDA_DEBUG_NO
	GdaSqlParserMode mode;
	g_object_get (G_OBJECT (parser), "mode", &mode, NULL);
	g_print ("===== TEST %s (%s mode) SQL: @%s@\n", id, 
		 mode == GDA_SQL_PARSER_MODE_PARSE ? "PARSE" : "DELIMIT", sql);
#endif
	if (expected) {
		GdaBatch *batch;
		GError *error = NULL;
		
		batch = gda_sql_parser_parse_string_as_batch (parser, (gchar *) sql, NULL, &error);
		if (batch) {
			gchar *ser;
			ser = gda_batch_serialize (batch);
			if (strcmp (ser, (gchar *) expected)) {
				g_print ("ERROR for test '%s':\n   *exp: %s\n   *got: %s\n",
					 id, expected, ser);
				failures ++;
			}
			g_free (ser);
			g_object_unref (batch);
		}
		else {
			if (error && (error->domain == GDA_SQL_PARSER_ERROR) &&
			    (error->code == GDA_SQL_PARSER_EMPTY_SQL_ERROR) && 
			    (*expected == 0))
				/* OK */;
			else {
				g_print ("ERROR for test '%s':\n   *got error: %s\n", id,
					 error && error->message ? error->message: "No detail");
				if (error)
					g_error_free (error);
				failures ++;
			}
		}
	}
	else if (error_line && error_col) {
		GdaBatch *batch;
		GError *error = NULL;
		
		/* rem: we need to use a mutex here because we actually perform 2 calls to the parser
		 * and we don't want them interrupted by a thread context change */
		gda_lockable_lock ((GdaLockable*) parser);
		batch = gda_sql_parser_parse_string_as_batch (parser, (gchar *) sql, NULL, &error);
		if (batch) {
			gchar *ser;
			ser = gda_batch_serialize (batch);
			g_print ("ERROR for test '%s':\n   *got: %s\n",
				 id, ser);
			g_free (ser);
			g_object_unref (batch);
			failures ++;
		}
		else {
			/* FIXME: test error line and col */
			gint line, col;
			g_object_get (parser, "line-error", &line, "column-error", &col, NULL);
			if ((atoi ((gchar*) error_line) != line) || (atoi ((gchar *) error_col) != col)) {
				g_print ("ERROR for test '%s':\n   *exp line=%s, col=%s\n   *got line=%d, col=%d\n",
					 id, error_line, error_col, line, col);
				failures ++;
			}
			if (error)
				g_error_free (error);
		}
		gda_lockable_unlock ((GdaLockable*) parser);
	}
	else
		g_print ("Missing or incomplete expected result for test '%s'!\n", id);	
	
	return failures;
}

static GdaSqlParser *
create_parser_for_provider (const gchar *prov_name)
{
	GdaServerProvider *prov;
	GdaSqlParser *parser;
	GError *error = NULL;

	prov = gda_config_get_provider (prov_name, &error);
	if (!prov) 
		g_error ("Could not create provider for '%s': %s\n", prov_name,
			 error && error->message ? error->message : "No detail");

	parser = gda_server_provider_create_parser (prov, NULL);
	if (!parser)
		parser = gda_sql_parser_new ();

	return parser;
}
