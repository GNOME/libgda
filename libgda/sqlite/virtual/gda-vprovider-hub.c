/* 
 * GDA common library
 * Copyright (C) 2007 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include "gda-vprovider-hub.h"
#include "gda-vconnection-hub.h"

struct _GdaVproviderHubPrivate {
	int foo;
};

/* properties */
enum
{
        PROP_0,
};

static void gda_vprovider_hub_class_init (GdaVproviderHubClass *klass);
static void gda_vprovider_hub_init       (GdaVproviderHub *prov, GdaVproviderHubClass *klass);
static void gda_vprovider_hub_finalize   (GObject *object);
static void gda_vprovider_hub_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void gda_vprovider_hub_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);
static GObjectClass  *parent_class = NULL;

static GdaConnection *gda_vprovider_hub_create_connection (GdaServerProvider *provider);
static gboolean       gda_vprovider_hub_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
							 GdaQuarkList *params, GdaQuarkList *auth,
							 guint *task_id, GdaServerProviderAsyncCallback async_cb, 
							 gpointer cb_data);
static gboolean       gda_vprovider_hub_close_connection (GdaServerProvider *provider, GdaConnection *cnc);


/*
 * GdaVproviderHub class implementation
 */
static void
gda_vprovider_hub_class_init (GdaVproviderHubClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *server_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_vprovider_hub_finalize;
	server_class->create_connection = gda_vprovider_hub_create_connection;
	server_class->open_connection = gda_vprovider_hub_open_connection;
	server_class->close_connection = gda_vprovider_hub_close_connection;

	/* Properties */
        object_class->set_property = gda_vprovider_hub_set_property;
        object_class->get_property = gda_vprovider_hub_get_property;
}

static void
gda_vprovider_hub_init (GdaVproviderHub *prov, G_GNUC_UNUSED GdaVproviderHubClass *klass)
{
	prov->priv = g_new (GdaVproviderHubPrivate, 1);
}

static void
gda_vprovider_hub_finalize (GObject *object)
{
	GdaVproviderHub *prov = (GdaVproviderHub *) object;

	g_return_if_fail (GDA_IS_VPROVIDER_HUB (prov));

	/* free memory */
	g_free (prov->priv);
	prov->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_vprovider_hub_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVproviderHubClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vprovider_hub_class_init,
				NULL, NULL,
				sizeof (GdaVproviderHub),
				0,
				(GInstanceInitFunc) gda_vprovider_hub_init
			};
			
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VPROVIDER_DATA_MODEL, "GdaVproviderHub", &info, 0);
		g_static_mutex_unlock (&registering);
		}
	}

	return type;
}

static void
gda_vprovider_hub_set_property (GObject *object,
				       guint param_id,
				       G_GNUC_UNUSED const GValue *value,
				       G_GNUC_UNUSED GParamSpec *pspec)
{
        GdaVproviderHub *prov;

        prov = GDA_VPROVIDER_HUB (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			break;
                }
        }
}

static void
gda_vprovider_hub_get_property (GObject *object,
				       guint param_id,
				       G_GNUC_UNUSED GValue *value,
				       G_GNUC_UNUSED GParamSpec *pspec)
{
        GdaVproviderHub *prov;

        prov = GDA_VPROVIDER_HUB (object);
        if (prov->priv) {
		switch (param_id) {
		default:
			break;
		}
        }
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
gda_vprovider_hub_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				   GdaQuarkList *params, GdaQuarkList *auth,
				   guint *task_id, GdaServerProviderAsyncCallback async_cb, gpointer cb_data)
{
	/* nothing to do here */
	return GDA_SERVER_PROVIDER_CLASS (parent_class)->open_connection (GDA_SERVER_PROVIDER (provider), 
									  cnc, params, auth, 
									  task_id, async_cb, cb_data);
}

static void
cnc_close_foreach_func (GdaConnection *cnc, G_GNUC_UNUSED const gchar *ns, GdaVconnectionHub *hub)
{
	/*g_print ("---- FOREACH: Removing connection %p ('%s') from HUB\n", cnc, ns);*/
	if (! gda_vconnection_hub_remove (hub, cnc, NULL))
		g_warning ("Internal GdaVproviderHub error");
}

static gboolean
gda_vprovider_hub_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_VPROVIDER_HUB (provider), FALSE);
	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (cnc), FALSE);

	gda_vconnection_hub_foreach (GDA_VCONNECTION_HUB (cnc),
				     (GdaVConnectionHubFunc) cnc_close_foreach_func, cnc);

	return GDA_SERVER_PROVIDER_CLASS (parent_class)->close_connection (GDA_SERVER_PROVIDER (provider), cnc);
}
