/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <libgda/libgda.h>
#include <libgda-ui/libgda-ui.h>
#include "common.h"

GdaConnection *cnc = NULL;

int 
main (int argc, char** argv)
{
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* open connection */
	cnc = ui_tests_common_open_connection ();

	GdauiRawGrid *grid;
	GdaDataModel *model;
	GdaDataModelIter *iter1, *iter2;
	GdaDataModel *gm1, *gm2;
	gint nrows1, nrows2;

	model = ui_tests_common_get_products (cnc, FALSE);
	nrows1 = gda_data_model_get_n_rows (model);
	grid = GDAUI_RAW_GRID (gdaui_raw_grid_new (model));
	g_object_unref (model);
	iter1 = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid));
	gm1 = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (grid));
	if (nrows1 != gda_data_model_get_n_rows (gm1)) {
		g_print ("Error: wrong number of rows\n");
		return 1;
	}

	model = ui_tests_common_get_products (cnc, TRUE);
	nrows2 = gda_data_model_get_n_rows (model);
	g_assert (nrows1 != nrows2);
	gdaui_data_selector_set_model (GDAUI_DATA_SELECTOR (grid), model);
	g_object_unref (model);
	iter2 = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid));
	gm2 = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (grid));
	if (nrows2 != gda_data_model_get_n_rows (gm2)) {
		g_print ("Error: wrong number of rows\n");
		return 1;
	}

	if (gm1 != gm2) {
		g_print ("Error: GdauiDataSelector's returned model changed when not expected\n");
		return 1;
	}


	if (nrows1 == gda_data_model_get_n_rows (gm2)) {
		g_print ("Error: data model not actually changed\n");
		return 1;
	}

	iter2 = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid));
	if (iter1 != iter2) {
		g_print ("Error: iterators are not the same!\n");
		return 1;
	}
	return 0;
}

