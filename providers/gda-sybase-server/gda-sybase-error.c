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
// Revision 1.3  2001/04/07 08:49:31  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
// 	* objects renaming (Gda_* to Gda*) to conform to the GNOME
// 	naming standards
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
static CS_RETCODE CS_PUBLIC
gda_sybase_servermsg_callback(CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *);
static CS_RETCODE CS_PUBLIC
gda_sybase_clientmsg_callback(CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *);
static CS_RETCODE CS_PUBLIC
gda_sybase_csmsg_callback(CS_CONTEXT *, CS_CLIENTMSG *);


GdaServerConnection *
gda_sybase_callback_get_connection(const CS_CONTEXT *,
				   const CS_CONNECTION *);

void
gda_sybase_error_make(GdaServerError *error,
                      GdaServerRecordset *recset,
                      GdaServerConnection *cnc,
                      gchar *where)
{
  sybase_Connection* scnc = NULL;
  gchar					 *err_msg = NULL;
  
#ifdef SYBASE_DEBUG
  gda_log_message(_("gda_sybase_error_make(%p,%p,%p,%p)"), error, recset, cnc,
		  where);
#endif
  g_return_if_fail(cnc != NULL);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(scnc != NULL);
  
  switch (scnc->serr.err_type) {
  case SYBASE_ERR_NONE:
    err_msg = g_strdup_printf(_("No error flag ist set, though %s "
				"is called. This may be a bug."), __PRETTY_FUNCTION__);
    gda_server_error_set_description(error,
				     (err_msg) ? err_msg : "Unknown error");
  case SYBASE_ERR_SERVER:
    err_msg = g_sprintf_servermsg("Server Message:",
				  &scnc->serr.server_msg);
    gda_server_error_set_description(error,
				     (err_msg) ? err_msg : "Server error");
    break;							
  case SYBASE_ERR_CLIENT:
    err_msg = g_sprintf_clientmsg("Client Message:",
				  &scnc->serr.client_msg);
    gda_server_error_set_description(error, 
				     (err_msg) ? err_msg : "Client error");
    break;
  case SYBASE_ERR_CSLIB:
    err_msg = g_sprintf_clientmsg("CS-Lib Message:",
				  &scnc->serr.cslib_msg);
    gda_server_error_set_description(error, 
				     (err_msg) ? err_msg : "Cslib error");
    break;
  case SYBASE_ERR_USERDEF:
    gda_server_error_set_description(error,
				     (scnc->serr.udeferr_msg) ? scnc->serr.udeferr_msg
				     : "gda-sybase error");
    if (scnc->serr.udeferr_msg) {
      g_free((gpointer) scnc->serr.udeferr_msg);
    }
    break;
  default:
    gda_server_error_set_description(error, "Unknown error");
    break;
  }
  
  gda_log_error(_("error '%s' at %s"),
		gda_server_error_get_description(error), where);
  
  if (err_msg) {
    g_free((gpointer) err_msg);
  }
  
  gda_server_error_set_number(error, 1);
  gda_server_error_set_source(error, "[gda-sybase]");
  gda_server_error_set_help_file(error, _("Not available"));
  gda_server_error_set_help_context(error, _("Not available"));
  gda_server_error_set_sqlstate(error, _("error"));
  gda_server_error_set_native(error,
			      gda_server_error_get_description(error));
}

gboolean
gda_sybase_messages_install(GdaServerConnection *cnc)
{
  sybase_Connection *scnc = NULL;
  
  g_return_val_if_fail(cnc != NULL, FALSE);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail(scnc != NULL, FALSE);
  
  if ((scnc->ctx) && ((scnc->ret = cs_diag(scnc->ctx, CS_INIT, CS_UNUSED,
					   CS_UNUSED, NULL)) == CS_SUCCEED)) {
    scnc->cs_diag = TRUE;
#ifdef SYBASE_DEBUG
    gda_log_message(_("cs_diag() initialized"));
#endif
  } else {
    gda_log_error(_("Could not initialize cs_diag()"));
  }
  
  if ((scnc->cnc) && ((scnc->ret = ct_diag(scnc->cnc, CS_INIT, CS_UNUSED,
					   CS_UNUSED, NULL)) == CS_SUCCEED)) {
    scnc->ct_diag = TRUE;
#ifdef SYBASE_DEBUG
    gda_log_message(_("ct_diag() initialized"));
#endif
  } else {
    gda_log_error(_("Could not initialize ct_diag()"));
  }
  
  // Succeeding means both succeed
  return ((scnc->cs_diag == TRUE) && (scnc->ct_diag == TRUE)) ? TRUE : FALSE;
}

void
gda_sybase_messages_uninstall(GdaServerConnection *cnc)
{
  // Seems the diag info is dropped with the context/connection structure
}

void
sybase_chkerr(GdaServerError      *error,
              GdaServerRecordset  *recset,
	      GdaServerConnection *_cnc,
	      GdaServerCommand    *_cmd,
	      gchar                *where)
{
  GdaServerConnection *cnc = NULL;
  GdaServerCommand    *cmd = NULL;
  sybase_Connection    *scnc = NULL;
  sybase_Command       *scmd = NULL;
  sybase_Recordset     *srecset = NULL;
  CS_RETCODE           ret = CS_SUCCEED;
  CS_INT               messages = 0;
  
  if (_cnc) {
    cnc = _cnc;
  } else if (recset) {
    cnc = gda_server_recordset_get_connection(recset);
    srecset = gda_server_recordset_get_user_data(recset);
  } else {
    gda_log_error(_("%s could not find server implementation"),
		  __PRETTY_FUNCTION__);
    return;
  }
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  if (scnc == NULL) {
    gda_log_error(_("%s could not get cnc userdata"),
		  __PRETTY_FUNCTION__);
    return;
  }
  if (_cmd) {
    cmd = _cmd;
  }
  
  if ((scnc->ct_diag == TRUE) && (scnc->cnc)) {
    ret = ct_diag(scnc->cnc, CS_STATUS, CS_ALLMSG_TYPE, CS_UNUSED,
		  &messages);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("Could not get # of messages"));
      return;
    } else if (messages > 0) {
#ifdef SYBASE_DEBUG
      if (messages > 1) {
	gda_log_message(_("%d ctlib messages to fetch"), messages);
      } else {
	gda_log_message(_("%d ctlib message to fetch"), messages);
      }
#endif
      gda_sybase_messages_handle_clientmsg(error, recset, cnc, where);
      gda_sybase_messages_handle_servermsg(error, recset, cnc, where);
    }
  }
  if ((scnc->cs_diag == TRUE) && (scnc->ctx)) {
    gda_sybase_messages_handle_csmsg(error, recset, cnc, where);
  }
  
  // Make sure err_type is set correct again
  scnc->serr.err_type = SYBASE_ERR_NONE;
}

CS_RETCODE
sybase_exec_chk(CS_RETCODE *retvar,
                CS_RETCODE retval,
                GdaServerError *err,
                GdaServerRecordset *rec,
                GdaServerConnection *cnc,
                GdaServerCommand *cmd,
                gchar *where)
{
  sybase_chkerr(err, rec, cnc, cmd, where);
  
  return ((retvar) ? (*retvar = retval) : retval);
}

void
gda_sybase_messages_handle(GdaServerError      *error,
                           GdaServerRecordset  *recset,
                           GdaServerConnection *cnc,
			   gchar                *where)
{
  if (error == NULL) {
    gda_log_error(_("error is nullpointer in gda_sybase_messages_handle"));
    return;
  }
  sybase_chkerr(error, recset, cnc, NULL, where);
}

void
gda_sybase_messages_handle_clientmsg(GdaServerError      *error,
                                     GdaServerRecordset  *recset,
                                     GdaServerConnection *cnc,
                                     gchar                *where)
{
  sybase_Connection *scnc = NULL;
  CS_RETCODE        ret = CS_SUCCEED;
  CS_INT            msgcnt = 0;
  CS_INT            msgcur = 0;
  
  g_return_if_fail(cnc != NULL);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(scnc != NULL);
  g_return_if_fail(scnc->ct_diag != FALSE);
  
  ret = ct_diag(scnc->cnc, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED,
		&msgcnt);
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not get # of server messages"));
    return;
  }
  if (msgcnt == 0) {
    return;
  }
#ifdef SYBASE_DEBUG
  if (msgcnt > 1) {
    gda_log_message(_("Fetching %d client messages"), msgcnt);
  } else {
    gda_log_message(_("Fetching %d client message"), msgcnt);
  }
#endif
  while (msgcur < msgcnt) {
    msgcur++;
    
    ret = ct_diag(scnc->cnc, CS_GET, CS_CLIENTMSG_TYPE, msgcur, 
		  &scnc->serr.client_msg);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("Could not get client msg # %d"), msgcur);
      return;
    }
    
    scnc->serr.err_type = SYBASE_ERR_CLIENT;
    //		if (!error) {
    gda_sybase_log_clientmsg("Client Message:", &scnc->serr.client_msg);
    //		} else {
    //			gda_server_error_make(error, recset, cnc, where);
    //		}
    
    // Check if any new message came in
    if (msgcur == msgcnt) {
      ret = ct_diag(scnc->cnc, CS_STATUS, CS_CLIENTMSG_TYPE,
		    CS_UNUSED, &msgcnt);
      if (ret != CS_SUCCEED) {
	gda_log_error(_("Could not get # of client messages"));
	return;
      }
    }
  }
  ret = ct_diag(scnc->cnc, CS_CLEAR, CS_CLIENTMSG_TYPE,
		CS_UNUSED, &scnc->serr.client_msg);
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not clear client messages"));
  }
}

void
gda_sybase_messages_handle_servermsg(GdaServerError      *error,
                                     GdaServerRecordset  *recset,
                                     GdaServerConnection *cnc,
                                     gchar                *where)
{
  GdaServerError   *serverr = error;
  sybase_Connection *scnc = NULL;
  CS_RETCODE         ret = CS_SUCCEED;
  CS_INT             msgcur = 0;
  CS_INT             msgcnt = 0;
  
  g_return_if_fail(cnc != NULL);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(scnc != NULL);
  g_return_if_fail(scnc->ct_diag != FALSE);
  
  ret = ct_diag(scnc->cnc, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED,
		&msgcnt);
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not get # of server messages"));
    return;
  }
  if (msgcnt == 0) {
    return;
  }
#ifdef SYBASE_DEBUG
  if (msgcnt > 1) {
    gda_log_message(_("Fetching %d server messages"), msgcnt);
  } else {
    gda_log_message(_("Fetching %d server message"), msgcnt);
  }
#endif
  while (msgcur < msgcnt) {
    msgcur++;
    
    ret = ct_diag(scnc->cnc, CS_GET, CS_SERVERMSG_TYPE, msgcur, 
		  &scnc->serr.server_msg);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("Could not get server msg # %d"), msgcur);
      return;
    }
    
    scnc->serr.err_type = SYBASE_ERR_SERVER;
    //		if (!error) {
    gda_sybase_log_servermsg("Server Message:", &scnc->serr.server_msg);
    //		} else {
    //		if (!serverr) {
    //		serverr = gda_server_error_new();
    //		}
    //		if (serverr) {
    //			gda_server_error_make(serverr, recset, cnc, where);
    //		}
    
    // Check if current message has any extra error data
    if (scnc->serr.server_msg.status & CS_HASEED) {
    }
    
    // Check if any new message came in
    if (msgcur == msgcnt) {
      ret = ct_diag(scnc->cnc, CS_STATUS, CS_SERVERMSG_TYPE,
		    CS_UNUSED, &msgcnt);
      if (ret != CS_SUCCEED) {
	gda_log_error(_("Could not get # of server messages"));
	return;
      }
    }
  }
  ret = ct_diag(scnc->cnc, CS_CLEAR, CS_SERVERMSG_TYPE,
		CS_UNUSED, &scnc->serr.server_msg);
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not clear server messages"));
  }
}

void
gda_sybase_messages_handle_csmsg(GdaServerError      *error,
                                 GdaServerRecordset  *recset,
                                 GdaServerConnection *cnc,
                                 gchar                *where)
{
  sybase_Connection *scnc = NULL;
  CS_RETCODE        ret = CS_SUCCEED;
  CS_INT            msgcnt = 0;
  CS_INT            msgcur = 0;
  
  g_return_if_fail(cnc != NULL);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_if_fail(scnc != NULL);
  ret = cs_diag(scnc->ctx, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED,
		&msgcnt);
  
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not get # of cslib messages"));
    return;
  }
  if (msgcnt == 0) {
    return;
  }
#ifdef SYBASE_DEBUG
  if (msgcnt > 1) {
    gda_log_message(_("Fetching %d cslib messages"), msgcnt);
  } else {
    gda_log_message(_("Fetching %d cslib message"), msgcnt);
  } 
#endif
	
  while (msgcur < msgcnt) {
    msgcur++;

    ret = cs_diag(scnc->ctx, CS_GET, CS_CLIENTMSG_TYPE, msgcur, 
		  &scnc->serr.cslib_msg);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("Could not get cslib msg # %d"), msgcur);
      return;
    }
    
    scnc->serr.err_type = SYBASE_ERR_CSLIB;
    //		if (!error) {
    gda_sybase_log_clientmsg("CS-Library message:", &scnc->serr.cslib_msg);
    //		} else {
    //			gda_server_error_make(error, recset, cnc, where);
    //		}
    
    // Check if any new message came in
    if (msgcur == msgcnt) {
      ret = cs_diag(scnc->ctx, CS_STATUS, CS_CLIENTMSG_TYPE,
		    CS_UNUSED, &msgcnt);
      if (ret != CS_SUCCEED) {
	gda_log_error(_("Could not get # of cslib messages"));
	return;
      }
    }
  }
  ret = cs_diag(scnc->ctx, CS_CLEAR, CS_CLIENTMSG_TYPE,
		CS_UNUSED, &scnc->serr.cslib_msg);
  if (ret != CS_SUCCEED) {
    gda_log_error(_("Could not clear cslib messages"));
  }
}

gchar *
g_sprintf_clientmsg(const gchar *head, CS_CLIENTMSG *msg)
{
  GString *txt = NULL;
  
  g_return_val_if_fail(msg != NULL, NULL);
  txt = g_string_new("");
  g_return_val_if_fail(txt != NULL, NULL);
  
  g_string_sprintfa(txt, "SYB: %s\n\tseverity(%ld) number(%ld) origin(%ld) "
		    "layer(%ld)\n\t%s",
		    head,
		    (long)CS_SEVERITY(msg->severity),
		    (long)CS_NUMBER(msg->msgnumber),
		    (long)CS_ORIGIN(msg->msgnumber),
		    (long)CS_LAYER(msg->msgnumber),
		    msg->msgstring);
  
  if (msg->osstringlen > 0) {
    g_string_sprintfa(txt, "\n\tOperating system error number(%ld):\n\t%s",
		      (long)msg->osnumber, msg->osstring);
  }
  
  return g_strdup(txt->str);
}

gchar *
g_sprintf_servermsg(const gchar *head, CS_SERVERMSG *msg)
{
  GString *txt = NULL;
  
  g_return_val_if_fail(msg != NULL, NULL);
  txt = g_string_new("");
  g_return_val_if_fail(txt != NULL, NULL);
  
  g_string_sprintfa(txt, "SYB: %s\n\tnumber(%ld) severity(%ld) state(%ld) "
		    "line(%ld)",
		    head,
		    (long)msg->msgnumber, (long)msg->severity,
		    (long)msg->state, (long)msg->line);
  if (msg->svrnlen > 0)
    g_string_sprintfa(txt, "\n\tServer name: %s", msg->svrname);
  if (msg->proclen > 0)
    g_string_sprintfa(txt, "\n\tProcedure name: %s", msg->proc);
  
  g_string_sprintfa(txt, "\n\t%s", msg->text);
  
  return (g_strdup(txt->str));
}

/*
 * Private functions
 *
 *
 */

gint
gda_sybase_install_error_handlers(GdaServerConnection *cnc)
{
  sybase_Connection    *scnc = NULL;
  
  g_return_val_if_fail(cnc != NULL, -1);
  scnc = (sybase_Connection *) gda_server_connection_get_user_data(cnc);
  
  if ((scnc->ret = cs_config(scnc->ctx, CS_SET, CS_MESSAGE_CB,
			     (CS_VOID *)gda_sybase_csmsg_callback,
			     CS_UNUSED, NULL)) != CS_SUCCEED) {
    gda_sybase_cleanup(scnc, scnc->ret,
		       "Could not install cslib callback\n");
    return -1;
  }
  if ((scnc->ret = ct_callback(scnc->ctx, NULL, CS_SET, CS_CLIENTMSG_CB,
			       (CS_VOID *)gda_sybase_clientmsg_callback))
      != CS_SUCCEED) {
    gda_sybase_cleanup(scnc, scnc->ret, "Could not install ct "
		       "callback for client messages\n");
    return -1;
  }
  if ((scnc->ret = ct_callback(scnc->ctx, NULL, CS_SET, CS_SERVERMSG_CB,
			       (CS_VOID *)gda_sybase_servermsg_callback))
      != CS_SUCCEED) {
    gda_sybase_cleanup(scnc, scnc->ret, "Could not install ct "
		       "callback for server messages\n");
    return -1;
  }
  
  return 0;
}

void
gda_sybase_log_clientmsg(const gchar *head, CS_CLIENTMSG *msg)
{
  gchar *txt = g_sprintf_clientmsg(head, msg);
  g_return_if_fail(txt != NULL);
  gda_log_message(txt);
  g_free((gpointer) txt);
}

void
gda_sybase_log_servermsg(const gchar *head, CS_SERVERMSG *msg)
{
  gchar *txt = g_sprintf_servermsg(head, msg);
  g_return_if_fail(txt != NULL);
  gda_log_message(txt);
  g_free((gpointer) txt);
}

/*
 * Sybase callback handlers
 * These do nothing so far
 */

GdaServerConnection *
gda_sybase_callback_get_connection(const CS_CONTEXT *ctx,
                                   const CS_CONNECTION *cnc)
{
  //	GList *server_list = gda_server_list();
  
  //	g_return_val_if_fail(server_list != NULL, NULL);
  
  return NULL;
}

static CS_RETCODE CS_PUBLIC
gda_sybase_servermsg_callback(CS_CONTEXT    *ctx,
                              CS_CONNECTION *cnc,
			      CS_SERVERMSG  *msg)
{
  gda_sybase_log_servermsg(_("Server message:"), msg);	
  return (CS_SUCCEED);
}

static CS_RETCODE CS_PUBLIC
gda_sybase_clientmsg_callback(CS_CONTEXT    *ctx,
                              CS_CONNECTION *cnc,
			      CS_CLIENTMSG  *msg)
{
  gda_sybase_log_clientmsg(_("Client message:"), msg);
  return (CS_SUCCEED);
}

static CS_RETCODE CS_PUBLIC
gda_sybase_csmsg_callback(CS_CONTEXT *ctx, CS_CLIENTMSG *msg)
{
  gda_sybase_log_clientmsg(_("CS-Library error:"), msg);
  return (CS_SUCCEED);
}
