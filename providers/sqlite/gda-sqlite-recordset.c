/* GDA SQLite provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <bonobo/bonobo-i18n.h>
#include "gda-sqlite.h"

#define OBJECT_DATA_RECSET_HANDLE "GDA_Sqlite_RecsetHandle"

/*
 * Private functions
 */

static void
free_srecset (gpointer data)
{
	SQLITE_Recordset *srecset = (SQLITE_Recordset *) data;

	g_return_if_fail (srecset != NULL);
	sqlite_free_table(srecset->data);
	g_free (srecset);
}

static GdaRow *
fetch_func (GdaServerRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gulong rowpos;
	gint row_count;
	gint i;
	SQLITE_Recordset *drecset;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	drecset = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!drecset) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid SQLITE handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = drecset->nrows;
	field_count = drecset->ncols;

	if (rownum < 0 || rownum >= row_count) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Row number out of range"));
		return NULL;
	}

	/* Jump the field names */
	rowpos = (field_count - 1) + (rownum * field_count);
	
	row = gda_row_new (field_count);

	for (i = 0; i < field_count; i++) {
		GdaField *field;

		field = gda_row_get_field (row, i);
		gda_field_set_actual_size (field, strlen (drecset->data[rowpos +i]));
		gda_field_set_defined_size (field, strlen (drecset->data[rowpos +i]));
		gda_field_set_name (field, drecset->data[i]);
		gda_field_set_scale (field, 0);
		gda_field_set_gdatype (field, GDA_TYPE_STRING);

		/* FIXME: We should look for a way to know the real type data */
		gda_field_set_string_value (field, drecset->data[rowpos +i]);
	}

	return row;
}

static GdaRowAttributes *
describe_func (GdaServerRecordset *recset)
{
	SQLITE_Recordset *drecset;
	gint field_count;
	gint i;
	GdaRowAttributes *attrs;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	drecset = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!drecset) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid Sqlite handle"));
		return NULL;
	}

	/* create the GdaRowAttributes to be returned */
	field_count = drecset->ncols;
	attrs = gda_row_attributes_new (field_count);
	
	for (i = 0; i < field_count; i++) {
		GdaFieldAttributes *field_attrs;

		field_attrs = gda_row_attributes_get_field (attrs, i);
		gda_field_attributes_set_name (field_attrs, drecset->data[i]);
		gda_field_attributes_set_defined_size (field_attrs,
						       strlen (drecset->data[i]));
		gda_field_attributes_set_scale (field_attrs, 0);
		gda_field_attributes_set_gdatype (field_attrs,
						  GDA_TYPE_STRING);
	}

	return attrs;
}

/*
 * Public functions
 */

GdaServerRecordset *
gda_sqlite_recordset_new (GdaServerConnection *cnc, SQLITE_Recordset *srecset)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (srecset != NULL, NULL);

	if (srecset->data) {
		recset = gda_server_recordset_new (cnc, fetch_func,
						   describe_func);
		g_object_set_data_full (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE,
					srecset, (GDestroyNotify) free_srecset);
		
		return recset;
	}

	return NULL;
}
