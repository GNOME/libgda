/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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
#include "gda-server-private.h"
#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif
#include <liboaf/liboaf.h>

#ifdef HAVE_GOBJECT
static void gda_server_finalize (GObject *object);
#else
static void gda_server_destroy  (GtkObject *object);
#endif

static GList* server_list = NULL;

/*
 * Private functions
 */
#ifdef HAVE_GOBJECT
static void
gda_server_class_init (GdaServerClass *klass, gpointer data)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = &gda_server_finalize;
	klass->parent = g_type_class_peek_parent (klass);
}
#else
static void
gda_server_class_init (GdaServerClass *klass)
{
	GtkObjectClass* object_class = (GtkObjectClass *) klass;

	object_class->destroy = gda_server_destroy;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_server_instance_init (GdaServer *server_impl, GdaServerClass *klass)
#else
static void
gda_server_instance_init (GdaServer *server_impl)
#endif
{
	g_return_if_fail (GDA_IS_SERVER (server_impl));
	
	server_impl->name = NULL;
	memset ((void *) &server_impl->functions, 0, sizeof (GdaServerImplFunctions));
}

#ifdef HAVE_GOBJECT
static void
gda_server_finalize (GObject *object)
{
	GdaServer *server_impl = GDA_SERVER (object);
	GdaServerClass *klass =
		G_TYPE_INSTANCE_GET_CLASS (object, GDA_SERVER_CLASS, GdaServerClass);

	server_list = g_list_remove (server_list, (gpointer) server_impl);	
	if (server_impl->name)
		g_free ((gpointer) server_impl->name);
	klass->parent->finalize (object);
}
#else
static void
gda_server_destroy (GtkObject *object)
{
	GtkObjectClass *parent_class;
	GdaServer *server_impl = (GdaServer *) object;

	g_return_if_fail(GDA_IS_SERVER (server_impl));
	
	server_list = g_list_remove (server_list, (gpointer) server_impl);
	if (server_impl->name)
		g_free ((gpointer) server_impl->name);
	bonobo_object_unref (BONOBO_OBJECT (server_impl->connection_factory));

	parent_class = gtk_type_class (gtk_object_get_type ());
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}
#endif

#ifdef HAVE_GOBJECT
GType
gda_server_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo info = {
			sizeof (GdaServerClass),
			NULL,
			NULL,
			(GClassInitFunc) gda_server_class_init,
			NULL,
			NULL,
			sizeof (GdaServer),
			0,
			(GInstanceInitFunc) gda_server_instance_init,
			NULL,
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaServer", &info, 0);
	}
	return (type);
}
#else
GtkType
gda_server_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo info = {
			"GdaServer",
			sizeof (GdaServer),
			sizeof (GdaServerClass),
			(GtkClassInitFunc) gda_server_class_init,
			(GtkObjectInitFunc) gda_server_instance_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		type = gtk_type_unique (gtk_object_get_type (), &info);
	}
	return type;
}
#endif

static BonoboObject *
factory_function (BonoboGenericFactory *factory, void *closure)
{
	GdaServer *server_impl = (GdaServer *) closure;
	return BONOBO_OBJECT (gda_server_connection_new (server_impl));
}

/**
 * gda_server_new
 * @name: name of the server
 * @functions: callback functions
 *
 * Create a new GDA provider implementation object. This function initializes
 * all the needed CORBA stuff, registers the server in the system's object
 * directory, and initializes all internal data. After successful return,
 * you've got a ready-to-go GDA provider. To start it, use #gda_server_start
 */
GdaServer *
gda_server_new (const gchar *name, GdaServerImplFunctions *functions)
{
	GdaServer *server_impl;
	
	g_return_val_if_fail(name != NULL, NULL);
	
	/* look for an already running instance */
	server_impl = gda_server_find(name);
	if (server_impl)
		return server_impl;
	
	/* create provider instance */
#ifdef HAVE_GOBJECT
	server_impl = GDA_SERVER (g_object_new (GDA_TYPE_SERVER, NULL));
#else
	server_impl = GDA_SERVER (gtk_type_new (gda_server_get_type ()));
#endif
	server_impl->name = g_strdup (name);
	g_set_prgname (server_impl->name);
	if (functions) {
		memcpy ((void *) &server_impl->functions,
			(const void *) functions,
			sizeof (GdaServerImplFunctions));
	}
	else
		gda_log_message (_("Starting provider %s with no implementation functions"), name);
	
	server_impl->connections = NULL;
	server_impl->is_running = FALSE;

	/* create CORBA connection factory */
	server_impl->connection_factory = bonobo_generic_factory_new (
		name, factory_function, server_impl);
	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (server_impl->connection_factory));
	server_list = g_list_append(server_list, (gpointer) server_impl);

	bonobo_activate ();

	return server_impl;
}

/**
 * gda_server_find
 * @id: object id
 *
 * Searches the list of loaded server implementations by object activation
 * identification
 */
GdaServer *
gda_server_find (const gchar *id)
{
	GList* node;
	
	g_return_val_if_fail(id != NULL, NULL);
	
	node = g_list_first(server_list);
	while (node) {
		GdaServer* server_impl = GDA_SERVER(node->data);
		if (server_impl && !strcmp(server_impl->name, id))
			return server_impl;
		node = g_list_next(node);
	}
	return NULL;
}

/**
 * gda_server_start
 * @server_impl: server implementation
 *
 * Starts the given GDA provider
 */
void
gda_server_start (GdaServer *server_impl)
{
	g_return_if_fail(server_impl != NULL);
	g_return_if_fail(server_impl->is_running == FALSE);
	
	server_impl->is_running = TRUE;
	bonobo_main ();
}

/**
 * gda_server_stop
 * @server_impl: server implementation
 *
 * Stops the given server implementation
 */
void
gda_server_stop (GdaServer *server_impl)
{
	g_return_if_fail(GDA_IS_SERVER(server_impl));
	g_return_if_fail(server_impl->is_running);
	
	gtk_main_quit ();
	server_impl->is_running = FALSE;
}

/*
 * Convenience functions
 */
gint
gda_server_exception (CORBA_Environment *ev)
{
	g_return_val_if_fail(ev != NULL, -1);
	
	switch (ev->_major) {
	case CORBA_SYSTEM_EXCEPTION :
		gda_log_error(_("CORBA system exception %s"), CORBA_exception_id(ev));
		return -1;
	case CORBA_USER_EXCEPTION :
		gda_log_error(_("CORBA user exception: %s"), CORBA_exception_id( ev ));
		return -1;
	default:
		break;
	}
	return 0;
}

