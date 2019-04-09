/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#define PROVIDER "MySQL"
#include "prov-test-common.h"

extern GdaProviderInfo *pinfo;
extern GdaConnection   *cnc;
extern gboolean         params_provided;

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	int number_failed = 0;
	gchar **env;
	const gchar *cnc_string;
	GError *error = NULL;

	env = g_get_environ ();
	cnc_string = g_environ_getenv (env, "MYSQL_CNC_PARAMS");
	if (cnc_string == NULL) {
		g_message ("No enviroment variable MYSQL_CNC_PARAMS was set. No PostgreSQL provider tests will be performed."
		          "Set this environment variable in order to get access to your server. Example: export MYSQL_CNC_PARAMS=\"DB_NAME=$MYSQL_DB;HOST=$MYSQL_HOST;USERNAME=$MYSQL_USER;PASSWORD=$MYSQL_PASSWORD\"");
		g_strfreev (env);
		return EXIT_SUCCESS;
	}

	gda_init ();

	pinfo = gda_config_get_provider_info (PROVIDER);
	if (!pinfo) {
		g_warning ("Could not find provider information for %s", PROVIDER);
		return EXIT_SUCCESS;
	}
	g_print ("Provider now tested: %s\n", pinfo->id);

	cnc = gda_connection_open_from_string (pinfo->id, cnc_string, NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
	if (cnc == NULL) {
		g_warning ("Error opening connection: %s",
			         error && error->message ? error->message : "No error was set");
		return EXIT_FAILURE;
	}
	if (cnc) {
		number_failed += prov_test_common_check_timestamp ();
		number_failed += prov_test_common_check_meta_full ();
		number_failed += prov_test_common_check_meta_partial ();
		number_failed += prov_test_common_check_meta_partial2 ();
		number_failed += prov_test_common_check_meta_partial3 ();
		number_failed += prov_test_common_clean ();
	}

	if (! params_provided)
		return EXIT_SUCCESS;
	else {
		g_print ("Test %s\n", (number_failed == 0) ? "Ok" : "failed");
		return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}

