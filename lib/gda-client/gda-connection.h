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

#if !defined(__gda_connection_h__)
#  define __gda_connection_h__

#include <gda-command.h>
#include <gda-error.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef struct _GdaConnection        GdaConnection;
typedef struct _GdaConnectionClass   GdaConnectionClass;
typedef struct _GdaConnectionPrivate GdaConnectionPrivate;

typedef struct _GdaClient GdaClient; /* defined in gda-client.h */

struct _GdaConnection {
	GObject object;
	GdaConnectionPrivate *priv;
};

struct _GdaConnectionClass {
	GObjectClass object_class;
};

GType             gda_connection_get_type (void);
GdaConnection    *gda_connection_new (GdaClient *client,
				      GNOME_Database_Connection corba_cnc,
				      const gchar *cnc_string,
				      const gchar *username,
				      const gchar *password);
gboolean          gda_connection_close (GdaConnection *cnc);

GdaClient        *gda_connection_get_client (GdaConnection *cnc);
void              gda_connection_set_client (GdaConnection *cnc, GdaClient *client);

const gchar      *gda_connection_get_string (GdaConnection *cnc);
const gchar      *gda_connection_get_username (GdaConnection *cnc);
const gchar      *gda_connection_get_password (GdaConnection *cnc);

void              gda_connection_add_error (GdaConnection *cnc, GdaError *error);
void              gda_connection_add_error_list (GdaConnection *cnc, GList *error_list);

GdaRecordsetList *gda_connection_execute_command (GdaConnection *cnc,
						  GdaCommand *cmd,
						  GdaParameterList *params);

gboolean          gda_connection_begin_transaction (GdaConnection *cnc, const gchar *id);
gboolean          gda_connection_commit_transaction (GdaConnection *cnc, const gchar *id);
gboolean          gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *id);

G_END_DECLS

#endif
