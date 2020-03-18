/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-vprovider-hub.h"
#include "gda-vconnection-hub.h"
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-server-provider-impl.h>

typedef struct {
	int foo;
} GdaVproviderHubPrivate;

/* properties */
enum
{
	PROP_0,
};

static void gda_vprovider_hub_class_init (GdaVproviderHubClass *klass);
static void gda_vprovider_hub_init       (GdaVproviderHub *prov);

G_DEFINE_TYPE_WITH_PRIVATE (GdaVproviderHub, gda_vprovider_hub, GDA_TYPE_VPROVIDER_DATA_MODEL)

static GdaConnection *gda_vprovider_hub_create_connection (GdaServerProvider *provider);
static gboolean       gda_vprovider_hub_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar   *gda_vprovider_hub_get_name (GdaServerProvider *provider);


/*
 * GdaVproviderHub class implementation
 */
static GdaServerProviderBase hub_base_functions = {
	gda_vprovider_hub_get_name,
	NULL,
	NULL,
	NULL,
	NULL,
	gda_vprovider_hub_create_connection,
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
	gda_vprovider_hub_close_connection,
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

static void
gda_vprovider_hub_class_init (GdaVproviderHubClass *klass)
{
	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &hub_base_functions);
}


static void
gda_vprovider_hub_init (GdaVproviderHub *prov)
{
}

/**
 * gda_vprovider_hub_new
 *
 * Creates a new GdaVirtualProvider object which allows one to 
 * add and remove GdaDataModel objects as tables within a connection
 *
 * Returns: a new #GdaVirtualProvider object.
 */
GdaVirtualProvider *
gda_vprovider_hub_new (void)
{
	GdaVirtualProvider *provider;

        provider = g_object_new (gda_vprovider_hub_get_type (), NULL);
        return provider;
}

static GdaConnection *
gda_vprovider_hub_create_connection (GdaServerProvider *provider)
{
	GdaConnection *cnc;
	g_return_val_if_fail (GDA_IS_VPROVIDER_HUB (provider), NULL);

	cnc = g_object_new (GDA_TYPE_VCONNECTION_HUB, "provider", provider, NULL);

	return cnc;
}

static gboolean
gda_vprovider_hub_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_VPROVIDER_HUB (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	GdaServerProviderBase *parent_functions;
	parent_functions = gda_server_provider_get_impl_functions_for_class (gda_vprovider_hub_parent_class, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);
	return parent_functions->close_connection (provider, cnc);
}

static const gchar *
gda_vprovider_hub_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return "Virtual hub";
}
