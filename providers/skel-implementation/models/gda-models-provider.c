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
#include <libgda/gda-server-provider-impl.h>
#include "gda-models.h"
#include "gda-models-provider.h"
#include <libgda/gda-debug-macros.h>

static void gda_models_provider_class_init (GdaModelsProviderClass *klass);
static void gda_models_provider_init       (GdaModelsProvider *provider);
static void gda_models_provider_finalize   (GObject *object);

static const gchar *gda_models_provider_get_name (GdaServerProvider *provider);
static const gchar *gda_models_provider_get_version (GdaServerProvider *provider);
static gboolean gda_models_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc, 
							GdaQuarkList *params, GdaQuarkList *auth);
static gboolean gda_models_provider_close_connection (GdaServerProvider *provider,
						      GdaConnection *cnc);
static const gchar *gda_models_provider_get_server_version (GdaServerProvider *provider,
							    GdaConnection *cnc);

/* 
 * private connection data destroy 
 */
static void gda_models_free_cnc_data (ModelsConnectionData *cdata);

/*
 * GdaModelsProvider class implementation
 */
GdaServerProviderBase data_model_base_functions = {
        gda_models_provider_get_name,
        gda_models_provider_get_version,
        gda_models_provider_get_server_version,
        NULL,
        NULL,
	NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        gda_models_provider_prepare_connection,
        gda_models_provider_close_connection,
        NULL,
        NULL,
	NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,

        NULL, NULL, NULL, NULL, /* padding */
};

G_DEFINE_TYPE(GdaModelsProvider, gda_models_provider, GDA_TYPE_VPROVIDER_DATA_MODEL)

static void
gda_models_provider_class_init (GdaModelsProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
                                                GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
                                                (gpointer) &data_model_base_functions);

	object_class->finalize = gda_models_provider_finalize;
}

static void
gda_models_provider_init (G_GNUC_UNUSED GdaModelsProvider *pg_prv)
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
	G_OBJECT_CLASS (gda_models_provider_parent_class)->finalize (object);
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
 * Prepare connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked, create one or
 *     more GdaDataModel objects and declare them to the virtual connection with table names
 *   - create a ModelsConnectionData structure and associate it to @cnc using gda_virtual_connection_internal_set_provider_data()
 *     to get it from other methods
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_models_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
					GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_MODELS_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

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

	/* add the data model(s) as table(s) */
	GError *error = NULL;
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), model, "a_table", &error)) {
		gda_connection_add_event_string (cnc, 
						 _("Could not add data model to connection: %s"),
						 error && error->message ? error->message : _("no detail"));
		g_error_free (error);
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
 * Close connection request
 */
static gboolean
gda_models_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	ModelsConnectionData *cdata;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	cdata = (ModelsConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return FALSE;

	/* liberate the ressources used by the virtual connection */
	TO_IMPLEMENT;

	/* link to parent implementation */
	GdaServerProviderBase *parent_functions;
        parent_functions = gda_server_provider_get_impl_functions_for_class (gda_models_provider_parent_class, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);
        return parent_functions->close_connection (provider, cnc);
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
 * Free connection's specific data
 */
static void
gda_models_free_cnc_data (ModelsConnectionData *cdata)
{
	TO_IMPLEMENT;
	g_free (cdata);
}
