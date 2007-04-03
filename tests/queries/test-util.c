#include <string.h>
#include "test-util.h"
 
/* returns a list of TestSuite structs. */
GSList *
test_suite_load_from_file (const gchar *xml_file)
{
	GSList *suites = NULL;

	xmlDocPtr doc;
        xmlNodePtr node, subnode;
	gchar *abs_xml_file;

	abs_xml_file = g_build_filename (CHECK_XML_FILES, "tests", "queries", xml_file, NULL);

        if (! g_file_test (abs_xml_file, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", abs_xml_file);
                exit (EXIT_FAILURE);
        }

        doc = xmlParseFile (abs_xml_file);
	if (!doc) {
		g_print ("'%s' file format error\n", abs_xml_file);
		exit (EXIT_FAILURE);
	}

        node = xmlDocGetRootElement (doc);
        if (strcmp ((gchar*)node->name, "suites")) {
                g_print ("XML file '%s' root node is not <suites>\n", abs_xml_file);
		exit (EXIT_FAILURE);
	}

        for (subnode = node->children; subnode; subnode = subnode->next) {
		if (!strcmp ((gchar*)subnode->name, "suite")) {
			TestSuite *suite1 = g_new0 (TestSuite, 1); /* without dict */
			TestSuite *suite2 = NULL; /* with dict if one is specified */
			xmlChar *prop;
			xmlNodePtr snode;

			suites = g_slist_append (suites, suite1);

			prop = xmlGetProp (subnode, "dict");
			if (prop) {
				GError *error = NULL;
				gchar *file;
				GdaDict *dict = NULL;

				dict = gda_dict_new ();
				file = g_build_filename (CHECK_DICT_FILES, "tests", "queries", prop, NULL);
				if (!gda_dict_load_xml_file (dict, file, &error)) {
					g_error ("Could not load dictionary file '%s': %s", prop,
						 error ? error->message : "No detail");
					g_object_unref (dict);
					dict = NULL;
				}
				g_free (file);
				xmlFree (prop);

				if (dict) {
					suite2 = g_new0 (TestSuite, 1);
					suites = g_slist_append (suites, suite2);
					suite2->dict = dict;
				}
			}

			prop = xmlGetProp (subnode, "name");
			if (prop) {
				suite1->name = g_strdup_printf ("%s (no dictionary)", prop);
				if (suite2)
					suite2->name = g_strdup_printf ("%s (with dictionary)", prop);
				xmlFree (prop);
			}
			else
				g_error ("File '%s' contains an unnamed test suite", abs_xml_file);
			
			suite1->unit_files = g_array_new (FALSE, FALSE, sizeof (gchar *));
			suite1->unit_tests = g_array_new (FALSE, FALSE, sizeof (GArray *));
			if (suite2) {
				suite2->unit_files = g_array_new (FALSE, FALSE, sizeof (gchar *));
				suite2->unit_tests = g_array_new (FALSE, FALSE, sizeof (GArray *));
			}
			for (snode = subnode->children; snode; snode = snode->next) {
				if (!strcmp ((gchar*)snode->name, "unit")) {
					prop = xmlGetProp (snode, "file");
					if (prop) {
						gchar *str = g_strdup (prop);
						g_array_append_val (suite1->unit_files, str);
						if (suite2) {
							str = g_strdup (prop);
							g_array_append_val (suite2->unit_files, str);
						}

						GArray *tests;
						gint i;
						tests = sql_tests_load_from_file (str);
						for (i = 0; i < tests->len; i++)
							g_array_index (tests, SqlTest *, i)->suite = suite1;
						g_array_append_val (suite1->unit_tests, tests);

						if (suite2) {
							tests = sql_tests_load_from_file (str);
							for (i = 0; i < tests->len; i++)
								g_array_index (tests, SqlTest *, i)->suite = suite2;
							g_array_append_val (suite2->unit_tests, tests);
						}

						xmlFree (prop);
					}
				}
			}
		}
	}
	xmlFreeDoc (doc);
	g_free (abs_xml_file);

	return suites;
}

static SqlTest *load_test_from_xml_node (xmlNodePtr node);

GArray *
sql_tests_load_from_file (const gchar *xml_file)
{
	xmlDocPtr doc;
        xmlNodePtr node, subnode;
	GArray *array;
	gchar *abs_xml_file;

	abs_xml_file = g_build_filename (CHECK_XML_FILES, "tests", "queries", xml_file, NULL);

        if (! g_file_test (abs_xml_file, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", abs_xml_file);
                exit (EXIT_FAILURE);
        }

        doc = xmlParseFile (abs_xml_file);
	if (!doc) {
		g_print ("'%s' file format error\n", abs_xml_file);
		exit (EXIT_FAILURE);
	}

        node = xmlDocGetRootElement (doc);
        if (strcmp ((gchar*)node->name, "unit")) {
                g_print ("XML file '%s' root node is not <unit>\n", abs_xml_file);
		exit (EXIT_FAILURE);
	}

	array = g_array_new (FALSE, FALSE, sizeof (SqlTest *));
        for (subnode = node->children; subnode; subnode = subnode->next) {
		if (!strcmp ((gchar*)subnode->name, "test")) {
			SqlTest *test;
			test = load_test_from_xml_node (subnode);
			g_array_append_val (array, test);
		}
	}
	xmlFreeDoc (doc);
	g_free (abs_xml_file);

	return array;
}

static SqlTest *
load_test_from_xml_node (xmlNodePtr node)
{
	SqlTest *test;
	xmlNodePtr subnode;
	xmlChar *prop;

	test = g_new0 (SqlTest, 1);

	prop = xmlGetProp (node, "name");
	if (prop) {
		test->test_name = g_strdup (prop);
		xmlFree (prop);
	}

	for (subnode = node->children; subnode; subnode = subnode->next) {
		if (!strcmp ((gchar*)subnode->name, "sql")) {
			prop = xmlGetProp (subnode, "expected");
			if (prop) {
				if (*prop == 'D')
					test->delim_parsed = TRUE;
				else if (*prop == 'S')
					test->sql_parsed = TRUE;
				else if (*prop == 'A')
					test->sql_active = TRUE;
					
				if (test->sql_active)
					test->sql_parsed = TRUE;
				if (test->sql_parsed)
					test->delim_parsed = TRUE;

				xmlFree (prop);
			}
			prop = xmlGetProp (subnode, "n_stmts");
			if (prop) {
				test->n_statements = atoi (prop);
				xmlFree (prop);
			}
			else
				test->n_statements = 1;
			prop = xmlNodeGetContent (subnode);
			if (prop) {
				test->sql_to_test = g_strdup (prop);
				xmlFree (prop);
			}
		}
		else if (!strcmp ((gchar*)subnode->name, "render_as")) {
			prop = xmlNodeGetContent (subnode);
			if (prop) {
				SqlRendering *ren;
				SqlRenderingAttr attr = 0;
				xmlChar *p2;

				p2 = xmlGetProp (subnode, "when_dict");
				if (p2) {
					if ((*p2 == 't') || (*p2 == 'T') || (*p2 == '1'))
						attr = TEST_RENDERING_WHEN_DICT;
					else
						attr = TEST_RENDERING_WHEN_NO_DICT;
					xmlFree (p2);
				}
				else
					attr = TEST_RENDERING_WHEN_ANY_DICT;

				p2 = xmlGetProp (subnode, "when_status");
				if (p2) {
					if (*p2 == 'D')
						attr |= TEST_RENDERING_SQL_DELIMITER;
					else if (*p2 == 'P')
						attr |= TEST_RENDERING_SQL_PARSER;
					else if (*p2 == 'A')
						attr |= TEST_RENDERING_SQL_ACTIVE;
						
					xmlFree (p2);
				}
				else
					attr |= TEST_RENDERING_SQL_ANY;

				p2 = xmlGetProp (subnode, "when_query");
				if (p2) {
					if ((*p2 == 't') || (*p2 == 'T') || (*p2 == '1'))
						attr |= TEST_RENDERING_GDA_QUERY;
					else
						attr |= TEST_RENDERING_GDA_DELIMITER;
						
					xmlFree (p2);
				}
				else
					attr |= TEST_RENDERING_GDA_ANY;

				if (!test->renderings)
					test->renderings = g_array_new (FALSE, FALSE, sizeof (SqlRendering*));

				ren = sql_test_rendering_new (attr, prop);
				g_array_append_val (test->renderings, ren);
				xmlFree (prop);
			}
		}
		else if (!strcmp ((gchar*)subnode->name, "params")) {
			xmlNodePtr params;
			for (params = subnode->children; params; params = params->next) {
				if (!strcmp ((gchar*)params->name, "param")) {
					SqlParam *param = g_new0 (SqlParam, 1);
					test->params = g_slist_append (test->params, param);

					prop = xmlGetProp (params, "name");
					if (prop) {
						param->name = g_strdup (prop);
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "type");
					if (prop) {
						param->type = g_strdup (prop);
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "descr");
					if (prop) {
						param->descr = g_strdup (prop);
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "nullok");
					param->nullok = TRUE;
					if (prop) {
						param->nullok = (*prop == '1') ? TRUE : FALSE;
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "isparam");
					if (prop) {
						param->isparam = (*prop == '1') ? TRUE : FALSE;
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "default");
					if (prop) {
						GType deftype = G_TYPE_INVALID;

						param->default_string = g_strdup (prop);
						xmlFree (prop);

						prop = xmlGetProp (params, "default_type");
						if (prop) {
							deftype = gda_g_type_from_string (prop);
							if (deftype == G_TYPE_INVALID)
								g_error ("Unknown type '%s'", prop);
							xmlFree (prop);
						}
						else {
							prop = xmlGetProp (params, "type");
							if (prop) {
								deftype = gda_g_type_from_string (prop);
								if (deftype == G_TYPE_INVALID)
									g_error ("Unknown type '%s'", prop);
								xmlFree (prop);
							}
						}
						if (deftype != G_TYPE_INVALID) {
							/* define a GValue */
							param->default_gvalue = 
								gda_value_new_from_string (param->default_string, deftype);
						}
					}
				}
			}
		}
	}

	return test;
}

void
sql_param_free (SqlParam *param)
{
	g_free (param->name);
	g_free (param->type);
	g_free (param->descr);
	g_free (param->default_string);
	if (param->default_gvalue)
		gda_value_free (param->default_gvalue);
	g_free (param);
}

SqlParam *
sql_tests_take_param (SqlTest *test, const gchar *pname)
{
	GSList *list;
	SqlParam *param = NULL;

	for (list = test->params; list && !param; list = list->next) {
		if ((!pname && !((SqlParam*) (list->data))->name) || 
		    !strcmp (((SqlParam*) (list->data))->name, pname))
			param = (SqlParam*) (list->data);
	}
	if (param)
		test->params = g_slist_remove (test->params, param);
	return param;
}

gboolean
sql_test_rendering_is_correct (SqlTest *test, const gchar *sql)
{
	gboolean retval = FALSE;
	gint i;

	if (!strcmp (test->sql_to_test, sql))
		retval = TRUE;

	for (i = 0; !retval && (i < (test->renderings ? test->renderings->len : 0)); i++) {
		SqlRendering *ren;
		ren = g_array_index (test->renderings, SqlRendering *, i);
		if (((ren->attrs & TEST_RENDERING_WHEN_DICT) && test->suite->dict) ||
		    ((ren->attrs & TEST_RENDERING_WHEN_NO_DICT) && !test->suite->dict) ||
		    ((ren->attrs & TEST_RENDERING_WHEN_ANY_DICT) == TEST_RENDERING_WHEN_ANY_DICT)) {
			if (((ren->attrs & TEST_RENDERING_SQL_DELIMITER) && test->delim_parsed) ||
			    ((ren->attrs & TEST_RENDERING_SQL_PARSER) && test->sql_parsed) ||
			    ((ren->attrs & TEST_RENDERING_SQL_ACTIVE) && test->sql_active) ||
			    ((ren->attrs & TEST_RENDERING_SQL_ANY) == TEST_RENDERING_SQL_ANY)) {
				if (((ren->attrs & TEST_RENDERING_GDA_QUERY) && (test->sql_parsed || test->sql_active)) ||
				    ((ren->attrs & TEST_RENDERING_GDA_DELIMITER) && !(test->sql_parsed || test->sql_active)) ||
				    ((ren->attrs & TEST_RENDERING_GDA_ANY) == TEST_RENDERING_GDA_ANY))
					if (!strcmp (ren->sql, sql))
						retval = TRUE;
			}
		}
	}

	return retval;
}

SqlRendering *
sql_test_rendering_new (SqlRenderingAttr attrs, const gchar *sql)
{
	SqlRendering *ren;

	ren = g_new (SqlRendering, 1);
	ren->attrs = attrs;
	ren->sql = g_strdup (sql);
	return ren;
}

void
sql_test_rendering_free (SqlRendering *ren)
{
	g_free (ren->sql);
	g_free (ren);
}
