#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include "common.h"
#include <sql-parser/gda-sql-parser.h>

gboolean copy_products (GdaConnection *virtual);

int
main (int argc, char *argv[])
{
        GdaConnection *s_cnc, *d_cnc, *virtual;

        gda_init ("LibgdaCopyTableEasier", "1.0", argc, argv);

	/* open "real" connections */
	s_cnc = open_source_connection ();
        d_cnc = open_destination_connection ();

	/* virtual connection settings */
	GdaVirtualProvider *provider;
	GError *error = NULL;
	provider = gda_vprovider_hub_new ();
        virtual = gda_virtual_connection_open (provider, NULL);

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
	GdaSqlParser *parser;
	GdaStatement *stmt;
        gchar *sql;
	GError *error = NULL;

        /* DROP table if it exists */
	parser = gda_connection_create_parser (virtual);
	sql = "INSERT INTO destination.products_copied3 SELECT ref, name, price, wh_stored FROM source.products LIMIT 10";
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_assert (stmt);
        g_object_unref (parser);

	if (gda_connection_statement_execute_non_select (virtual, stmt, NULL, NULL, &error) == -1) {
		g_print ("Could not copy table's contents: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		g_object_unref (stmt);
		return FALSE;
	}
	g_object_unref (stmt);
	return TRUE;
}
