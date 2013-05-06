/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <glib/gi18n-lib.h>
#include <virtual/gda-vconnection-data-model.h>
#include <libgda/gda-connection-private.h>
#include "gda-models.h"
#include "gda-models-provider.h"

static void gda_models_provider_class_init (GdaModelsProviderClass *klass);
static void gda_models_provider_init       (GdaModelsProvider *provider,
					    GdaModelsProviderClass *klass);
static void gda_models_provider_finalize   (GObject *object);

static const gchar *gda_models_provider_get_name (GdaServerProvider *provider);
static const gchar *gda_models_provider_get_version (GdaServerProvider *provider);
static gboolean gda_models_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc, 
						     GdaQuarkList *params, GdaQuarkList *auth,
						     guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data);
static const gchar *gda_models_provider_get_server_version (GdaServerProvider *provider,
							    GdaConnection *cnc);
static const gchar *gda_models_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc);

static GObjectClass *parent_class = NULL;

/* 
 * private connection data destroy 
 */
static void gda_models_free_cnc_data (ModelsConnectionData *cdata);

/*
 * GdaModelsProvider class implementation
 */
static void
gda_models_provider_class_init (GdaModelsProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_models_provider_finalize;

	provider_class->get_name = gda_models_provider_get_name;
	provider_class->get_version = gda_models_provider_get_version;
	provider_class->open_connection = gda_models_provider_open_connection;
	provider_class->get_server_version = gda_models_provider_get_server_version;
	provider_class->get_database = gda_models_provider_get_database;
}

static void
gda_models_provider_init (G_GNUC_UNUSED GdaModelsProvider *pg_prv,
			  G_GNUC_UNUSED GdaModelsProviderClass *klass)
{
	/* initialization of provider instance is to add here */
	TO_IMPLEMENT;
}

static void
gda_models_provider_finalize (GObject *object)
{
	GdaModelsProvider *pg_prv = (GdaModelsProvider *) object;

	g_return_if_fail (GDA_IS_MODELS_PROVIDER (pg_prv));

	/* chain to parent class */
	parent_class->finalize(object);
}

GType
gda_models_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaModelsProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_models_provider_class_init,
			NULL, NULL,
			sizeof (GdaModelsProvider),
			0,
			(GInstanceInitFunc) gda_models_provider_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VPROVIDER_DATA_MODEL, "GdaModelsProvider", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

/*
 * Get provider name request
 */
static const gchar *
gda_models_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return MODELS_PROVIDER_NAME;
}

/* 
 * Get version request
 */
static const gchar *
gda_models_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* 
 * Open connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked, create one or
 *     more GdaDataModel objects and declare them to the virtual connection with table names
 *   - open virtual (SQLite) connection
 *   - create a ModelsConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_models_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth,
				     G_GNUC_UNUSED guint *task_id, GdaServerProviderAsyncCallback async_cb,
				     G_GNUC_UNUSED gpointer cb_data)
{
	g_return_val_if_fail (GDA_IS_MODELS_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Don't allow asynchronous connection opening for virtual providers */
	if (async_cb) {
		gda_connection_add_event_string (cnc, _("Provider does not support asynchronous connection open"));
                return FALSE;
	}

	/* Check for connection parameters */
        /* TO_ADD: your own connection parameters */
        const gchar *db_name;
        db_name = gda_quark_list_find (params, "DB_NAME");
        if (!db_name) {
                gda_connection_add_event_string (cnc,
                                                 _("The connection string must contain the DB_NAME value"));
                return FALSE;
        }

	/* create GdaDataModelModels object, only one show here
	 * If the data model(s) can not be created at this time, then it is possible to still declare the
	 * table(s) for which the data model(s) will be created only when the table(s) are accessed 
	 */
	GdaDataModel *model;
	model = NULL; TO_IMPLEMENT;

	/* open virtual connection */
	if (! GDA_SERVER_PROVIDER_CLASS (parent_class)->open_connection (GDA_SERVER_PROVIDER (provider), cnc, params,
                                                                         NULL, NULL, NULL, NULL)) {
                gda_connection_add_event_string (cnc, _("Can't open virtual connection"));
                return FALSE;
        }

	/* add the data model(s) as table(s) */
	GError *error = NULL;
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), model, "a_table", &error)) {
		gda_connection_add_event_string (cnc, 
						 _("Could not add data model to connection: %s"),
						 error && error->message ? error->message : _("no detail"));
		g_error_free (error);
		gda_connection_close_no_warning (cnc);
		g_object_unref (model);

		return FALSE;
	}
	else {
		/* set associated data to the virtual connection */
		ModelsConnectionData *cdata;
		cdata = g_new0 (ModelsConnectionData, 1);
		TO_IMPLEMENT;
		gda_virtual_connection_internal_set_provider_data (GDA_VIRTUAL_CONNECTION (cnc), 
								   cdata, (GDestroyNotify) gda_models_free_cnc_data);
	}

	return TRUE;
}

/*
 * Server version request
 */
static const gchar *
gda_models_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	ModelsConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

        cdata = (ModelsConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return FALSE;
        TO_IMPLEMENT;
        return NULL;
}

/*
 * Get database request
 */
static const gchar *
gda_models_provider_get_database (GdaServerProvider *provider, GdaConnection *cnc)
{
	ModelsConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

        cdata = (ModelsConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;
        TO_IMPLEMENT;
        return NULL;
}

/*
 * Free connection's specific data
 */
static void
gda_models_free_cnc_data (ModelsConnectionData *cdata)
{
	TO_IMPLEMENT;
	g_free (cdata);
}
