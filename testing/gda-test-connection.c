#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL }
};


int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GOptionContext *context;

	GdaClient *client;
	GdaConnection *cnc;
	gchar *auth_string = NULL;

	/* command line parsing */
	context = g_option_context_new ("Tests opening a connection");
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

	gda_init ("Gda connection tester", PACKAGE_VERSION, argc, argv);

	/* open connection */
	if (user) {
		if (pass)
			auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", user, pass);
		else
			auth_string = g_strdup_printf ("USERNAME=%s", user);
	}
	client = gda_client_new ();
	if (dsn) {
		GdaDataSourceInfo *info = NULL;
		info = gda_config_get_dsn (dsn);
		if (!info)
			g_error (_("DSN '%s' is not declared"), dsn);
		else {
			cnc = gda_client_open_connection (client, info->name, 
							  auth_string ? auth_string : info->auth_string,
							  0, &error);
			if (!cnc) {
				g_warning (_("Can't open connection to DSN %s: %s\n"), info->name,
				   error && error->message ? error->message : "???");
				exit (1);
			}
		}
	}
	else {
		
		cnc = gda_client_open_connection_from_string (client, prov, direct, auth_string, 0, &error);
		if (!cnc) {
			g_warning (_("Can't open specified connection: %s\n"),
				   error && error->message ? error->message : "???");
			exit (1);
		}
	}
	g_free (auth_string);

	g_print (_("Connection successfully opened!\n"));
	gda_connection_close (cnc);
	g_object_unref (G_OBJECT (client));

	return 0;
}
