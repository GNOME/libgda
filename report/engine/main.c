/* GDA Report Engine
 * Copyright (C) 2000-2001 The Free Software Foundation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
 * 	Carlos Perelló Marín <carlos@gnome-db.org>
 */

#include <config.h>
#include <gda-report-common.h>

static void setup_factory (void);

static GList *object_list = NULL;

int
main (int argc, char *argv[])
{
	struct sigaction act;
	sigset_t empty_mask;

	bindtextdomain (PACKAGE, GDA_LOCALEDIR);
	textdomain (PACKAGE);
	
	gda_init ("gda-report-engine", VERSION, argc, argv);
	
	/* initialize everything */
	setup_factory ();
	
	/* run the application */
	gda_main_run (NULL, NULL);

	return 0;
}

static void
object_finalized_cb (GObject *object, gpointer user_data)
{
	g_return_if_fail (BONOBO_IS_OBJECT (object));

	object_list = g_list_remove (object_list, object);
	if (!object_list) {
		/* if no more objects left, terminate */
		gda_main_quit ();
	}
}

static BonoboObject *
factory_callback (BonoboGenericFactory *factory,
		  const char *id,
		  gpointer closure)
{
	BonoboObject *object;

	g_return_val_if_fail (id != NULL, NULL);

	if (!strcmp (id, GDA_COMPONENT_ID_REPORT))
		object = gda_report_engine_new ();

	if (BONOBO_IS_OBJECT (object)) {
		object_list = g_list_append (object_list, object);
		g_signal_connect (G_OBJECT (object), "finalize"
				  G_CALLBACK (object_finalized_cb), NULL);
	}
}

static void
setup_factory (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (
		GDA_COMPONENT_ID_REPORT_FACTORY,
		factory_callback,
		NULL);
}
