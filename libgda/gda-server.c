/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
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

#include "gda-server.h"
#include <bonobo/bonobo-generic-factory.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaServerPrivate {
	BonoboGenericFactory *factory;
	gchar *factory_id;
	GHashTable *components;
	GList *clients;
};

static void gda_server_class_init (GdaServerClass *klass);
static void gda_server_init       (GdaServer *server, GdaServerClass *klass);
static void gda_server_finalize   (GObject *object);

enum {
	LAST_COMPONENT_GONE,
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static guint gda_server_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
remove_component_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

/*
 * Callbacks
 */

static void
component_destroyed_cb (GObject *object, gpointer user_data)
{
	GList *l;
	BonoboObject *comp;
	GdaServer *server = (GdaServer *) user_data;

	g_return_if_fail (GDA_IS_SERVER (server));

	comp = BONOBO_OBJECT (object);

	for (l = g_list_first (server->priv->clients); l; l = l->next) {
		BonoboObject *lcomp = BONOBO_OBJECT (l->data);

		if (lcomp == comp) {
			server->priv->clients = g_list_remove (
				server->priv->clients, lcomp);
			if (!server->priv->clients) {
				g_signal_emit (G_OBJECT (server),
					       gda_server_signals[LAST_CLIENT_GONE],
					       0);
			}
		}
	}
}

static BonoboObject *
factory_callback (BonoboGenericFactory *factory,
		  const char *component_id,
		  gpointer closure)
{
	GType *ptype;
	BonoboObject *object;
	GdaServer *server = (GdaServer *) closure;

	g_return_val_if_fail (GDA_IS_SERVER (server), NULL);

	/* search the component in our hash table */
	ptype = g_hash_table_lookup (server->priv->components, component_id);
	if (!ptype) {
		gda_log_error (_("Don't know how to create components with IID %s"), component_id);
		return NULL;
	}

	/* create the new object */
	object = g_object_new (*ptype, NULL);
	if (!BONOBO_IS_OBJECT (object)) {
		gda_log_error (_("Could not create component of type %s"), component_id);
		return NULL;
	}

	server->priv->clients = g_list_append (server->priv->clients, object);
	g_signal_connect (G_OBJECT (object),
			  "destroy",
			  G_CALLBACK (component_destroyed_cb),
			  server);

	return object;
}

/*
 * GdaServer class implementation
 */

static void
gda_server_class_init (GdaServerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_finalize;
	klass->last_component_gone = NULL;
	klass->last_client_gone = NULL;
}

static void
gda_server_init (GdaServer *server, GdaServerClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER (server));

	server->priv = g_new0 (GdaServerPrivate, 1);
	server->priv->factory = NULL;
	server->priv->factory_id = NULL;
	server->priv->components = g_hash_table_new (g_str_hash, g_str_equal);
	server->priv->clients = NULL;
}

static void
gda_server_finalize (GObject *object)
{
	GdaServer *server = (GdaServer *) object;

	g_return_if_fail (GDA_IS_SERVER (server));

	/* free memory */
	if (server->priv->factory != NULL)
		bonobo_object_unref (BONOBO_OBJECT (server->priv->factory));
	if (server->priv->factory_id != NULL)
		g_free (server->priv->factory_id);

	g_hash_table_foreach_remove (server->priv->components,
				     (GHRFunc) remove_component_hash,
				     NULL);
	g_hash_table_destroy (server->priv->components);
	g_list_free (server->priv->clients);

	g_free (server->priv);
	server->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_server_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaServerClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_server_class_init,
				NULL, NULL,
				sizeof (GdaServer),
				0,
				(GInstanceInitFunc) gda_server_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaServer", &info, 0);
		}
	}

	return type;
}

/**
 * gda_server_new
 * @factory_iid: IID of the factory for this server.
 *
 * Creates a new #GdaServer object, which is the main object you
 * should create when creating new providers or other extension
 * components.
 *
 * Returns: a newly allocated #GdaServer object.
 */
GdaServer *
gda_server_new (const gchar *factory_iid)
{
	GdaServer *server;

	g_return_val_if_fail (factory_iid != NULL, NULL);

	server = g_object_new (GDA_TYPE_SERVER, NULL);

	/* create the object factory for this server */
	server->priv->factory_id = g_strdup (factory_iid);
	server->priv->factory = bonobo_generic_factory_new (
		server->priv->factory_id,
		factory_callback,
		server);
	if (!BONOBO_IS_GENERIC_FACTORY (server->priv->factory)) {
		g_object_unref (G_OBJECT (server));
		return NULL;
	}

	return server;
}

/**
 * gda_server_register_component
 * @server: a #GdaServer object.
 * @iid: IID of the component to be registered.
 * @type: type of the class for creating instances for the component
 * being registered.
 *
 * Registers a component on the given #GdaServer object. If successfull,
 * the server will be able to create instances of the component
 * specified.
 */
void
gda_server_register_component (GdaServer *server,
			       const gchar *iid,
			       GType type)
{
	gpointer orig_key;
	gpointer value;
	GType *ptype;

	g_return_if_fail (GDA_IS_SERVER (server));
	g_return_if_fail (iid != NULL);
	g_return_if_fail (type > 0);

	if (g_hash_table_lookup_extended (server->priv->components, iid,
					  &orig_key, &value)) {
		ptype = (GType *) ptype;

		/* already exists, so replace the component information
		   if it has changed */
		if (*ptype == type)
			return;

		g_hash_table_remove (server->priv->components, orig_key);
		g_free (orig_key);
		g_free (value);
	}

	/* add the component to our hash table */
	ptype = g_new0 (GType, 1);
	*ptype = type;
	g_hash_table_insert (server->priv->components, g_strdup (iid), ptype);
}

/**
 * gda_server_unregister_component
 */
void
gda_server_unregister_component (GdaServer *server, const char *iid)
{
	gpointer orig_key;
	gpointer value;

	g_return_if_fail (GDA_IS_SERVER (server));
	g_return_if_fail (iid != NULL);

	if (g_hash_table_lookup_extended (server->priv->components, iid,
					  &orig_key, &value)) {
		g_hash_table_remove (server->priv->components, orig_key);
		g_free (orig_key);
		g_free (value);

		if (g_hash_table_size (server->priv->components) <= 0)
			g_signal_emit (G_OBJECT (server),
				       gda_server_signals[LAST_COMPONENT_GONE],
				       0);
	}
}
