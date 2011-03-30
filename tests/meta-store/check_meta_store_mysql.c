#include <libgda/libgda.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main(int argc, char ** argv)
{
	GdaMetaStore *store;
	gchar *cnc_string;

	gda_init ();

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





