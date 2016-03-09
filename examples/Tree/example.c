/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>

int
main (int argc, char *argv[])
{
	GdaConnection *cnc;

	GdaTree *tree;
        GdaTreeManager *schemas_mgr, *tables_mgr, *columns_mgr;
        GError *error = NULL;

        gda_init ();

        cnc = gda_connection_open_from_dsn ("SalesTest", NULL, GDA_CONNECTION_OPTIONS_NONE, NULL);
        g_assert (cnc);
        g_print ("Updating meta store...");
        GdaMetaContext context = {"_tables", 0};
        gda_connection_update_meta_store (cnc, &context, NULL);
        g_print ("done.\n");

        tree = gda_tree_new ();

	/* create tree managers */
        schemas_mgr = gda_tree_mgr_schemas_new (cnc);
        tables_mgr = gda_tree_mgr_tables_new (cnc, NULL);
        columns_mgr = gda_tree_mgr_columns_new (cnc, NULL, NULL);

	/* set up the tree managers in the tree */
        gda_tree_add_manager (tree, schemas_mgr);
        gda_tree_add_manager (tree, tables_mgr);

        gda_tree_manager_add_manager (schemas_mgr, tables_mgr);
        gda_tree_manager_add_manager (tables_mgr, columns_mgr);

	/* we don't need object references anymore */
        g_object_unref (schemas_mgr);
        g_object_unref (tables_mgr);
        g_object_unref (columns_mgr);

	/* populate the tree and display its contents */
        if (!gda_tree_update_all (tree, &error))
                g_print ("Can't populate tree: %s\n", error && error->message ? error->message : "No detail");
        else
                gda_tree_dump (tree, NULL, NULL);

        g_object_unref (tree);
        gda_connection_close (cnc);
        g_object_unref (cnc);

        return 0;
}
