/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include <libpq/libpq-fs.h>
#include "gda-postgres.h"

GdaError *
gda_postgres_make_error (PGconn *pconn, PGresult *pg_res)
{
	GdaError *error;

	error = gda_error_new ();
	if (pconn != NULL) {
		gchar *message;
		
		if (pg_res != NULL)
			message = PQresultErrorMessage (pg_res);
		else
			message = PQerrorMessage (pconn);

		gda_error_set_description (error, message);
	} else {
		gda_error_set_description (error, _("NO DESCRIPTION"));
	}

	gda_error_set_number (error, -1);
	gda_error_set_source (error, "gda-postgres");
	gda_error_set_sqlstate (error, _("Not available"));

	return error;
}

GdaValueType
gda_postgres_type_name_to_gda (GHashTable *h_table, const gchar *name)
{
	GdaValueType *type;

	type = g_hash_table_lookup (h_table, name);
	if (type)
		return *type;

	return GDA_VALUE_TYPE_STRING;
}

GdaValueType
gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, gint ntypes, Oid postgres_type)
{
	gint i;

	for (i = 0; i < ntypes; i++)
		if (type_data[i].oid == postgres_type) 
			break;

  	if (type_data[i].oid != postgres_type)
		return GDA_VALUE_TYPE_STRING;

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
		timegda->timezone = TIMEZONE_INVALID;
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

typedef struct _PostgresBlobPrivateData PostgresBlobPrivateData;

struct _PostgresBlobPrivateData {
	gint blobid;
	GdaBlobMode mode;
	gint fd;
	GdaConnection *cnc;
};

static PGconn *
get_pconn (GdaConnection *cnc)
{
	GdaPostgresConnectionData *priv_data;

	priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);
	if (!priv_data) {
		gda_connection_add_error_string (cnc, _("Invalid PostgreSQL handle"));
		return NULL;
	}

	return priv_data->pconn;
}

static gint 
gda_postgres_blob_open (GdaBlob *blob, GdaBlobMode mode)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;
	gint pg_mode;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	priv->mode = mode;
	pg_mode = 0;
	if ((mode & GDA_BLOB_MODE_READ) == GDA_BLOB_MODE_READ)
		pg_mode |= INV_READ;

	if ((mode & GDA_BLOB_MODE_WRITE) == GDA_BLOB_MODE_WRITE)
		pg_mode |= INV_WRITE;

	pconn = get_pconn (priv->cnc);
	priv->fd = lo_open (pconn, priv->blobid, pg_mode);
	if (priv->fd < 0) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint 
gda_postgres_blob_read (GdaBlob *blob, gpointer buf, gint size, 
			gint *bytes_read)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);
	g_return_val_if_fail (bytes_read != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	pconn = get_pconn (priv->cnc);
	*bytes_read = lo_read (pconn, priv->fd, (gchar *) buf, size);
	if (*bytes_read == -1) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint 
gda_postgres_blob_write (GdaBlob *blob, gpointer buf, gint size,
			gint *bytes_written)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);
	g_return_val_if_fail (bytes_written != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);
	pconn = get_pconn (priv->cnc);
	*bytes_written = lo_write (pconn, priv->fd, (gchar *) buf, size);
	if (*bytes_written == -1) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
		return -1;
	}

	return 0;
}

static gint 
gda_postgres_blob_lseek (GdaBlob *blob, gint offset, gint whence)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	g_return_val_if_fail (priv->fd >= 0, -1);

	pconn = get_pconn (priv->cnc);
	result = lo_lseek (pconn, priv->fd, offset, whence);
	if (result == -1) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
	}

	return result;
}

static gint 
gda_postgres_blob_close (GdaBlob *blob)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	g_return_val_if_fail (priv->fd >= 0, -1);

	pconn = get_pconn (priv->cnc);
	result = lo_close (pconn, priv->fd);
	if (result < 0) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
	}

	return (result >= 0) ? 0 : -1;
}

static gint 
gda_postgres_blob_remove (GdaBlob *blob)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;
	gint result;

	g_return_val_if_fail (blob != NULL, -1);
	g_return_val_if_fail (blob->priv_data != NULL, -1);

	priv = blob->priv_data;
	g_return_val_if_fail (GDA_IS_CONNECTION (priv->cnc), -1);

	pconn = get_pconn (priv->cnc);
	result = lo_unlink (pconn, priv->blobid);
	if (result < 0) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (priv->cnc, error);
	}

	return (result >= 0) ? 0 : -1;
}

static void
gda_postgres_blob_free_data (GdaBlob *blob)
{
	PostgresBlobPrivateData *priv;

	g_return_if_fail (blob != NULL);
	priv = blob->priv_data;
	blob->priv_data = NULL;

	g_free (priv);
}

static void
gda_postgres_blob_new (GdaBlob *blob)
{
	PostgresBlobPrivateData *priv = g_new0 (PostgresBlobPrivateData, 1);

	priv->blobid = -1;
	priv->mode = -1;
	priv->fd = -1;

	blob->priv_data = priv;
	blob->open = gda_postgres_blob_open;
	blob->read = gda_postgres_blob_read;
	blob->write = gda_postgres_blob_write;
	blob->lseek = gda_postgres_blob_lseek;
	blob->close = gda_postgres_blob_close;
	blob->remove = gda_postgres_blob_remove;
	blob->free_data = gda_postgres_blob_free_data;
}

gboolean
gda_postgres_blob_create (GdaBlob *blob, GdaConnection *cnc)
{
	PostgresBlobPrivateData *priv;
	PGconn *pconn;
	gint oid;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	gda_postgres_blob_new (blob);
	priv = (PostgresBlobPrivateData *) blob->priv_data;

	pconn = get_pconn (cnc);
	oid = lo_creat (pconn, INV_READ | INV_WRITE);
	if (oid == 0) {
		GdaError *error = gda_postgres_make_error (pconn, NULL);
		gda_connection_add_error (cnc, error);
		return FALSE;
	}

	priv->blobid = oid;
	priv->cnc = cnc;
	return TRUE;
}

static void
gda_postgres_blob_from_id (GdaBlob *blob, gint value)
{
	PostgresBlobPrivateData *priv;

	gda_postgres_blob_new (blob);

	priv = (PostgresBlobPrivateData *) blob->priv_data;
	priv->blobid = value;
}

void
gda_postgres_blob_set_connection (GdaBlob *blob, GdaConnection *cnc)
{
	PostgresBlobPrivateData *priv;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	priv = (PostgresBlobPrivateData *) blob->priv_data;
	priv->cnc = cnc;
}

void
gda_postgres_set_value (GdaValue *value,
			GdaValueType type,
			const gchar *thevalue,
			gboolean isNull,
			gint length)
{
	GDate *gdate;
	GdaDate date;
	GdaTime timegda;
	GdaTimestamp timestamp;
	GdaGeometricPoint point;
	GdaNumeric numeric;
	GdaBlob blob;
	/* guchar *unescaped; See comment below on BINARY */

	if (isNull){
		gda_value_set_null (value);
		return;
	}

	switch (type) {
	case GDA_VALUE_TYPE_BOOLEAN :
		gda_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
		break;
	case GDA_VALUE_TYPE_STRING :
		gda_value_set_string (value, thevalue);
		break;
	case GDA_VALUE_TYPE_BIGINT :
		gda_value_set_bigint (value, atoll (thevalue));
		break;
	case GDA_VALUE_TYPE_INTEGER :
		gda_value_set_integer (value, atol (thevalue));
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		gda_value_set_smallint (value, atoi (thevalue));
		break;
	case GDA_VALUE_TYPE_SINGLE :
		gda_value_set_single (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		gda_value_set_double (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		break;
	case GDA_VALUE_TYPE_DATE :
		gdate = g_date_new ();
		g_date_set_parse (gdate, thevalue);
		if (!g_date_valid (gdate)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", thevalue);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		date.day = g_date_get_day (gdate);
		date.month = g_date_get_month (gdate);
		date.year = g_date_get_year (gdate);
		gda_value_set_date (value, &date);
		g_date_free (gdate);
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		make_timestamp (&timestamp, thevalue);
		gda_value_set_timestamp (value, &timestamp);
		break;
	case GDA_VALUE_TYPE_TIME :
		make_time (&timegda, thevalue);
		gda_value_set_time (value, &timegda);
		break;
	case GDA_VALUE_TYPE_BINARY :
		gda_value_set_binary (value, thevalue, length);
		/*
		 * No PQescapeBytea in 7.3??
		unescaped = PQunescapeBytea (thevalue, &length);
		if (unescaped != NULL) {
			gda_value_set_binary (value, unescaped, length);
			free (unescaped);
		} else {
			g_warning ("Error unescaping string: %s\n", thevalue);
			gda_value_set_string (value, thevalue);
		}
		*/
		break;
	case GDA_VALUE_TYPE_BLOB :
		gda_postgres_blob_from_id (&blob, atoi (thevalue));
		gda_value_set_blob (value, &blob);
 		break;
	default :
		gda_value_set_string (value, thevalue);
	}
}

gchar *
gda_postgres_value_to_sql_string (GdaValue *value)
{
	gchar *val_str;
	gchar *ret;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	switch (value->type) {
	case GDA_VALUE_TYPE_BIGINT :
	case GDA_VALUE_TYPE_DOUBLE :
	case GDA_VALUE_TYPE_INTEGER :
	case GDA_VALUE_TYPE_NUMERIC :
	case GDA_VALUE_TYPE_SINGLE :
	case GDA_VALUE_TYPE_SMALLINT :
	case GDA_VALUE_TYPE_TINYINT :
		ret = g_strdup (val_str);
		break;
	default :
		ret = g_strdup_printf ("\'%s\'", val_str);
	}

	g_free (val_str);

	return ret;
}
