/* GDA sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      mike wingert <wingert.3@postbox.acs.ohio-state.edu>
	*      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	        Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#include "gda-sybase.h"

GdaError *
gda_sybase_make_error (sybase_connection *sconn)
{
	GdaError *error;

	error = gda_error_new ();
	if (sconn->err.error_msg != NULL) 
			{
					gda_error_set_description (error, sconn->err.error_msg);
/*  					gda_error_set_number (error, mysql_errno (handle)); */
			}
	else 
			{
					gda_error_set_description (error, "NO DESCRIPTION");
					gda_error_set_number (error, -1);
			}
	gda_error_set_source (error, "gda-sybase");
	gda_error_set_sqlstate (error, "Not available");

	return error;
}

gboolean
sybase_check_messages(GdaServerConnection *cnc)
{
		CS_RETCODE ret;
		CS_INT msg_count;
		gboolean clientmsg_retval = FALSE;
		gboolean servermsg_retval = FALSE;
		gboolean cmsg_retval = FALSE;
		gboolean msgcheck = FALSE;
		sybase_connection *sconn;

		/* with sybase, after every call to the client library */
		/* we check the messages */
		sconn = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_SYBASE_HANDLE);

		if (sconn)
				{
/*  						 do we have a valid connection structure?  */
						if (sconn->connection)
								{     	
/*  										 see if there is any messages  */
										ret = ct_diag (sconn->connection, 
																									CS_STATUS, 
																									CS_ALLMSG_TYPE,
																									CS_UNUSED, 
																									&msg_count); 										
										if (ret != CS_SUCCEED)
												{
														/* this is bad, we can't get a message count  */
														/* FIXME - i'll worry about this later 					 */
														return TRUE;	
												}
										if (msg_count < 1)
												{
														/* woo hoo! no messages, so return FALSE */
														return FALSE;						
												}
/*  										if any of the retval = TRUE, there was an error  */
										clientmsg_retval = sybase_check_clientmsg(cnc);
										servermsg_retval = sybase_check_servermsg(cnc);
							}
					if (sconn->context)
							{
									cmsg_retval = sybase_check_cmsg(cnc);										
							}
			}	
	if ((cmsg_retval) || (clientmsg_retval) || (servermsg_retval))
			{
					return TRUE;
			}
	else
			{
					return FALSE;
			}
		return TRUE;

}

gboolean 
sybase_check_clientmsg(GdaServerConnection *cnc)
{
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcnt = 0;
		CS_INT msgcur = 0;
		GdaError *err;
		gchar *message;
		gboolean error_occured = FALSE;
		CS_CLIENTMSG msg;
		sybase_connection *sconn;

		sconn = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_SYBASE_HANDLE);

		if (sconn)
				{
						if (sconn->connection)
								{
										ret = ct_diag (sconn->connection, 
																									CS_STATUS, 
																									CS_CLIENTMSG_TYPE, 
																									CS_UNUSED,
																									&msgcnt);												
										if (ret != CS_SUCCEED)
												{
														sconn->err.error_msg = g_new(gchar,201);
														g_snprintf(message,
																									200,
																									"%s",
																									_("ct_diag 1 failed in sybase_check_clientmsg"));
														err = gda_sybase_make_error(sconn);
														gda_server_connection_add_error(cnc,err);
														g_free((gpointer)sconn->err.error_msg);
														return TRUE;
												}
										if (!msgcnt)
												return FALSE;
										while (msgcur < msgcnt)
												{
														msgcur++;						
														ret = ct_diag (sconn->connection, 
																													CS_GET, 
																													CS_CLIENTMSG_TYPE, 
																													msgcur,
																													&msg);	
														if (ret != CS_SUCCEED)
																{
																		sconn->err.error_msg = g_new(gchar,201);
																		g_snprintf(message,
																													200,
																													"%s",
																													_("ct_diag 2 failed in sybase_check_clientmsg"));
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);
																		error_occured = TRUE;
																}
														if (msg.severity != CS_SV_INFORM)
																{
																		sconn->err.error_msg = sprintf_clientmsg(_("Client Message: "),
																																																											&msg);
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);																		
																		error_occured = TRUE;
																}
														else
																{
																		/* this will just print to a debug file for now */
																		sconn->err.error_msg = 
																				sprintf_clientmsg(_("Informational Client  Message: "),
																																																											&msg);
																		sybase_debug_msg(sconn->err.error_msg);
																		g_free((gpointer)sconn->err.error_msg);	
																}
														/* check if we have any new messages to worry about */
														if (msgcur == msgcnt) 
																{
																		ret = ct_diag (sconn->connection, 
																																	CS_STATUS,
																																	CS_CLIENTMSG_TYPE, 
																																	CS_UNUSED, 
																																	&msgcnt);
																		if (ret != CS_SUCCEED) 
																				{
																						sconn->err.error_msg = g_new(gchar,201);
																						g_snprintf(message,
																																	200,
																																	"%s",
																																	_("ct_diag 2 failed in sybase_check_clientmsg"));
																						err = gda_sybase_make_error(sconn);
																						gda_server_connection_add_error(cnc,err);
																						g_free((gpointer)sconn->err.error_msg);
																						error_occured = TRUE; 			
																				} /* if ret != CS_SUCCEED */
																} /* if msgcur == msgcnt */
												} /* while msgcur < msgcnt */ 
								} /* if sconn->connection */	
				} /* if sconn */
		return error_occured;	
}

gboolean 
sybase_check_servermsg(GdaServerConnection *cnc)
{
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcnt = 0;
		CS_INT msgcur = 0;
		GdaError *err;
		gchar *message;
		gboolean error_occured = FALSE;
		CS_SERVERMSG msg;
		sybase_connection *sconn;

		sconn = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_SYBASE_HANDLE);

		if (sconn)
				{
						if (sconn->connection)
								{
										ret = ct_diag (sconn->connection, 
																									CS_STATUS, 
																									CS_SERVERMSG_TYPE, 
																									CS_UNUSED,
																									&msgcnt);
										if (ret != CS_SUCCEED)
												{
														sconn->err.error_msg = g_new(gchar,201);
														g_snprintf(message,
																									200,
																									"%s",
																									_("ct_diag 1 failed in sybase_check_servermsg"));
														err = gda_sybase_make_error(sconn);
														gda_server_connection_add_error(cnc,err);
														g_free((gpointer)sconn->err.error_msg);
														return TRUE;
												}
										if (!msgcnt)
												return FALSE;										
										while (msgcur < msgcnt)
												{
														msgcur++;						
														ret = ct_diag (sconn->connection, 
																													CS_GET, 
																													CS_SERVERMSG_TYPE, 
																													msgcur,
																													&msg);	
														if (ret != CS_SUCCEED)
																{
																		sconn->err.error_msg = g_new(gchar,201);
																		g_snprintf(message,
																													200,
																													"%s",
																													_("ct_diag 2 failed in sybase_check_servermsg"));
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);
																		error_occured = TRUE;
																}

														/* for some wierd reason, severity of 10 occurs often */
														/* whenever you change databases using the 'using' SQL */
														/* command. Make sure that that message doesn't signal an error*/

														if ((msg.severity != CS_SV_INFORM) || (msg.severity != 10))
																{
																		sconn->err.error_msg = sprintf_servermsg(_("Server Message: "),
																																																											&msg);
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);																		
																		error_occured = TRUE;
																}
														else
																{
																		/* this will just print to a debug file for now */
																		sconn->err.error_msg = 
																				sprintf_servermsg(_("Informational Server  Message: "),
																																						&msg);
																		sybase_debug_msg(sconn->err.error_msg);
																		g_free((gpointer)sconn->err.error_msg);	
																}
														/* check if we have any new messages to worry about */
														if (msgcur == msgcnt) 
																{
																		ret = ct_diag (sconn->connection, 
																																	CS_STATUS,
																																	CS_SERVERMSG_TYPE, 
																																	CS_UNUSED, 
																																	&msgcnt);
																		if (ret != CS_SUCCEED) 
																				{
																						sconn->err.error_msg = g_new(gchar,201);
																						g_snprintf(message,
																																	200,
																																	"%s",
																																	_("ct_diag 2 failed in sybase_check_servermsg"));
																						err = gda_sybase_make_error(sconn);
																						gda_server_connection_add_error(cnc,err);
																						g_free((gpointer)sconn->err.error_msg);
																						error_occured = TRUE; 			
																				} /* if ret != CS_SUCCEED */
																} /* if msgcur == msgcnt */
												} /* while msgcur < msgcnt */ 
								} /* if sconn->connection */	
				} /* if sconn */
		return error_occured;	
}
										
gboolean 
sybase_check_cmsg(GdaServerConnection *cnc)
{
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcnt = 0;
		CS_INT msgcur = 0;
		GdaError *err;
		gchar *message;
		gboolean error_occured = FALSE;
		CS_CLIENTMSG msg;
		sybase_connection *sconn;

		sconn = g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_SYBASE_HANDLE);

		if (sconn)
				{
						if (sconn->connection)
								{
										ret = cs_diag (sconn->context, 
																									CS_STATUS, 
																									CS_CLIENTMSG_TYPE, 
																									CS_UNUSED,
																									&msgcnt);												
										if (ret != CS_SUCCEED)
												{
														sconn->err.error_msg = g_new(gchar,201);
														g_snprintf(message,
																									200,
																									"%s",
																									_("cs_diag 1 failed in sybase_check_clientmsg"));
														err = gda_sybase_make_error(sconn);
														gda_server_connection_add_error(cnc,err);
														g_free((gpointer)sconn->err.error_msg);
														return TRUE;
												}
										if (!msgcnt)
												return FALSE;
										while (msgcur < msgcnt)
												{
														msgcur++;						
														ret = cs_diag (sconn->context, 
																													CS_GET, 
																													CS_CLIENTMSG_TYPE, 
																													msgcur,
																													&msg);	
														if (ret != CS_SUCCEED)
																{
																		sconn->err.error_msg = g_new(gchar,201);
																		g_snprintf(message,
																													200,
																													"%s",
																													_("cs_diag 2 failed in sybase_check_clientmsg"));
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);
																		error_occured = TRUE;
																}
														if (msg.severity != CS_SV_INFORM)
																{
																		sconn->err.error_msg = sprintf_clientmsg(_("Client Message: "),
																																																											&msg);
																		err = gda_sybase_make_error(sconn);
																		gda_server_connection_add_error(cnc,err);
																		g_free((gpointer)sconn->err.error_msg);																		
																		error_occured = TRUE;
																}
														else
																{
																		/* this will just print to a debug file for now */
																		sconn->err.error_msg = 
																				sprintf_clientmsg(_("Informational Client  Message: "),
																																						&msg);
																		sybase_debug_msg(sconn->err.error_msg);
																		g_free((gpointer)sconn->err.error_msg);	
																}
														/* check if we have any new messages to worry about */
														if (msgcur == msgcnt) 
																{
																		ret = cs_diag (sconn->context, 
																																	CS_STATUS,
																																	CS_CLIENTMSG_TYPE, 
																																	CS_UNUSED, 
																																	&msgcnt);
																		if (ret != CS_SUCCEED) 
																				{
																						sconn->err.error_msg = g_new(gchar,201);
																						g_snprintf(message,
																																	200,
																																	"%s",
																																	_("cs_diag 2 failed in sybase_check_clientmsg"));
																						err = gda_sybase_make_error(sconn);
																						gda_server_connection_add_error(cnc,err);
																						g_free((gpointer)sconn->err.error_msg);
																						error_occured = TRUE; 			
																				} /* if ret != CS_SUCCEED */
																} /* if msgcur == msgcnt */
												} /* while msgcur < msgcnt */ 
								} /* if sconn->connection */	
				} /* if sconn */
		return error_occured;	
}

gchar *
sprintf_clientmsg (const gchar * head, CS_CLIENTMSG * msg)
{
		GString *txt = NULL;
		gchar *junk;
		g_return_val_if_fail (msg != NULL, NULL);
		txt = g_string_new ("");
		g_return_val_if_fail (txt != NULL, NULL);
		g_string_sprintfa (txt,
																					"%s %s %s(%ld) %s(%ld) %s(%ld) %s(%ld) %s ",
																					_("SYB"),
																					head,
																					_("severity"),
																					(long) CS_SEVERITY (msg->severity),	
																					_("number"),
																					(long) CS_NUMBER (msg->msgnumber),																					
																					_("origin"),
																					(long) CS_ORIGIN (msg->msgnumber),
																					_("layer"),
																					(long) CS_LAYER (msg->msgnumber),
																					msg->msgstring);
																					    
		if (msg->osstringlen > 0) 
				{
						g_string_sprintfa (txt,
																									_("\n\tOperating system error number(%ld):\n\t%s"),
																									(long) msg->osnumber, msg->osstring);
				}		
		junk = g_strdup(txt->str);
		g_string_free(txt,TRUE);
		return (junk);
}

gchar *
sprintf_servermsg (const gchar * head, CS_SERVERMSG * msg)
{
		GString *txt = NULL;
		gchar *junk;

		g_return_val_if_fail (msg != NULL, NULL);
		txt = g_string_new ("");
		g_return_val_if_fail (txt != NULL, NULL);

		g_string_sprintfa (txt,
																					"%s\n\tnumber(%ld) severity(%ld) state(%ld) line(%ld)", 
																					head, 
																					(long) msg->msgnumber,
																					(long) msg->severity, 
																					(long) msg->state,
																					(long) msg->line);

		if (msg->svrnlen > 0)
				g_string_sprintfa (txt, "\n\tServer name: %s", msg->svrname);
		if (msg->proclen > 0)
				g_string_sprintfa (txt, "\n\tProcedure name: %s", msg->proc);
		g_string_sprintfa (txt, "\n\t%s", msg->text);		
		junk = g_strdup(txt->str);
		g_string_free(txt,TRUE);
		return (junk);
}

void sybase_debug_msg(gchar *message)
{
		FILE *foo;
		foo = fopen ("/home/projects/gda_sybase_debug.log","a+");
		fprintf(foo,"%s\n",message);
		fflush(foo);
		fclose(foo);


}






