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


G_BEGIN_DECLS
                             
gboolean sybase_add_server_errors_to_list(GdaConnection *cnc);
gboolean sybase_add_client_errors_to_list(GdaConnection *cnc);
gboolean sybase_make_errors_from_list(GdaConnection *cnc);
gboolean sybase_add_cmsg_errors_to_list(GdaConnection *cnc);

G_END_DECLS

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
	/* when inline error handling is enabled, this function */
	/* will return TRUE if there are any messages where generated */
	/* by open client or the server. */


	GdaSybaseConnectionData *sconn;
	
	//	CS_CLIENTMSG msg;
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
	} 
	else {
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

	//sybase_debug_msg (_("%d messages fetched."), msgcnt);

	/* add any messages to the GdaConnection error list*/
	/* there are some messages we can safely ignore, so */
	/* use the return of sybase_make_errors_from_list() */
	/* to determine if there was an error or not */
	return (sybase_make_errors_from_list(cnc));
}

gboolean sybase_make_errors_from_list(GdaConnection *cnc)
{
	gboolean cmsg_errors = FALSE;
	gboolean server_errors = FALSE;
	gboolean client_errors = FALSE;

	cmsg_errors = sybase_add_cmsg_errors_to_list(cnc);
	client_errors = sybase_add_client_errors_to_list(cnc);
	server_errors = sybase_add_server_errors_to_list(cnc);

	if ( cmsg_errors || server_errors || client_errors ) 
		return TRUE;
	else
		return FALSE;
}

gboolean
sybase_add_cmsg_errors_to_list(GdaConnection *cnc)
{
	CS_RETCODE ret; 
	CS_INT msgcnt = 0; 
	CS_INT msgcur = 0; 
	CS_CLIENTMSG msg; 
	char *tempspace = NULL; 
	GdaConnectionEvent *error;
	gboolean returner = FALSE;
	GdaSybaseConnectionData *sconn;

	sconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, FALSE);

	ret = cs_diag (sconn->context, 
		       CS_STATUS, 
		       CS_CLIENTMSG_TYPE, 
		       CS_UNUSED, 
		       &msgcnt);   
	if ( ret != CS_SUCCEED ) { 
		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		g_return_val_if_fail (error != NULL, FALSE);
		
		gda_connection_event_set_description (error, _("Failed when attempting to retrieve the amount of client messages"));
		gda_connection_event_set_code (error, -1);
		gda_connection_event_set_source (error, "gda-sybase");
		gda_connection_event_set_sqlstate (error, _("Not available"));					
		gda_connection_add_event (cnc, error);
		return TRUE;
	} 
	while ( msgcur < msgcnt ) { 
		msgcur++;            
		ret = cs_diag (sconn->context, 
			       CS_GET, 
			       CS_CLIENTMSG_TYPE, 
			       msgcur, 
			       &msg); 
		if ( ret != CS_SUCCEED ) { 
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, _("An error occurred when attempting to retrieve a client message"));
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);
			return TRUE;
		} 
		else { 
			if (msg.osstringlen > 0) {
				tempspace = g_strdup_printf("%s %ld %s %s",
							    _("OS_Error:("),
							    msg.osnumber,
							    _(") Message: "),
							    msg.osstring);
				error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				g_return_val_if_fail (error != NULL, FALSE);
				gda_connection_event_set_description (error,tempspace);
				gda_connection_event_set_code (error, -1);
				gda_connection_event_set_source (error, "gda-sybase");
				gda_connection_event_set_sqlstate (error, _("Not available"));					
				gda_connection_add_event (cnc, error);
				returner = TRUE;
			}
			else {
				tempspace = g_strdup_printf(_("Sybase OpenClient Msg: severity(%ld), number(%ld), origin(%ld), layer(%ld): %s"),
							    (long) CS_SEVERITY(msg.severity),
							    (long) CS_NUMBER(msg.msgnumber),
							    (long) CS_ORIGIN(msg.msgnumber),
							    (long) CS_LAYER(msg.msgnumber),
							    (msg.msgstring) ? msg.msgstring : "");
				error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				g_return_val_if_fail (error != NULL, FALSE);
				gda_connection_event_set_description (error,tempspace);
				gda_connection_event_set_code (error, -1);
				gda_connection_event_set_source (error, "gda-sybase");
				gda_connection_event_set_sqlstate (error, _("Not available"));					
				gda_connection_add_event (cnc, error);
				returner = TRUE;
			} 
		}
	} 
	g_free((gpointer)tempspace); 

	if ( returner ) {
		ret = cs_diag (sconn->context, 
			       CS_CLEAR, 
			       CS_CLIENTMSG_TYPE, 
			       CS_UNUSED, 
			       NULL);   
		if ( ret != CS_SUCCEED ) { 
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, _("call to cs_diag failed when attempting to clear the client messages"));
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);
			return TRUE;
		} 
	}
	return returner; 
}

gboolean sybase_add_server_errors_to_list(GdaConnection *cnc)
{
	CS_RETCODE ret; 
	CS_INT msgcnt = 0; 
	CS_INT msgcur = 0; 
	char *error_msg;
	CS_SERVERMSG msg; 
	char *servername = NULL; 
	char *procname = NULL; 
	char *messagenumber = NULL; 
	char *severity = NULL; 
	char *state = NULL; 
	char *line = NULL; 
	gboolean returner = FALSE;
	gboolean stupid_5701_message = FALSE;
	GdaSybaseConnectionData *sconn;
	GdaConnectionEvent *error;
	

	sconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, FALSE);

	ret = ct_diag (sconn->connection, 
		       CS_STATUS, 
		       CS_SERVERMSG_TYPE, 
		       CS_UNUSED, 
		       &msgcnt);   
	if ( ret != CS_SUCCEED ) { 
		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		g_return_val_if_fail (error != NULL, FALSE);
		gda_connection_event_set_description (error, _("Failed when attempting to retrieve the amount of server messages"));
		gda_connection_event_set_code (error, -1);
		gda_connection_event_set_source (error, "gda-sybase");
		gda_connection_event_set_sqlstate (error, _("Not available"));					
		gda_connection_add_event (cnc, error);
		return TRUE;			
	} 
	while ( msgcur < msgcnt ) { 
		msgcur++;            
		ret = ct_diag (sconn->connection, 
			       CS_GET, 
			       CS_SERVERMSG_TYPE, 
			       msgcur, 
			       &msg); 
		if ( ret != CS_SUCCEED ) { 
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, _("An error occurred when attempting to retrieve a server message"));
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);
			return TRUE;					
		} 
		else { 
			if ( msg.msgnumber != 5701 ) {
				returner = TRUE;
				if (msg.svrnlen > 0) 
					servername = g_strdup_printf("%s %s",
								     _("Server:"),
								     msg.svrname); 
				if (msg.proclen > 0) 
					procname = g_strdup_printf("%s %s",
								   _("Stored Procedure:"),
								   msg.proc);
					
				messagenumber = g_strdup_printf("%s (%ld)",
								_("Number"),
								msg.msgnumber); 
				severity = g_strdup_printf("%s (%ld)",
							   _("Severity"),
							   msg.severity); 
				state = g_strdup_printf("%s (%ld)",
							_("State"),
							msg.state); 
				line = g_strdup_printf("%s (%ld)",
						       _("Line"),
						       msg.line);
				if ( procname)
					error_msg = 
						g_strdup_printf("Sybase Server Message:%s %s %s %s %s %s %s", 
								servername, 
								severity, 
								state, 
								procname, 
								messagenumber, 
								line,
								msg.text);           
				else
					error_msg = 
						g_strdup_printf("Sybase Server Message:%s %s %s %s %s %s", 
								servername, 
								severity, 
								state, 
								messagenumber, 
								line,
								msg.text);           
				error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
				g_return_val_if_fail (error != NULL, FALSE);
				gda_connection_event_set_description (error, error_msg);
				gda_connection_event_set_code (error, -1);
				gda_connection_event_set_source (error, "gda-sybase");
				gda_connection_event_set_sqlstate (error, _("Not available"));					
				gda_connection_add_event (cnc, error);
			}
			else
				stupid_5701_message = TRUE;
		} 
	} 
	g_free((gpointer)servername); 
	g_free((gpointer)procname); 
	g_free((gpointer)messagenumber); 
	g_free((gpointer)severity); 
	g_free((gpointer)state); 
	g_free((gpointer)line); 

	if ( ( returner ) || ( stupid_5701_message ) ) {
		ret = ct_diag (sconn->connection, 
			       CS_CLEAR, 
			       CS_SERVERMSG_TYPE, 
			       CS_UNUSED, 
			       NULL);   
		if ( ret != CS_SUCCEED ) { 
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, _("call to ct_diag failed when attempting to clear the server messages"));
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);
			return TRUE;
		} 
	}
	return returner; 
}

gboolean sybase_add_client_errors_to_list(GdaConnection *cnc)
{
	CS_RETCODE ret; 
	CS_INT msgcnt = 0; 
	CS_INT msgcur = 0; 
	CS_CLIENTMSG msg; 
	char *tempspace = NULL; 
	GdaSybaseConnectionData *sconn;
	GdaConnectionEvent *error;
	gboolean returner= FALSE;

	sconn = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (sconn != NULL, FALSE);


	ret = ct_diag (sconn->connection, 
		       CS_STATUS, 
		       CS_CLIENTMSG_TYPE, 
		       CS_UNUSED, 
		       &msgcnt);   
	if ( ret != CS_SUCCEED ) { 
		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		g_return_val_if_fail (error != NULL, FALSE);
		gda_connection_event_set_description (error, _("Failed when attempting to retrieve the amount of client messages"));
		gda_connection_event_set_code (error, -1);
		gda_connection_event_set_source (error, "gda-sybase");
		gda_connection_event_set_sqlstate (error, _("Not available"));					
		gda_connection_add_event (cnc, error);
		return TRUE;			
	} 
	while ( msgcur < msgcnt ) { 
		msgcur++;            
		ret = ct_diag (sconn->connection, 
			       CS_GET, 
			       CS_CLIENTMSG_TYPE, 
			       msgcur, 
			       &msg); 
		if ( ret != CS_SUCCEED ) { 
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, _("An error occurred when attempting to retrieve a client message"));
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);
			return TRUE;					
		} 
		else { 
			returner = TRUE;

			tempspace = g_strdup_printf("%s %ld %s %ld %s %ld %s %ld : %s %s",
						    _("Severity"),
						    (long) CS_SEVERITY (msg.msgnumber),
						    _("Layer"),
						    (long) CS_LAYER (msg.msgnumber),
						    _("Origin"),
						    (long) CS_ORIGIN (msg.msgnumber),
						    _("Message Number"),
						    (long) CS_NUMBER (msg.msgnumber),
						    msg.msgstring,
						    msg.osstring);
			error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
			g_return_val_if_fail (error != NULL, FALSE);
			gda_connection_event_set_description (error, tempspace);
			gda_connection_event_set_code (error, -1);
			gda_connection_event_set_source (error, "gda-sybase");
			gda_connection_event_set_sqlstate (error, _("Not available"));					
			gda_connection_add_event (cnc, error);					
		} 
	} 

	ret = ct_diag (sconn->connection, 
		       CS_CLEAR, 
		       CS_CLIENTMSG_TYPE, 
		       CS_UNUSED, 
		       NULL);   
	if ( ret != CS_SUCCEED ) { 
		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
		g_return_val_if_fail (error != NULL, FALSE);
		gda_connection_event_set_description (error, _("call to ct_diag failed when attempting to clear the client messages"));
		gda_connection_event_set_code (error, -1);
		gda_connection_event_set_source (error, "gda-sybase");
		gda_connection_event_set_sqlstate (error, _("Not available"));					
		gda_connection_add_event (cnc, error);
		return TRUE;
	} 

	return returner; 
}

GdaConnectionEvent *
gda_sybase_make_error (GdaSybaseConnectionData *scnc, gchar *fmt, ...)
{
	GdaConnectionEvent *error;
	va_list args;
	const size_t buf_size = 4096;
	char buf[buf_size + 1];

	if (scnc != NULL) {
		switch (scnc->ret) {
		case CS_BUSY:	sybase_error_msg(_("Operation not possible, connection busy."));
			break;
		}
	}
	
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (error) {
		if (fmt) {
			va_start(args, fmt);
			vsnprintf(buf, buf_size, fmt, args);
			va_end(args);

			gda_connection_event_set_description (error, fmt);
		} 
		else {
			gda_connection_event_set_description (error, _("NO DESCRIPTION"));
		}

		gda_connection_event_set_code (error, -1);
		gda_connection_event_set_source (error, "gda-sybase");
		gda_connection_event_set_sqlstate (error, _("Not available"));
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
