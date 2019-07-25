/*
 * Copyright (C) 2005 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-connection-event"

#include <libgda/gda-connection-event.h>
#include <glib/gi18n-lib.h>

typedef struct {
	gchar                  *description;
	glong                   provider_code;
	GdaConnectionEventCode  gda_code;
	gchar                  *source;
	gchar                  *sqlstate;
	GdaConnectionEventType  type; /* default is GDA_CONNECTION_EVENT_ERROR */
} GdaConnectionEventPrivate;

enum {
	PROP_0,

	PROP_TYPE
};

static void gda_connection_event_dispose      (GObject *object);
static void gda_connection_event_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gda_connection_event_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

/*
 * GdaConnectionEvent class implementation
 */

G_DEFINE_TYPE_WITH_PRIVATE (GdaConnectionEvent, gda_connection_event, G_TYPE_OBJECT)


static void
gda_connection_event_class_init (GdaConnectionEventClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_connection_event_dispose;
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
gda_connection_event_init (GdaConnectionEvent *event)
{
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	priv->type = GDA_CONNECTION_EVENT_ERROR;
	priv->gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
}

static void
gda_connection_event_dispose (GObject *object)
{
	GdaConnectionEvent *event = (GdaConnectionEvent *) object;

	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	/* free memory */
	if (priv->description) {
		g_free (priv->description);
		priv->description = NULL;
	}
	if (priv->source) {
		g_free (priv->source);
		priv->source = NULL;
	}
	if (priv->sqlstate) {
		g_free (priv->sqlstate);
		priv->sqlstate = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_connection_event_parent_class)->dispose (object);
}

static void gda_connection_event_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GdaConnectionEvent *event;

	g_return_if_fail (GDA_IS_CONNECTION_EVENT (object));
	event = GDA_CONNECTION_EVENT (object);

	switch(prop_id)	{
	case PROP_TYPE:
		gda_connection_event_set_event_type (event, g_value_get_int (value));
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	switch(prop_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gda_connection_event_set_event_type:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	if (priv->type == type)
		return;

	priv->type = type;
	if (!priv->sqlstate && (priv->type == GDA_CONNECTION_EVENT_ERROR))
		gda_connection_event_set_sqlstate (event, GDA_SQLSTATE_GENERAL_ERROR);
	else if (((priv->type == GDA_CONNECTION_EVENT_NOTICE) ||
		  (priv->type == GDA_CONNECTION_EVENT_COMMAND)) &&
		 priv->sqlstate)
		gda_connection_event_set_sqlstate (event, NULL);
}

/**
 * gda_connection_event_get_event_type:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	g_return_val_if_fail (priv, GDA_CONNECTION_EVENT_ERROR);

	return priv->type;
}

/**
 * gda_connection_event_get_description:
 * @event: a #GdaConnectionEvent.
 *
 * Get the description of the event. Note that is @event's type is GDA_CONNECTION_EVENT_COMMAND,
 * the the description is the SQL of the command.
 *
 * Returns: @event's description.
 */
const gchar *
gda_connection_event_get_description (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), NULL);
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	return priv->description;
}

/**
 * gda_connection_event_set_description:
 * @event: a #GdaConnectionEvent.
 * @description: (nullable): a description, or %NULL (to unset current description if any)
 *
 * Sets @event's @description. This function should not be called directly.
 */
void
gda_connection_event_set_description (GdaConnectionEvent *event, const gchar *description)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	if (priv->description)
		g_free (priv->description);
	if (description)
		priv->description = g_strdup (description);
	else
		priv->description = NULL;
}

/**
 * gda_connection_event_get_code:
 * @event: a #GdaConnectionEvent.
 *
 * Returns: @event's code (the code is specific to the provider being used)
 */
glong
gda_connection_event_get_code (GdaConnectionEvent * event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), -1);
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	return priv->provider_code;
}

/**
 * gda_connection_event_set_code:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	priv->provider_code = code;
}

/**
 * gda_connection_event_get_gda_code:
 * @event: a #GdaConnectionEvent
 *
 * Retrieve the code associated to @event.
 *
 * Returns: the #GdaConnectionEventCode event's code
 */
GdaConnectionEventCode
gda_connection_event_get_gda_code (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), GDA_CONNECTION_EVENT_CODE_UNKNOWN);
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	return priv->gda_code;
}

/**
 * gda_connection_event_set_gda_code:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	priv->gda_code = code;
}


/**
 * gda_connection_event_get_source:
 * @event: a #GdaConnectionEvent.
 *
 * Returns: @event's source. 
 */
const gchar *
gda_connection_event_get_source (GdaConnectionEvent *event)
{
	g_return_val_if_fail (GDA_IS_CONNECTION_EVENT (event), 0);
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);
	return priv->source;
}

/**
 * gda_connection_event_set_source:
 * @event: a #GdaConnectionEvent.
 * @source: a source.
 *
 * Sets @event's @source; this function should not be called directly
 */
void
gda_connection_event_set_source (GdaConnectionEvent *event, const gchar *source)
{
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	if (priv->source)
		g_free (priv->source);
	priv->source = g_strdup (source);
}

/**
 * gda_connection_event_get_sqlstate:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	return priv->sqlstate ? priv->sqlstate : GDA_SQLSTATE_NO_ERROR;
}

/**
 * gda_connection_event_set_sqlstate:
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
	GdaConnectionEventPrivate *priv = gda_connection_event_get_instance_private (event);

	if (priv->sqlstate)
		g_free (priv->sqlstate);

	if (sqlstate) 
		priv->sqlstate = g_strdup (sqlstate);
	else {
		priv->sqlstate = NULL;
		if (priv->type == GDA_CONNECTION_EVENT_ERROR)
			priv->sqlstate = g_strdup (GDA_SQLSTATE_GENERAL_ERROR);
	}
}

