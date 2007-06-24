/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
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
#include <stdlib.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-blob-op.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <libpq/libpq-fs.h>
#include <libgda/gda-connection-private.h>

GdaConnectionEventCode
gda_postgres_sqlsate_to_gda_code (const gchar *sqlstate)
{
	guint64 gda_code = g_ascii_strtoull (sqlstate, NULL, 0);

	switch (gda_code)
	{
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

GdaConnectionEvent *
gda_postgres_make_error (GdaConnection *cnc, PGconn *pconn, PGresult *pg_res)
{
	GdaConnectionEvent *error;
	GdaConnectionEventCode gda_code;
	GdaTransactionStatus *trans;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (pconn != NULL) {
		gchar *message;
		
		if (pg_res != NULL) {
			gchar *sqlstate;

			message = PQresultErrorMessage (pg_res);
			sqlstate = PQresultErrorField (pg_res, PG_DIAG_SQLSTATE);
			gda_connection_event_set_sqlstate (error, sqlstate);
			gda_code = gda_postgres_sqlsate_to_gda_code (sqlstate);
		}
		else {
			message = PQerrorMessage (pconn);
			gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
		}

		gda_connection_event_set_description (error, message);
		gda_connection_event_set_gda_code (error, gda_code);
	} 
	else {
		gda_connection_event_set_description (error, _("NO DESCRIPTION"));
		gda_connection_event_set_gda_code (error, gda_code);
	}

	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, "gda-postgres");
	gda_connection_add_event (cnc, error);

	/* change the transaction status if there is a problem */
	trans = gda_connection_get_transaction_status (cnc);
	if (trans) {
		if ((PQtransactionStatus (pconn) == PQTRANS_INERROR) &&
		    (trans->state != GDA_TRANSACTION_STATUS_STATE_FAILED))
			gda_connection_internal_change_transaction_state (cnc,
									  GDA_TRANSACTION_STATUS_STATE_FAILED);
	}
	return error;
}

GType
gda_postgres_type_name_to_gda (GHashTable *h_table, const gchar *name)
{
	GType *type;

	type = g_hash_table_lookup (h_table, name);
	if (type)
		return *type;

	return G_TYPE_STRING;
}

GType
gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, gint ntypes, Oid postgres_type)
{
	gint i;

	for (i = 0; i < ntypes; i++)
		if (type_data[i].oid == postgres_type) 
			break;

  	if (type_data[i].oid != postgres_type)
		return G_TYPE_STRING;

	return type_data[i].type;
}

/* Makes a point from a string like "(3.2,5.6)" */
static void
make_point (GdaGeometricPoint *point, const gchar *value)
{
	value++;
	point->x = atof (value);
	value = strchr (value, ',');
	value++;
	point->y = atof (value);
}

/* Makes a GdaTime from a string like "12:30:15+01" */
static void
make_time (GdaTime *timegda, const gchar *value)
{
	timegda->hour = atoi (value);
	value += 3;
	timegda->minute = atoi (value);
	value += 3;
	timegda->second = atoi (value);
	value += 2;
	if (*value)
		timegda->timezone = atoi (value);
	else
		timegda->timezone = GDA_TIMEZONE_INVALID;
}

/* Makes a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
make_timestamp (GdaTimestamp *timestamp, const gchar *value)
{
	timestamp->year = atoi (value);
	value += 5;
	timestamp->month = atoi (value);
	value += 3;
	timestamp->day = atoi (value);
	value += 3;
	timestamp->hour = atoi (value);
	value += 3;
	timestamp->minute = atoi (value);
	value += 3;
	timestamp->second = atoi (value);
	value += 2;
	if (*value != '.') {
		timestamp->fraction = 0;
	} else {
		gint ndigits = 0;
		gint64 fraction;

		value++;
		fraction = atol (value);
		while (*value && *value != '+') {
			value++;
			ndigits++;
		}

		while (ndigits < 3) {
			fraction *= 10;
			ndigits++;
		}

		while (fraction > 0 && ndigits > 3) {
			fraction /= 10;
			ndigits--;
		}
		
		timestamp->fraction = fraction;
	}

	if (*value != '+') {
		timestamp->timezone = 0;
	} else {
		value++;
		timestamp->timezone = atol (value) * 60 * 60;
	}
}

void
gda_postgres_set_value (GdaConnection *cnc,
			GValue *value,
			GType type,
			const gchar *thevalue,
			gboolean isNull,
			gint length)
{
	if (isNull) {
		gda_value_set_null (value);
		return;
	}

	gda_value_reset_with_type (value, type);

	if (type == G_TYPE_BOOLEAN)
		g_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
	else if (type == G_TYPE_STRING)
		g_value_set_string (value, thevalue);
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, atoll (thevalue));
	else if (type == G_TYPE_ULONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == G_TYPE_LONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == G_TYPE_INT)
		g_value_set_int (value, atol (thevalue));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, atoi (thevalue));
	else if (type == G_TYPE_FLOAT) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_float (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == G_TYPE_DOUBLE) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_double (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();
		g_date_set_parse (gdate, thevalue);
		if (!g_date_valid (gdate)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", thevalue);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		g_value_take_boxed (value, gdate);
	}
	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		GdaGeometricPoint point;
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		GdaTimestamp timestamp;
		make_timestamp (&timestamp, thevalue);
		gda_value_set_timestamp (value, &timestamp);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime timegda;
		make_time (&timegda, thevalue);
		gda_value_set_time (value, &timegda);
	}
	else if (type == GDA_TYPE_BINARY) {
		/*
		 * Requires PQunescapeBytea in libpq (present since 7.3.x)
		 */
		guchar *unescaped;
                size_t pqlength = 0;

		unescaped = PQunescapeBytea ((guchar*)thevalue, &pqlength);
		if (unescaped != NULL) {
			GdaBinary bin;
			bin.data = unescaped;
			bin.binary_length = pqlength;
			gda_value_set_binary (value, &bin);
			PQfreemem (unescaped);
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		GdaPostgresConnectionData *priv_data;
		GdaBlobOp *op;
		priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
		blob = g_new0 (GdaBlob, 1);
		op = gda_postgres_blob_op_new_with_id (cnc, thevalue);
		gda_blob_set_op (blob, op);
		g_object_unref (op);

		gda_value_take_blob (value, blob);
	}
	else if (type == G_TYPE_STRING) 
		g_value_set_string (value, thevalue);
	else {
		g_warning ("Type %s not translated for value '%s' => set as string", g_type_name (type), thevalue);
		gda_value_reset_with_type (value, G_TYPE_STRING);
		g_value_set_string (value, thevalue);
	}
}

gchar *
gda_postgres_value_to_sql_string (GValue *value)
{
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	type = G_VALUE_TYPE (value);

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == G_TYPE_CHAR))
		ret = g_strdup (val_str);
	else
		ret = g_strdup_printf ("\'%s\'", val_str);

	g_free (val_str);

	return ret;
}

PGresult *
gda_postgres_PQexec_wrap (GdaConnection *cnc, PGconn *conn, const char *query)
{
	GdaConnectionEvent *event;

	event = gda_connection_event_new (GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event, query);
	gda_connection_add_event (cnc, event);

	return PQexec (conn, query);
}

gboolean
gda_postgres_check_transaction_started (GdaConnection *cnc)
{
	GdaTransactionStatus *trans;
	trans = gda_connection_get_transaction_status (cnc);
	if (!trans && !gda_connection_begin_transaction (cnc, NULL,
							  GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL))
		return FALSE;
	else
		return TRUE;
}

/* Checks if the given column number of the given data model
 * has the given constraint.
 *
 * contype may be one of the following:
 *    'f': foreign key
 *    'p': primary key
 *    'u': unique key
 *    etc...
 */
static gboolean 
check_constraint (GdaConnection *cnc,
		  PGresult *pg_res,
		  const gchar *table_name,
		  const gint col,
		  const gchar contype)
{
        GdaPostgresConnectionData *cnc_priv_data;
        PGresult *pg_con_res;
        gchar *column_name;
        gchar *query;
        gboolean state = FALSE;

        g_return_val_if_fail (table_name != NULL, FALSE);

        cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

        column_name = PQfname (pg_res, col);
        if (column_name != NULL) {
                query = g_strdup_printf ("SELECT 1 FROM pg_catalog.pg_class c, pg_catalog.pg_constraint c2, pg_catalog.pg_attribute a WHERE c.relname = '%s' AND c.oid = c2.conrelid and a.attrelid = c.oid and c2.contype = '%c' and c2.conkey[1] = a.attnum and a.attname = '%s'", table_name, contype, column_name);
                pg_con_res = PQexec (cnc_priv_data->pconn, query);
                if (pg_con_res != NULL) {
                        state = PQntuples (pg_con_res) == 1;
                        PQclear (pg_con_res);
                }
                g_free (query);
        }

        return state;
}


void 
gda_postgres_recordset_describe_column (GdaDataModel *model, GdaConnection *cnc, PGresult *pg_res, 
					GdaPostgresTypeOid *type_data, gint ntypes, const gchar *table_name,
					gint col)
{
        GType ftype;
        gint scale;
        GdaColumn *field_attrs;
        gboolean ispk = FALSE, isuk = FALSE;

        g_return_if_fail (pg_res != NULL);

        field_attrs = gda_data_model_describe_column (model, col);
        gda_column_set_name (field_attrs, PQfname (pg_res, col));
        gda_column_set_title (field_attrs, PQfname (pg_res, col));

        ftype = gda_postgres_type_oid_to_gda (type_data, ntypes, PQftype (pg_res, col));

        scale = (ftype == G_TYPE_DOUBLE) ? DBL_DIG :
                (ftype == G_TYPE_FLOAT) ? FLT_DIG : 0;

        gda_column_set_scale (field_attrs, scale);
        gda_column_set_g_type (field_attrs, ftype);

        /* PQfsize() == -1 => variable length */
        gda_column_set_defined_size (field_attrs, PQfsize (pg_res, col));

        gda_column_set_references (field_attrs, "");

        gda_column_set_table (field_attrs, table_name);

        if (table_name) {
                ispk = check_constraint (cnc, pg_res, table_name,  col, 'P');
                isuk = check_constraint (cnc, pg_res, table_name,  col, 'u');
        }
	
        gda_column_set_primary_key (field_attrs, ispk);
        gda_column_set_unique_key (field_attrs, isuk);
        /* FIXME: set_allow_null? */
}

/* Try to guess the table name involved in the given data model. 
 * It can fail on complicated queries, where several tables are
 * involved in the same time.
 */
gchar *
gda_postgres_guess_table_name (GdaConnection *cnc, PGresult *pg_res)
{
	GdaPostgresConnectionData *cnc_priv_data;
	PGresult *pg_name_res;
	PGconn *pg_conn;
	gchar *table_name = NULL;
	
   
   	/* This code should be changed to use libsql, its proberly faster
	 * in long run to store a parsed sql statement and then
	 * just grab the tablename from that. */
	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;

	if (PQnfields (pg_res) > 0) {
		gchar *query = g_strdup_printf ("SELECT c.relname FROM pg_catalog.pg_class c WHERE c.relkind = 'r' AND c.relnatts = '%d'", PQnfields (pg_res));
		gint i;
		for (i = 0; i < PQnfields (pg_res); i++) {
			const gchar *column_name = PQfname (pg_res, i);
			gchar *cond = g_strdup_printf (" AND '%s' IN (SELECT a.attname FROM pg_catalog.pg_attribute a WHERE a.attrelid = c.oid)", column_name);
			gchar *tmp_query = NULL;

			tmp_query = g_strconcat (query, cond, NULL);
			g_free (query);
			g_free (cond);
			query = tmp_query;
		}
		pg_name_res = PQexec (pg_conn, query);
		if (pg_name_res != NULL) {
			if (PQntuples (pg_name_res) == 1)
				table_name = g_strdup (PQgetvalue (pg_name_res, 0, 0));
			PQclear (pg_name_res);
		}
		g_free (query);
	}
	return table_name;
}

GType *
gda_postgres_get_column_types (PGresult *pg_res, GdaPostgresTypeOid *type_data, gint ntypes)
{
	gint ncolumns, i;
	GType *types;
	
	ncolumns = PQnfields (pg_res);
	types = g_new (GType, ncolumns);
	for (i = 0; i < ncolumns; i++)
		types [i] = gda_postgres_type_oid_to_gda (type_data, ntypes, PQftype (pg_res, i));

	return types;
}
