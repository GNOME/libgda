#include <libgda/libgda.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main(int argc, char ** argv)
{
	GdaMetaStore *store;
	gchar *cnc_string;
	
	/* set up test environment */
	g_setenv ("GDA_TOP_SRC_DIR", TOP_SRC_DIR, TRUE);
	g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, TRUE);
	gda_init ("GdaMetaStore", "0.1", argc, argv);

	/* connection parameters */
	cnc_string = getenv ("MYSQL_META_CNC");
	if (!cnc_string) {
		g_print ("MySQL test not run, please set the MYSQL_META_CNC environment variable \n"
			"For example 'DB_NAME=meta'\n");
		return EXIT_SUCCESS;
	}
	
	/* Clean eveything which might exist in the store */
	gchar *str;
	str = g_strdup_printf ("MySQL://%s", cnc_string);
	store = gda_meta_store_new (str);
	common_drop_all_tables (store);
	g_object_unref (store);
	
	/* Test store setup */
	store = gda_meta_store_new (str);
	g_free (str);

	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);

	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);
	
	g_print ("Test Ok.\n");
	
	return EXIT_SUCCESS;
}





