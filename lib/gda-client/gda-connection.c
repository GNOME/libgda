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
#include "gda-connection.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaConnectionPrivate {
	GdaClient *client;
	gchar *id;
	gchar *cnc_string;
	gchar *username;
	gchar *password;
};

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

/*
 * GdaConnection class implementation
 */

static void
gda_connection_class_init (GdaConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_connection_finalize;
}

static void
gda_connection_init (GdaConnection *cnc, GdaConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaConnectionPrivate, 1);
	cnc->priv->client = NULL;
	cnc->priv->id = NULL;
	cnc->priv->cnc_string = NULL;
	cnc->priv->username = NULL;
	cnc->priv->password = NULL;
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	gda_client_remove_connection (cnc->priv->client, cnc);
	g_object_unref (G_OBJECT (cnc->priv->client));

	g_free (cnc->priv->id);
	g_free (cnc->priv->cnc_string);
	g_free (cnc->priv->username);
	g_free (cnc->priv->password);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_connection_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaConnectionClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_connection_class_init,
				NULL, NULL,
				sizeof (GdaConnection),
				0,
				(GInstanceInitFunc) gda_connection_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaConnection", &info, 0);
		}
	}

	return type;
}

/**
 * gda_connection_new
 */
GdaConnection *
gda_connection_new (GdaClient *client,
		    const gchar *id,
		    const gchar *cnc_string,
		    const gchar *username,
		    const gchar *password)
{
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	cnc = g_object_new (GDA_TYPE_CONNECTION, NULL);

	gda_connection_set_client (cnc, client);
	cnc->priv->id = g_strdup (id);
	cnc->priv->cnc_string = g_strdup (cnc_string);
	cnc->priv->username = g_strdup (username);
	cnc->priv->password = g_strdup (password);

	return cnc;
}

/**
 * gda_connection_get_client
 */
GdaClient *
gda_connection_get_client (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return cnc->priv->client;
}

/**
 * gda_connection_set_client
 */
void
gda_connection_set_client (GdaConnection *cnc, GdaClient *client)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	if (cnc->priv->client) {
		gda_client_remove_connection (cnc->priv->client, cnc);
		g_object_unref (G_OBJECT (cnc->priv->client));
	}

	g_object_ref (G_OBJECT (client));
	cnc->priv->client = client;
	gda_client_add_connection (cnc->priv->client, cnc);
}

/**
 * gda_connection_close
 */
gboolean
gda_connection_close (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	return gda_client_close_connection (cnc->priv->client, cnc);
}
