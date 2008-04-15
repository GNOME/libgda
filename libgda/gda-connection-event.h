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

#ifndef __GDA_CONNECTION_EVENT_H__
#define __GDA_CONNECTION_EVENT_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_CONNECTION_EVENT            (gda_connection_event_get_type())
#define GDA_CONNECTION_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION_EVENT, GdaConnectionEvent))
#define GDA_CONNECTION_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CONNECTION_EVENT, GdaConnectionEventClass))
#define GDA_IS_CONNECTION_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CONNECTION_EVENT))
#define GDA_IS_CONNECTION_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION_EVENT))

struct _GdaConnectionEvent {
	GObject object;
	GdaConnectionEventPrivate *priv;
};

struct _GdaConnectionEventClass {
	GObjectClass parent_class;
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

GType                   gda_connection_event_get_type (void) G_GNUC_CONST;
GdaConnectionEvent     *gda_connection_event_new (GdaConnectionEventType type);

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
