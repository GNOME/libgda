/* GDA client library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "gda-client.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaClientPrivate {
	gchar *iid;
	GNOME_Database_Provider corba_provider;
	GList *connections;
};

static void gda_client_class_init (GdaClientClass *klass);
static void gda_client_init       (GdaClient *client, GdaClientClass *klass);
static void gda_client_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
connection_finalized_cb (GObject *object, gpointer user_data)
{
	GdaConnection *cnc = (GdaConnection *) object;
	GdaClient *client = (GdaClient *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	gda_client_remove_connection (client, cnc);
}

/*
 * GdaClient class implementation
 */

static void
gda_client_class_init (GdaClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_client_finalize;
}

static void
gda_client_init (GdaClient *client, GdaClientClass *klass)
{
	g_return_if_fail (GDA_IS_CLIENT (client));

	client->priv = g_new0 (GdaClientPrivate, 1);
	client->priv->iid = NULL;
	client->priv->corba_provider = CORBA_OBJECT_NIL;
	client->priv->connections = NULL;
}

static void
gda_client_finalize (GObject *object)
{
	GdaClient *client = (GdaClient *) object;

	g_return_if_fail (GDA_IS_CLIENT (client));

	/* free memory */
	if (client->priv->iid != NULL)
		g_free (client->priv->iid);
	if (client->priv->corba_provider != NULL)
		CORBA_object_release (client->priv->corba_provider);

	g_list_foreach (client->priv->connections, g_object_unref, NULL);
	g_list_free (client->priv->connections);

	g_free (client->priv);
	client->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_client_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaClientClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_client_class_init,
				NULL, NULL,
				sizeof (GdaClient),
				0,
				(GInstanceInitFunc) gda_client_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaClient", &info, 0);
		}
	}

	return type;
}

/**
 * gda_client_new
 */
GdaClient *
gda_client_new (const gchar *iid)
{
	GdaClient *client;
	CORBA_environment ev;

	g_return_val_if_fail (iid != NULL, NULL);

	CORBA_exception_init (&ev);

	client = g_object_new (GDA_TYPE_CLIENT, NULL);

	/* activate the CORBA component */
	client->priv->corba_provider = bonobo_get_object (
		iid, "IDL:GNOME/Database/Provider:1.0", &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		g_object_unref (G_OBJECT (client));
		gda_log_error (_("Could not activate provider %s"), iid);
		return NULL;
	}

	CORBA_exception_free (&ev);

	client->priv->iid = g_strdup (iid);
	return client;
}

/**
 * gda_client_open_connection
 */
GdaConnection *
gda_client_open_connection (GdaClient *client,
			    const gchar *cnc_string,
			    const gchar *username,
			    const gchar *password)
{
	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* search for the connection in our private list */
}

/**
 * gda_client_close_connection
 */
gboolean
gda_client_close_connection (GdaClient *client, GdaConnection *cnc)
{
}

/**
 * gda_client_add_connection
 */
void
gda_client_add_connection (GdaClient *client, GdaConnection *cnc)
{
	GList *l;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* look if we already have this connection stored */
	for (l = g_list_first (client->priv->connections); l; l = l->next) {
		GdaConnection *lcnc = GDA_CONNECTION (l->data);
		if (lcnc == cnc)
			return;
	}

	client->priv->connections = g_list_append (client->priv->connections, cnc);
	g_signal_connect (G_OBJECT (cnc), "finalize",
			  G_CALLBACK (connection_finalized_cb), client);
}

/**
 * gda_client_remove_connection
 */
void
gda_client_remove_connection (GdaClient *client, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	client->priv->connections = g_list_remove (client->priv->connections, cnc);
	g_signal_disconnect_by_data (G_OBJECT (cnc), client);
}
