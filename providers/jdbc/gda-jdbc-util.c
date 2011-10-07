/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-jdbc-util.h"

extern JavaVM *_jdbc_provider_java_vm;

/**
 * _gda_jdbc_make_error
 *
 * warning: STEALS @sql_state and @error.
 * Create a new #GdaConnectionEvent object and "adds" it to @cnc
 *
 * Returns: a new GdaConnectionEvent which must not be unrefed()
 */
GdaConnectionEvent *
_gda_jdbc_make_error (GdaConnection *cnc, gint error_code, gchar *sql_state, GError *error)
{
	GdaConnectionEvent *error_ev;
        GdaConnectionEventCode gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
        GdaTransactionStatus *trans;

        error_ev = GDA_CONNECTION_EVENT (g_object_new (GDA_TYPE_CONNECTION_EVENT, "type",
						       (gint) GDA_CONNECTION_EVENT_ERROR, NULL));
	if (error) {
		gda_connection_event_set_description (error_ev,
						      error->message ? error->message : _("No detail"));
		g_error_free (error);
	}
	gda_connection_event_set_sqlstate (error_ev, sql_state);
	g_free (sql_state);
	gda_connection_event_set_code (error_ev, error_code);	
	gda_connection_event_set_gda_code (error_ev, gda_code);
        gda_connection_event_set_source (error_ev, "gda-jdbc");
        gda_connection_add_event (cnc, error_ev);

        /* change the transaction status if there is a problem */
        trans = gda_connection_get_transaction_status (cnc);
        if (trans) {
		/*
                if ((PQtransactionStatus (pconn) == PQTRANS_INERROR) &&
                    (trans->state != GDA_TRANSACTION_STATUS_STATE_FAILED))
                        gda_connection_internal_change_transaction_state (cnc,
                                                                          GDA_TRANSACTION_STATUS_STATE_FAILED);
		*/
        }
        return error_ev;
}

/**
 * _gda_jdbc_get_jenv
 * @out_needs_detach: SHOULD NOT BE NULL
 *
 * Returns: a JNIEnv or %NULL if an error occurred
 */
JNIEnv *
_gda_jdbc_get_jenv (gboolean *out_needs_detach, GError **error)
{
	jint atres;
	JNIEnv *env;

	*out_needs_detach = FALSE;
	atres = (*_jdbc_provider_java_vm)->GetEnv (_jdbc_provider_java_vm, (void**) &env, JNI_VERSION_1_2);
	if (atres == JNI_EDETACHED) {
		if ((*_jdbc_provider_java_vm)->AttachCurrentThread (_jdbc_provider_java_vm, (void**) &env, NULL) < 0)
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", "Could not attach JAVA virtual machine's current thread");
		else
			*out_needs_detach = TRUE;
	}
	else if (atres == JNI_EVERSION)
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", "Could not attach JAVA virtual machine's current thread");
	return env;
}

void
_gda_jdbc_release_jenv (gboolean needs_detach)
{
	if (needs_detach)
		(*_jdbc_provider_java_vm)->DetachCurrentThread (_jdbc_provider_java_vm);
}

/*
 * converts a GType to an INT identifier used to communicate types with JDBC. The corresponding
 * decoding method on the JAVA side is GdaJValue.proto_type_to_jvalue()
 *
 * Any new type added here must also be added to the GdaJValue.proto_type_to_jvalue() method
 */
int
_gda_jdbc_gtype_to_proto_type (GType type)
{
	if (type == G_TYPE_STRING)
		return 1;
	else if (type == G_TYPE_INT)
		return 2;
	else if (type == G_TYPE_CHAR)
		return 3;
	else if (type == G_TYPE_DOUBLE)
		return 4;
	else if (type == G_TYPE_FLOAT)
		return 5;
	else if (type == G_TYPE_BOOLEAN)
		return 6;
	else if (type == G_TYPE_DATE)
		return 7;
	else if (type == GDA_TYPE_TIME)
		return 8;
	else if (type == GDA_TYPE_TIMESTAMP)
		return 9;
	else if (type == GDA_TYPE_BINARY)
		return 10;
	else if (type == GDA_TYPE_BLOB)
		return 11;
	else if (type == G_TYPE_INT64)
		return 12;
	else if (type == GDA_TYPE_SHORT)
		return 13;
	else if (type == GDA_TYPE_NUMERIC)
		return 14;
	else
		return 0; /* GDA_TYPE_NULL */
}
