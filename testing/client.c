/* GDA - Test suite
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

/* Shows the object schemas */
static void
show_schema (GdaConnection *cnc, GdaConnectionSchema schema, const gchar *label)
{
	GdaDataModel *model;
	gint r, c;
	gint row_count, col_count;

	g_print ("\t%s\n", label);

	model = gda_connection_get_schema (cnc, schema, NULL);
	if (!GDA_IS_DATA_MODEL (model)) {
		g_print ("\t\tNONE\n");
		return;
	}

	row_count = gda_data_model_get_n_rows (model);
	col_count = gda_data_model_get_n_columns (model);
	for (r = 0; r < row_count; r++) {
		g_print ("\t");
		for (c = 0; c < col_count; c++) {
			GdaValue *value;
			gchar *str;

			value = gda_data_model_get_value_at (model, c, r);
			str = gda_value_stringify (value);
			g_print ("\t%s", str);
			g_free (str);
		}
		g_print ("\n");
	}

}

/* Opens a connection and test basic operations on it */
static void
open_connection (GdaClient *client,
		 const gchar *name,
		 const gchar *username,
		 const gchar *password)
{
	GdaConnection *cnc;
	gboolean res;

	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc = gda_client_open_connection (client, name, username, password);
	if (!GDA_IS_CONNECTION (cnc)) {
		g_print ("** ERROR: could not open connection to %s\n", name);
		return;
	}

	/* show provider features */
	g_print ("\tProvider capabilities...\n");
	g_print ("\t\tTransactions: %s\n",
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_TRANSACTIONS) ?
		 _("Supported") : _("Not supported"));

	/* show connection schemas */
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TABLES, "Connection Tables");
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TYPES, "Available types");

	/* test transactions */
	g_print ("\tStarting transaction...");
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tFinishing transaction...");
	res = gda_connection_commit_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tStarting transaction...");
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	g_print ("\tRolling back transaction...");
	res = gda_connection_rollback_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : "Error");

	/* close the connection */
	gda_connection_close (cnc);
}

/* Main entry point for the basic client API test */
void
test_client (void)
{
	GdaClient *client;
	GList *dsnlist;
	GList *l;

	DISPLAY_MESSAGE ("Testing GDA client API");

	client = gda_client_new ();

	dsnlist = gda_config_get_data_source_list ();
	for (l = dsnlist; l != NULL; l = l->next) {
		GdaDataSourceInfo *info = (GdaDataSourceInfo *) l->data;

		if (!info) {
			g_print ("** ERROR: gda_config_get_data_source_list returned a NULL item\n");
			gda_main_quit ();
		}

		g_print (" Data source = %s, User = %s\n", info->name, info->username);
		open_connection (client, info->name, info->username, "");
	}

	/* free memory */
	gda_config_free_data_source_list (dsnlist);
	g_object_unref (G_OBJECT (client));
}
