#include <stdio.h>
#include <glib.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

gint
main (int argc, char **argv) {
	GdaDataModel *model;

	gda_init ();
	g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "TRUE", TRUE);

	model = gda_config_list_providers ();
	gda_data_model_dump (model, stdout);
	g_object_unref (model);

	g_print ("\n\n");
	
	model = gda_config_list_dsn ();
	gda_data_model_dump (model, stdout);
	g_object_unref (model);

	return 0;
}
