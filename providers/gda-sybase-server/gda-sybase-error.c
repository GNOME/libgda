/* 
 * $Id$
 * 
 * GNOME DB Sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Holger Thon
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// $Log$
// Revision 1.4.4.2  2001/11/22 12:12:26  rodrigo
// 2001-11-22  Mike <wingert.3@postbox.acs.ohio-state.edu>
//
// 	* Make error handling work and fix some problems when valid queries
// 	didn't return rows
//
// Revision 1.4.4.1  2001/09/16 20:12:39  rodrigo
// 2001-09-16  Mike <wingert.3@postbox.acs.ohio-state.edu>
//
// 	* adapted to new GdaError API
//
// Revision 1.4  2001/07/18 23:05:42  vivien
// Ran indent -br -i8 on the files
//
// Revision 1.3  2001/04/07 08:49:31  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * objects renaming (Gda_* to Gda*) to conform to the GNOME
//      naming standards
//
// Revision 1.2  2000/12/14 13:10:01  bansz
// Typo fix
//
// Revision 1.1.1.1  2000/08/10 09:32:38  rodrigo
// First version of libgda separated from GNOME-DB
//
// Revision 1.2  2000/08/04 11:36:30  rodrigo
// Things I forgot in the last commit
//
// Revision 1.1  2000/08/04 10:20:37  rodrigo
// New Sybase provider
//
//

#include "gda-sybase.h"
#include "ctype.h"

//  callback handlers
/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_servermsg_callback (CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *); */
/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_clientmsg_callback (CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *); */
/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_csmsg_callback (CS_CONTEXT *, CS_CLIENTMSG *); */


/* GdaServerConnection *gda_sybase_callback_get_connection (const CS_CONTEXT *, */
/* 							 const CS_CONNECTION */
/* 							 *); */

void
gda_sybase_error_make (GdaError * error,
																							GdaServerRecordset * recset,
                       GdaServerConnection * cnc, 
                       gchar * where)
{
		sybase_Connection *scnc = NULL;
		gchar *err_msg = NULL;

		/*  #ifdef SYBASE_DEBUG */
		/*  	gda_log_message (_("gda_sybase_error_make(%p,%p,%p,%p)"), error, */
		/*  			 recset, cnc, where); */
		/*  #endif */

		g_return_if_fail (cnc != NULL);
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		g_return_if_fail (scnc != NULL);

		err_msg = g_strdup_printf(scnc->serr.eMessage);

  gda_error_set_description (error,
                             (g_strdup(err_msg)) ? err_msg :
                             "Unknown error");

		gda_log_error (_("error '%s' at %s"),
																	gda_error_get_description (error), where);

		
		gda_sybase_debug_msg("--------------------------------------------");		
		gda_sybase_debug_msg("in error make");		
		gda_sybase_debug_msg(scnc->serr.eMessage);
		gda_sybase_debug_msg("--------------------------------------------");		


		gda_error_set_number (error, 1);
		gda_error_set_source (error, "[gda-sybase]");
		gda_error_set_help_context (error, _("Not available"));
		gda_error_set_sqlstate (error, _("error"));
		gda_error_set_native (error,
																								gda_error_get_description
																								(error));

		gda_log_error (_("error '%s' at %s"),
			       gda_error_get_description (error), where);

		if (err_msg) 
				{
						g_free ((gpointer) err_msg);
				}

  if (scnc->serr.eMessage)
				{
						g_free((gpointer)scnc->serr.eMessage);
						scnc->serr.eMessage = NULL;
				}
}

gboolean
gda_sybase_messages_install (GdaServerConnection * cnc)
{

		/* this function set up open client to use inline error */ 
		/* handling */
    
  sybase_Connection *scnc = NULL;
  CS_RETCODE ret;

		g_return_val_if_fail (cnc != NULL, FALSE);
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		g_return_val_if_fail (scnc != NULL, FALSE);

		if (scnc->ctx) 
				{
						scnc->ret = cs_diag (scnc->ctx, 
																											CS_INIT, 
																											CS_UNUSED, 
																											CS_UNUSED,
																											NULL);
						if (scnc->ret == CS_SUCCEED)
								{
										scnc->cs_diag = TRUE;
          

#ifdef SYBASE_DEBUG
										gda_log_message (_("cs_diag() initialized"));
#endif

								}

						else 
								{
										gda_log_error (_("Could not initialize cs_diag()"));
								}
				}
		if (scnc->cnc)
				{
						scnc->ret = ct_diag (scnc->cnc, 
																											CS_INIT, 
																											CS_UNUSED, 
																											CS_UNUSED,
																											NULL);
						if (scnc->ret == CS_SUCCEED)
								{

										scnc->ct_diag = TRUE;


#ifdef SYBASE_DEBUG
										gda_log_message (_("ct_diag() initialized"));
#endif


								}
				}
		else 
				{
						gda_log_error (_("Could not initialize ct_diag()"));
				}

  if ((scnc->ct_diag)&&(scnc->cs_diag))
				return TRUE;
  else
				return FALSE;
}

/* void */
/* gda_sybase_messages_uninstall (GdaServerConnection * cnc) */
/* { */
// Seems the diag info is dropped with the context/connection structure
/* } */

gboolean
sybase_chkerr (GdaError * error,
               GdaServerRecordset * recset,
               GdaServerConnection * _cnc,
               GdaServerCommand * _cmd, 
               gchar * where)
{

		/* this function will get called after every open client function */
		/* call to check for error messages.  it will return TRUE if there */
		/* is a server/openclient message that we need to worry about */

		GdaServerConnection *cnc = NULL;
		GdaServerCommand *cmd = NULL;
		sybase_Connection *scnc = NULL;
		sybase_Command *scmd = NULL;
		sybase_Recordset *srecset = NULL;
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT messages = 0;
		gboolean oneWasBad = FALSE;
		gboolean msgRet = FALSE;
		gchar message[2000];

		
		if (_cnc) 
				{
						cnc = _cnc;
				}
		else 
				{
						gda_log_error (_("%s could not find server implementation"),
																					__PRETTY_FUNCTION__);
						return TRUE;
				}
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		if (scnc == NULL) 
				{
						gda_log_error (_("%s could not get cnc userdata"),
																					__PRETTY_FUNCTION__);
						return TRUE;
				}

		if (_cmd) 
				{
						cmd = _cmd;
				}
		if ((scnc->ct_diag == TRUE) && (scnc->cnc)) 
				{
						ret = ct_diag (scnc->cnc, 
																					CS_STATUS, 
																					CS_ALLMSG_TYPE,
																					CS_UNUSED, 
																					&messages);
						if (ret != CS_SUCCEED) 
								{
										gda_log_error (_("Could not get # of messages"));
										return TRUE;
								}			
						else if (messages > 0) 
								{
										msgRet = gda_sybase_messages_handle_clientmsg (error, 
																																																									recset, 
																																																									cnc, 
																																																									where);
										if (msgRet)
												{
														oneWasBad = TRUE;
														gda_sybase_debug_msg("handle clientmsg returned a bad thing");
												}

										msgRet = gda_sybase_messages_handle_servermsg (error, 
																																																									recset, 
																																																									cnc, 
																																																									where);
										if (msgRet)
												{
														oneWasBad = TRUE;
														gda_sybase_debug_msg("handle servermsg returned a bad thing");
												}
								}
				}
		if ((scnc->cs_diag == TRUE) && (scnc->ctx)) 
				{
						msgRet = gda_sybase_messages_handle_csmsg (error, 
																																																	recset,  
																																																	cnc, 
																																																	where);
						if (msgRet)
								{
										oneWasBad = TRUE;
										gda_sybase_debug_msg("handle cmsg  returned a bad thing");
								}
				}

		// Make sure err_type is set correct again
		/*  scnc->serr.err_type = SYBASE_ERR_NONE; */

		if (oneWasBad)
				{
						g_snprintf(message,200,"%s - something bad happened above me!?!",where);
						gda_sybase_debug_msg(message);
						return TRUE;
				}
		else
				{
						g_snprintf(message,200,"%s - aok!",where);
						gda_sybase_debug_msg(message);
						return FALSE;
				}
}

gboolean
gda_sybase_messages_handle_clientmsg (GdaError * error,
                                      GdaServerRecordset * recset,
                                      GdaServerConnection * cnc,
                                      gchar * where)
{

		/* this function will return TRUE if the client message is one we should */
		/*worry about. */

		sybase_Connection *scnc = NULL;
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcnt = 0;
		CS_INT msgcur = 0;
		GdaError *err;
		gchar *message;
		gboolean error_occured = FALSE;
		CS_CLIENTMSG msg;


		g_return_if_fail (cnc != NULL);
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		g_return_if_fail (scnc != NULL);
		g_return_if_fail (scnc->ct_diag != FALSE);

		ret = ct_diag (scnc->cnc, 
																	CS_STATUS, 
																	CS_CLIENTMSG_TYPE, 
																	CS_UNUSED,
																	&msgcnt);

		if (ret != CS_SUCCEED) 
				{
						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("ct_diag failed, Could not get # of client messages(1)"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						err = gda_error_new();
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return TRUE;
				}
		if (msgcnt == 0) 
				{
						return FALSE;
				}
		while (msgcur < msgcnt) 
				{
						msgcur++;
						
						ret = ct_diag (scnc->cnc, 
																					CS_GET, 
																					CS_CLIENTMSG_TYPE, 
																					msgcur,
																					&msg);
						if (ret != CS_SUCCEED) 
								{
										message = g_new0(gchar,200);
										g_snprintf(message,200,"%s%d",
																					_("Could not get client msg #"),msgcur);
										gda_log_error (message);
										err = gda_error_new();
										scnc->serr.eMessage=g_strdup(message);
										gda_server_error_make(err,recset,cnc,__PRETTY_FUNCTION__);
										g_free((gpointer) message);
										return TRUE;                    
								}

						/* only do something if the error is not informational does */
						/* gnome-db have a way of showing just informational messages? */
						if (msg.severity != CS_SV_INFORM)
								{
										gda_sybase_log_clientmsg (_("Client Message:"), 
																																				&msg); 
										message = g_sprintf_clientmsg (_("Client Message:"), 
																																									&msg); 
										scnc->serr.eMessage=g_strdup(message);                    
										err = gda_error_new();
										gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__); 
										g_free((gpointer)message);
										error_occured = TRUE;
 
								}
						/* for now, just dump it to the log */
						else
								{
										message = g_sprintf_clientmsg("!!!Client Message INFORMATIONAL!!!:",
																																								&msg);
										g_free((gpointer)message);
								}

						if (msgcur == msgcnt) 
								{
										ret = ct_diag (scnc->cnc, 
																									CS_STATUS,
																									CS_CLIENTMSG_TYPE, 
																									CS_UNUSED, 
																									&msgcnt);
										if (ret != CS_SUCCEED) 
												{
														message = g_new0(gchar,200);
														g_snprintf(message,200,"%s",
																									_("ct_diag failed, Could not get # of client messages(2)"));
														gda_log_error (message);
														scnc->serr.eMessage=g_strdup(message);
														gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
														g_free((gpointer) message);
														return TRUE;                            
												}
								}
				}
		ret = ct_diag (scnc->cnc, 
																	CS_CLEAR, 
																	CS_CLIENTMSG_TYPE,
																	CS_UNUSED, 
																	&msg);
		if (ret != CS_SUCCEED) 
				{
						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("Could not clear client messages"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return TRUE;                
				}
		return (error_occured);
}


gboolean
gda_sybase_messages_handle_servermsg (GdaError * error,
																																						GdaServerRecordset * recset,
																																						GdaServerConnection * cnc,
																																						gchar * where)
{
		GdaError *serverr = error;
		sybase_Connection *scnc = NULL;
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcur = 0;
		CS_INT msgcnt = 0;
		gchar *message;
		GdaError *err;
		gboolean error_occured = FALSE;
		CS_SERVERMSG msg;
		gchar junk[1000];
		

		g_return_if_fail (cnc != NULL);
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		g_return_if_fail (scnc != NULL);
		g_return_if_fail (scnc->ct_diag != FALSE);

		ret = ct_diag (scnc->cnc, 
																	CS_STATUS, 
																	CS_SERVERMSG_TYPE, 
																	CS_UNUSED,
																	&msgcnt);
		if (ret != CS_SUCCEED) 
				{
						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("Could not get # of server messages(1)"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						err = gda_error_new();
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return TRUE;
				}
		if (msgcnt == 0) 
				{
						return FALSE;
				}
		while (msgcur < msgcnt) 
				{
						msgcur++;            
						ret = ct_diag (scnc->cnc, 
																					CS_GET, 
																					CS_SERVERMSG_TYPE, 
																					msgcur,
																					&msg);
						if (ret != CS_SUCCEED) 
								{
										message = g_new0(gchar,200);
										g_snprintf(message,200,"%s%d",
																					_("Could not get server msg #"),msgcur);
										gda_log_error (message);
										scnc->serr.eMessage=g_strdup(message);
										err = gda_error_new();
										gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
										g_free((gpointer) message);
										return TRUE;
								}

						/* for now, just dump informational messages to the log */
						/* severity of 10 is informational, I think */


/*  						g_snprintf(junk,200,"severity = %d %ld",msg.severity,msg.severity); */
/*  						gda_sybase_debug_msg(junk); */


						if ((msg.severity == CS_SV_INFORM) || (msg.severity == 10))
								{
										message = g_sprintf_servermsg("!!!Server Message INFORMATIONAL!!!:",
																																								&msg);
										g_free((gpointer)message);
								}
						/* show an error if it is not informational */
						else
								{

										gda_sybase_log_servermsg ("Server Message:",
																																				&msg);

										message = g_sprintf_servermsg("Server Message:",
																																								&msg);
										scnc->serr.eMessage = g_strdup(message);
										err = gda_error_new();
										gda_server_error_make(err, 
																																recset, 
																																cnc, 
																																__PRETTY_FUNCTION__);        
										g_free((gpointer)message);
										error_occured = TRUE;
								}        


						/* Check if current message has any extra error data */
						if (msg.status == CS_HASEED) 
								{
								}

						/* Check if any new messages have come in */
						if (msgcur == msgcnt) 
								{
										ret = ct_diag (scnc->cnc, 
																									CS_STATUS,
																									CS_SERVERMSG_TYPE, 
																									CS_UNUSED, 
																									&msgcnt);
										if (ret != CS_SUCCEED) 
												{
														message = g_new0(gchar,200);
														g_snprintf(message,200,"%s",
																									_("Could not get # of server messages"));
														gda_log_error (message);
														scnc->serr.eMessage=g_strdup(message);
														err = gda_error_new();
														gda_server_error_make(err, 
																																				recset, 
																																				cnc, 
																																				__PRETTY_FUNCTION__);
														g_free((gpointer) message);
														return TRUE;
												}               
								}
				}
		ret = ct_diag (scnc->cnc, 
																	CS_CLEAR, 
																	CS_SERVERMSG_TYPE,
																	CS_UNUSED, 
																	&msg);
		if (ret != CS_SUCCEED) 
				{
						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("Could not clear server messages"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						err = gda_error_new();
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return TRUE;
				}
		return(error_occured);
}

gboolean
gda_sybase_messages_handle_csmsg (GdaError * error,
																																		GdaServerRecordset * recset,
																																		GdaServerConnection * cnc, 
																																		gchar * where)
{
		sybase_Connection *scnc = NULL;
		CS_RETCODE ret = CS_SUCCEED;
		CS_INT msgcnt = 0;
		CS_INT msgcur = 0;
		gchar *message;
		GdaError *err;
		gboolean error_occured = FALSE;
		CS_CLIENTMSG msg;

		g_return_if_fail (cnc != NULL);
		scnc = (sybase_Connection *)
				gda_server_connection_get_user_data (cnc);
		g_return_if_fail (scnc != NULL);

		ret = cs_diag (scnc->ctx, 
																	CS_STATUS, 
																	CS_CLIENTMSG_TYPE, 
																	CS_UNUSED,
																	&msgcnt);

		if (ret != CS_SUCCEED) 
				{

						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("Could not get # of cslib messages"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						err = gda_error_new();
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return (TRUE);            
				}
		if (msgcnt == 0) 
				{
						return (FALSE);
				}


#ifdef SYBASE_DEBUG
		if (msgcnt > 1) {
				gda_log_message (_("Fetching %d cslib messages"), msgcnt);
		}
		else {
				gda_log_message (_("Fetching %d cslib message"), msgcnt);
		}
#endif



		while (msgcur < msgcnt) 
				{
						msgcur++;
						ret = cs_diag (scnc->ctx, 
																					CS_GET, 
																					CS_CLIENTMSG_TYPE, 
																					msgcur,
																					&msg);

						if (ret != CS_SUCCEED) 
								{
										message = g_new0(gchar,200);
										g_snprintf(message,200,"%s%d",
																					_("Could not get cslib msg #"),msgcur);
										gda_log_error (message);
										scnc->serr.eMessage=g_strdup(message);
										err = gda_error_new();
										gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
										g_free((gpointer) message);
										return (TRUE);                           
								}

						gda_sybase_log_clientmsg ("CS-Library message:",
																																&msg);
        
						/* only do something if the error is not informational does */
						/* gnome-db have a way of showing just informational messages? */
						if (msg.severity != CS_SV_INFORM)
								{
										gda_sybase_log_clientmsg (_("Client Message:"), 
																																				&msg); 
										message = g_sprintf_clientmsg (_("Client Message:"), 
																																									&msg); 
										scnc->serr.eMessage=g_strdup(message);                    
										err = gda_error_new();
										gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__); 
										g_free((gpointer)message);
										error_occured = TRUE;                
								}
        
						/* Check if any new messages have come in */
						if (msgcur == msgcnt) 
								{
										ret = cs_diag (scnc->ctx, 
																									CS_STATUS,
																									CS_CLIENTMSG_TYPE, 
																									CS_UNUSED, 
																									&msgcnt);
										if (ret != CS_SUCCEED) 
												{
														message = g_new0(gchar,200);
														g_snprintf(message,200,"%s",
																									_("Could not get # of cslib messages(2)"));
														gda_log_error (message);
														scnc->serr.eMessage=g_strdup(message);
														err = gda_error_new();
														gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
														g_free((gpointer) message);
														return TRUE;                    
												}
								}
				}
		ret = cs_diag (scnc->ctx, 
																	CS_CLEAR, 
																	CS_CLIENTMSG_TYPE,
																	CS_UNUSED, &msg);
		if (ret != CS_SUCCEED) 
				{
						message = g_new0(gchar,200);
						g_snprintf(message,200,"%s",
																	_("Could not clear cslib messages"));
						gda_log_error (message);
						scnc->serr.eMessage=g_strdup(message);
						err = gda_error_new();
						gda_server_error_make(err, recset, cnc, __PRETTY_FUNCTION__);
						g_free((gpointer) message);
						return TRUE;                    
				}
		return (error_occured);
}

gchar *
g_sprintf_clientmsg (const gchar * head, CS_CLIENTMSG * msg)
{
		GString *txt = NULL;
		gchar *junk;

		g_return_val_if_fail (msg != NULL, NULL);
		txt = g_string_new ("");
		g_return_val_if_fail (txt != NULL, NULL);

/*  		g_string_sprintfa (txt, */
/*  																					_("SYB: %s\n\tseverity(%ld) number(%ld) origin(%ld) "), */
/*  																					_("layer(%ld)\n\t%s"), head, */
/*  																					(long) CS_SEVERITY (msg->severity), */
/*  																					(long) CS_NUMBER (msg->msgnumber), */
/*  																					(long) CS_ORIGIN (msg->msgnumber), */
/*  																					(long) CS_LAYER (msg->msgnumber), msg->msgstring); */


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

/*  		gda_sybase_debug_msg(junk); */

		g_string_free(txt,TRUE);
		return (junk);
}

gchar *
g_sprintf_servermsg (const gchar * head, CS_SERVERMSG * msg)
{
		GString *txt = NULL;
		gchar *junk;

		g_return_val_if_fail (msg != NULL, NULL);
		txt = g_string_new ("");
		g_return_val_if_fail (txt != NULL, NULL);

		g_string_sprintfa (txt,
																					"SYB SWERVER MSG:%s\n\tnumber(%ld) severity(%ld) state(%ld) line(%ld)", 
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

/*  		gda_sybase_debug_msg(junk); */

		g_string_free(txt,TRUE);
		
		
		
		return (junk);
}


void
gda_sybase_log_clientmsg (const gchar * head, CS_CLIENTMSG * msg)
{
		gchar *txt = g_sprintf_clientmsg (head, msg);
		g_return_if_fail (txt != NULL);
		gda_log_message (txt);
		g_free ((gpointer) txt);
}

void
gda_sybase_log_servermsg (const gchar * head, CS_SERVERMSG * msg)
{
		gchar *txt = g_sprintf_servermsg (head, msg);
		g_return_if_fail (txt != NULL);
		gda_log_message (txt);
		g_free ((gpointer) txt);
}

void
gda_sybase_debug_msg(gchar *msg)
{
		/* this will be a stub when I don't need it */
/*  		FILE *debug; */
/*  		debug = fopen("/home/projects/gda_sybase_debug.log","a+"); */
/*  		fprintf(debug,"%s\n",msg); */
/*  		fflush(debug); */
/*  		fclose(debug); */
}



/*
	* Private functions
	*
	*
	*/

/* gint */
/* gda_sybase_install_error_handlers (GdaServerConnection * cnc) */
/* { */
/* 		sybase_Connection *scnc = NULL; */

/* 		g_return_val_if_fail (cnc != NULL, -1); */
/* 		scnc = (sybase_Connection *) */
/* 				gda_server_connection_get_user_data (cnc); */

/* 		if ((scnc->ret = cs_config (scnc->ctx, CS_SET, CS_MESSAGE_CB, */
/* 																														(CS_VOID *) gda_sybase_csmsg_callback, */
/* 																														CS_UNUSED, NULL)) != CS_SUCCEED) { */
/* 				gda_sybase_cleanup (scnc, scnc->ret, */
/* 																								"Could not install cslib callback\n"); */
/* 				return -1; */
/* 		} */
/* 		if ((scnc->ret = */
/* 							ct_callback (scnc->ctx, NULL, CS_SET, CS_CLIENTMSG_CB, */
/* 																				(CS_VOID *) gda_sybase_clientmsg_callback)) */
/* 						!= CS_SUCCEED) { */
/* 				gda_sybase_cleanup (scnc, scnc->ret, "Could not install ct " */
/* 																								"callback for client messages\n"); */
/* 				return -1; */
/* 		} */
/* 		if ((scnc->ret = */
/* 							ct_callback (scnc->ctx, NULL, CS_SET, CS_SERVERMSG_CB, */
/* 																				(CS_VOID *) gda_sybase_servermsg_callback)) */
/* 						!= CS_SUCCEED) { */
/* 				gda_sybase_cleanup (scnc, scnc->ret, "Could not install ct " */
/* 																								"callback for server messages\n"); */
/* 				return -1; */
/* 		} */

/* 		return 0; */
/* } */

/*
	* Sybase callback handlers
	* These do nothing so far
	*/

/* GdaServerConnection * */
/* gda_sybase_callback_get_connection (const CS_CONTEXT * ctx, */
/* 																																				const CS_CONNECTION * cnc) */
/* { */
//    GList *server_list = gda_server_list();

//    g_return_val_if_fail(server_list != NULL, NULL);

/* 		return NULL; */
/* } */

/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_servermsg_callback (CS_CONTEXT * ctx, */
/* 																															CS_CONNECTION * cnc, CS_SERVERMSG * msg) */
/* { */
/* 		gda_sybase_log_servermsg (_("Server message:"), msg); */
/* 		return (CS_SUCCEED); */
/* } */

/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_clientmsg_callback (CS_CONTEXT * ctx, */
/* 																															CS_CONNECTION * cnc, CS_CLIENTMSG * msg) */
/* { */
/* 		gda_sybase_log_clientmsg (_("Client message:"), msg); */
/* 		return (CS_SUCCEED); */
/* } */

/* static CS_RETCODE CS_PUBLIC */
/* gda_sybase_csmsg_callback (CS_CONTEXT * ctx, CS_CLIENTMSG * msg) */
/* { */
/* 		gda_sybase_log_clientmsg (_("CS-Library error:"), msg); */
/* 		return (CS_SUCCEED); */
/* } */
