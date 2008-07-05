/* GDA Berkeley-DB Provider
 * Copyright (C) 1998 - 2008 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org> 
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifdef USING_MINGW
#define _NO_OLDNAMES
#endif
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-bdb.h>
#include <virtual/gda-vconnection-data-model.h>
#include <libgda/gda-connection-private.h>
#include "gda-bdb.h"
#include "gda-bdb-provider.h"
#include <db.h> /* used only for its DB_VERSION_STRING declaration */

static void gda_bdb_provider_class_init (GdaBdbProviderClass *klass);
static void gda_bdb_provider_init       (GdaBdbProvider *provider,
					 GdaBdbProviderClass *klass);
static void gda_bdb_provider_finalize   (GObject *object);

static const gchar *gda_bdb_provider_get_name (GdaServerProvider *provider);
static const gchar *gda_bdb_provider_get_version (GdaServerProvider *provider);
static gboolean gda_bdb_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
						  GdaQuarkList *params, GdaQuarkList *auth,
						  guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data);
static const gchar *gda_bdb_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar *gda_bdb_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc);

static GObjectClass *parent_class = NULL;

/* 
 * private connection data destroy 
 */
static void gda_bdb_free_cnc_data (BdbConnectionData *cdata);

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

	provider_class->get_name = gda_bdb_provider_get_name;
	provider_class->get_version = gda_bdb_provider_get_version;
	provider_class->open_connection = gda_bdb_provider_open_connection;
	provider_class->get_server_version = gda_bdb_provider_get_server_version;
	provider_class->get_database = gda_bdb_provider_get_database;
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

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VPROVIDER_DATA_MODEL, "GdaBdbProvider", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/*
 * Get provider name request
 */
static const gchar *
gda_bdb_provider_get_name (GdaServerProvider *provider)
{
	return BDB_PROVIDER_NAME;
}

/* 
 * Get version request
 */
static const gchar *
gda_bdb_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* 
 * Open connection request
 */
static gboolean
gda_bdb_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				  GdaQuarkList *params, GdaQuarkList *auth,
				  guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data)
{
	BdbConnectionData *cdata;
	gchar *bdb_file, *bdb_db, *dirname;
	GdaDataModel *model;
	GError *error = NULL;
	gboolean retval = TRUE;
	const GSList *clist;
	
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (async_cb) {
		gda_connection_add_event_string (cnc, _("Provider does not support asynchronous connection open"));
                return FALSE;
	}

	/* parse connection string */
	dirname = g_strdup (gda_quark_list_find (params, "DB_DIR"));
	bdb_file = g_strdup (gda_quark_list_find (params, "DB_NAME"));
	bdb_db = g_strdup (gda_quark_list_find (params, "DB_PART"));
	if (bdb_file == NULL || *(g_strstrip (bdb_file)) == '\0') {
		gda_connection_add_event_string (cnc, _("The DB_NAME parameter is not defined in the " 
							"connection string."));
		return FALSE;
	}
	if (bdb_db != NULL && *(g_strstrip (bdb_db)) == '\0')
		bdb_db = NULL;

	/* create GdaDataModelBdb object */
	if (dirname) {
		gchar *file;
		file = g_build_filename (dirname, bdb_file, NULL);
		model = gda_data_model_bdb_new (file, bdb_db);
		g_free (file);
	}
	else
		model = gda_data_model_bdb_new (bdb_file, bdb_db);

	/* check for errors in the BDB data model */
	if ((clist = gda_data_model_bdb_get_errors (GDA_DATA_MODEL_BDB (model)))) {
		gboolean hasmsg = FALSE;
		for (; clist; clist = clist->next) {
			GError *lerror = (GError *) clist->data;
			if (lerror && lerror->message) {
				gda_connection_add_event_string (cnc, lerror->message);
				hasmsg = TRUE;
			}
		}
		if (!hasmsg)
			gda_connection_add_event_string (cnc, _("An error occurred while accessing the BDB database"));
		g_object_unref (model);
		return FALSE;
	}

	/* open virtual connection */
	if (! GDA_SERVER_PROVIDER_CLASS (parent_class)->open_connection (GDA_SERVER_PROVIDER (provider), cnc, params,
									 NULL, NULL, NULL, NULL)) {
		gda_connection_add_event_string (cnc, _("Can't open virtual connection"));
		return FALSE;
	}

	/* add the BDB data model as table "data" */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), model, bdb_db ? bdb_db : "data", &error)) {
		gda_connection_add_event_string (cnc, 
						 _("Could not add BDB data model to connection: %s"),
						 error && error->message ? error->message : _("no detail"));
		g_error_free (error);
		gda_connection_close_no_warning (cnc);
		g_object_unref (model);

		retval = FALSE;
	}
	else {
		/* set associated data */
		cdata = g_new0 (BdbConnectionData, 1);
		cdata->table_model = model;
		cdata->dbname = g_strdup_printf ("%s (%s)", bdb_file,bdb_db ? bdb_db : _("-"));
		gda_virtual_connection_internal_set_provider_data (GDA_VIRTUAL_CONNECTION (cnc), 
								   cdata, (GDestroyNotify) gda_bdb_free_cnc_data);
	}

	g_free (bdb_file);
	g_free (bdb_db);
	g_free (dirname);

	return retval;
}

/*
 * Server version request
 */
static const gchar *
gda_bdb_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);

	return DB_VERSION_STRING;
}

/*
 * Get database request
 */
static const gchar *
gda_bdb_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	BdbConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);

	cdata = gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
	if (!cdata) 
		return NULL;

	return cdata->dbname;
}

/*
 * Free connection's specific data
 */
static void
gda_bdb_free_cnc_data (BdbConnectionData *cdata)
{
	g_object_unref (cdata->table_model);
	g_free (cdata->dbname);
	g_free (cdata);
}
