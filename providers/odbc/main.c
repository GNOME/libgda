/* GNOME DB ODBC Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Michael Lausch <michael@lausch.at>
 *         Nick Gorham <nick@lurcher.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgda/libgda.h>
#include "gda-odbc.h"

GdaServer *gda_odbc_server = NULL;

static void
last_client_gone_cb (GdaServer *server, gpointer user_data)
{
        g_return_if_fail (GDA_IS_SERVER (server));
        g_return_if_fail (server == gda_odbc_server);

        /* terminate the program when no more clients are connected */
        gda_main_quit ();
        g_object_unref (G_OBJECT (gda_odbc_server));
}

static void
setup_factory (void)
{
	if (gda_odbc_server != NULL)
		return;

	gda_odbc_server = gda_server_new (GDA_ODBC_COMPONENT_FACTORY_ID);
	if (!GDA_IS_SERVER (gda_odbc_server)) {
                gda_log_error (_("Could not initiate ODBC component factory"));
                exit (-1);
        }
        g_signal_connect (G_OBJECT (gda_odbc_server), "last_client_gone",
                          G_CALLBACK (last_client_gone_cb), NULL);

        /* register the components for this server */
        gda_server_register_component (gda_odbc_server,
                                       GDA_ODBC_PROVIDER_ID,
                                       GDA_TYPE_ODBC_PROVIDER);
}

int
main (int argc, char *argv[])
{
	/* initialize application */
	gda_init ("gda-odbc-srv", VERSION, argc, argv);

	setup_factory ();

	/* run the application loop */
        gda_main_run (NULL, NULL);

        return 0;
}
