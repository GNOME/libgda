/* GDA - Test suite
 * Copyright (C) 1998-2002 The GNOME Foundation
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

#include <config.h>
#include <libgda/gda-intl.h>
#include "gda-test.h"
#include "postgres-test.h"

/* Shows the object schemas */
static void
show_schema (GdaConnection *cnc, GdaConnectionSchema schema, const gchar *label)
{
	GdaDataModel *model;
	gint r, c;
	gint row_count, col_count;

	model = gda_connection_get_schema (cnc, schema, NULL);
	if (!GDA_IS_DATA_MODEL (model)) {
		g_print ("\t%s\n", label);
		g_print (_("\t\tNONE\n"));
		return;
	}

	row_count = gda_data_model_get_n_rows (model);
	col_count = gda_data_model_get_n_columns (model);
	g_print ("\t%s %d\n", label, row_count);

	for (r = 0; r < row_count; r++) {
		g_print ("\t");
		for (c = 0; c < col_count; c++) {
			const GdaValue *value;
			gchar *str;

			value = gda_data_model_get_value_at (model, c, r);
			str = gda_value_stringify ((GdaValue *) value);
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
	GdaDataSourceInfo *dsn_info;

	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc = gda_client_open_connection (client, name, username, password);
	if (!GDA_IS_CONNECTION (cnc)) {
		g_print (_("** ERROR: could not open connection to %s\n"), name);
		return;
	}

	/* show provider features */
	g_print (_("\tProvider capabilities...\n"));
	g_print (_("\t\tAggregates: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_AGGREGATES) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tIndexes: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_INDEXES) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tProcedures: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_PROCEDURES) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tSequences: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_SEQUENCES) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tSQL language: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_SQL) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tTransactions: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_TRANSACTIONS) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tTriggers: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_TRIGGERS) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tUsers: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_USERS) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tViews: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_VIEWS) ?
		 _("Supported") : _("Not supported"));
	g_print (_("\t\tXML queries: %s\n"),
		 gda_connection_supports (cnc, GDA_CONNECTION_FEATURE_XML_QUERIES) ?
		 _("Supported") : _("Not supported"));

	/* show connection schemas */
	show_schema (cnc, GDA_CONNECTION_SCHEMA_DATABASES, _("Databases"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_PROCEDURES, _("Stored procedures"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TABLES, _("Connection Tables"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TYPES, _("Available types"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_VIEWS, _("Views"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_INDEXES, _("Indexes"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_AGGREGATES, _("Aggregates"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_USERS, _("Users"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_SEQUENCES, _("Sequences"));
	show_schema (cnc, GDA_CONNECTION_SCHEMA_TRIGGERS, _("Triggers"));

	/* test transactions */
	g_print (_("\tStarting transaction..."));
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : _("Error"));

	g_print (_("\tFinishing transaction..."));
	res = gda_connection_commit_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : _("Error"));

	g_print (_("\tStarting transaction..."));
	res = gda_connection_begin_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : _("Error"));

	g_print (_("\tRolling back transaction..."));
	res = gda_connection_rollback_transaction (cnc, NULL);
	g_print ("%s\n", res ? "OK" : _("Error"));

	dsn_info = gda_config_find_data_source (name);
	/* Postgres own tests */
	if (!strcmp (dsn_info->provider, POSTGRES_PROVIDER_NAME))
		do_postgres_test (cnc);
	gda_config_free_data_source_info (dsn_info);

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

	DISPLAY_MESSAGE (_("Testing GDA client API"));

	client = gda_client_new ();

	dsnlist = gda_config_get_data_source_list ();
	for (l = dsnlist; l != NULL; l = l->next) {
		GdaDataSourceInfo *info = (GdaDataSourceInfo *) l->data;

		if (!info) {
			g_print (_("** ERROR: gda_config_get_data_source_list returned a NULL item\n"));
			gda_main_quit ();
		}

		g_print (_(" Data source = %s, User = %s\n"), info->name, info->username);
		open_connection (client, info->name, info->username, "");
	}

	/* free memory */
	gda_config_free_data_source_list (dsnlist);
	g_object_unref (G_OBJECT (client));
}
