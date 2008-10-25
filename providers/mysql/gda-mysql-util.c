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
			     mysql_error (mysql));
		
		//g_print ("%s: %s\n", __func__, mysql_error (mysql));
		
	} else if (mysql_stmt) {
		gda_connection_event_set_sqlstate
			(event_error, mysql_stmt_sqlstate (mysql_stmt));
		gda_connection_event_set_description
			(event_error, mysql_stmt_error (mysql_stmt));
		gda_connection_event_set_code
			(event_error, GDA_CONNECTION_EVENT_CODE_UNKNOWN);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     mysql_stmt_error (mysql_stmt));
		
		//g_print ("%s : %s\n", __func__, mysql_stmt_error (mysql_stmt));
		
	} else {
		gda_connection_event_set_sqlstate
			(event_error, _("Unknown"));
		gda_connection_event_set_description
			(event_error, _("No description"));
		gda_connection_event_set_code
			(event_error, GDA_CONNECTION_EVENT_CODE_UNKNOWN);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     _("No detail"));
	}
	gda_connection_event_set_source (event_error, "gda-mysql");

	gda_connection_add_event (cnc, event_error);

	return event_error;
}

GType
_gda_mysql_type_to_gda (MysqlConnectionData    *cdata,
			enum enum_field_types   mysql_type)
{
	GType gtype = 0;
	switch (mysql_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_YEAR:
		gtype = G_TYPE_INT;
		break;
	case MYSQL_TYPE_LONGLONG:
		gtype = G_TYPE_LONG;
		break;
	case MYSQL_TYPE_FLOAT:
		gtype = G_TYPE_FLOAT;
		break;
	case MYSQL_TYPE_DECIMAL:
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_DOUBLE:
		gtype = G_TYPE_DOUBLE;
		break;
	case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_BLOB:
		gtype = GDA_TYPE_BLOB;
		break;
	case MYSQL_TYPE_TIMESTAMP:
	case MYSQL_TYPE_DATETIME:
		gtype = GDA_TYPE_TIMESTAMP;
		break;
	case MYSQL_TYPE_DATE:
		gtype = G_TYPE_DATE;
		break;
	case MYSQL_TYPE_TIME:
		gtype = GDA_TYPE_TIME;
		break;
	case MYSQL_TYPE_NULL:
		gtype = GDA_TYPE_NULL;
		break;
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_SET:
	case MYSQL_TYPE_ENUM:
	case MYSQL_TYPE_GEOMETRY:
	default:
		gtype = G_TYPE_STRING;
	}

	/* g_print ("%s: ", __func__); */
	/* switch (mysql_type) { */
	/* case MYSQL_TYPE_TINY:  g_print ("MYSQL_TYPE_TINY");  break; */
	/* case MYSQL_TYPE_SHORT:  g_print ("MYSQL_TYPE_SHORT");  break; */
	/* case MYSQL_TYPE_LONG:  g_print ("MYSQL_TYPE_LONG");  break; */
	/* case MYSQL_TYPE_INT24:  g_print ("MYSQL_TYPE_INT24");  break; */
	/* case MYSQL_TYPE_YEAR:  g_print ("MYSQL_TYPE_YEAR");  break; */
	/* case MYSQL_TYPE_LONGLONG:  g_print ("MYSQL_TYPE_LONGLONG");  break; */
	/* case MYSQL_TYPE_FLOAT:  g_print ("MYSQL_TYPE_FLOAT");  break; */
	/* case MYSQL_TYPE_DECIMAL:  g_print ("MYSQL_TYPE_DECIMAL");  break; */
	/* case MYSQL_TYPE_NEWDECIMAL:  g_print ("MYSQL_TYPE_NEWDECIMAL");  break; */
	/* case MYSQL_TYPE_DOUBLE:  g_print ("MYSQL_TYPE_DOUBLE");  break; */
	/* case MYSQL_TYPE_BIT:  g_print ("MYSQL_TYPE_BIT");  break; */
	/* case MYSQL_TYPE_BLOB:  g_print ("MYSQL_TYPE_BLOB");  break; */
	/* case MYSQL_TYPE_TIMESTAMP:  g_print ("MYSQL_TYPE_TIMESTAMP");  break; */
	/* case MYSQL_TYPE_DATETIME:  g_print ("MYSQL_TYPE_DATETIME");  break; */
	/* case MYSQL_TYPE_DATE:  g_print ("MYSQL_TYPE_DATE");  break; */
	/* case MYSQL_TYPE_TIME:  g_print ("MYSQL_TYPE_TIME");  break; */
	/* case MYSQL_TYPE_NULL:  g_print ("MYSQL_TYPE_NULL");  break; */
	/* case MYSQL_TYPE_STRING:  g_print ("MYSQL_TYPE_STRING");  break; */
	/* case MYSQL_TYPE_VAR_STRING:  g_print ("MYSQL_TYPE_VAR_STRING");  break; */
	/* case MYSQL_TYPE_SET:  g_print ("MYSQL_TYPE_SET");  break; */
	/* case MYSQL_TYPE_ENUM:  g_print ("MYSQL_TYPE_ENUM");  break; */
	/* case MYSQL_TYPE_GEOMETRY:  g_print ("MYSQL_TYPE_GEOMETRY");  break; */
	/* default:  g_print ("UNKNOWN %d: MYSQL_TYPE_STRING", mysql_type);  break; */
	/* } */
	/* g_print ("\n"); */

	return gtype;
}
