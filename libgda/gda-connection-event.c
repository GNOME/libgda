/* GDA server library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-connection-event.h>
#include <libgda/gda-intl.h>

struct _GdaConnectionEventPrivate {
	gchar                  *description;
	glong                   provider_code;
	GdaConnectionEventCode  gda_code;
	gchar                  *source;
	gchar                  *sqlstate;
	GdaConnectionEventType  type; /* default is GDA_CONNNECTION_EVENT_FATAL */
};

static void gda_connection_event_class_init (GdaConnectionEventClass *klass);
static void gda_connection_event_init       (GdaConnectionEvent *event, GdaConnectionEventClass *klass);
static void gda_connection_event_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaConnectionEvent class implementation
 */

GType
gda_connection_event_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaConnectionEventClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_connection_event_class_init,
			NULL,
			NULL,
			sizeof (GdaConnectionEvent),
			0,
			(GInstanceInitFunc) gda_connection_event_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaConnectionEvent", &info, 0);
	}
	return type;
}

static void
gda_connection_event_class_init (GdaConnectionEventClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_connection_event_finalize;
}

static void
gda_connection_event_init (GdaConnectionEvent *event, GdaConnectionEventClass *klass)
{
	event->priv = g_new0 (GdaConnectionEventPrivate, 1);
	event->priv->type = GDA_CONNNECTION_EVENT_FATAL;
	event->priv->gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
}

/*
 * gda_connection_event_new:
 *
 * Creates a new unitialized event object. This class is used for communicating
 * events from the different providers to the clients.
 *
 * Returns: the event object.
 */
GdaConnectionEvent *
gda_connection_event_new (void)
{
	GdaConnectionEvent *event;

	event = GDA_CONNNECTION_EVENT (g_object_new (GDA_TYPE_CONNNECTION_EVENT, NULL));
	return event;
}

static void
gda_connection_event_finalize (GObject *object)
{
	GdaConnectionEvent *event = (GdaConnectionEvent *) object;

	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));

	/* free memory */
	if (event->priv->description)
		g_free (event->priv->description);
	if (event->priv->source)
		g_free (event->priv->source);
	if (event->priv->sqlstate)
		g_free (event->priv->sqlstate);

	g_free (event->priv);
	event->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_connection_event_free:
 * @event: the event object.
 *
 * Frees the memory allocated by the event object.
 */
void
gda_connection_event_free (GdaConnectionEvent *event)
{
	g_object_unref (G_OBJECT (event));
}

/**
 * gda_connection_event_list_copy:
 * @events: a GList holding event objects.
 *
 * Creates a new list which contains the same events as @events and
 * adds a reference for each event in the list.
 *
 * You must free the list using #gda_connection_event_list_free.
 * Returns: a list of events.
 */
GList *
gda_connection_event_list_copy (const GList * events)
{
	GList *l;
	GList *new_list;

	new_list = g_list_copy ((GList *) events);
	for (l = new_list; l; l = l->next)
		g_object_ref (G_OBJECT (l->data));

	return new_list;
}

/**
 * gda_connection_event_list_free:
 * @events: a GList holding event objects.
 *
 * Frees all event objects in the list and the list itself.
 * After this function has been called, the @events parameter doesn't point
 * to valid storage any more.
 */
void
gda_connection_event_list_free (GList * events)
{
	g_list_foreach (events, (GFunc) gda_connection_event_free, NULL);
	g_list_free (events);
}

/**
 * gda_connection_event_set_event_type
 * @event: a #GdaConnectionEvent object
 * @type: the severity of the event
 *
 * Sets @event's severity (from a simple notice to a fatal event)
 */
void
gda_connection_event_set_event_type (GdaConnectionEvent *event, GdaConnectionEventType type)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));
	g_return_if_fail (event->priv);

	event->priv->type = type;
}

/**
 * gda_connection_event_get_event_type
 * @event: a #GdaConnectionEvent object
 *
 * Get @event's severity (from a simple notice to a fatal event)
 *
 * Returns: the event type
 */
GdaConnectionEventType
gda_connection_event_get_event_type (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), GDA_CONNNECTION_EVENT_FATAL);
	g_return_val_if_fail (event->priv, GDA_CONNNECTION_EVENT_FATAL);

	return event->priv->type;
}

/**
 * gda_connection_event_get_description
 * @event: a #GdaConnectionEvent.
 *
 * Returns: @event's description.
 */
const gchar *
gda_connection_event_get_description (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), NULL);
	return event->priv->description;
}

/**
 * gda_connection_event_set_description
 * @event: a #GdaConnectionEvent.
 * @description: a description.
 *
 * Sets @event's @description.
 */
void
gda_connection_event_set_description (GdaConnectionEvent *event, const gchar *description)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));

	if (event->priv->description)
		g_free (event->priv->description);
	event->priv->description = g_strdup (description);
}

/**
 * gda_connection_event_get_code
 * @event: a #GdaConnectionEvent.
 *
 * Returns: @event's code (the code is specific to the provider being used)
 */
glong
gda_connection_event_get_code (GdaConnectionEvent * event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), -1);
	return event->priv->provider_code;
}

/**
 * gda_connection_event_set_code
 * @event: a #GdaConnectionEvent.
 * @code: a code.
 *
 * Sets @event's code: the code is specific to the provider being used.
 * If you want to have a common understanding of the event codes, use
 * gda_connection_event_get_gda_code() instead.
 */
void
gda_connection_event_set_code (GdaConnectionEvent *event, glong code)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));
	event->priv->provider_code = code;
}

/**
 * gda_connection_event_get_gda_code
 * @event: a #GdaConnectionEvent
 *
 * Retreive the code associated to @event.
 *
 * Returns: the #GdaConnectionEventCode event's code
 */
GdaConnectionEventCode
gda_connection_event_get_gda_code (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), GDA_CONNECTION_EVENT_CODE_UNKNOWN);
	return event->priv->gda_code;
}

/**
 * gda_connection_event_set_gda_code
 * @event: a #GdaConnectionEvent
 * @code: a code
 *
 * Sets @event's gda code: that code is standardized by the libgda
 * library. If you want to specify the corresponding provider specific code,
 * use gda_connection_event_get_code() instead.
 */ 
void
gda_connection_event_set_gda_code (GdaConnectionEvent *event, GdaConnectionEventCode code)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));
	event->priv->gda_code = code;
}


/**
 * gda_connection_event_get_source
 * @event: a #GdaConnectionEvent.
 *
 * Returns: @event's source. 
 */
const gchar *
gda_connection_event_get_source (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), 0);
	return event->priv->source;
}

/**
 * gda_connection_event_set_source
 * @event: a #GdaConnectionEvent.
 * @source: a source.
 *
 * Sets @event's @source.
 */
void
gda_connection_event_set_source (GdaConnectionEvent *event, const gchar *source)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));

	if (event->priv->source)
		g_free (event->priv->source);
	event->priv->source = g_strdup (source);
}

/**
 * gda_connection_event_get_sqlstate
 * @event: a #GdaConnectionEvent.
 * 
 * Returns: @event's SQL state.
 */
const gchar *
gda_connection_event_get_sqlstate (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNNECTION_EVENT (event), NULL);
	return event->priv->sqlstate;
}

/**
 * gda_connection_event_set_sqlstate
 * @event: a #GdaConnectionEvent.
 * @sqlstate: SQL state.
 *
 * Sets @event's SQL state.
 */
void
gda_connection_event_set_sqlstate (GdaConnectionEvent *event, const gchar *sqlstate)
{
	g_return_if_fail (GDA_IS_CONNNECTION_EVENT (event));

	if (event->priv->sqlstate)
		g_free (event->priv->sqlstate);
	event->priv->sqlstate = g_strdup (sqlstate);
}

