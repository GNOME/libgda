/* GDA DB Postgres provider
 * Copyright (C) 1998-2001 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include <bonobo/bonobo-i18n.h>
#include "gda-postgres-recordset.h"

#define OBJECT_DATA_RECSET_HANDLE "GDA_Mysql_RecsetHandle"

/*
 * Private functions
 */

static void
free_postgres_res (gpointer data)
{
	PGresult *pg_res = (PGresult *) data;

	g_return_if_fail (pg_res != NULL);
	PQclear (pg_res);
}

static GdaRow *
fetch_func (GdaServerRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	PGresult *pg_res;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	pg_res = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!pg_res) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = PQntuples (pg_res);
	field_count = PQnfields (pg_res);

	if (rownum < 0 || rownum >= row_count) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Row number out of range"));
		return NULL;
	}

	row = gda_row_new (field_count);

	for (i = 0; i < field_count; i++) {
		GdaField *field;
		gchar *thevalue;
		GdaType ftype;

		field = gda_row_get_field (row, i);
		//TODO: What do I do when PQfsize() returns -1 to show that the
		//field is of variable length?
		//TODO: actual_size == defined_size!!!
		//TODO: What do I do with NULLs?
		//TODO: What do I do with BLOBs?
		//TODO: PQfsize() returns the length of the attribute in the server, not
		//the same length as PQgetlength(), which contains the length of the 
		//string returned by PQgetvalue()!!! So PQfsize may be < PQgetlength!!!
		gda_field_set_actual_size (field, PQgetlength (pg_res, rownum, i));
		gda_field_set_defined_size (field, PQfsize (pg_res, i));
		gda_field_set_name (field, PQfname (pg_res, i));
		//TODO: Don't know what the scale is!
		//gda_field_set_scale (field, mysql_fields[i].decimals);
		gda_field_set_scale (field, 0);
		ftype = gda_postgres_type_to_gda (PQftype (pg_res, i));
		thevalue = PQgetvalue(pg_res, rownum, i);
		gda_postgres_set_type_value(field, ftype, thevalue);
	}

	return row;
}

static GdaRowAttributes *
describe_func (GdaServerRecordset *recset)
{
	PGresult *pg_res;
	gint field_count;
	gint i;
	GdaRowAttributes *attrs;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	pg_res = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!pg_res) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* create the GdaRowAttributes to be returned */
	field_count = PQnfields (pg_res);
	attrs = gda_row_attributes_new (field_count);
	
	for (i = 0; i < field_count; i++) {
		GdaFieldAttributes *field_attrs;

		field_attrs = gda_row_attributes_get_field (attrs, i);
		gda_field_attributes_set_name (field_attrs, PQfname (pg_res, i));
		//TODO: PQfsize() or PQgetlength()?
		gda_field_attributes_set_defined_size (field_attrs, PQfsize (pg_res, i));
		//TODO: What is the scale?
		gda_field_attributes_set_scale (field_attrs, 0);
		gda_field_attributes_set_gdatype (field_attrs,
						  gda_postgres_type_to_gda (PQftype (pg_res, i)));
	}

	return attrs;
}

/*
 * Public functions
 */

GdaServerRecordset *
gda_postgres_recordset_new (GdaServerConnection *cnc, PGresult *pg_res)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	recset = gda_server_recordset_new (cnc, fetch_func, describe_func);
	g_object_set_data_full (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE,
				pg_res, (GDestroyNotify) free_postgres_res);

	return recset;
}

