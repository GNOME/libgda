/* GDA server library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
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
#include <glib/gi18n-lib.h>

struct _GdaConnectionEventPrivate {
	gchar                  *description;
	glong                   provider_code;
	GdaConnectionEventCode  gda_code;
	gchar                  *source;
	gchar                  *sqlstate;
	GdaConnectionEventType  type; /* default is GDA_CONNECTION_EVENT_ERROR */
};

enum {
	PROP_0,

	PROP_TYPE
};

static void gda_connection_event_class_init   (GdaConnectionEventClass *klass);
static void gda_connection_event_init         (GdaConnectionEvent *event, GdaConnectionEventClass *klass);
static void gda_connection_event_finalize     (GObject *object);
static void gda_connection_event_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gda_connection_event_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static GObjectClass *parent_class = NULL;

/*
 * GdaConnectionEvent class implementation
 */

GType
gda_connection_event_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaConnectionEvent", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
gda_connection_event_class_init (GdaConnectionEventClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_connection_event_finalize;
	object_class->set_property = gda_connection_event_set_property;
	object_class->get_property = gda_connection_event_get_property;

	g_object_class_install_property(object_class,
	                                PROP_TYPE,
	                                g_param_spec_int("type",
	                                                 "Type",
	                                                 "Connection event type",
	                                                 0,
	                                                 GDA_CONNECTION_EVENT_COMMAND,
	                                                 GDA_CONNECTION_EVENT_ERROR,
	                                                 G_PARAM_READWRITE));
}

static void
gda_connection_event_init (GdaConnectionEvent *event, GdaConnectionEventClass *klass)
{
	event->priv = g_new0 (GdaConnectionEventPrivate, 1);
	event->priv->type = GDA_CONNECTION_EVENT_ERROR;
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
gda_connection_event_new (GdaConnectionEventType type)
{
	GdaConnectionEvent *event;

	event = GDA_CONNECTION_EVENT (g_object_new (GDA_TYPE_CONNECTION_EVENT, "type", (int)type, NULL));
	return event;
}

static void
gda_connection_event_finalize (GObject *object)
{
	GdaConnectionEvent *event = (GdaConnectionEvent *) object;

	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

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

static void gda_connection_event_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GdaConnectionEvent *event;

	g_return_if_fail (GDA_IS_CONNECTION_EVENT (object));
	event = GDA_CONNECTION_EVENT (object);

	switch(prop_id)
	{
	case PROP_TYPE:
		event->priv->type = g_value_get_int (value);
		if (!event->priv->sqlstate && (event->priv->type == GDA_CONNECTION_EVENT_ERROR)) 
			gda_connection_event_set_sqlstate (event, GDA_SQLSTATE_GENERAL_ERROR);
		else if (((event->priv->type == GDA_CONNECTION_EVENT_NOTICE) || 
			  (event->priv->type == GDA_CONNECTION_EVENT_COMMAND)) &&
			 event->priv->sqlstate)
			gda_connection_event_set_sqlstate (event, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void gda_connection_event_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GdaConnectionEvent *event;

	g_return_if_fail (GDA_IS_CONNECTION_EVENT (object));
	event = GDA_CONNECTION_EVENT (object);

	switch(prop_id)
	{
	case PROP_TYPE:
		g_value_set_int(value, event->priv->type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gda_connection_event_set_event_type
 * @event: a #GdaConnectionEvent object
 * @type: the severity of the event
 *
 * Sets @event's severity (from a simple notice to a fatal event)
 * This function should not be called directly.
 */
void
gda_connection_event_set_event_type (GdaConnectionEvent *event, GdaConnectionEventType type)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
	g_return_if_fail (event->priv);

	g_object_set (G_OBJECT (event), "type", (int) type, NULL);
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
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), GDA_CONNECTION_EVENT_ERROR);
	g_return_val_if_fail (event->priv, GDA_CONNECTION_EVENT_ERROR);

	return event->priv->type;
}

/**
 * gda_connection_event_get_description
 * @event: a #GdaConnectionEvent.
 *
 * Get the description of the event. Note that is @event's type is GDA_CONNECTION_EVENT_COMMAND,
 * the the dsecription is the SQL of the command.
 *
 * Returns: @event's description.
 */
const gchar *
gda_connection_event_get_description (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), NULL);
	return event->priv->description;
}

/**
 * gda_connection_event_set_description
 * @event: a #GdaConnectionEvent.
 * @description: a description.
 *
 * Sets @event's @description. This function should not be called directly.
 */
void
gda_connection_event_set_description (GdaConnectionEvent *event, const gchar *description)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

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
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), -1);
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
 *
 * This function should not be called directly
 */
void
gda_connection_event_set_code (GdaConnectionEvent *event, glong code)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
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
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), GDA_CONNECTION_EVENT_CODE_UNKNOWN);
	return event->priv->gda_code;
}

/**
 * gda_connection_event_set_gda_code
 * @event: a #GdaConnectionEvent
 * @code: a code
 *
 * Sets @event's gda code: that code is standardized by the libgda
 * library. If you want to specify the corresponding provider specific code,
 * use gda_connection_event_get_code() or gda_connection_event_get_sqlstate() instead.
 *
 * This function should not be called directly
 */ 
void
gda_connection_event_set_gda_code (GdaConnectionEvent *event, GdaConnectionEventCode code)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
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
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), 0);
	return event->priv->source;
}

/**
 * gda_connection_event_set_source
 * @event: a #GdaConnectionEvent.
 * @source: a source.
 *
 * Sets @event's @source; this function should not be called directly
 */
void
gda_connection_event_set_source (GdaConnectionEvent *event, const gchar *source)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

	if (event->priv->source)
		g_free (event->priv->source);
	event->priv->source = g_strdup (source);
}

/**
 * gda_connection_event_get_sqlstate
 * @event: a #GdaConnectionEvent.
 *
 * Get the SQLSTATE value of @event. Even though the SQLSTATE values are specified by ANSI SQL and ODBC,
 * consult each DBMS for the possible values. However, the "00000" (success) value means that there is no error,
 * and the "HY000" (general error) value means an error but no better error code available.
 *
 * Returns: @event's SQL state.
 */
const gchar *
gda_connection_event_get_sqlstate (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), NULL);

	return event->priv->sqlstate ? event->priv->sqlstate : GDA_SQLSTATE_NO_ERROR;
}

/**
 * gda_connection_event_set_sqlstate
 * @event: a #GdaConnectionEvent.
 * @sqlstate: SQL state.
 *
 * Changes the SQLSTATE code of @event, this function should not be called directly
 *
 * Sets @event's SQL state.
 */
void
gda_connection_event_set_sqlstate (GdaConnectionEvent *event, const gchar *sqlstate)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

	if (event->priv->sqlstate)
		g_free (event->priv->sqlstate);

	if (sqlstate) 
		event->priv->sqlstate = g_strdup (sqlstate);
	else {
		event->priv->sqlstate = NULL;
		if (event->priv->type == GDA_CONNECTION_EVENT_ERROR)
			event->priv->sqlstate = g_strdup (GDA_SQLSTATE_GENERAL_ERROR);
	}
}

