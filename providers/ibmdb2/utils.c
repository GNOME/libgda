/* GDA IBMDB2 Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
 *	   Sergey N. Belinsky <sergey_be@mail.ru>
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include <sqlcli1.h>
#include "gda-ibmdb2.h"

GdaError *
gda_ibmdb2_make_error (SQLHANDLE henv, SQLHANDLE hdbc, SQLHANDLE hstmt)
{
        GdaError *error = NULL;
	SQLRETURN rc;
        SQLCHAR sql_state[6];
	SQLINTEGER native_error;
	SQLCHAR error_msg[SQL_MAX_MESSAGE_LENGTH];
	SQLSMALLINT error_msg_len;
				       

	if (henv != SQL_NULL_HANDLE) {
    		rc = SQLError (henv,
		    	       hdbc,
		    	       hstmt,
		               (SQLCHAR*)&sql_state,
		               &native_error,
		               (SQLCHAR*)&error_msg,
		               sizeof (error_msg), 
		               &error_msg_len);
	    	
		if (rc == SQL_SUCCESS) {
    	 		error = gda_error_new ();
			
			//g_message("Error: %s\n", (gchar*)error_msg);
			
			gda_error_set_description (error, (gchar*)error_msg);
			gda_error_set_number (error, native_error);
			gda_error_set_source (error, "gda-ibmdb2");
			gda_error_set_sqlstate (error, sql_state);
		}
	}

	return error;
}


void
gda_ibmdb2_emit_error (GdaConnection * cnc,
		       SQLHANDLE henv, SQLHANDLE hdbc, SQLHANDLE hstmt)
{
	GdaError *error;
	GList *list = NULL;
	while ((error = gda_ibmdb2_make_error (henv, hdbc, hstmt))) {
		list = g_list_append (list, error);
	}
	gda_connection_add_error_list (cnc, list);
}
