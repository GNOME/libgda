/* GDA FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db-org>
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
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include "gda-freetds.h"
#include "gda-freetds-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_FREETDS_HANDLE "GDA_FreeTDS_FreeTDSHandle"

/////////////////////////////////////////////////////////////////////////////
// Private declarations and functions
/////////////////////////////////////////////////////////////////////////////

static GObjectClass *parent_class = NULL;

static void gda_freetds_provider_class_init (GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_init       (GdaFreeTDSProvider      *provider,
                                             GdaFreeTDSProviderClass *klass);
static void gda_freetds_provider_finalize   (GObject                 *object);

static gboolean gda_freetds_provider_open_connection (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      GdaQuarkList *params,
                                                      const gchar *username,
                                                      const gchar *password);
static gboolean gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                                       GdaConnection *cnc);
static const gchar *gda_freetds_provider_get_database (GdaServerProvider *provider,
                                                       GdaConnection *cnc);
static gboolean gda_freetds_provider_change_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      const gchar *name);
static gboolean gda_freetds_provider_create_database (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      const gchar *name);
static gboolean gda_freetds_provider_drop_database (GdaServerProvider *provider,
                                                    GdaConnection *cnc,
                                                    const gchar *name);
static GList *gda_freetds_provider_execute_command (GdaServerProvider *provider,
                                                    GdaConnection *cnc,
                                                    GdaCommand *cmd,
                                                    GdaParameterList *params);
static gboolean gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                                        GdaConnection *cnc,
                                                        GdaTransaction *xaction);
static gboolean gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                                         GdaConnection *cnc,
                                                         GdaTransaction *xaction);
static gboolean gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                                           GdaConnection *cnc,
                                                           GdaTransaction *xaction);
static gboolean gda_freetds_provider_supports (GdaServerProvider *provider,
                                               GdaConnection *cnc,
                                               GdaConnectionFeature feature);
static GdaDataModel *gda_freetds_provider_get_schema (GdaServerProvider *provider,
                                                      GdaConnection *cnc,
                                                      GdaConnectionSchema schema,
                                                      GdaParameterList *params);


static gboolean
gda_freetds_provider_open_connection (GdaServerProvider *provider,
                                      GdaConnection *cnc,
                                      GdaQuarkList *params,
                                      const gchar *username,
                                      const gchar *password)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_close_connection (GdaServerProvider *provider,
                                       GdaConnection *cnc)
{
	return FALSE;
}

static const gchar
*gda_freetds_provider_get_database (GdaServerProvider *provider,
                                    GdaConnection *cnc)
{
	return NULL;
}

static gboolean
gda_freetds_provider_change_database (GdaServerProvider *provider,
                                      GdaConnection *cnc)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_create_database (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                      const gchar *name)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_drop_database (GdaServerProvider *provider,
                                    GdaConnection *cnc,
                                    const gchar *name)
{
	return FALSE;
}

static GList
*gda_freetds_provider_execute_command (GdaServerProvider *provider,
                                       GdaConnection *cnc,
                                       GdaCommand *cmd,
                                       GdaParameterList *params)
{
	return NULL;
}

static gboolean
gda_freetds_provider_begin_transaction (GdaServerProvider *provider,
                                        GdaConnection *cnc,
                                        GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_commit_transaction (GdaServerProvider *provider,
                                         GdaConnection *cnc,
                                         GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_rollback_transaction (GdaServerProvider *provider,
                                           GdaConnection *cnc,
                                           GdaTransaction *xaction)
{
	return FALSE;
}

static gboolean
gda_freetds_provider_supports (GdaServerProvider *provider,
                               GdaConnection *cnc,
                               GdaConnectionFeature feature)
{
	return FALSE;
}

static GdaDataModel
*gda_freetds_provider_get_schema (GdaServerProvider *provider,                                                    GdaConnection *cnc,
                                  GdaConnectionSchema schema,
                                  GdaParameterList *params)
{
	return NULL;
}


/* Initialization */
static void
gda_freetds_provider_class_init (GdaFreeTDSProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_freetds_provider_finalize;
	provider_class->open_connection = gda_freetds_provider_open_connection;
	provider_class->close_connection = gda_freetds_provider_close_connection;
	provider_class->get_database = gda_freetds_provider_get_database;
	provider_class->change_database = gda_freetds_provider_change_database;
	provider_class->create_database = gda_freetds_provider_create_database;
	provider_class->drop_database = gda_freetds_provider_drop_database;
	provider_class->execute_command = gda_freetds_provider_execute_command;
	provider_class->begin_transaction = gda_freetds_provider_begin_transaction;
	provider_class->commit_transaction = gda_freetds_provider_commit_transaction;
	provider_class->rollback_transaction = gda_freetds_provider_rollback_transaction;
	provider_class->supports = gda_freetds_provider_supports;
	provider_class->get_schema = gda_freetds_provider_get_schema;
}

static void
gda_freetds_provider_init (GdaFreeTDSProvider *provider,
                           GdaFreeTDSProviderClass *klass)
{
}

static void
gda_freetds_provider_finalize (GObject *object)
{
	GdaFreeTDSProvider *provider = (GdaFreeTDSProvider *) object;

	g_return_if_fail (GDA_IS_FREETDS_PROVIDER (provider));

	/* chain to parent class */
	parent_class->finalize (object);
}


/////////////////////////////////////////////////////////////////////////////
// Public functions                                                       
/////////////////////////////////////////////////////////////////////////////

gda_freetds_provider_get_type (void)
{
        static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaFreeTDSProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_freetds_provider_class_init,
			NULL, NULL,
			sizeof (GdaFreeTDSProvider),
			0,
			(GInstanceInitFunc) gda_freetds_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFreeTDSProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_freetds_provider_new (void)
{
	GdaFreeTDSProvider *provider;

	provider = g_object_new (gda_freetds_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}
