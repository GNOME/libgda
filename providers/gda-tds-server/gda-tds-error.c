/* 
 * $Id$
 * 
 * GNOME DB Tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Rodrigo Moya
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
// Revision 1.4  2001/07/18 23:05:43  vivien
// Ran indent -br -i8 on the files
//
// Revision 1.3  2001/04/07 08:49:34  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * objects renaming (Gda_* to Gda*) to conform to the GNOME
//      naming standards
//
// Revision 1.2  2000/11/21 19:57:14  holger
// 2000-11-21 Holger Thon <holger@gidayu.max.uni-duisburg.de>
//
//      - Fixed missing parameter in gda-sybase-server
//      - Removed ct_diag/cs_diag stuff completly from tds server
//
// Revision 1.1  2000/11/04 23:42:17  holger
// 2000-11-05  Holger Thon <gnome-dms@users.sourceforge.net>
//
//   * Added gda-tds-server
//

#include "gda-tds.h"
#include "ctype.h"

//  callback handlers
static CS_RETCODE CS_PUBLIC
gda_tds_servermsg_callback (CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *);
static CS_RETCODE CS_PUBLIC
gda_tds_clientmsg_callback (CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *);
static CS_RETCODE CS_PUBLIC
gda_tds_csmsg_callback (CS_CONTEXT *, CS_CLIENTMSG *);


GdaServerConnection *gda_tds_callback_get_connection (const CS_CONTEXT *,
						      const CS_CONNECTION *);

void
gda_tds_error_make (GdaServerError * error,
		    GdaServerRecordset * recset,
		    GdaServerConnection * cnc, gchar * where)
{
	tds_Connection *scnc = NULL;
	gchar *err_msg = NULL;

#ifdef TDS_DEBUG
	gda_log_message (_("gda_tds_error_make(%p,%p,%p,%p)"), error, recset,
			 cnc, where);
#endif
	g_return_if_fail (cnc != NULL);
	scnc = (tds_Connection *) gda_server_connection_get_user_data (cnc);
	g_return_if_fail (scnc != NULL);

	switch (scnc->serr.err_type) {
	case TDS_ERR_NONE:
		err_msg =
			g_strdup_printf (_
					 ("No error flag ist set, though %s "
					  "is called. This may be a bug."),
					 __PRETTY_FUNCTION__);
		gda_server_error_set_description (error,
						  (err_msg) ? err_msg :
						  "Unknown error");
	case TDS_ERR_SERVER:
		err_msg = g_sprintf_servermsg ("Server Message:",
					       &scnc->serr.server_msg);
		gda_server_error_set_description (error,
						  (err_msg) ? err_msg :
						  "Server error");
		break;
	case TDS_ERR_CLIENT:
		err_msg = g_sprintf_clientmsg ("Client Message:",
					       &scnc->serr.client_msg);
		gda_server_error_set_description (error,
						  (err_msg) ? err_msg :
						  "Client error");
		break;
	case TDS_ERR_CSLIB:
		err_msg = g_sprintf_clientmsg ("CS-Lib Message:",
					       &scnc->serr.cslib_msg);
		gda_server_error_set_description (error,
						  (err_msg) ? err_msg :
						  "Cslib error");
		break;
	case TDS_ERR_USERDEF:
		gda_server_error_set_description (error,
						  (scnc->serr.
						   udeferr_msg) ? scnc->serr.
						  udeferr_msg :
						  "gda-tds error");
		if (scnc->serr.udeferr_msg) {
			g_free ((gpointer) scnc->serr.udeferr_msg);
		}
		break;
	default:
		gda_server_error_set_description (error, "Unknown error");
		break;
	}

	gda_log_error (_("error '%s' at %s"),
		       gda_server_error_get_description (error), where);

	if (err_msg) {
		g_free ((gpointer) err_msg);
	}

	gda_server_error_set_number (error, 1);
	gda_server_error_set_source (error, "[gda-tds]");
	gda_server_error_set_help_file (error, _("Not available"));
	gda_server_error_set_help_context (error, _("Not available"));
	gda_server_error_set_sqlstate (error, _("error"));
	gda_server_error_set_native (error,
				     gda_server_error_get_description
				     (error));
}

gchar *
g_sprintf_clientmsg (const gchar * head, CS_CLIENTMSG * msg)
{
	GString *txt = NULL;

	g_return_val_if_fail (msg != NULL, NULL);
	txt = g_string_new ("");
	g_return_val_if_fail (txt != NULL, NULL);

	g_string_sprintfa (txt,
			   "TDS: %s\n\tseverity(%ld) number(%ld) origin(%ld) "
			   "layer(%ld)\n\t%s", head,
			   (long) CS_SEVERITY (msg->severity),
			   (long) CS_NUMBER (msg->msgnumber),
			   (long) CS_ORIGIN (msg->msgnumber),
			   (long) CS_LAYER (msg->msgnumber), msg->msgstring);

	if (msg->osstringlen > 0) {
		g_string_sprintfa (txt,
				   "\n\tOperating system error number(%ld):\n\t%s",
				   (long) msg->osnumber, msg->osstring);
	}

	return g_strdup (txt->str);
}

gchar *
g_sprintf_servermsg (const gchar * head, CS_SERVERMSG * msg)
{
	GString *txt = NULL;

	g_return_val_if_fail (msg != NULL, NULL);
	txt = g_string_new ("");
	g_return_val_if_fail (txt != NULL, NULL);

	g_string_sprintfa (txt,
			   "TDS: %s\n\tnumber(%ld) severity(%ld) state(%ld) "
			   "line(%ld)", head, (long) msg->msgnumber,
			   (long) msg->severity, (long) msg->state,
			   (long) msg->line);
	if (msg->svrnlen > 0)
		g_string_sprintfa (txt, "\n\tServer name: %s", msg->svrname);
	if (msg->proclen > 0)
		g_string_sprintfa (txt, "\n\tProcedure name: %s", msg->proc);

	g_string_sprintfa (txt, "\n\t%s", msg->text);

	return (g_strdup (txt->str));
}

/*
 * Private functions
 *
 *
 */

gint
gda_tds_install_error_handlers (GdaServerConnection * cnc)
{
	tds_Connection *scnc = NULL;

	g_return_val_if_fail (cnc != NULL, -1);
	scnc = (tds_Connection *) gda_server_connection_get_user_data (cnc);

	if ((scnc->ret = cs_config (scnc->ctx, CS_SET, CS_MESSAGE_CB,
				    (CS_VOID *) gda_tds_csmsg_callback,
				    CS_UNUSED, NULL)) != CS_SUCCEED) {
		gda_tds_cleanup (scnc, scnc->ret,
				 "Could not install cslib callback\n");
		return -1;
	}
	if ((scnc->ret =
	     ct_callback (scnc->ctx, NULL, CS_SET, CS_CLIENTMSG_CB,
			  (CS_VOID *) gda_tds_clientmsg_callback))
	    != CS_SUCCEED) {
		gda_tds_cleanup (scnc, scnc->ret, "Could not install ct "
				 "callback for client messages\n");
		return -1;
	}
	if ((scnc->ret =
	     ct_callback (scnc->ctx, NULL, CS_SET, CS_SERVERMSG_CB,
			  (CS_VOID *) gda_tds_servermsg_callback))
	    != CS_SUCCEED) {
		gda_tds_cleanup (scnc, scnc->ret, "Could not install ct "
				 "callback for server messages\n");
		return -1;
	}

	return 0;
}

void
gda_tds_log_clientmsg (const gchar * head, CS_CLIENTMSG * msg)
{
	gchar *txt = g_sprintf_clientmsg (head, msg);
	g_return_if_fail (txt != NULL);
	gda_log_message (txt);
	g_free ((gpointer) txt);
}

void
gda_tds_log_servermsg (const gchar * head, CS_SERVERMSG * msg)
{
	gchar *txt = g_sprintf_servermsg (head, msg);
	g_return_if_fail (txt != NULL);
	gda_log_message (txt);
	g_free ((gpointer) txt);
}

/*
 * Tds callback handlers
 * These do nothing so far
 */

GdaServerConnection *
gda_tds_callback_get_connection (const CS_CONTEXT * ctx,
				 const CS_CONNECTION * cnc)
{
//  GList *server_list = gda_server_list();

//  g_return_val_if_fail(server_list != NULL, NULL);

	return NULL;
}

static CS_RETCODE CS_PUBLIC
gda_tds_servermsg_callback (CS_CONTEXT * ctx,
			    CS_CONNECTION * cnc, CS_SERVERMSG * msg)
{
	gda_tds_log_servermsg (_("Server message:"), msg);
	return (CS_SUCCEED);
}

static CS_RETCODE CS_PUBLIC
gda_tds_clientmsg_callback (CS_CONTEXT * ctx,
			    CS_CONNECTION * cnc, CS_CLIENTMSG * msg)
{
	gda_tds_log_clientmsg (_("Client message:"), msg);
	return (CS_SUCCEED);
}

static CS_RETCODE CS_PUBLIC
gda_tds_csmsg_callback (CS_CONTEXT * ctx, CS_CLIENTMSG * msg)
{
	gda_tds_log_clientmsg (_("CS-Library error:"), msg);
	return (CS_SUCCEED);
}
