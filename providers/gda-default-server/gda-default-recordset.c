/* GDA Default Provider
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-default.h"

gboolean
gda_default_recordset_new (GdaServerRecordset *recset)
{
	DEFAULT_Recordset* default_recset;

	default_recset = g_new0(DEFAULT_Recordset, 1);
	gda_server_recordset_set_user_data(recset, (gpointer) default_recset);

	return TRUE;
}

static void
fill_field_values (GdaServerRecordset *recset, gulong pos)
{
	gint n;
	DEFAULT_Recordset *default_recset;
	gulong col_to_fetch;

	g_return_if_fail(recset != NULL);

	default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
	if (default_recset) {
		/* fill in the information about each field */
		for (n = 0; n < default_recset->number_of_cols; n++) {
			GList *node;
			GdaServerField *field;

			node = g_list_nth(gda_server_recordset_get_fields(recset), n);
			field = node != NULL ? (GdaServerField *) node->data : NULL;
			if (field) {
				/* all data are returned as strings */
				col_to_fetch = ((pos + 1) * default_recset->number_of_cols) + n;
				gda_server_field_set_varchar(field, default_recset->data[col_to_fetch]);
			}
		}

		default_recset->position = pos;
	}
}

gint
gda_default_recordset_move_next (GdaServerRecordset *recset)
{
	DEFAULT_Recordset *default_recset;

	g_return_val_if_fail(recset != NULL, -1);

	default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
	if (default_recset) {
		gint pos = default_recset->position + 1;

		if (pos < default_recset->number_of_rows) {
			fill_field_values(recset, pos);
			return 0;
		}
		else {
			gda_server_recordset_set_at_begin(recset, FALSE);
			gda_server_recordset_set_at_end(recset, TRUE);
			return 1;
		}
	}

	return -1;
}

gint
gda_default_recordset_move_prev (GdaServerRecordset *recset)
{
	DEFAULT_Recordset *default_recset;

	g_return_val_if_fail(recset != NULL, -1);

	default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
	if (default_recset) {
		gint pos = default_recset->position - 1;

		if (pos >= 0) {
			fill_field_values(recset, pos);
			return 0;
		}
		else {
			gda_server_recordset_set_at_begin(recset, TRUE);
			return 1;
		}
	}

	return -1;
}

gint
gda_default_recordset_close (GdaServerRecordset *recset)
{
	DEFAULT_Recordset *default_recset;

	g_return_if_fail(recset != NULL);

	default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
	if (default_recset) {
		if (default_recset->data) {
			sqlite_free_table(default_recset->data);
			default_recset->data = NULL;
		}
		default_recset->number_of_cols = 0;
		default_recset->number_of_rows = 0;
		default_recset->position = -1;
	}
}

void
gda_default_recordset_free (GdaServerRecordset *recset)
{
	DEFAULT_Recordset *default_recset;

	g_return_if_fail(recset != NULL);

	default_recset = (DEFAULT_Recordset *) gda_server_recordset_get_user_data(recset);
	if (default_recset) {
		if (default_recset->data)
			sqlite_free_table(default_recset->data);
		g_free((gpointer) default_recset);
		gda_server_recordset_set_user_data(recset, NULL);
	}
}


