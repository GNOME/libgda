#include <libgda/libgda.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *outfile = NULL;
gboolean diff = FALSE;
gchar *operation = NULL;

gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},

	{ "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "XML output file", "output file"},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL }
};

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GdaClient *client = NULL;
	GdaConnection *cnc = NULL;
	GOptionContext *context;
	GdaServerProvider *provider;
	GdaServerOperation *op;
	GdaServerOperationType type = GDA_SERVER_OPERATION_CREATE_TABLE;
	xmlNodePtr node;

	/* Initialize i18n support */
        setlocale (LC_ALL, "");

	/* command line parsing */
	context = g_option_context_new ("Test a provider's operations features using the GdaServerOperation object");
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

	if (!prov) {
		g_print ("You must specify a provider\n");
		exit (1);
	}

	gda_init ("Gda test provider operations", PACKAGE_VERSION, argc, argv);

	/* open connection if specified */
	if (direct || dsn) {
		client = gda_client_new ();
		if (dsn) {
			GdaDataSourceInfo *info = NULL;
			info = gda_config_get_dsn (dsn);
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
	}

	/* create the GdaServerProvider object */
	provider = gda_config_get_provider_object (prov, NULL);
	if (!provider)
		g_error ("Could not create GdaServerProvider object from plugin ('%s' provider)", prov);

	if (cnc)
		if (provider != gda_connection_get_provider_obj (cnc))
			g_error ("Connection's provider and tested provider are different!");

	/* test the GdaServerOperation object here */
	if (!gda_server_provider_supports_operation (provider, cnc, type, NULL)) {
		g_print ("Operation not supported by provider\n");
		exit (0);
	}
	op = gda_server_provider_create_operation (provider, cnc, type, NULL, &error);
	if (!op) 
		g_error ("Could not create GdaServerOperation object: %s",
			 error && error->message ? error->message : "No detail");
	g_print ("GdaServerOperation object created: %p\n", op);
	node = gda_server_operation_save_data_to_xml (op, NULL);
	if (node) {
		xmlDocPtr doc;
		xmlChar *buffer;

		doc = xmlNewDoc ("1.0");
		xmlDocSetRootElement (doc, node);
		xmlIndentTreeOutput = 1;
		xmlKeepBlanksDefault (0);
		xmlDocDumpFormatMemory (doc, &buffer, NULL, 1);
		g_print ("%s\n", buffer);
		xmlFree (buffer);
		xmlFreeDoc (doc);
	}
	else 
		g_warning ("Saving to XML failed!");
	g_object_unref (op);

	/* end... */
	if (cnc)
		gda_connection_close (cnc);
	if (client)
		g_object_unref (G_OBJECT (client));

	return 0;
}
