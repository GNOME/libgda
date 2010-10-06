#ifdef USING_MINGW
#define _NO_OLDNAMES
#endif
#include <libgda/libgda.h>
#define SO_NAME "libgda-jdbc." G_MODULE_SUFFIX

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	GdaDataModel *providers;
	gint i, nrows;
	gboolean some_found = FALSE;

	/* set up test environment */
	gda_init ();

	providers = gda_config_list_providers ();
	nrows = gda_data_model_get_n_rows (providers);
	for (i = 0; i < nrows; i++) {
		const GValue *sovalue = gda_data_model_get_value_at (providers, 4, i, NULL);
		g_assert (sovalue);
		if (g_str_has_suffix (g_value_get_string (sovalue), SO_NAME)) {
			const GValue *namevalue = gda_data_model_get_value_at (providers, 0, i, NULL);
			g_assert (namevalue);
			if (! some_found) {
				some_found = TRUE;
				g_print ("Usable JDBC drivers:\n");
			}
			g_print ("%s\n", g_value_get_string (namevalue));
		}
	}
	g_object_unref (providers);

	if (! some_found) 
		g_print ("No usable JDBC driver\n");

	return 0;
}
