/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org> 
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

#include <stdlib.h>
#include <string.h>
#include <libgda/gda-data-model-array.h>
#include "gda-bdb.h"
#include "gda-bdb-provider.h"

static void gda_bdb_provider_class_init (GdaBdbProviderClass *klass);
static void gda_bdb_provider_init       (GdaBdbProvider *provider,
					 GdaBdbProviderClass *klass);
static void gda_bdb_provider_finalize   (GObject *object);

static const gchar *gda_bdb_provider_get_version (GdaServerProvider *provider);
static gboolean gda_bdb_provider_open_connection (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaQuarkList *params,
						  const gchar *username,
						  const gchar *password);
static gboolean gda_bdb_provider_close_connection (GdaServerProvider *provider,
						   GdaConnection *cnc);
static const gchar *gda_bdb_provider_get_server_version (GdaServerProvider *provider,
						         GdaConnection *cnc);
static const gchar *gda_bdb_provider_get_database (GdaServerProvider *provider,
						   GdaConnection *cnc);
static GdaDataModel *gda_bdb_provider_get_schema (GdaServerProvider *provider,
						  GdaConnection *cnc,
						  GdaConnectionSchema schema,
						  GdaParameterList *params);

static GObjectClass *parent_class = NULL;

/*
 * GdaBdbProvider class implementation
 */
static void
gda_bdb_provider_class_init (GdaBdbProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_bdb_provider_finalize;
	provider_class->get_version = gda_bdb_provider_get_version;
	provider_class->open_connection = gda_bdb_provider_open_connection;
	provider_class->close_connection = gda_bdb_provider_close_connection;
	provider_class->get_server_version = gda_bdb_provider_get_server_version;
	provider_class->get_database = gda_bdb_provider_get_database;
	provider_class->get_schema = gda_bdb_provider_get_schema;
}

static void
gda_bdb_provider_init (GdaBdbProvider *pg_prv, GdaBdbProviderClass *klass)
{
}

static void
gda_bdb_provider_finalize (GObject *object)
{
	GdaBdbProvider *pg_prv = (GdaBdbProvider *) object;

	g_return_if_fail (GDA_IS_BDB_PROVIDER (pg_prv));

	/* chain to parent class */
	parent_class->finalize(object);
}

GType
gda_bdb_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaBdbProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_bdb_provider_class_init,
			NULL, NULL,
			sizeof (GdaBdbProvider),
			0,
			(GInstanceInitFunc) gda_bdb_provider_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaBdbProvider",
					       &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_bdb_provider_new (void)
{
	GdaBdbProvider *provider;

	provider = g_object_new (gda_bdb_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* get_version handler for the GdaBdbProvider class */
static const gchar *
gda_bdb_provider_get_version (GdaServerProvider *provider)
{
	GdaBdbProvider *bdb_prv = (GdaBdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), NULL);
	return PACKAGE_VERSION;
}

/* open_connection handler for the GdaBdbProvider class */
static gboolean
gda_bdb_provider_open_connection (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  GdaQuarkList *params,
				  const gchar *username,
				  const gchar *password)
{
	GdaBdbConnectionData *priv_data;
	gchar *bdb_file, *bdb_db;
	GdaBdbProvider *bdb_prv;
	DB *dbp;
	int ret;
	
	bdb_prv = (GdaBdbProvider *) provider;
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* parse connection string */
	bdb_file = g_strdup (gda_quark_list_find (params, "FILE"));
	bdb_db = g_strdup (gda_quark_list_find (params, "DATABASE"));
	if (bdb_file == NULL || *(g_strstrip (bdb_file)) == '\0') {
		gda_connection_add_event_string (cnc, 
						 _("The FILE parameter is "
						   "not defined in the " 
						   "connection string."));
		return FALSE;
	}
	if (bdb_db != NULL && *(g_strstrip (bdb_db)) == '\0')
		bdb_db = NULL;

	/* open database */
	ret = db_create (&dbp, NULL, 0);
	if (ret != 0) {
		gda_connection_add_event (cnc, gda_bdb_make_error (ret));
		return FALSE;
	}
	ret = dbp->open (dbp, 
#if BDB_VERSION >= 40124 
			 NULL,
#endif
			 bdb_file,
			 bdb_db, 
		   	 DB_UNKNOWN, 	/* autodetect DBTYPE */
			 0, 0);
	if (ret != 0) {
		gda_connection_add_event (cnc, gda_bdb_make_error (ret));
		return FALSE;
	}

	/* set associated data */
	priv_data = g_new0 (GdaBdbConnectionData, 1);
	priv_data->dbname = g_strdup_printf ("%s (%s)",
		 	 		     bdb_file,
			  	             bdb_db ? bdb_db : _("-"));
	priv_data->dbver = g_strdup (DB_VERSION_STRING);
	priv_data->dbp = dbp;
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE, priv_data);
	g_free (bdb_file);
	g_free (bdb_db);

	return TRUE;
}

/* close_connection handler for the GdaBdbProvider class */
static gboolean
gda_bdb_provider_close_connection (GdaServerProvider *provider,
				   GdaConnection *cnc)
{
	GdaBdbConnectionData *priv_data;
	GdaBdbProvider *bdb_prv;
	int ret;
	DB *dbp;

	bdb_prv = (GdaBdbProvider *) provider;
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE);
	if (priv_data == NULL || priv_data->dbp == NULL)
		return FALSE;
	dbp = priv_data->dbp;

	ret = dbp->close(dbp, 0);
	g_free (priv_data->dbname);
	g_free (priv_data->dbver);
	g_free (priv_data);

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE, NULL);
	
	if (ret != 0) {
		gda_connection_add_event (cnc, gda_bdb_make_error (ret));
		return FALSE;
	}
	return TRUE;
}

/* get_server_version handler for the GdaBdbProvider class */
static const gchar *
gda_bdb_provider_get_server_version (GdaServerProvider *provider,
				     GdaConnection *cnc)
{
	GdaBdbConnectionData *priv_data;
	GdaBdbProvider *bdb_prv;

	bdb_prv = (GdaBdbProvider *) provider;
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE);
	if (priv_data == NULL) {
		gda_connection_add_event_string (cnc, _("Invalid BDB handle"));
		return NULL;
	}

	return priv_data->dbver;
}

/* get_database handler for the GdaBdbProvider class */
static const gchar *
gda_bdb_provider_get_database (GdaServerProvider *provider,
			       GdaConnection *cnc)
{
	GdaBdbConnectionData *priv_data;
	GdaBdbProvider *bdb_prv;
	
	bdb_prv = (GdaBdbProvider *) provider;
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE);
	if (priv_data == NULL) {
		gda_connection_add_event_string (cnc, _("Invalid BDB handle"));
		return NULL;
	}

	return priv_data->dbname;
}

/* get_schema handler for the GdaBdbProvider class */
static GdaDataModel *
gda_bdb_provider_get_schema (GdaServerProvider *provider,
			     GdaConnection *cnc,
			     GdaConnectionSchema schema,
			     GdaParameterList *params)
{
	GdaBdbConnectionData *priv_data;
	GdaBdbProvider *bdb_prv;

	bdb_prv = (GdaBdbProvider *) provider;
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (bdb_prv), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_BDB_HANDLE);
	if (priv_data == NULL) {
		gda_connection_add_event_string (cnc, _("Invalid BDB handle"));
		return NULL;
	}
	
	if (schema != GDA_CONNECTION_SCHEMA_TABLES)
		return NULL;

	return gda_bdb_recordset_new (cnc, priv_data->dbp);
}
