/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-postgres-util.h"


static GdaConnectionEventCode
gda_postgres_sqlsate_to_gda_code (const gchar *sqlstate)
{
        guint64 gda_code = 0;
	if (sqlstate)
		gda_code = g_ascii_strtoull (sqlstate, NULL, 0);

        switch (gda_code) {
                case 42501:
                        return GDA_CONNECTION_EVENT_CODE_INSUFFICIENT_PRIVILEGES;
                case 23505:
                        return GDA_CONNECTION_EVENT_CODE_UNIQUE_VIOLATION;
                case 23502:
                        return GDA_CONNECTION_EVENT_CODE_NOT_NULL_VIOLATION;
                default:
                        return GDA_CONNECTION_EVENT_CODE_UNKNOWN;
        }
}

/*
 * Returns: @str
 */
static gchar *
utf8_validate (gchar *str)
{
	gchar *tmp;
	if (g_utf8_validate (str, -1, (const gchar **) &tmp))
		return str;
	else {
		*tmp = ' ';
		for (tmp++; tmp; tmp++) {
			if (g_utf8_validate (tmp, -1, (const gchar **) &tmp))
				return str;
			else
				*tmp = ' ';
		}
		return tmp;
	}
}

/*
 * Create a new #GdaConnectionEvent object and "adds" it to @cnc
 *
 * Returns: a new GdaConnectionEvent which must not be unrefed()
 */
GdaConnectionEvent *
_gda_postgres_make_error (GdaConnection *cnc, PGconn *pconn, PGresult *pg_res, GError **error)
{
	GdaConnectionEvent *error_ev;
        GdaConnectionEventCode gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
        GdaTransactionStatus *trans;

        error_ev = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
        if (pconn != NULL) {
                gchar *message;

                if (pg_res != NULL) {
                        gchar *sqlstate;

                        message = g_strdup (PQresultErrorMessage (pg_res));
                        sqlstate = PQresultErrorField (pg_res, PG_DIAG_SQLSTATE);
                        gda_connection_event_set_sqlstate (error_ev, sqlstate);
                        gda_code = gda_postgres_sqlsate_to_gda_code (sqlstate);
                }
                else {
                        message = g_strdup (PQerrorMessage (pconn));
                        gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
                }
		utf8_validate (message);
		
		gchar *ptr = message;
		if (g_str_has_prefix (message, "ERROR:"))
			ptr += 6;
		g_strstrip (ptr);

                gda_connection_event_set_description (error_ev, ptr);
                gda_connection_event_set_gda_code (error_ev, gda_code);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s", ptr);
		g_free (message);
        }
        else {
                gda_connection_event_set_description (error_ev, _("No detail"));
                gda_connection_event_set_gda_code (error_ev, gda_code);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
			     "%s", _("No detail"));
        }

        gda_connection_event_set_code (error_ev, -1);
        gda_connection_event_set_source (error_ev, "gda-postgres");
        gda_connection_add_event (cnc, error_ev);

        /* change the transaction status if there is a problem */
        trans = gda_connection_get_transaction_status (cnc);
        if (trans) {
                if ((PQtransactionStatus (pconn) == PQTRANS_INERROR) &&
                    (gda_transaction_status_get_state (trans) != GDA_TRANSACTION_STATUS_STATE_FAILED))
                        gda_connection_internal_change_transaction_state (cnc,
                                                                          GDA_TRANSACTION_STATUS_STATE_FAILED);
        }
        return error_ev;
}

/* 
 * Returns NULL if an error occurred
 *
 * WARNING: it does not check that @pconn can safely be used
 */
PGresult *
_gda_postgres_PQexec_wrap (GdaConnection *cnc, PGconn *pconn, const char *query)
{
	GdaConnectionEvent *event;

        if (cnc) {
                event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
                gda_connection_event_set_description (event, query);
                gda_connection_add_event (cnc, event);
        }

        return PQexec (pconn, query);
}
