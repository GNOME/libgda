#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *outfile = NULL;
gboolean diff = FALSE;
gboolean noact = FALSE;
gboolean silent = FALSE;

gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},

	{ "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "XML output file", "output file"},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ "diff", 'd', 0, G_OPTION_ARG_NONE, &diff, "Show differences with existing dictionary, if any", NULL },
	{ "no-action", 'n', 0, G_OPTION_ARG_NONE, &noact, "Don't write any dictionary file (just build it into memory)", NULL },
	{ "silent", 'i', 0, G_OPTION_ARG_NONE, &silent, "Silent mode: don't write activity indicators", NULL },
	{ NULL }
};

static void
update_progress_cb (GdaDict *dict, gchar *msg, guint now, guint total, gpointer data)
{
	static gboolean first = TRUE;
	if (msg) {
		if (first)
			g_print ("%s:", msg);
		g_print (".");
		first = FALSE;
	}
	else {
		first = TRUE;
		g_print ("\n");
		/*g_print ("%s : %d/%d\n", msg, now, total);*/
	}
}

static void
dict_change_cb (GdaDict *dict, GdaObject *obj, gpointer what)
{
	if (GDA_IS_DICT_TYPE (obj))
		g_print ("Data type %s ", gda_object_get_name (obj));
	else {
		if (GDA_IS_DICT_FUNCTION (obj))
			g_print ("Function %s ", gda_object_get_name (obj));
		else {
			if (GDA_IS_DICT_AGGREGATE (obj))
				g_print ("Aggregate %s ", gda_object_get_name (obj));
			else {
				if (GDA_IS_DICT_TABLE (obj))
					g_print ("Table %s ", gda_object_get_name (obj));
				else {
					if (GDA_IS_DICT_FIELD (obj)) {
						GdaEntity *table;

						table = gda_entity_field_get_entity (GDA_ENTITY_FIELD (obj));
						g_print ("Field %s.%s ", 
							 gda_object_get_name (GDA_OBJECT (table)),
							 gda_object_get_name (obj));
					}
					else {
						if (GDA_IS_DICT_CONSTRAINT (obj)) {
							gchar *str;
							
							str = (gchar *) gda_object_get_name (obj);
							if (!str || !(*str))
								str = "unnamed";
							g_print ("Constraint %s on table %s ", str,
								 gda_object_get_name (GDA_OBJECT (
								  gda_dict_constraint_get_table (GDA_DICT_CONSTRAINT (obj)))));
						}
						else {
							g_assert_not_reached ();
						}
					}
				}
			}
		}
	}

	switch (GPOINTER_TO_INT (what)) {
	case 1:
		g_print ("Added\n");
		break;
	case 2:
		g_print ("Updated\n");
		break;
	case 3:
		g_print ("Removed\n");
		break;
	default:
		g_assert_not_reached ();
	}
}

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GOptionContext *context;
	GdaDict *dict;
	GdaDictDatabase *db;
	GdaClient *client;
	GdaConnection *cnc;
	gchar *filename;

	/* command line parsing */
	context = g_option_context_new ("Create a dictionary file for a data source");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	if (direct && dsn) {
		g_print ("DSN and connection string are exclusive\n");
		exit (1);
	}

	if (!direct && !dsn) {
		g_print ("You must specify a connection to open either as a DSN or a connection string\n");
		exit (1);
	}

	if (direct && !prov) {
		g_print ("You must specify a provider when using a connection string\n");
		exit (1);
	}

	gda_init ("Gda author dictionary file", PACKAGE_VERSION, argc, argv);

	/* open connection */
	client = gda_client_new ();
	if (dsn) {
		GdaDataSourceInfo *info = NULL;
		info = gda_config_find_data_source (dsn);
		if (!info)
			g_error (_("DSN '%s' is not declared"), dsn);
		else {
			cnc = gda_client_open_connection (client, info->name, 
							  user ? user : info->username, 
							  pass ? pass : ((info->password) ? info->password : ""),
							  0, &error);
			if (!cnc) {
				g_warning (_("Can't open connection to DSN %s: %s\n"), info->name,
				   error && error->message ? error->message : "???");
				exit (1);
			}
			gda_data_source_info_free (info);
		}
	}
	else {
		
		cnc = gda_client_open_connection_from_string (client, prov, direct, 
							      user, pass, 0, &error);
		if (!cnc) {
			g_warning (_("Can't open specified connection: %s\n"),
				   error && error->message ? error->message : "???");
			exit (1);
		}
	}

	/* create dictionary */
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnc);
	db = gda_dict_get_database (dict);

	/* file to save to */
	filename = outfile;
	if (!outfile) {
		filename = gda_dict_compute_xml_filename (dict, NULL, NULL, &error);
		if (!filename) {
			g_warning ("Could not compute output XML file name: %s", error ? error->message:
				   _("Unknown error"));
			exit (1);
		}
	}

	if (diff && g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* try to load an existing distionary */
		if (!silent)
			g_print ("Loading existing dictionary: %s\n", filename);
		if (! gda_dict_load_xml_file (dict, filename, &error)) {
			g_warning ("Could not load existing XML file named: %s", error ? error->message:
				   _("Unknown error"));
			exit (1);
		}
		g_assert (gda_dict_save_xml_file (dict, "/tmp/old", &error));
	}
	else 
		gda_dict_extend_with_functions (dict);

	g_print (_("Fetching meta-data from the DBMS server. This may take some time...\n"));

	/* signal handlers for info display */
	if (!silent)
		g_signal_connect (G_OBJECT (dict), "update_progress",
				  G_CALLBACK (update_progress_cb), NULL);

	if (diff) {
		g_signal_connect (G_OBJECT (dict), "object_added",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_connect (G_OBJECT (dict), "object_updated",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_connect (G_OBJECT (dict), "object_removed",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));

		g_signal_connect (G_OBJECT (db), "table_added",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_connect (G_OBJECT (db), "table_updated",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_connect (G_OBJECT (db), "table_removed",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));

		g_signal_connect (G_OBJECT (db), "field_added",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_connect (G_OBJECT (db), "field_updated",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_connect (G_OBJECT (db), "field_removed",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));
		
		g_signal_connect (G_OBJECT (db), "constraint_added",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_connect (G_OBJECT (db), "constraint_updated",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_connect (G_OBJECT (db), "constraint_removed",
				  G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));
	}

	/* update DBMS data */
	if (!gda_dict_update_dbms_meta_data (dict, 0, NULL, &error))
		g_warning ("Could not update DBMS data: %s", error ? error->message:
			   _("Unknown error"));

	/* save to file */
	if (! noact) {
		if (!gda_dict_save_xml_file (dict, filename, &error))
			g_warning (_("Can't write dictionary to file %s: %s\n"), filename,
				   error ? error->message : _("Unknown error"));
		else
			g_print (_("Dictionary file written to %s\n"), filename);
	}

	/* disconnect signal handlers for info display */
	if (!silent)
		g_signal_handlers_disconnect_by_func (G_OBJECT (dict), 
						      G_CALLBACK (update_progress_cb), NULL);

	if (diff) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (dict),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_handlers_disconnect_by_func (G_OBJECT (dict),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_handlers_disconnect_by_func (G_OBJECT (dict),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));

		g_signal_handlers_disconnect_by_func (G_OBJECT (db),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (1));
		g_signal_handlers_disconnect_by_func (G_OBJECT (db),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (2));
		g_signal_handlers_disconnect_by_func (G_OBJECT (db),
						      G_CALLBACK (dict_change_cb), GINT_TO_POINTER (3));

	}

	/* cleanups */
	if (!outfile) 
		g_free (filename);
	g_object_unref (G_OBJECT (dict));
	gda_connection_close (cnc);
	g_object_unref (G_OBJECT (client));

	return 0;
}
