/* GDA MDB provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include "gda-mdb-table.h"

GdaTable *
gda_mdb_table_new (GdaMdbConnection *mdb_cnc, const gchar *name)
{
	gint i;
	MdbCatalogEntry *entry;
	MdbTableDef *mdb_table;
	MdbColumn *mdb_col;
	GdaFieldAttributes *fa;
	GdaTable *table = NULL;

	g_return_val_if_fail (mdb_cnc != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (i = 0; i < mdb_cnc->mdb->num_catalog; i++) {
		entry = g_ptr_array_index (mdb_cnc->mdb->catalog, i);
		if (entry->object_type == MDB_TABLE
		    && !strcmp (entry->object_type, name))
			break;
		else
			entry = NULL;
	}

	if (!entry) {
		gda_connection_add_error_string (mdb_cnc->cnc, _("Table %s not found"), name);
		return NULL;
	}

	mdb_table = mdb_read_table (entry);
	mdb_read_columns (mdb_table);
	mdb_rewind_table (mdb_table);

	/* create the table and its fields */
	table = gda_table_new (name);
	for (i = 0; i < mdb_table->num_cols; i++) {
		mdb_col = g_ptr_array_index (mdb_table->columns, i);

		fa = gda_field_attributes_new ();
		gda_field_attributes_set_name (fa, mdb_col->name);
		gda_field_attributes_set_gdatype (fa, gda_mdb_type_to_gda (mdb_col->col_type));
		gda_field_attributes_set_defined_size (fa, mdb_col->col_size);

		gda_table_add_field (table, (const GdaFieldAttributes *) fa);

		gda_field_attributes_free (fa);
	}

	return table;
}
