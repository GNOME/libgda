#include <libgda/libgda.h>
#include <gda-report-engine.h>

GdaConnection *open_connection (GdaClient *client);
GSList        *create_queries (GdaDict *dict);

int 
main (int argc, char **argv)
{
	gchar *spec;
	GdaReportEngine *eng;
	GdaClient *client;
	GdaDict *dict;
	GdaConnection *cnc;
	GdaParameter *param;

	gda_init ("Customers report example", "3.1.1", argc, argv);

	/* Engine object */
	g_assert (g_file_get_contents ("customers-report-spec.xml", &spec, NULL, NULL));
	eng = gda_report_engine_new_from_string (spec);
	g_free (spec);

	/* GdaConnection */
	client = gda_client_new ();
	cnc = open_connection (client);
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnc);
	gda_report_engine_declare_object (eng, G_OBJECT (cnc), "main_cnc");
	gda_report_engine_declare_object (eng, G_OBJECT (dict), "main_dict");

	/* define parameters */
	param = gda_parameter_new_string ("abstract", "-- This text is from a parameter set in the code, not in the spec. file --");
	gda_report_engine_declare_object (eng, G_OBJECT (param), "abstract");
	g_object_unref (param);

	/* create queries */
	GSList *queries, *list;
	queries = create_queries (dict);
	for (list = queries; list; list = list->next) {
		gda_report_engine_declare_object (eng, G_OBJECT (list->data), gda_object_get_name (GDA_OBJECT (list->data)));
		g_object_unref (G_OBJECT (list->data));
	}
	g_slist_free (queries);

	/* run the report */
	GError *error = NULL;
	xmlDocPtr doc;
	doc = gda_report_engine_run_as_doc (eng, &error);
	if (!doc) {
		g_print ("gda_report_engine_run error: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	else {
		g_print ("Report generated in customers-report.xml.\n");
		xmlSaveFile ("customers-report.xml", doc);
		xmlFreeDoc (doc);
	}

	g_object_unref (eng);
	g_object_unref (cnc);

	return 0;
}

GdaConnection *
open_connection (GdaClient *client)
{
        GdaConnection *cnc;
        GError *error = NULL;
        cnc = gda_client_open_connection (client, "SalesTest", NULL, NULL,
                                          GDA_CONNECTION_OPTIONS_DONT_SHARE,
                                          &error);
        if (!cnc) {
                g_print ("Could not open connection to DSN 'SalesTest': %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        return cnc;
}

GSList *
create_queries (GdaDict *dict)
{
	GdaQuery *query;
	GSList *list = NULL;

	query = gda_query_new_from_sql (dict, "SELECT * FROM customers", NULL);
	gda_object_set_name ((GdaObject*) query, "customers");
	list = g_slist_prepend (list, query);

	query = gda_query_new_from_sql (dict, "SELECT s.* FROM salesrep s "
					"INNER JOIN customers c ON (s.id=c.default_served_by) "
					"WHERE c.id=## /* name:\"customers/id\" type:gint */", NULL);
	gda_object_set_name ((GdaObject*) query, "salesrep_for_customer");
	list = g_slist_prepend (list, query);

	return list;
}
