/* GNOME DB FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <libgda/gda-intl.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include "gda-freetds.h"

GdaFreeTDSMessage
*gda_freetds_message_new (GdaConnection *cnc,
                          TDSMSGINFO *info,
                          const gboolean is_err_msg)
{
	GdaFreeTDSConnectionData *tds_cnc;
	GdaFreeTDSMessage *message;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);

	message = g_new0 (GdaFreeTDSMessage, 1);
	g_return_val_if_fail (message != NULL, NULL);

	message->is_err_msg = is_err_msg;
	memcpy ((void *) &message->msg, (void *) info, sizeof (TDSMSGINFO));
	if (info->server != NULL)
		message->msg.server = g_strdup (info->server);
	if (info->message != NULL)
		message->msg.message = g_strdup (info->message);
	if (info->proc_name != NULL)
		message->msg.proc_name = g_strdup (info->proc_name);
	if (info->sql_state != NULL)
		message->msg.sql_state = g_strdup (info->sql_state);

	return message;
}

GdaFreeTDSMessage *
gda_freetds_message_add (GdaConnection *cnc,
                         TDSMSGINFO *info,
                         const gboolean is_err_msg)
{
	GdaFreeTDSMessage *msg = NULL;
	GdaFreeTDSConnectionData *tds_cnc = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);
	g_return_val_if_fail (tds_cnc->msg_arr != NULL, NULL);
	g_return_val_if_fail (tds_cnc->err_arr != NULL, NULL);
	
	msg = gda_freetds_message_new(cnc, info, is_err_msg);
	g_return_val_if_fail (msg != NULL, NULL);

	if (msg->is_err_msg) {
		g_ptr_array_add (tds_cnc->err_arr, msg);
	} else {
		g_ptr_array_add (tds_cnc->msg_arr, msg);
	}

	return msg;
}

void
gda_freetds_message_free (GdaFreeTDSMessage *message)
{
	g_return_if_fail (message != NULL);

	if (message->msg.sql_state != NULL) {
		g_free (message->msg.sql_state);
		message->msg.sql_state = NULL;
	}
	if (message->msg.proc_name != NULL) {
		g_free (message->msg.proc_name);
		message->msg.proc_name = NULL;
	}
	if (message->msg.message != NULL) {
		g_free (message->msg.message);
		message->msg.message = NULL;
	}
	if (message->msg.server != NULL) {
		g_free (message->msg.server);
		message->msg.server = NULL;
	}
	g_free (message);
	message = NULL;
}
