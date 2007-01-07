#include <libgda/libgda.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

/* options */
gboolean dotest = FALSE;
gboolean show_queries = FALSE;
gboolean dot_queries = FALSE;
gboolean overwrite = FALSE;

static GOptionEntry entries[] = {
	{ "test", 't', 0, G_OPTION_ARG_NONE, &dotest, "Just test the loading and saving of the dictionary", NULL },
	{ "queries", 'l', 0, G_OPTION_ARG_NONE, &show_queries, "Shows the queries in the dictionary", NULL },
	{ "dot-queries", 'q', 0, G_OPTION_ARG_NONE, &dot_queries, "Produce a .dot (GraphViz) file for each query", NULL },
	{ "overwrite", 'o', 0, G_OPTION_ARG_NONE, &overwrite, "Overwrites the dictionary file with the new one", NULL },
	{ NULL }
};

int 
main (int argc, char **argv)
{
	GOptionContext *context;
	GdaDict *dict;
	gchar *filename;
	GError *error = NULL;
	GSList *list, *lptr;
	
	/* command line parsing */
	context = g_option_context_new ("<Dictionary XML file> - Test and list the contents of a dictionary file");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);

	if (! show_queries && !dotest)
		dotest = TRUE;

	if (dot_queries)
		show_queries = TRUE;

	gda_init ("Gda verify dictionary file", PACKAGE_VERSION, argc, argv);

	if (argc <= 1) {
		g_print (_("Usage: %s <Dictionary XML file>\n"), argv[0]);
		exit (1);
	}

	filename = argv[1];
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_print (_("File '%s' does not exist or can't be read!\n"), filename);
		exit (1);
	}

	if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		g_print (_("File '%s' is a directory!\n"), filename);
		exit (1);
	}       

	dict = gda_dict_new ();
	if (!gda_dict_load_xml_file (dict, filename, &error)) {
		g_print (_("Error loading dictionary file '%s':\n%s\n"), filename, 
			 error ? error->message : _("Unspecified"));
		if (error)
			g_error_free (error);
		exit (1);
	}

	g_print ("Dictionary file '%s' has correctly been loaded.\n", filename);

	if (dotest) {
		/* testing saving back the dictionary */
		gchar *tmpfile, *cmde, *chout = NULL, *cherr = NULL;
		gint chstatus;
		gchar *dirname;

		dirname = g_path_get_dirname (filename);
		tmpfile = g_build_filename (dirname, ".temp", NULL);
		g_free (dirname);
		mkstemp (tmpfile);
		if (!gda_dict_save_xml_file (dict, tmpfile, &error)) {
			g_print (_("Error saving dictionary to file '%s':\n%s\n"), tmpfile, error->message);
			g_error_free (error);
			exit (1);
		}
		
		cmde = g_strdup_printf ("cmp %s %s", tmpfile, filename);
		if (! g_spawn_command_line_sync (cmde, &chout, &cherr, &chstatus, &error)) {
			g_print (_("Can't run command '%s':\n%s\n"), cmde, error->message);
			g_error_free (error);
			g_unlink (tmpfile);
			exit (1);
		}
		g_free (cmde);

		switch (chstatus) {
		case 0:
			g_print ("Loading and saving dictionary is OK\n");
			g_free (chout);
			g_free (cherr);
			break;
		default:
			g_print ("Loading and saving the dictionary is not OK:\n");
			g_free (chout);
			g_free (cherr);
			cmde = g_strdup_printf ("diff -u %s %s", filename, tmpfile);
			if (! g_spawn_command_line_sync (cmde, &chout, &cherr, &chstatus, &error)) {
				g_print (_("Can't run command '%s':\n%s\n"), cmde, error->message);
				g_error_free (error);
				g_unlink (tmpfile);
				exit (1);
			}
			g_print ("%s\n", chout);
			g_free (cmde);
			g_free (chout);
			g_free (cherr);
			break;
		}

		if (!overwrite)
			g_unlink (tmpfile);
		else {
			gchar *str = g_strdup_printf ("%s~", filename);
			if (! g_rename (filename, str)) {
				if (g_rename (tmpfile, filename))
					g_warning ("Cant' rename dictionary '%s' to '%s'", tmpfile, filename);
			}
			else
				g_warning ("Cant' rename '%s' to '%s', dictionary not overwritten!", filename, str);
			g_free (str);
		}
		g_free (tmpfile);
	}

	if (show_queries) {
		/* queries listing */
		list = gda_dict_get_objects (dict, GDA_TYPE_QUERY);
		if (list) 
			g_print (_("List of queries in this file:\n"));
		else
			g_print (_("There is no query in this file.\n"));
		lptr = list;
		while (lptr) {
			GdaGraphviz *graph;
			GdaParameterList *context;
			GError *error = NULL;
			
			gchar *str;
			g_print (_("########### Query \"%s\" ###########\n"), gda_object_get_name (GDA_OBJECT (lptr->data)));
			str = gda_renderer_render_as_sql (GDA_RENDERER (lptr->data), NULL, 
							  GDA_RENDERER_EXTRA_PRETTY_SQL | GDA_RENDERER_PARAMS_AS_DETAILED,
							  &error);
			g_print (_("## SQL:\n"));
			if (str) {
				g_print ("%s\n", str);
				g_free (str);
			}
			else {
				g_print (_("SQL ERROR: %s\n"), error->message);
				g_error_free (error);
				error = NULL;
			}
			
			g_print (_("## information:\n"));
			str = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (lptr->data));
			g_print ("XML Id: %s\n", str);
			g_free (str);
			context = gda_query_get_parameter_list (GDA_QUERY (lptr->data));
			if (context) {
				if (!gda_parameter_list_is_coherent (context, &error)) {
					g_print ("Generated context is not coherent: %s\n", error->message);
					g_error_free (error);
				}
				else
					g_print ("Generated context is coherent\n");
				g_object_unref (G_OBJECT (context));
			}
			
			if (dot_queries) {
				gchar *dotfile;
				str = g_strdup_printf (_("Query_%02d"), g_slist_position (list, lptr) + 1);
				dotfile = g_strdup_printf ("%s.dot", str);
				graph = GDA_GRAPHVIZ (gda_graphviz_new (dict));
				gda_graphviz_add_to_graph (graph, G_OBJECT (lptr->data));
				if (!gda_graphviz_save_file (graph, dotfile, &error)) {
					g_print (_("Could not write graph to '%s' (%s)\n\n"), str, error->message);
					g_error_free (error);
					error = NULL;
				}
				else {
					g_print (_("Written graph to '%s'\n"), str);
					g_print ("Use 'dot' or 'neato' to create a graphical representation, for example \n"
						 "dot -Tsvg -o %s.svg %s / neato -Tsvg -o %s.svg %s\n\n", 
						 str, dotfile, str, dotfile);
				}
				g_free (dotfile);
				g_free (str);
			}
			
			lptr = g_slist_next (lptr);
		}
		g_slist_free (list);
	}
	
	g_object_unref (G_OBJECT (dict));

	return 0;
}
