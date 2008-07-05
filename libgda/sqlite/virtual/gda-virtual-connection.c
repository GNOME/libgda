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
#include <sqlite3.h>
#include "gda-virtual-connection.h"

#define PARENT_TYPE GDA_TYPE_CONNECTION
#define CLASS(obj) (GDA_VIRTUAL_CONNECTION_CLASS (G_OBJECT_GET_CLASS (obj)))
#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

struct _GdaVirtualConnectionPrivate {
	gpointer              v_provider_data;
        GDestroyNotify        v_provider_data_destroy_func;
};


static void gda_virtual_connection_class_init (GdaVirtualConnectionClass *klass);
static void gda_virtual_connection_init       (GdaVirtualConnection *vcnc, GdaVirtualConnectionClass *klass);
static void gda_virtual_connection_finalize   (GObject *object);
static GObjectClass *parent_class = NULL;

/*
 * GdaVirtualConnection class implementation
 */
static void
conn_closed_cb (GdaVirtualConnection *vcnc)
{
	if (vcnc->priv->v_provider_data) {
                if (vcnc->priv->v_provider_data_destroy_func)
                        vcnc->priv->v_provider_data_destroy_func (vcnc->priv->v_provider_data);
                else
                        g_warning ("Provider did not clean its connection data");
                vcnc->priv->v_provider_data = NULL;
        }
}

static void
gda_virtual_connection_class_init (GdaVirtualConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	object_class->finalize = gda_virtual_connection_finalize;
	GDA_CONNECTION_CLASS (klass)->conn_closed = (void (*) (GdaConnection*)) conn_closed_cb;
}

static void
gda_virtual_connection_init (GdaVirtualConnection *vcnc, GdaVirtualConnectionClass *klass)
{
	vcnc->priv = g_new0 (GdaVirtualConnectionPrivate, 1);
	vcnc->priv->v_provider_data = NULL;
	vcnc->priv->v_provider_data_destroy_func = NULL;
}

static void
gda_virtual_connection_finalize (GObject *object)
{
	GdaVirtualConnection *vcnc = (GdaVirtualConnection *) object;

	g_return_if_fail (GDA_IS_VIRTUAL_CONNECTION (vcnc));

	/* free memory */
	g_free (vcnc->priv);
	vcnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_virtual_connection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVirtualConnectionClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_virtual_connection_class_init,
				NULL, NULL,
				sizeof (GdaVirtualConnection),
				0,
				(GInstanceInitFunc) gda_virtual_connection_init
			};
			
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaVirtualConnection", &info, G_TYPE_FLAG_ABSTRACT);
		g_static_mutex_unlock (&registering);
		}
	}

	return type;
}

/**
 * gda_virtual_connection_open
 * @virtual_provider: a #GdaVirtualProvider object
 * @error: a place to store errors, or %NULL
 *
 * Creates and opens a new virtual connection using the @virtual_provider provider
 *
 * Returns: a new #GdaConnection object, or %NULL if an error occurred
 */
GdaConnection *
gda_virtual_connection_open (GdaVirtualProvider *virtual_provider, GError **error)
{
	GdaConnection *cnc = NULL;
	g_return_val_if_fail (GDA_IS_VIRTUAL_PROVIDER (virtual_provider), NULL);

	if (PROV_CLASS (virtual_provider)->create_connection) {
		cnc = PROV_CLASS (virtual_provider)->create_connection ((GdaServerProvider*) virtual_provider);
		if (cnc) {
			g_object_set (G_OBJECT (cnc), "provider_obj", virtual_provider, NULL);
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
	}
	else
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_PROVIDER_ERROR,
			     _("Internal error: virtual provider does not implement the create_operation() virtual method"));
	return cnc;
}

/**
 * gda_virtual_connection_internal_set_provider_data
 * @vcnc: a #GdaConnection object
 * @data: an opaque structure, known only to the provider for which @vcnc is opened
 * @destroy_func: function to call when the connection closes and @data needs to be destroyed
 *
 * Note: calling this function more than once will not make it call @destroy_func on any previously
 * set opaque @data, you'll have to do it yourself.
 */
void
gda_virtual_connection_internal_set_provider_data (GdaVirtualConnection *vcnc, gpointer data, GDestroyNotify destroy_func)
{
        g_return_if_fail (GDA_IS_VIRTUAL_CONNECTION (vcnc));
        vcnc->priv->v_provider_data = data;
        vcnc->priv->v_provider_data_destroy_func = destroy_func;
}

/**
 * gda_virtual_connection_internal_get_provider_data
 * @vcnc: a #GdaConnection object
 *
 * Get the opaque pointer previously set using gda_virtual_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: the pointer to the opaque structure set using gda_virtual_connection_internal_set_provider_data()
 */
gpointer
gda_virtual_connection_internal_get_provider_data (GdaVirtualConnection *vcnc)
{
	g_return_val_if_fail (GDA_IS_VIRTUAL_CONNECTION (vcnc), NULL);
        if (! vcnc->priv->v_provider_data)
                gda_connection_add_event_string (GDA_CONNECTION (vcnc), _("Internal error: invalid provider handle"));
        return vcnc->priv->v_provider_data;
}
