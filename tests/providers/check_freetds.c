#include <stdlib.h>
#include <string.h>

#define PROVIDER "FreeTDS"
#include "prov-test-common.h"

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;

int
main (int argc, char **argv)
{
	int number_failed = 0;

	gda_init ("check-providers", PACKAGE_VERSION, argc, argv);

	setenv ("GDA_PROVIDERS_ROOT_DIR", GDA_PROVIDERS_ROOT_DIR, 0);
	pinfo = gda_config_get_provider_info (PROVIDER);
	if (!pinfo) {
		g_warning ("Could not find provider information for %s", PROVIDER);
		return EXIT_FAILURE;
	}
	g_print ("Provider now tested: %s\n", pinfo->id);
	
	number_failed = prov_test_common_setup ();

	if (cnc) {
		number_failed += prov_test_common_clean ();
	}

	if (! params_provided)
		return EXIT_SUCCESS;
	else
		return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

