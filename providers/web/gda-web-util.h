/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_WEB_UTIL_H__
#define __GDA_WEB_UTIL_H__

#include "gda-web.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

typedef enum {
	MESSAGE_HELLO,
	MESSAGE_CONNECT,
	MESSAGE_BYE,
	MESSAGE_PREPARE,
	MESSAGE_UNPREPARE,
	MESSAGE_EXEC,
	MESSAGE_META
} WebMessageType;

gchar *_gda_web_compute_token (WebConnectionData *cdata);

xmlDocPtr _gda_web_send_message_to_frontend (GdaConnection *cnc, WebConnectionData *cdata,
					     WebMessageType msgtype, const gchar *message,
					     const gchar *hash_key, gchar *out_status_chr);

xmlDocPtr _gda_web_decode_response (GdaConnection *cnc, WebConnectionData *cdata, SoupMessageBody *body,
				    gchar *out_status_chr, guint *out_counter_id);

GdaConnectionEvent *_gda_web_set_connection_error_from_xmldoc (GdaConnection *cnc, xmlDocPtr doc, GError **error);

void      _gda_web_do_server_cleanup (GdaConnection *cnc, WebConnectionData *cdata);
void      _gda_web_change_connection_to_closed (GdaConnection *cnc, WebConnectionData *cdata);
void      _gda_web_free_cnc_data (WebConnectionData *cdata);


G_END_DECLS

#endif

