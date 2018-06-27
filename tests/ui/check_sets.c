/*
 * Copyright (C) 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2012 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

static gboolean test_iter (GdaConnection *cnc);

int 
main (int argc, char** argv)
{
	int number_failed = 0;

	gtk_init (&argc, &argv);
	gdaui_init ();
	cnc = ui_tests_common_open_connection ();

	if (!test_iter (cnc))
		number_failed++;
	
	if (number_failed == 0)
                g_print ("Ok.\n");
        else
                g_print ("%d failed\n", number_failed);
        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
dump_iter (GdaDataModelIter *iter)
{
	GdaSet *set;
	GSList *list;
	set = GDA_SET (iter);
	g_print ("Dump of GdaDataModelIter %p, @row %d\n", set, gda_data_model_iter_get_row (iter));
	for (list = gda_set_get_holders (set); list; list = list->next) {
		GdaHolder *h = GDA_HOLDER (list->data);
		gchar *str;
		const GValue *cvalue;
		cvalue = gda_holder_get_value (h);
		str = gda_value_stringify (cvalue);
		g_print ("   [%s] type: %s value:[%s]\n", gda_holder_get_id (h),
			 gda_g_type_to_string (gda_holder_get_g_type (h)), str);
		g_free (str);
	}
}

static gboolean
compare_iter (GdaDataModelIter *iter, gint exp_row, const gchar **col_ids, const gchar **col_types,
	      const gchar **col_values)
{
	
	GdaSet *set;
	GSList *list;
	gint i;
	set = GDA_SET (iter);

	if (gda_data_model_iter_get_row (iter) != exp_row) {
		g_print ("Iter is at wrong row: got %d and expected %d\n",
			 gda_data_model_iter_get_row (iter), exp_row);
		return FALSE;
	}

	if (!col_ids)
		return TRUE;

	for (i = 0, list = gda_set_get_holders (set);
	     col_ids[i] && list;
	     i++, list = list->next) {
		GdaHolder *h = GDA_HOLDER (list->data);
		gchar *str;
		const GValue *cvalue;
		if (strcmp (col_ids[i], gda_holder_get_id (h))) {
			g_print ("Wrong column %d ID: got [%s] and expected [%s]\n", i,
				 gda_holder_get_id (h), col_ids[i]);
			return FALSE;
		}

		if (strcmp (col_types[i], gda_g_type_to_string (gda_holder_get_g_type (h)))) {
			g_print ("Wrong column %d type: got [%s] and expected [%s]\n", i,
				 gda_g_type_to_string (gda_holder_get_g_type (h)),
				 col_types[i]);
			return FALSE;
		}

		cvalue = gda_holder_get_value (h);
		str = gda_value_stringify (cvalue);
		if (strcmp (col_values[i], str)) {
			g_print ("Wrong column %d value: got [%s] and expected [%s]\n", i,
				 str, col_values[i]);
			g_free (str);
			return FALSE;
		}
		g_free (str);
	}

	if (col_ids[i]) {
		g_print ("Missing at least the [%s] column %d\n", col_ids[i], i);
		return FALSE;
	}

	if (list) {
		GdaHolder *h = GDA_HOLDER (list->data);
		g_print ("Too much columns, at least [%s] column %d\n",
			 gda_holder_get_id (h), i);
		return FALSE;
	}

	return TRUE;
}

static gboolean
test_iter (GdaConnection *cnc)
{
	gboolean retval = FALSE;
	GdaDataModel *model1, *model2;
	GdaDataModelIter *iter;

	model1 = ui_tests_common_get_products_2cols (cnc, TRUE);
	model2 = ui_tests_common_get_products (cnc, FALSE);
	iter = gda_data_model_create_iter (model1);

	const gchar *col_ids[] = {"ref", "category", NULL};
	const gchar *col_types[] = {"string", "int", NULL};
	const gchar *col_values[] = {"1", "14", NULL};

	g_assert (gda_data_model_iter_move_next (iter));
	dump_iter (iter);
	if (!compare_iter (iter, 0, col_ids, col_types, col_values)) {
		g_print ("Error, step1\n");
		goto out;
	}

	g_object_set (iter, "data-model", model2, NULL);
	dump_iter (iter);
	if (!compare_iter (iter, -1, NULL, NULL, NULL)) {
		g_print ("Error, step2\n");
		goto out;
	}

	const gchar *col_ids3[] = {"ref", "category", "name", "price", "wh_stored", NULL};
	const gchar *col_types3[] = {"string", "int", "string", "gdouble", "int", NULL};
	const gchar *col_values3[] = {"1", "14", "ACADEMY ACADEMY", "25.990000", "0", NULL};

	g_assert (gda_data_model_iter_move_next (iter));
	dump_iter (iter);
	if (!compare_iter (iter, 0, col_ids3, col_types3, col_values3)) {
		g_print ("Error, step3\n");
		goto out;
	}

	g_object_set (iter, "data-model", model1, NULL);
	dump_iter (iter);
	if (!compare_iter (iter, -1, NULL, NULL, NULL)) {
		g_print ("Error, step2\n");
		goto out;
	}
	g_assert (gda_data_model_iter_move_next (iter));
	dump_iter (iter);
	if (!compare_iter (iter, 0, col_ids, col_types, col_values)) {
		g_print ("Error, step3\n");
		goto out;
	}
	

	retval = TRUE;

 out:
	g_object_unref (model1);
	g_object_unref (model2);
	g_object_unref (iter);

	return retval;
}
