/* GDA - Test suite
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-test.h"
#include <string.h>

/* Lists all sections and their entries */
static void
list_all_sections_and_keys (const gchar *root)
{
	GList *sections;
	GList *keys;
	GList *l;
	gchar *tmp;

	g_return_if_fail (root != NULL);

	g_print (" Section %s\n", root);

	/* first, list all keys in this section */
	keys = gda_config_list_keys (root);
	for (l = keys; l != NULL; l = l->next) {
		gchar *value;
		gchar *key = (gchar *) l->data;

		tmp = g_strdup_printf ("%s/%s", root, key);
		value = gda_config_get_string (tmp);
		g_free (tmp);

		g_print ("\tKey '%s' = '%s'\n", key, value);
		g_free (value);
	}
	gda_config_free_list (keys);

	/* get all sections and list recursively */
	sections = gda_config_list_sections (root);
	for (l = sections; l != NULL; l = l->next) {
		gchar *section = (gchar *) l->data;

		if (root[strlen (root) - 1] == '/')
			tmp = g_strdup_printf ("%s%s", root, section);
		else
			tmp = g_strdup_printf ("%s/%s", root, section);
		list_all_sections_and_keys (tmp);
		g_free (tmp);
	}
	gda_config_free_list (sections);
}

/* Lists all installed providers */
static void
list_all_providers (void)
{
	GList *providers;
	GList *l;

	DISPLAY_MESSAGE ("Testing provider configuration API");

	providers = gda_config_get_provider_list ();
	for (l = providers; l != NULL; l = l->next) {
		GdaProviderInfo *info = (GdaProviderInfo *) l->data;
		GList *ll;

		if (!info) {
			g_print (_("** ERROR: gda_config_get_provider_list returned a NULL item\n"));
			gda_main_quit ();
		}

		g_print (_(" Provider = %s\n"), info->id);
		g_print (_("\tlocation = %s\n"), info->location);
		g_print (_("\tdescription = %s\n"), info->description);

		g_print (_("\tgda_params ="));
		for (ll = info->gda_params; ll != NULL; ll = ll->next) {
			if (ll->data != NULL) {
				GdaProviderParameterInfo *param_info = ll->data;
				g_print (" %s (%s) ", param_info->name, param_info->short_description);
			}
		}
		g_print ("\n");
	}
}

/* Lists configured data sources */
static void
list_data_sources (void)
{
	GList *dsnlist;
	GList *l;

	DISPLAY_MESSAGE ("Testing data source configuration");

	dsnlist = gda_config_get_data_source_list ();
	for (l = dsnlist; l != NULL; l = l->next) {
		GdaDataSourceInfo *info = (GdaDataSourceInfo *) l->data;

		if (!info) {
			g_print ("** ERROR: gda_config_get_data_source_list returned a NULL item\n");
			gda_main_quit ();
		}

		g_print (" Data source = %s\n", info->name);
		g_print ("\tprovider = %s\n", info->provider);
		g_print ("\tcnc_string = %s\n", info->cnc_string);
		g_print ("\tdescription = %s\n", info->description);
		g_print ("\tusername = %s\n", info->username);
	}

	gda_config_free_data_source_list (dsnlist);
}

/* Main entry point for the configuration test suite */
void
test_config (void)
{
	/* test configuration API */
	DISPLAY_MESSAGE ("Testing configuration API");
	list_all_sections_and_keys ("/apps/libgda");

	/* test providers' info retrieval API */
	list_all_providers ();

	/* test data sources configuration API */
	list_data_sources ();
}
