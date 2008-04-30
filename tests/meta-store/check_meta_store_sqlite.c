#include <libgda/libgda.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main(int argc, char ** argv)
{
	GdaMetaStore *store;

	/* set up test environment */
	g_setenv ("GDA_TOP_SRC_DIR", TOP_SRC_DIR, TRUE);
	g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, TRUE);
	gda_init ();

	/* Clean eveything which might exist in the store */
	store = gda_meta_store_new_with_file ("test.db");
	common_drop_all_tables (store);
	g_object_unref (store);

	/* Test store setup*/
	store = gda_meta_store_new_with_file ("test.db");
	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);
	
	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);
	
	g_print ("Test Ok.\n");
	
	return EXIT_SUCCESS;
}





