/* GDA sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *      Holger Thon <holger.thon@gnome-db.org>
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

#include <libgda/gda-log.h>
#include <cspublic.h>
#include <ctpublic.h>

#include "gda-sybase.h"

CS_RETCODE CS_PUBLIC
gda_sybase_csmsg_callback(CS_CONTEXT *context, CS_CLIENTMSG *msg)
{
	sybase_debug_msg (_("Call: csmsg callback"));
	if (!msg) {
		return CS_SUCCEED;
	}
	
	sybase_debug_msg (_("CS-Library error:\n\tseverity(%ld) layer(%ld) origin (%ld) number(%ld)\n\t%s"),
	                  (long) CS_SEVERITY (msg->msgnumber),
	                  (long) CS_LAYER (msg->msgnumber),
	                  (long) CS_ORIGIN (msg->msgnumber),
	                  (long) CS_NUMBER (msg->msgnumber),
	                  (msg->msgstring) ? msg->msgstring : "");
	if (msg->osstringlen > 0)
		sybase_debug_msg (_("OS error: %s"), msg->osstring);

	return CS_SUCCEED;
}

CS_RETCODE CS_PUBLIC
gda_sybase_clientmsg_callback (CS_CONTEXT *context,
                               CS_CONNECTION *conn,
                               CS_CLIENTMSG *msg)
{
	sybase_debug_msg (_("Call: Client callback."));
	if (!msg) {
		return CS_SUCCEED;
	}

	sybase_debug_msg (_("CT-Client error:\n\tseverity(%ld) layer(%ld) origin (%ld) number(%ld)\n\t%s"),
	                  (long) CS_SEVERITY (msg->msgnumber),
	                  (long) CS_LAYER (msg->msgnumber),
	                  (long) CS_ORIGIN (msg->msgnumber),
	                  (long) CS_NUMBER (msg->msgnumber),
	                  (msg->msgstring) ? msg->msgstring : "");

	return CS_SUCCEED;
}

CS_RETCODE CS_PUBLIC
gda_sybase_servermsg_callback (CS_CONTEXT *context,
                               CS_CONNECTION *conn,
                               CS_SERVERMSG *msg)
{
	sybase_debug_msg (_("Call: server callback"));
	if (!msg) {
		return CS_SUCCEED;
	}

	sybase_debug_msg (_("CT-Server message:\n\tnumber(%ld) severity(%ld) state(%ld) line(%ld)"),
	                  (long) msg->msgnumber, (long) msg->severity,
	                  (long) msg->state, (long) msg->line);
	
	if (msg->svrnlen > 0)
		sybase_debug_msg(_("\tServer name: %s\n"), msg->svrname);

	if (msg->proclen > 0)
		sybase_debug_msg(_("\tProcedure name: %s\n"), msg->proc);

	if (msg->text)
		sybase_debug_msg ("\t%s\n", msg->text);

	return CS_SUCCEED;
}


gboolean
sybase_check_messages(GdaConnection *cnc)
{
	GdaSybaseConnectionData *sconn;
	
	CS_CLIENTMSG msg;
	CS_INT       msgcnt = 0;
	CS_INT       msgcur = 0;

	g_return_val_if_fail (cnc != NULL, FALSE);
	sconn = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
	                                                       OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, FALSE);
	g_return_val_if_fail (sconn->context != NULL, FALSE);

	if (sconn->connection) {
		sconn->mret = ct_diag (sconn->connection,
		                       CS_STATUS, 
		                       CS_ALLMSG_TYPE, 
		                       CS_UNUSED,
		                       &msgcnt);
	} else {
		sconn->mret = cs_diag (sconn->context,
		                       CS_STATUS,
				       CS_CLIENTMSG_TYPE,
				       CS_UNUSED,
				       &msgcnt);
	}
	if (sconn->mret != CS_SUCCEED) {
		sybase_debug_msg (_("ct_diag() failed determining # of client messages."));
		return FALSE;
	}
	
	while (msgcur < msgcnt) {
		msgcur++;
	}

	sybase_debug_msg (_("%d messages fetched."), msgcnt);
	return TRUE;
}

GdaError *
gda_sybase_make_error (GdaSybaseConnectionData *scnc, gchar *fmt, ...)
{
	GdaError *error;
	va_list args;
	const size_t buf_size = 4096;
	char buf[buf_size + 1];

	if (scnc != NULL) {
		switch (scnc->ret) {
			case CS_BUSY:	sybase_error_msg(_("Operation not possible, connection busy."));
					break;
		}
	}
	
	error = gda_error_new();
	if (error) {
		if (fmt) {
			va_start(args, fmt);
			vsnprintf(buf, buf_size, fmt, args);
			va_end(args);

			gda_error_set_description (error, fmt);
		} else {
			gda_error_set_description (error, _("NO DESCRIPTION"));
		}

		gda_error_set_number (error, -1);
		gda_error_set_source (error, "gda-sybase");
		gda_error_set_sqlstate (error, _("Not available"));
	}

	return error;
}

void
sybase_debug_msg(gchar *fmt, ...)
{
// FIXME: remove comment after reviewing error code
//#ifdef SYBASE_DEBUG
	va_list args;
	const size_t buf_size = 4096;
	char buf[buf_size + 1];
	
	va_start(args, fmt);
	vsnprintf(buf, buf_size, fmt, args);
	va_end(args);
	
	gda_log_message("Sybase: %s", buf);
// FIXME: remove comment after reviewing error code
//#endif
}

void
sybase_error_msg(gchar *fmt, ...)
{
	va_list args;
	const size_t buf_size = 4096;
	char buf[buf_size + 1];

	va_start(args, fmt);
	vsnprintf(buf, buf_size, fmt, args);
	va_end(args);

	gda_log_message("Sybase: %s", buf);
}
