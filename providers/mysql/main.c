/* GDA MySQL provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include "gda-mysql.h"

GdaServer *gda_mysql_server = NULL;

static void
last_client_gone_cb (GdaServer *server, gpointer user_data)
{
	g_return_if_fail (GDA_IS_SERVER (server));
	g_return_if_fail (server == gda_mysql_server);

	/* terminate the program when no more clients are connected */
	gda_main_quit ();
	g_object_unref (G_OBJECT (gda_mysql_server));
}

static void
setup_factory (void)
{
	if (gda_mysql_server != NULL)
		return;

	gda_mysql_server = gda_server_new (GDA_MYSQL_COMPONENT_FACTORY_ID);
	if (!GDA_IS_SERVER (gda_mysql_server)) {
		gda_log_error (_("Could not initiate MySQL component factory"));
		exit (-1);
	}
	g_signal_connect (G_OBJECT (gda_mysql_server), "last_client_gone",
			  G_CALLBACK (last_client_gone_cb), NULL);

	/* register the components for this server */
	gda_server_register_component (gda_mysql_server,
				       GDA_MYSQL_PROVIDER_ID,
				       GDA_TYPE_MYSQL_PROVIDER);
}

int
main (int argc, char *argv[])
{
	/* initialize application */
	gda_init ("gda-mysql-srv", VERSION, argc, argv);

	setup_factory ();

	/* run the application loop */
	gda_main_run (NULL, NULL);

	return 0;
}
