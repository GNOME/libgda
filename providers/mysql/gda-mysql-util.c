/* GDA mysql provider
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "gda-mysql-util.h"

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.c" /* this one is dynamically generated */

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
		gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (mysql) {
		gda_connection_event_set_sqlstate
			(event_error, mysql_sqlstate (mysql));
		gda_connection_event_set_description
			(event_error, mysql_error (mysql));
		gda_connection_event_set_code
			(event_error, GDA_CONNECTION_EVENT_CODE_UNKNOWN);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     "%s", mysql_error (mysql));
		
		//g_print ("%s: %s\n", __func__, mysql_error (mysql));
		
	} else if (mysql_stmt) {
		gda_connection_event_set_sqlstate
			(event_error, mysql_stmt_sqlstate (mysql_stmt));
		gda_connection_event_set_description
			(event_error, mysql_stmt_error (mysql_stmt));
		gda_connection_event_set_code
			(event_error, GDA_CONNECTION_EVENT_CODE_UNKNOWN);
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

#ifdef GDA_DEBUG
void
_gda_mysql_test_keywords (void)
{
	V50test_keywords();
	V51test_keywords();
	V54test_keywords();
	V60test_keywords();
}
#endif

GdaSqlReservedKeywordsFunc
_gda_mysql_get_reserved_keyword_func (MysqlConnectionData *cdata)
{
	if (cdata && cdata->short_version) {
		switch (*cdata->short_version) {
		case '5':
			if (cdata->short_version[1] == '1')
				return V51is_keyword;
			if (cdata->short_version[1] == '0')
				return V50is_keyword;
			return V54is_keyword;
		case '6':
		default:
			return V60is_keyword;
		break;
		}
	}
	return V60is_keyword;
}
