/* GDA library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_CLIENT_PRIVATE_H__
#define __GDA_CLIENT_PRIVATE_H__

#include <libgda/gda-client.h>

G_BEGIN_DECLS

void _gda_client_notify_event (GdaClient *client, GdaConnection *cnc, GdaClientEvent event, GdaSet *params);
void _gda_client_notify_error_event (GdaClient *client, GdaConnection *cnc, GdaConnectionEvent *error);
void _gda_client_notify_connection_opened_event (GdaClient *client, GdaConnection *cnc);
void _gda_client_notify_connection_closed_event (GdaClient *client, GdaConnection *cnc);

G_END_DECLS

#endif
