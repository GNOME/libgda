/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#define PROVIDER "PostgreSQL"
#include "prov-test-common.h"
#include <sql-parser/gda-sql-parser.h>
#include "../test-errors.h"

#define CHECK_EXTRA_INFO 1

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;
extern gboolean         fork_tests;

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	int number_failed = 0;
	fork_tests = FALSE;
	gchar **env;
	const gchar *cnc_string;

	env = g_get_environ ();
	cnc_string = g_environ_getenv (env, "POSTGRESQL_DBCREATE_PARAMS");
	if (cnc_string == NULL) {
		g_message ("No enviroment variable POSTGRESQL_DBCREATE_PARAMS was set. No PostgreSQL provider tests will be performed."
		          "Set this environment variable in order to get access to your server. Example: export POSTGRESQL_DBCREATE_PARAMS=\"HOST=postgres;ADM_LOGIN=$POSTGRES_USER;ADM_PASSWORD=$POSTGRES_PASSWORD\"");
		g_strfreev (env);
		return EXIT_SUCCESS;
	}

	gda_init ();

	pinfo = gda_config_get_provider_info (PROVIDER);
	if (!pinfo) {
		g_warning ("Could not find provider information for %s", PROVIDER);
		return EXIT_SUCCESS;
	}
	g_print ("============= Provider Meta Partial 1 Update now testing: %s =============\n", pinfo->id);

	number_failed = prov_test_common_setup ();

	if (cnc) {
		number_failed += prov_test_common_check_meta_partial ();
		number_failed += prov_test_common_clean ();
	}

	g_print ("Test %s\n", (number_failed == 0) ? "Ok" : "failed");
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

