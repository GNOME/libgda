/* GDA postgres provider
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
#include "gda-postgres-util.h"

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.c" /* this one is dynamically generated */

static GdaConnectionEventCode
gda_postgres_sqlsate_to_gda_code (const gchar *sqlstate)
{
        guint64 gda_code = g_ascii_strtoull (sqlstate, NULL, 0);

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

        error_ev = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
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
                    (trans->state != GDA_TRANSACTION_STATUS_STATE_FAILED))
                        gda_connection_internal_change_transaction_state (cnc,
                                                                          GDA_TRANSACTION_STATUS_STATE_FAILED);
        }
        return error_ev;
}

/* to be used only while initializing a connection */
PGresult *
_gda_postgres_PQexec_wrap (GdaConnection *cnc, PGconn *pconn, const char *query)
{
	GdaConnectionEvent *event;

        if (cnc) {
                event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
                gda_connection_event_set_description (event, query);
                gda_connection_add_event (cnc, event);
        }

        return PQexec (pconn, query);
}

GType
_gda_postgres_type_oid_to_gda (PostgresConnectionData *cdata, Oid postgres_type)
{
	GType *type;
	type = g_hash_table_lookup (cdata->h_table, GUINT_TO_POINTER (postgres_type));
	if (type)
		return *type;
	else
		return G_TYPE_STRING;
}

#ifdef GDA_DEBUG
void
_gda_postgres_test_keywords (void)
{
        V82test_keywords();
        V83test_keywords();
        V84test_keywords();
}
#endif

GdaSqlReservedKeywordsFunc
_gda_postgres_get_reserved_keyword_func (PostgresConnectionData *cdata)
{
	if (cdata && cdata->short_version) {
		switch (*cdata->short_version) {
                case '8':
                        if (cdata->short_version[1] == '2')
                                return V82is_keyword;
                        if (cdata->short_version[1] == '3')
                                return V83is_keyword;
                        if (cdata->short_version[1] == '4')
                                return V84is_keyword;
                        return V84is_keyword;
                default:
                        return V84is_keyword;
                break;
                }
	}
        return V84is_keyword;
}
