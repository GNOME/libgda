/* GDA Xbase Provider
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

#include <libgda/gda-intl.h>
#include "gda-xbase-database.h"
#include "gda-xbase-provider.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define CONNECTION_DATA "GDA_Xbase_ConnectionData"

static void gda_xbase_provider_class_init (GdaXbaseProviderClass *klass);
static void gda_xbase_provider_init       (GdaXbaseProvider *provider,
					   GdaXbaseProviderClass *klass);
static void gda_xbase_provider_finalize   (GObject *object);

static const gchar *gda_xbase_provider_get_version (GdaServerProvider *provider);
static gboolean gda_xbase_provider_open_connection (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_xbase_provider_close_connection (GdaServerProvider *provider,
						     GdaConnection *cnc);
static const gchar *gda_xbase_provider_get_server_version (GdaServerProvider *provider,
							   GdaConnection *cnc);
static const gchar *gda_xbase_provider_get_database (GdaServerProvider *provider,
						     GdaConnection *cnc);
static gboolean gda_xbase_provider_change_database (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *name);
static gboolean gda_xbase_provider_create_database (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    const gchar *name);
static gboolean gda_xbase_provider_drop_database (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  const gchar *name);
static GList *gda_xbase_provider_execute_command (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);
static gboolean gda_xbase_provider_begin_transaction (GdaServerProvider *provider,
						      GdaConnection *cnc,
						      GdaTransaction *xaction);
static gboolean gda_xbase_provider_commit_transaction (GdaServerProvider *provider,
						       GdaConnection *cnc,
						       GdaTransaction *xaction);
static gboolean gda_xbase_provider_rollback_transaction (GdaServerProvider *provider,
							 GdaConnection *cnc,
							 GdaTransaction *xaction);
static gboolean gda_xbase_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);
static GdaDataModel *gda_xbase_provider_get_schema (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaConnectionSchema schema,
						    GdaParameterList *params);

static GObjectClass *parent_class = NULL;

typedef struct {
	gboolean using_directory;
	GHashTable *databases;
} GdaXbaseProviderData;

/*
 * GdaXbaseProvider class implementation
 */

static void
gda_xbase_provider_class_init (GdaXbaseProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	object_class->finalize = gda_xbase_provider_finalize;
	provider_class->get_version = gda_xbase_provider_get_version;
	provider_class->open_connection = gda_xbase_provider_open_connection;
	provider_class->close_connection = gda_xbase_provider_close_connection;
	provider_class->get_server_version = gda_xbase_provider_get_server_version;
	provider_class->get_database = gda_xbase_provider_get_database;
	provider_class->change_database = gda_xbase_provider_change_database;
	provider_class->create_database = gda_xbase_provider_create_database;
	provider_class->drop_database = gda_xbase_provider_drop_database;
	provider_class->execute_command = gda_xbase_provider_execute_command;
	provider_class->begin_transaction = gda_xbase_provider_begin_transaction;
	provider_class->commit_transaction = gda_xbase_provider_commit_transaction;
	provider_class->rollback_transaction = gda_xbase_provider_rollback_transaction;
	provider_class->supports = gda_xbase_provider_supports;
	provider_class->get_schema = gda_xbase_provider_get_schema;
}

static void
gda_xbase_provider_init (GdaXbaseProvider *provider, GdaXbaseProviderClass *klass)
{
}

static void
gda_xbase_provider_finalize (GObject *object)
{
	GdaXbaseProvider *xprv = (GdaXbaseProvider *) object;

	g_return_if_fail (GDA_IS_XBASE_PROVIDER (xprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_xbase_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaXbaseProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xbase_provider_class_init,
			NULL, NULL,
			sizeof (GdaXbaseProvider),
			0,
			(GInstanceInitFunc) gda_xbase_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaXbaseProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_xbase_provider_new (void)
{
	GdaXbaseProvider *xprv;

	xprv = (GdaXbaseProvider *) g_object_new (GDA_TYPE_XBASE_PROVIDER, NULL);
	return GDA_SERVER_PROVIDER (xprv);
}

/* get_version handler for the GdaXbaseProvider class */
static const gchar *
gda_xbase_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_open_connection (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
	GdaXbaseProviderData *pdata;
	GdaXbaseDatabase *xdb;
	const gchar *p_filename, *p_directory;

	/* get parameters */
	p_filename = gda_quark_list_find (params, "FILENAME");
	p_directory = gda_quark_list_find (params, "DIRECTORY");

	if ((p_filename && p_directory)) {
		gda_connection_add_error_string (
			cnc, _("Either FILENAME or DIRECTORY can be specified, but not both or neither"));
		return FALSE;
	}

	/* initialize private structure */
	pdata = g_new0 (GdaXbaseProviderData, 1);
	pdata->databases = g_hash_table_new (g_str_hash, g_str_equal);

	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, (gpointer) pdata);

	/* open all databases */
	if (p_filename) {
		pdata->using_directory = FALSE;
		xdb = gda_xbase_database_open (cnc, p_filename);
		if (xdb) {
			g_hash_table_insert (pdata->databases, gda_xbase_database_get_name (xdb), xdb);
		} else {
			gda_xbase_provider_close_connection (provider, cnc);
			return FALSE;
		}
	} else if (p_directory) {
		GDir *dir;
		GError *error;

		pdata->using_directory = TRUE;
		dir = g_dir_open (p_directory, 0, &error);
		if (error) {
			gda_connection_add_error_string (cnc, error->message);
			g_error_free (error);
			return FALSE;
		}

		while ((p_filename = g_dir_read_name (dir))) {
			gchar *s = g_build_path (p_directory, p_filename);
			xdb = gda_xbase_database_open (cnc, p_filename);
			if (xdb) {
				g_hash_table_insert (pdata->databases, gda_xbase_database_get_name (xdb), xdb);
			} else {
				gda_connection_add_error_string (cnc, _("Could not open file %s"), s);
			}

			g_free (s);
		}

		g_dir_close (dir);
	} else {
		gda_connection_add_error_string (
			cnc, _("Either FILENAME or DIRECTORY must be specified in the connection string"));
		gda_xbase_provider_close_connection (provider, cnc);
		return FALSE;
	}

	return TRUE;
}

static void
close_database (gpointer key, gpointer value, gpointer user_data)
{
	gchar *name = (gchar *) key;
	GdaXbaseDatabase *xdb = (GdaXbaseDatabase *) user_data;

	g_free (name);
	gda_xbase_database_close (xdb);
}

/* close_connection handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaXbaseProviderData *pdata;

	g_return_val_if_fail (GDA_IS_XBASE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	pdata = (GdaXbaseProviderData *) g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!pdata) {
		gda_connection_add_error_string (cnc, _("Invalid Xbase handle"));
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), CONNECTION_DATA, NULL);

	g_hash_table_foreach (pdata->databases, (GHFunc) close_database, NULL);
	g_hash_table_destroy (pdata->databases);

	g_free (pdata);

	return TRUE;
}

/* get_server_version handler for the GdaXbaseProvider class */
static const gchar *
gda_xbase_provider_get_server_version (GdaServerProvider *provider,
				       GdaConnection *cnc)
{
}

/* get_database handler for the GdaXbaseProvider class */
static const gchar *
gda_xbase_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
}

/* change_database handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_change_database (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *name)
{
	/* FIXME */
	return FALSE;
}

/* create_database handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_create_database (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    const gchar *name)
{
}

/* drop_database handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_drop_database (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      const gchar *name)
{
	return FALSE;
}

/* execute_command handler for the GdaXbaseProvider class */
static GList *
gda_xbase_provider_execute_command (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaCommand *cmd,
					GdaParameterList *params)
{
}

/* begin_transaction handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_begin_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
}

/* commit_transaction handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_commit_transaction (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GdaTransaction *xaction)
{
}

/* rollback_transaction handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_rollback_transaction (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaTransaction *xaction)
{
}

/* supports handler for the GdaXbaseProvider class */
static gboolean
gda_xbase_provider_supports (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 GdaConnectionFeature feature)
{
	/* FIXME */
	return FALSE;
}

/* get_schema handler for the GdaXbaseProvider class */
static GdaDataModel *
gda_xbase_provider_get_schema (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaConnectionSchema schema,
				   GdaParameterList *params)
{
	/* FIXME */
	return NULL;
}

void
gda_xbase_provider_make_error (GdaConnection *cnc)
{
	GdaError *error;
	GdaXbaseProviderData *pdata;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	pdata = (GdaXbaseProviderData *) g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!pdata) {
		gda_connection_add_error_string (cnc, _("Invalid Xbase handle"));
		return;
	}

	error = gda_error_new ();
	/* FIXME: get error information */
	gda_error_set_source (error, "[GDA Xbase]");

	gda_connection_add_error (cnc, error);
}
