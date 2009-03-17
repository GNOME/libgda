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
