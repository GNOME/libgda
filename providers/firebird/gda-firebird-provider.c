/* GDA FireBird Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <libgda/gda-intl.h>
#include "gda-firebird-provider.h"
#include "gda-firebird-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define CONNECTION_DATA "GDA_Firebird_ConnectionData"
#define TRANSACTION_DATA "GDA_Firebird_TransactionData"
#define STATEMENT_DATA "GDA_Firebird_StatementData"

static void gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass);
static void gda_firebird_provider_init       (GdaFirebirdProvider *provider,
					       GdaFirebirdProviderClass *klass);
static void gda_firebird_provider_finalize   (GObject *object);

static const gchar *gda_firebird_provider_get_version (GdaServerProvider *provider);
static gboolean gda_firebird_provider_open_connection (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaQuarkList *params,
							const gchar *username,
							const gchar *password);
static gboolean gda_firebird_provider_close_connection (GdaServerProvider *provider,
							 GdaConnection *cnc);
static const gchar *gda_firebird_provider_get_server_version (GdaServerProvider *provider,
							       GdaConnection *cnc);
static const gchar *gda_firebird_provider_get_database (GdaServerProvider *provider,
							 GdaConnection *cnc);
static gboolean gda_firebird_provider_change_database (GdaServerProvider *provider,
							GdaConnection *cnc,
							const gchar *name);
static gboolean gda_firebird_provider_create_database (GdaServerProvider *provider,
							GdaConnection *cnc,
							const gchar *name);
static gboolean gda_firebird_provider_drop_database (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      const gchar *name);
static GList *gda_firebird_provider_execute_command (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      GdaCommand *cmd,
						      GdaParameterList *params);
static gboolean gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
							  GdaConnection *cnc,
							  GdaTransaction *xaction);
static gboolean gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
							   GdaConnection *cnc,
							   GdaTransaction *xaction);
static gboolean gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
							     GdaConnection *cnc,
							     GdaTransaction *xaction);
static gboolean gda_firebird_provider_supports (GdaServerProvider *provider,
						 GdaConnection *cnc,
						 GdaConnectionFeature feature);
static GdaDataModel *gda_firebird_provider_get_schema (GdaServerProvider *provider,
							GdaConnection *cnc,
							GdaConnectionSchema schema,
							GdaParameterList *params);

static GObjectClass *parent_class = NULL;

typedef struct {
	gchar *dbname;
	isc_db_handle handle;
	ISC_STATUS status[20];
} GdaFirebirdConnection;

/*
 * GdaFirebirdProvider class implementation
 */

static void
gda_firebird_provider_class_init (GdaFirebirdProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_provider_finalize;
	provider_class->get_version = gda_firebird_provider_get_version;
	provider_class->open_connection = gda_firebird_provider_open_connection;
	provider_class->close_connection = gda_firebird_provider_close_connection;
	provider_class->get_server_version = gda_firebird_provider_get_server_version;
	provider_class->get_database = gda_firebird_provider_get_database;
	provider_class->change_database = gda_firebird_provider_change_database;
	provider_class->create_database = gda_firebird_provider_create_database;
	provider_class->drop_database = gda_firebird_provider_drop_database;
	provider_class->execute_command = gda_firebird_provider_execute_command;
	provider_class->begin_transaction = gda_firebird_provider_begin_transaction;
	provider_class->commit_transaction = gda_firebird_provider_commit_transaction;
	provider_class->rollback_transaction = gda_firebird_provider_rollback_transaction;
	provider_class->supports = gda_firebird_provider_supports;
	provider_class->get_schema = gda_firebird_provider_get_schema;
}

static void
gda_firebird_provider_init (GdaFirebirdProvider *provider, GdaFirebirdProviderClass *klass)
{
}

static void
gda_firebird_provider_finalize (GObject *object)
{
	GdaFirebirdProvider *iprv = (GdaFirebirdProvider *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_PROVIDER (iprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_firebird_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaFirebirdProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_provider_class_init,
			NULL, NULL,
			sizeof (GdaFirebirdProvider),
			0,
			(GInstanceInitFunc) gda_firebird_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFirebirdProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_firebird_provider_new (void)
{
	GdaFirebirdProvider *iprv;

	iprv = g_object_new (GDA_TYPE_FIREBIRD_PROVIDER, NULL);
	return GDA_SERVER_PROVIDER (iprv);
}

/* get_version handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_version (GdaServerProvider *provider)
{
	return VERSION;
}

/* open_connection handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_open_connection (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaQuarkList *params,
					const gchar *username,
					const gchar *password)
{
	GdaFirebirdConnection *icnc;
	gchar *ib_db, *ib_user, *ib_password;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* get parameters */
	ib_db = gda_quark_list_find (params, "DATABASE");
	if (!ib_db) {
		gda_connection_add_error_string (cnc, _("No database specified"));
		return FALSE;
	}

	if (username)
		ib_user = username;
	else
		ib_user = gda_quark_list_find (params, "USERNAME");

	if (password)
		ib_password = password;
	else
		ib_password = gda_quark_list_find (params, "PASSWORD");

	icnc = g_new0 (GdaFirebirdConnection, 1);

	/* prepare connection to firebird server */
	if (ib_user)
		setenv ("ISC_USER", ib_user, 1);
	if (ib_password)
		setenv ("ISC_PASSWORD", ib_password, 1);

	if (isc_attach_database (icnc->status, strlen (ib_db), ib_db, &icnc->handle, 0, NULL)) {
		gda_firebird_connection_make_error (cnc);
		g_free (icnc);
		return FALSE;
	}

	icnc->dbname = g_strdup (ib_db);

	/* attach the GdaFirebirdConnection struct to the connection */
	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, icnc);

	return TRUE;
}

/* close_connection handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaFirebirdConnection *icnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	/* detach from database */
	isc_detach_database (icnc->status, &icnc->handle);

	/* free memory */
	g_free (icnc->dbname);
	g_free (icnc);
	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, NULL);

	return TRUE;
}

/* get_server_version handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_server_version (GdaServerProvider *provider,
					   GdaConnection *cnc)
{
	GdaFirebirdConnection *icnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	return "FIXME";
}

/* get_database handler for the GdaFirebirdProvider class */
static const gchar *
gda_firebird_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaFirebirdConnection *icnc;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	return (const gchar *) icnc->dbname;
}

/* change_database handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_change_database (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name)
{
	/* FIXME */
	return FALSE;
}

/* create_database handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_create_database (GdaServerProvider *provider,
					GdaConnection *cnc,
					const gchar *name)
{
	gchar *sql;
	GdaFirebirdConnection *icnc;
	isc_db_handle db_handle = NULL;
	isc_tr_handle tr_handle = NULL;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	/* build and execute the SQL command */
	sql = g_strdup_printf ("CREATE DATABASE '%s'", name);
	if (isc_dsql_execute_immediate (icnc->status, &db_handle, &tr_handle,
					0, sql, 1, NULL)) {
		gda_firebird_connection_make_error (cnc);
		return FALSE;
	}

	/* we need to detach from the newly created database */
	isc_detach_database (icnc->status, &db_handle);

	return TRUE;
}

/* drop_database handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_drop_database (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name)
{
	return FALSE;
}

static GdaDataModel *
run_sql (GdaConnection *cnc, GdaFirebirdConnection *icnc, isc_tr_handle *itr, const gchar *sql)
{
	GdaDataModel *model;

	model = gda_firebird_recordset_new (cnc, itr, sql);
	return model;
}

/* execute_command handler for the GdaFirebirdProvider class */
static GList *
gda_firebird_provider_execute_command (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaCommand *cmd,
					GdaParameterList *params)
{
	GdaFirebirdConnection *icnc;
	isc_tr_handle *itr;
	gchar **arr;
	gint n;
	GList *reclist = NULL;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	itr = gda_firebird_command_get_transaction (cmd);

	/* parse command */
	switch (gda_command_get_command_type (cmd)) {
	case GDA_COMMAND_TYPE_SQL :
		arr = g_strsplit (gda_command_get_text (cmd), ";", 0);
		if (arr) {
			for (n = 0; arr[n]; n++) {
				reclist = g_list_append (reclist,
							 run_sql (cnc, icnc, itr, arr[n]));
			}

			g_strfreev (arr);
		}
		break;
	}

	return reclist;
}

/* begin_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_begin_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
	GdaFirebirdConnection *icnc;
	isc_tr_handle *itr;
	static char tpb[] = {
		isc_tpb_version3,
		isc_tpb_write,
		isc_tpb_read_committed,
		isc_tpb_rec_version,
		isc_tpb_wait
	};

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	/* start the transaction */
	itr = g_new0 (isc_tr_handle, 1);

	if (isc_start_transaction (icnc->status, itr, 1, &icnc->handle,
				   (unsigned short) sizeof (tpb), &tpb)) {
		gda_firebird_connection_make_error (cnc);
		g_free (itr);
		return FALSE;
	}

	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, itr);

	return TRUE;
}

/* commit_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_commit_transaction (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GdaTransaction *xaction)
{
	GdaFirebirdConnection *icnc;
	isc_tr_handle *itr;
	gboolean result;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	itr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	if (!itr) {
		gda_connection_add_error_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_commit_transaction (icnc->status, itr)) {
		gda_firebird_connection_make_error (cnc);
		result = FALSE;
	}
	else
		result = TRUE;

	g_free (itr);
	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, NULL);

	return result;
}

/* rollback_transaction handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_rollback_transaction (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaTransaction *xaction)
{
	GdaFirebirdConnection *icnc;
	isc_tr_handle *itr;
	gboolean result;

	g_return_val_if_fail (GDA_IS_FIREBIRD_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return FALSE;
	}

	itr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	if (!itr) {
		gda_connection_add_error_string (cnc, _("Invalid transaction handle"));
		return FALSE;
	}

	if (isc_rollback_transaction (icnc->status, itr)) {
		gda_firebird_connection_make_error (cnc);
		result = FALSE;
	}
	else
		result = TRUE;

	g_free (itr);
	g_object_set_data (G_OBJECT (xaction), TRANSACTION_DATA, NULL);

	return result;
}

/* supports handler for the GdaFirebirdProvider class */
static gboolean
gda_firebird_provider_supports (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 GdaConnectionFeature feature)
{
	/* FIXME */
	return FALSE;
}

/* get_schema handler for the GdaFirebirdProvider class */
static GdaDataModel *
gda_firebird_provider_get_schema (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaConnectionSchema schema,
				   GdaParameterList *params)
{
	/* FIXME */
	return NULL;
}

void
gda_firebird_connection_make_error (GdaConnection *cnc)
{
	GdaError *error;
	GdaFirebirdConnection *icnc;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	icnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!icnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return;
	}

	error = gda_error_new ();
	gda_error_set_number (error, isc_sqlcode (icnc->status));
	gda_error_set_source (error, "[GDA Firebird]");

	gda_connection_add_error (cnc, error);
}

isc_tr_handle *
gda_firebird_command_get_transaction (GdaCommand *cmd)
{
	GdaTransaction *xaction;
	isc_tr_handle *itr;

	xaction = gda_command_get_transaction (cmd);
	if (!GDA_IS_TRANSACTION (xaction))
		return NULL;

	itr = g_object_get_data (G_OBJECT (xaction), TRANSACTION_DATA);
	return itr;
}
