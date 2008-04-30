#include <stdlib.h>
#include <string.h>

#define PROVIDER "Berkeley-DB"
#include "prov-test-common.h"

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;

int
main (int argc, char **argv)
{
	int number_failed = 0;

	/* set up test environment */
	g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, 0);
	gda_init ();

	pinfo = gda_config_get_provider_info (PROVIDER);
	if (!pinfo) {
		g_warning ("Could not find provider information for %s", PROVIDER);
		return EXIT_SUCCESS;
	}
	g_print ("Provider now tested: %s\n", pinfo->id);
	
	number_failed = prov_test_common_setup ();

	if (cnc) {
		number_failed += prov_test_common_check_meta ();
		number_failed += prov_test_common_clean ();
	}

	if (! params_provided)
		return EXIT_SUCCESS;
	else {
		g_print ("Test %s\n", (number_failed == 0) ? "Ok" : "failed");
		return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}

