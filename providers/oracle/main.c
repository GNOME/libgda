/* GDA ORACLE Provider
 * Copyright (C) 2000-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <bonobo/bonobo-i18n.h>
#include "libgda/libgda.h"
#include "gda-oracle.h"

GdaServer *gda_oracle_server = NULL;

static void
last_client_gone_cb (GdaServer *server, gpointer user_data)
{
        g_return_if_fail (GDA_IS_SERVER (server));
        g_return_if_fail (server == gda_oracle_server);

        /* terminate the program when no more clients are connected */
        gda_main_quit ();
        g_object_unref (G_OBJECT (gda_oracle_server));
}

static void
setup_factory (void)
{
        if (gda_oracle_server != NULL)
                return;

        gda_oracle_server = gda_server_new (GDA_ORACLE_COMPONENT_FACTORY_ID);
        if (!GDA_IS_SERVER (gda_oracle_server)) {
                gda_log_error (_("Could not initiate Oracle component factory"));
		exit (-1);
        }
        g_signal_connect (G_OBJECT (gda_oracle_server), "last_client_gone",
                          G_CALLBACK (last_client_gone_cb), NULL);

        /* register the components for this server */
        gda_server_register_component (gda_oracle_server,
                                       GDA_ORACLE_PROVIDER_ID,
                                       GDA_TYPE_ORACLE_PROVIDER);
}

int
main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");

	/* initialize application */
        gda_init ("gda-oracle-srv", VERSION, argc, argv);

	if (OCI_SUCCESS !=
	    OCIInitialize ((ub4) OCI_THREADED | OCI_OBJECT,
			   (dvoid *) 0,
			   (dvoid * (*)(dvoid *, size_t)) 0,
			   (dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
			   (void (*)(dvoid *, dvoid *)) 0)) {
		gda_log_error (_("Could not initialize OCI library"));
		exit (-1);
	}

        setup_factory ();

        /* run the application loop */
        gda_main_run (NULL, NULL);

        return 0;
}
