#include <string.h>
#include "test-util.h"
 
static SqlTest *load_test_from_xml_node (xmlNodePtr node);

GArray *
sql_tests_load_from_file (const gchar *xml_file)
{
	xmlDocPtr doc;
        xmlNodePtr node, subnode;
	GArray *array;

        if (! g_file_test (xml_file, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", xml_file);
                exit (EXIT_FAILURE);
        }

        doc = xmlParseFile (xml_file);
	if (!doc) {
		g_print ("'%s' file format error\n", xml_file);
		exit (EXIT_FAILURE);
	}

        node = xmlDocGetRootElement (doc);
        if (strcmp ((gchar*)node->name, "unit")) {
                g_print ("XML file '%s' root node is not <unit>\n", xml_file);
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
			prop = xmlGetProp (subnode, "parsed");
			if (prop) {
				test->parsed = (*prop == '1') ? TRUE : FALSE;
				xmlFree (prop);
			}
			prop = xmlNodeGetContent (subnode);
			if (prop) {
				test->sql_to_test = g_strdup (prop);
				xmlFree (prop);
			}
		}
		else if (!strcmp ((gchar*)subnode->name, "render_as")) {
			prop = xmlNodeGetContent (subnode);
			if (prop) {
				test->sql_rendered = g_strdup (prop);
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
					if (prop) {
						param->nullok = (*prop == '1') ? TRUE : FALSE;
						xmlFree (prop);
					}
					prop = xmlGetProp (params, "isparam");
					if (prop) {
						param->isparam = (*prop == '1') ? TRUE : FALSE;
						xmlFree (prop);
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
