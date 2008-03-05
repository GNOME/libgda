#include <libgda/libgda.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main(int argc, char ** argv)
{
	GdaMetaStore *store;
	gchar *cnc_string;
	
	gda_init ("GdaMetaStore", "0.1", argc, argv);

	/* Clean eveything which might exist in the store */
	store = gda_meta_store_new (NULL);
	common_drop_all_tables (store);
	g_object_unref (store);
	
	/* Test store setup */
	store = gda_meta_store_new (NULL);

	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);

	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);
	
	g_print ("Test Ok.\n");
	
	return EXIT_SUCCESS;
}





