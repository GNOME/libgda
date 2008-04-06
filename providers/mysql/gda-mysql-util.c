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

/* static GdaConnectionEventCode */
/* gda_mysql_sqlsate_to_gda_code (const gchar  *sqlstate) */
/* { */
/*         guint64 gda_code = g_ascii_strtoull (sqlstate, NULL, 0); */

/*         switch (gda_code) { */
/*                 case 42501: */
/*                         return GDA_CONNECTION_EVENT_CODE_INSUFFICIENT_PRIVILEGES; */
/*                 case 23505: */
/*                         return GDA_CONNECTION_EVENT_CODE_UNIQUE_VIOLATION; */
/*                 case 23502: */
/*                         return GDA_CONNECTION_EVENT_CODE_NOT_NULL_VIOLATION; */
/*                 default: */
/*                         return GDA_CONNECTION_EVENT_CODE_UNKNOWN; */
/*         } */
/* } */

/*
 * Create a new #GdaConnectionEvent object and "adds" it to @cnc
 *
 * Returns: a new GdaConnectionEvent which must not be unrefed()
 */
GdaConnectionEvent *
_gda_mysql_make_error (GdaConnection  *cnc,
		       MYSQL          *mysql, /* PGresult *pg_res, */
		       GError        **error)
{
	/* GdaConnectionEvent *error_ev; */
        /* GdaConnectionEventCode gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN; */
        /* GdaTransactionStatus *trans; */

        /* error_ev = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR); */
        /* if (mysql != NULL) { */
        /*         gchar *message; */

        /*         if (pg_res != NULL) { */
        /*                 gchar *sqlstate; */

        /*                 message = g_strdup (PQresultErrorMessage (pg_res)); */
        /*                 // sqlstate = PQresultErrorField (pg_res, PG_DIAG_SQLSTATE); */
        /*                 gda_connection_event_set_sqlstate (error_ev, sqlstate); */
        /*                 gda_code = gda_mysql_sqlsate_to_gda_code (sqlstate); */
        /*         } */
        /*         else { */
        /*                 message = g_strdup (PQerrorMessage (mysql)); */
        /*                 gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN; */
        /*         } */

		
	/* 	gchar *ptr = message; */
	/* 	if (g_str_has_prefix (message, "ERROR:")) */
	/* 		ptr += 6; */
	/* 	g_strstrip (ptr); */

        /*         gda_connection_event_set_description (error_ev, ptr); */
        /*         gda_connection_event_set_gda_code (error_ev, gda_code); */
	/* 	g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, ptr); */
	/* 	g_free (message); */
        /* } */
        /* else { */
        /*         gda_connection_event_set_description (error_ev, _("No detail")); */
        /*         gda_connection_event_set_gda_code (error_ev, gda_code); */
	/* 	g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, */
	/* 		     _("No detail")); */
        /* } */

        /* gda_connection_event_set_code (error_ev, -1); */
        /* gda_connection_event_set_source (error_ev, "gda-mysql"); */
        /* gda_connection_add_event (cnc, error_ev); */

        /* /\* change the transaction status if there is a problem *\/ */
        /* trans = gda_connection_get_transaction_status (cnc); */
        /* if (trans) { */
        /*         if ((PQtransactionStatus (mysql) == PQTRANS_INERROR) && */
        /*             (trans->state != GDA_TRANSACTION_STATUS_STATE_FAILED)) */
        /*                 gda_connection_internal_change_transaction_state (cnc, */
        /*                                                                   GDA_TRANSACTION_STATUS_STATE_FAILED); */
        /* } */
        /* return error_ev; */
	
	GdaConnectionEvent *event_error =
		gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (mysql) {
		gda_connection_event_set_description
			(event_error, mysql_error (mysql));
		gda_connection_event_set_code
			(event_error, mysql_errno (mysql));
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     mysql_sqlstate (mysql));
	} else {
		gda_connection_event_set_description
			(event_error, _("No description"));
		gda_connection_event_set_code
			(event_error, -1);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     _("No detail"));
	}
	gda_connection_event_set_code (event_error, -1);
	gda_connection_event_set_source (event_error, "gda-mysql");

	gda_connection_add_event (cnc, event_error);

	return event_error;
}

/* /\* to be used only while initializing a connection *\/ */
/* int */
/* _gda_mysql_real_query_wrap (GdaConnection  *cnc, */
/* 			    MYSQL          *mysql, */
/* 			    const char     *query, */
/* 			    unsigned long   length) */
/* { */
/* 	GdaConnectionEvent *event; */

/*         if (cnc) { */
/*                 event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND); */
/*                 gda_connection_event_set_description (event, query); */
/*                 gda_connection_add_event (cnc, event); */
/*         } */

/*         return mysql_real_query (mysql, query, length); */
/* } */

GType
_gda_mysql_type_to_gda (MysqlConnectionData    *cdata,
			enum enum_field_types   mysql_type)
{
	/* 	gint i; */

	/* 	for (i = 0; i < cdata->ntypes; i++) */
	/* 		if (cdata->type_data[i].oid == mysql_type) */
	/* 			break; */

	/*   	if (cdata->type_data[i].oid != mysql_type) */
	/* 		return G_TYPE_STRING; */

	/* 	return cdata->type_data[i].type; */
	
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
	return gtype;
	
}
