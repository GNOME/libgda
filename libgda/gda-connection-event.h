/*
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_CONNECTION_EVENT_H__
#define __GDA_CONNECTION_EVENT_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_CONNECTION_EVENT            (gda_connection_event_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaConnectionEvent, gda_connection_event, GDA, CONNECTION_EVENT, GObject)

struct _GdaConnectionEventClass {
	GObjectClass parent_class;

	gpointer padding[12];
};

typedef enum {
	GDA_CONNECTION_EVENT_NOTICE,
	GDA_CONNECTION_EVENT_WARNING,
	GDA_CONNECTION_EVENT_ERROR,
	GDA_CONNECTION_EVENT_COMMAND
	
} GdaConnectionEventType;

typedef enum
{
       GDA_CONNECTION_EVENT_CODE_CONSTRAINT_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_RESTRICT_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_NOT_NULL_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_FOREIGN_KEY_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_UNIQUE_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_CHECK_VIOLATION,
       GDA_CONNECTION_EVENT_CODE_INSUFFICIENT_PRIVILEGES,
       GDA_CONNECTION_EVENT_CODE_UNDEFINED_COLUMN,
       GDA_CONNECTION_EVENT_CODE_UNDEFINED_FUNCTION,
       GDA_CONNECTION_EVENT_CODE_UNDEFINED_TABLE,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_COLUMN,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_DATABASE,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_FUNCTION,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_SCHEMA,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_TABLE,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_ALIAS,
       GDA_CONNECTION_EVENT_CODE_DUPLICATE_OBJECT,
       GDA_CONNECTION_EVENT_CODE_SYNTAX_ERROR,
       GDA_CONNECTION_EVENT_CODE_UNKNOWN
} GdaConnectionEventCode;

#define GDA_SQLSTATE_NO_ERROR "00000"
#define GDA_SQLSTATE_GENERAL_ERROR "HY000"

/**
 * SECTION:gda-connection-event
 * @short_description: Any event which has occurred on a #GdaConnection
 * @title: GdaConnectionEvent
 * @stability: Stable
 * @see_also: #GdaConnection
 *
 * Events occurring on a connection are each represented as a #GdaConnectionEvent object. Each #GdaConnection
 * is responsible for keeping a list of past events; that list can be consulted using the 
 * gda_connection_get_events() function.
 */

void                    gda_connection_event_set_event_type (GdaConnectionEvent *event, GdaConnectionEventType type);
GdaConnectionEventType  gda_connection_event_get_event_type (GdaConnectionEvent *event);

const gchar            *gda_connection_event_get_description (GdaConnectionEvent *event);
void                    gda_connection_event_set_description (GdaConnectionEvent *event, const gchar *description);
glong                   gda_connection_event_get_code (GdaConnectionEvent *event);
void                    gda_connection_event_set_code (GdaConnectionEvent *event, glong code);
GdaConnectionEventCode  gda_connection_event_get_gda_code (GdaConnectionEvent *event);
void                    gda_connection_event_set_gda_code (GdaConnectionEvent *event, GdaConnectionEventCode code);
const gchar            *gda_connection_event_get_source (GdaConnectionEvent *event);
void                    gda_connection_event_set_source (GdaConnectionEvent *event, const gchar *source);
const gchar            *gda_connection_event_get_sqlstate (GdaConnectionEvent *event);
void                    gda_connection_event_set_sqlstate (GdaConnectionEvent *event, const gchar *sqlstate);

G_END_DECLS

#endif
