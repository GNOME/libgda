/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "gda-mysql-util.h"

/*
 * Create a new #GdaConnectionEvent object and "adds" it to @cnc
 *
 * Returns: a new GdaConnectionEvent which must not be unrefed()
 */
GdaConnectionEvent *
_gda_mysql_make_error (GdaConnection  *cnc,
		       MYSQL          *mysql,
		       MYSQL_STMT     *mysql_stmt,
		       GError        **error)
{
	GdaConnectionEvent *event_error =
		gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
	if (mysql) {
		gda_connection_event_set_sqlstate
			(event_error, mysql_sqlstate (mysql));
		gda_connection_event_set_description
			(event_error, mysql_error (mysql));
		gda_connection_event_set_code
			(event_error, (glong) mysql_errno (mysql));
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     "%s", mysql_error (mysql));
		
		//g_print ("%s: %s\n", __func__, mysql_error (mysql));
		
	} else if (mysql_stmt) {
		gda_connection_event_set_sqlstate
			(event_error, mysql_stmt_sqlstate (mysql_stmt));
		gda_connection_event_set_description
			(event_error, mysql_stmt_error (mysql_stmt));
		gda_connection_event_set_code
			(event_error, (glong) mysql_stmt_errno (mysql_stmt));
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     "%s", mysql_stmt_error (mysql_stmt));
		
		//g_print ("%s : %s\n", __func__, mysql_stmt_error (mysql_stmt));
		
	} else {
		gda_connection_event_set_sqlstate
			(event_error, _("Unknown"));
		gda_connection_event_set_description
			(event_error, _("No description"));
		gda_connection_event_set_code
			(event_error, GDA_CONNECTION_EVENT_CODE_UNKNOWN);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     "%s", _("No detail"));
	}
	gda_connection_event_set_source (event_error, "gda-mysql");

	gda_connection_add_event (cnc, event_error);

	return event_error;
}
