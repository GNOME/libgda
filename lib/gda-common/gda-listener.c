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

#include <gtk/gtksignal.h>
#include "gda-listener.h"

struct _GdaListenerPrivate
{
};

static void gda_listener_class_init (GdaListenerClass * klass);
static void gda_listener_init (GdaListener * listener);
static void gda_listener_destroy (GtkObject * object);

enum
{
	NOTIFY_ACTION,
	LAST_SIGNAL
};

static gint gda_listener_signals[LAST_SIGNAL] = { 0, };

/*
 * Stub implementations
 */
static void
impl_GDA_Listener_notifyAction (PortableServer_Servant servant,
				const CORBA_char * message,
				GDA_ListenerAction action,
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
static void
gda_listener_class_init (GdaListenerClass * klass)
{
	POA_GDA_Listener__epv *epv;
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	/* set class signals */
	gda_listener_signals[NOTIFY_ACTION] =
		gtk_signal_new ("notify_action",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaListenerClass,
						   notify_action),
				gtk_marshal_NONE__POINTER_INT_POINTER,
				GTK_TYPE_NONE, 3, GTK_TYPE_INT,
				GTK_TYPE_POINTER, GTK_TYPE_POINTER);
	gtk_object_class_add_signals (object_class, gda_listener_signals,
				      LAST_SIGNAL);

	object_class->destroy = gda_listener_destroy;
	klass->notify_action = NULL;

	/* set the epv */
	epv = &klass->epv;
	epv->notifyAction = impl_GDA_Listener_notifyAction;
}

static void
gda_listener_init (GdaListener * listener)
{
	listener->priv = g_new (GdaListenerPrivate, 1);
}

static void
gda_listener_destroy (GtkObject * object)
{
	GtkObjectClass *parent_class;
	GdaListener *listener = (GdaListener *) object;

	g_return_if_fail (GDA_IS_LISTENER (listener));

	/* free memory */
	g_free (listener->priv);

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

GtkType
gda_listener_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaListener",
			sizeof (GdaListener),
			sizeof (GdaListenerClass),
			(GtkClassInitFunc) gda_listener_class_init,
			(GtkObjectInitFunc) gda_listener_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		type = bonobo_x_type_unique (BONOBO_X_OBJECT_TYPE,
					     POA_GDA_Listener__init, NULL,
					     GTK_STRUCT_OFFSET
					     (GdaListenerClass, epv), &info);
	}

	return type;
}

/**
 * gda_listener_new
 */
GdaListener *
gda_listener_new (void)
{
	GdaListener *listener;

	listener = GDA_LISTENER (gtk_type_new (GDA_TYPE_LISTENER));
	return listener;
}

/**
 * gda_listener_notify_action
 */
void
gda_listener_notify_action (GdaListener * listener,
			    const gchar * message,
			    GDA_ListenerAction action,
			    const gchar * description)
{
	g_return_if_fail (GDA_IS_LISTENER (listener));
	gtk_signal_emit_by_name (GTK_OBJECT (listener),
				 "notify_action",
				 action, message, description);
}
