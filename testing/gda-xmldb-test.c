/* GDA - XML database test program
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
 *	Gerhard Dieringer <gdieringer@compuserve.com>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include <libgda/libgda.h>

static void
list_tables (GdaXmlDatabase *xmldb)
{
	GList *list;
	GList *l;

	list = gda_xml_database_get_tables (xmldb);
	for (l = list; l; l = l->next) {
		gchar *tname = (gchar *) l->data;
		g_print ("Found table: %s\n", tname);
	}

	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);
}

static void
stress_xml_database (gpointer user_data)
{
	GdaXmlDatabase *xmldb = (GdaXmlDatabase *) user_data;

	g_return_if_fail (GDA_IS_XML_DATABASE (xmldb));

	list_tables (xmldb);

	/* finish the program */
	gda_main_quit ();
}

static void
xmldb_changed_cb (GdaXmlDatabase *xmldb, gpointer user_data)
{
	g_print ("\n\nXML Database has changed!!!\n\n");
}

int
main (int argc, char *argv[])
{
	GdaXmlDatabase *xmldb;

	gda_init ("gda-xmldb-test", VERSION, argc, argv);

	/* open the XML file */
	xmldb = gda_xml_database_new_from_file ("sample_database.xml");
	if (!GDA_IS_XML_DATABASE (xmldb))
		g_error ("Could not load file");

	g_signal_connect (G_OBJECT (xmldb), "changed", G_CALLBACK (xmldb_changed_cb), NULL);

	gda_main_run ((GdaInitFunc) stress_xml_database, xmldb);

	gda_xml_database_free (xmldb);
	return 0;
}
