/* GDA Common Library
 * Copyright (C) 2001, The Free Software Foundation
 *
 * Authors:
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

#include <gobject/gsignal.h>
#include <gobject/gmarshal.h>
#include <bonobo/bonobo-marshal.h>
#include "gda-listener.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaListenerPrivate {
};

static void gda_listener_class_init (GdaListenerClass * klass);
static void gda_listener_init       (GdaListener * listener, GdaListenerClass *klass);
static void gda_listener_finalize   (GObject * object);

enum {
	NOTIFY_ACTION,
	LAST_SIGNAL
};

static gint gda_listener_signals[LAST_SIGNAL] = { 0, };

/*
 * Stub implementations
 */
static void
impl_Listener_notifyAction (PortableServer_Servant servant,
			    const CORBA_char * message,
			    GNOME_Database_ListenerAction action,
			    const CORBA_char * description,
			    CORBA_Environment * ev)
{
	GdaListener *listener = (GdaListener *) bonobo_x_object (servant);

	g_return_if_fail (GDA_IS_LISTENER (listener));
	gda_listener_notify_action (listener, message, action, description);
}

/*
 * GdaListener class implementation
 */

BONOBO_TYPE_FUNC_FULL (GdaListener,
		       GNOME_Database_Listener,
		       PARENT_TYPE,
		       gda_listener);

static void
gda_listener_class_init (GdaListenerClass * klass)
{
	POA_GNOME_Database_Listener__epv *epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* set class signals */
	gda_listener_signals[NOTIFY_ACTION] =
		g_signal_new ("notify_action",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaListenerClass, notify_action),
			      NULL, NULL,
			      bonobo_marshal_VOID__POINTER_INT_POINTER,
			      G_TYPE_NONE, 3, G_TYPE_INT,
			      G_TYPE_POINTER, G_TYPE_POINTER);

	object_class->finalize = gda_listener_finalize;
	klass->notify_action = NULL;

	/* set the epv */
	epv = &klass->epv;
	epv->notifyAction = impl_Listener_notifyAction;
}

static void
gda_listener_init (GdaListener *listener, GdaListenerClass *klass)
{
	listener->priv = g_new (GdaListenerPrivate, 1);
}

static void
gda_listener_finalize (GObject * object)
{
	GObjectClass *parent_class;
	GdaListener *listener = (GdaListener *) object;

	g_return_if_fail (GDA_IS_LISTENER (listener));

	/* free memory */
	g_free (listener->priv);

	parent_class = g_type_class_peek (BONOBO_X_OBJECT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}


/**
 * gda_listener_new
 */
GdaListener *
gda_listener_new (void)
{
	GdaListener *listener;

	listener = GDA_LISTENER (g_object_new (GDA_TYPE_LISTENER, NULL));
	return listener;
}

/**
 * gda_listener_notify_action
 */
void
gda_listener_notify_action (GdaListener *listener,
			    const gchar *message,
			    GNOME_Database_ListenerAction action,
			    const gchar *description)
{
	g_return_if_fail (GDA_IS_LISTENER (listener));

	g_signal_emit (G_OBJECT (listener),
		       gda_listener_signals[NOTIFY_ACTION],
		       action, message, description);
}
