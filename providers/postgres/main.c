/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include "gda-postgres.h"

GdaServer *gda_postgres_server = NULL;

static void
last_client_gone_cb (GdaServer *server, gpointer user_data)
{
	g_return_if_fail (GDA_IS_SERVER (server));
	g_return_if_fail (server == gda_postgres_server);

	/* terminate the program when no more clients are connected */
	gda_main_quit ();
	g_object_unref (G_OBJECT (gda_postgres_server));
}

static void
setup_factory (void)
{
	if (gda_postgres_server != NULL)
		return;

	gda_postgres_server = gda_server_new (GDA_POSTGRES_COMPONENT_FACTORY_ID);
	if (!GDA_IS_SERVER (gda_postgres_server)) {
		gda_log_error (_("Could not initiate PostgreSQL component factory"));
		exit (-1);
	}
	g_signal_connect (G_OBJECT (gda_postgres_server), "last_client_gone",
			  G_CALLBACK (last_client_gone_cb), NULL);

	/* register the components for this server */
	gda_server_register_component (gda_postgres_server,
				       GDA_POSTGRES_PROVIDER_ID,
				       GDA_TYPE_POSTGRES_PROVIDER);
}

int
main (int argc, char *argv[])
{
	/* initialize application */
	gda_init ("gda-postgres-srv", VERSION, argc, argv);

	setup_factory ();

	/* run the application loop */
	gda_main_run (NULL, NULL);

	return 0;
}

