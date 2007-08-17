#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include "common.h"

gboolean copy_products (GdaConnection *virtual);

int
main (int argc, char *argv[])
{
        GdaClient *client;
        GdaConnection *s_cnc, *d_cnc, *virtual;

        gda_init ("LibgdaCopyTableEasier", "1.0", argc, argv);

        /* Create a GdaClient object which is the central object which manages all connections */
        client = gda_client_new ();

	/* open "real" connections */
	s_cnc = open_source_connection (client);
        d_cnc = open_destination_connection (client);

	/* virtual connection settings */
	GdaVirtualProvider *provider;
	GError *error = NULL;
	provider = gda_vprovider_hub_new ();
        virtual = gda_server_provider_create_connection (NULL, GDA_SERVER_PROVIDER (provider), NULL, NULL, NULL, 0);
        g_assert (gda_connection_open (virtual, NULL));

	/* adding connections to the cirtual connection */
        if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), s_cnc, "source", &error)) {
                g_print ("Could not add connection to virtual connection: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), d_cnc, "destination", &error)) {
                g_print ("Could not add connection to virtual connection: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

	/* copy some contents of the 'products' table into the 'products_copied', method 1 */
	if (! copy_products (virtual)) 
		exit (1);

        gda_connection_close (virtual);
        gda_connection_close (s_cnc);
        gda_connection_close (d_cnc);
        return 0;
}

/*
 * Data copy
 */
gboolean
copy_products (GdaConnection *virtual)
{
	GdaCommand *command;
        gchar *sql;
	GError *error = NULL;

        /* DROP table if it exists */
	sql = "INSERT INTO destination.products_copied3 SELECT ref, name, price, wh_stored FROM source.products LIMIT 10";
	command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	if (gda_connection_execute_non_select_command (virtual, command, NULL, &error) == -1) {
		g_print ("Could not copy table's contents: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		gda_command_free (command);
		return FALSE;
	}
	gda_command_free (command);
	return TRUE;
}
