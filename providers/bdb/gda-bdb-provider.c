/*
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-bdb.h>
#include <virtual/gda-vconnection-data-model.h>
#include <libgda/gda-server-provider-impl.h>
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
static gboolean gda_bdb_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
						     GdaQuarkList *params, GdaQuarkList *auth);
static const gchar *gda_bdb_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

static GObjectClass *parent_class = NULL;

/* 
 * private connection data destroy 
 */
static void gda_bdb_free_cnc_data (BdbConnectionData *cdata);

/*
 * GdaBdbProvider class implementation
 */
GdaServerProviderBase data_model_base_functions = {
        gda_bdb_provider_get_name, // get_name
        gda_bdb_provider_get_version,  // get_version
        gda_bdb_provider_get_server_version, // get_server_version
        NULL, //supports_feature
        NULL, // create_worker
	NULL, // create_connection
        NULL, // create_parser
        NULL, // get_data_handler
        NULL, // get_def_dbms_type
        NULL, // supports_operation
        NULL, // create_operation
        NULL, // render_operation
        NULL, // statement_to_sql
        NULL, // identifier_quote
        NULL, // statement_rewrite
        NULL, // open_connection
        gda_bdb_provider_prepare_connection, // prepare_connection
	NULL, // close_connection
        NULL, // escape_string
        NULL, // unescape_string
	NULL, // perform_operation
        NULL, // begin_transaction
        NULL, // commit_transaction
        NULL, // rollback_transaction
        NULL, // add_savepoint
        NULL, // rollback_savepoint
        NULL, // delete_savepoint
        NULL, // statement_prepare
        NULL, // statement_execute

        NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_bdb_provider_class_init (GdaBdbProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
                                                GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
                                                (gpointer) &data_model_base_functions);

	object_class->finalize = gda_bdb_provider_finalize;
}

static void
gda_bdb_provider_init (G_GNUC_UNUSED GdaBdbProvider *pg_prv, G_GNUC_UNUSED GdaBdbProviderClass *klass)
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
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaBdbProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_bdb_provider_class_init,
			NULL, NULL,
			sizeof (GdaBdbProvider),
			0,
			(GInstanceInitFunc) gda_bdb_provider_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VPROVIDER_DATA_MODEL, "GdaBdbProvider", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

/*
 * Get provider name request
 */
static const gchar *
gda_bdb_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return BDB_PROVIDER_NAME;
}

/* 
 * Get version request
 */
static const gchar *
gda_bdb_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

/* 
 * Prepare connection request
 */
static gboolean
gda_bdb_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	BdbConnectionData *cdata;
	gchar *bdb_file, *bdb_db, *dirname;
	GdaDataModel *model;
	GError *error = NULL;
	gboolean retval = TRUE;
	const GSList *clist;
	
	g_return_val_if_fail (GDA_IS_BDB_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* parse connection string */
	dirname = g_strdup (gda_quark_list_find (params, "DB_DIR"));
	bdb_file = g_strdup (gda_quark_list_find (params, "DB_NAME"));
	bdb_db = g_strdup (gda_quark_list_find (params, "DB_PART"));
	if (bdb_file == NULL || *(g_strstrip (bdb_file)) == '\0') {
		gda_connection_add_event_string (cnc, _("The DB_NAME parameter is not defined in the " 
							"connection string."));
		return FALSE;
	}
	if (bdb_db && (*(g_strstrip (bdb_db)) == '\0')) {
		g_free (bdb_db);
		bdb_db = NULL;
	}

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

	/* add the BDB data model as table "data" */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), model, bdb_db ? bdb_db : "data", &error)) {
    gda_connection_add_event_string (cnc,
						 _("Could not add BDB data model to connection: %s"),
						 error && error->message ? error->message : _("no detail"));
		g_error_free (error);
		g_object_unref (model);

		retval = FALSE;
	}
	else {
		/* set associated data */
		cdata = g_new0 (BdbConnectionData, 1);
		cdata->table_model = model;
		cdata->dbname = g_strdup_printf ("%s (%s)", bdb_file, bdb_db ? bdb_db : _("-"));
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
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	return DB_VERSION_STRING;
}

/*
 * Free connection's specific data
 */
static void
gda_bdb_free_cnc_data (BdbConnectionData *cdata)
{
	if (cdata->table_model)
		g_object_unref (cdata->table_model);
	g_free (cdata->dbname);
	g_free (cdata);
}
