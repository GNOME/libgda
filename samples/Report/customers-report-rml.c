#include <libgda/libgda.h>
#include <libgda-report/libgda-report.h>

GdaConnection *open_connection (GdaClient *client);
GSList        *create_queries (GdaDict *dict);

int 
main (int argc, char **argv)
{
	GdaReportEngine *eng;
	GdaClient *client;
	GdaDict *dict;
	GdaConnection *cnc;
	GdaParameter *param;
	GdaReportDocument *doc;

	gda_init ("Customers report example (RML)", "3.1.1", argc, argv);

	/* Doc object */
	doc = gda_report_rml_document_new (NULL);
	g_object_get (G_OBJECT (doc), "engine", &eng, NULL);
	gda_report_document_set_template (doc, "customers-report-rml.rml");
	
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
	g_object_unref (eng);

	/* use the doc object */
	GError *error = NULL;
	gchar *outfile = "customers-report-rml.pdf";
	if (! (gda_report_document_run_as_pdf (doc, outfile, &error))) {
		g_print ("gda_report_document_run_as_pdf error: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	else
		g_print ("%s file generated\n", outfile);

#ifdef HTML
	outfile = "customers-report-rml.html";
	if (! (gda_report_document_run_as_html (doc, outfile, &error))) {
		g_print ("gda_report_document_run_as_html error: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	else
		g_print ("%s file generated\n", outfile);
#endif

	g_object_unref (cnc);
	g_object_unref (doc);

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
