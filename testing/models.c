/* GDA - Test suite
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-test.h"

static void
display_data_model (GdaDataModel *model)
{
	gint cols, rows;
	gint c, r;

	cols = gda_data_model_get_n_columns (model);
	rows = gda_data_model_get_n_rows (model);

	g_print (" Displaying data model %p, with %d columns and %d rows\n",
		 model, cols, rows);

	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++) {
			GdaValue *value;
			gchar *str;

			value = (GdaValue *) gda_data_model_get_value_at (model, c, r);
			str = gda_value_stringify (value);
			g_print ("\tColumn %d - Row %d: %s\n", c, r, str);
		}
	}
}

static void
model_changed_cb (GdaDataModel *model, gpointer user_data)
{
	g_print ("\tModel %p has changed\n", model);
	display_data_model (model);
}

/* Test a data model object */
static void
setup_data_model (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	display_data_model (model);
	g_signal_connect (G_OBJECT (model), "changed",
			  G_CALLBACK (model_changed_cb), NULL);
}

/* Test the list data model */
static void
test_list_model (void)
{
	GdaDataModelList *model;
	GList *sections;

	g_print (" Testing list model...\n");

	/* retrieve a string list */
	sections = gda_config_list_keys ("/desktop/gnome/interface");
	model = gda_data_model_list_new_from_string_list (sections);
	gda_config_free_list (sections);

	if (!GDA_IS_DATA_MODEL (model)) {
		g_print ("** ERROR: Could not create GdaDataModelList\n");
		gda_main_quit ();
	}

	setup_data_model (GDA_DATA_MODEL (model));
}

/* Tests the array data model */
static void
test_array_model (void)
{
	gint i;
	GdaDataModelArray *model;

	g_print (" Testing array model...\n");

	model = gda_data_model_array_new (6);
	if (!GDA_IS_DATA_MODEL_ARRAY (model)) {
		g_print ("** ERROR: Could not create GdaDataModelArray\n");
		gda_main_quit ();
	}

	for (i = 0; i < 50; i++) {
		GList *values = NULL;

		values = g_list_append (values, gda_value_new_string ("This is a string"));
		values = g_list_append (values, gda_value_new_integer (200));
		values = g_list_append (values, gda_value_new_boolean (FALSE));
		values = g_list_append (values, gda_value_new_single (3.1416));
		values = g_list_append (values, gda_value_new_string ("Another string"));
		values = g_list_append (values, gda_value_new_double (4560.45672113323093));

		gda_data_model_array_append_row (model, values);

		g_list_foreach (values, gda_value_free, NULL);
		g_list_free (values);
	}

	setup_data_model (GDA_DATA_MODEL (model));
}

/* Main entry point for the data models test suite */
void
test_models (void)
{
	DISPLAY_MESSAGE ("Testing data models");
	test_list_model ();
	test_array_model ();
}
