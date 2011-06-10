/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdlib.h>
#include <string.h>

#define PROVIDER "Oracle"
#include "prov-test-common.h"

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;

int
main (int argc, char **argv)
{
	int number_failed = 0;

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

