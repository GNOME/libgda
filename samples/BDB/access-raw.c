/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include <libgda/gda-data-model-bdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int
main (int argc, char *argv [])
{
	GdaDataModel *model;
	gint i, nrows;
	GError *error = NULL;

	gda_init ();
	if (! g_file_test (DATABASE, G_FILE_TEST_EXISTS)) {
		g_print ("File '%s' does not exist\n", DATABASE);
		exit (1);
	}
	model = gda_data_model_bdb_new (DATABASE, NULL);
	gda_data_model_dump (model, stdout);

	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		Key *key;
		Value *value;
		const GValue *c_value;
		const GdaBinary *bin;
		
		g_print ("=============== ROW %d\n", i);

		c_value= gda_data_model_get_value_at (model, 0, i, &error);
		if (!c_value) {
			g_print ("Could not get value from data model: %s\n",
				 error && error->message ? error->message : "No detail");
			exit (1);
		}
		bin = gda_value_get_binary (c_value);
		key = (Key *)bin->data;
		g_print ("color/type = %s/%d\n", key->color, key->type);

		c_value= gda_data_model_get_value_at (model, 1, i, &error);
		if (!c_value) {
			g_print ("Could not get value from data model: %s\n",
				 error && error->message ? error->message : "No detail");
			exit (1);
		}
		bin = gda_value_get_binary (c_value);
		value = (Value *)bin->data;
		g_print ("size/name = %f/%s\n", value->size, value->name);
		
	}

	i = gda_data_model_append_row (model, &error);
	if (i < 0) {
		g_print ("Could not append row: %s\n", 
			 error && error->message ? error->message : "no detail");
		exit (1);
	}
	else {
		{
			/* EXTRA tests */
			gda_data_model_dump (model, stdout);
			if (!gda_data_model_remove_row (model, i, &error)) {
				g_print ("Could not remove row: %s\n", 
					 error && error->message ? error->message : "no detail");
				exit (1);
			}
			gda_data_model_dump (model, stdout);

			gchar *str = "AAA";
			GValue *value = gda_value_new_binary ((guchar*) str, 4);
			if (!gda_data_model_set_value_at (model, 1, 2, value, &error)) {
				g_print ("Could not set value: %s\n", 
					 error && error->message ? error->message : "no detail");
				exit (1);
			}
			gda_data_model_dump (model, stdout);

			exit (0);
		}

		GValue *value;
		GdaBinary bin;
		Key m_key;
		Value m_value;
		
		strncpy (m_key.color, "black", COLORSIZE);
		m_key.type = 100;
		
		bin.data = (gpointer) &m_key;
		bin.binary_length = sizeof (Key);
		value = gda_value_new (GDA_TYPE_BINARY);
		gda_value_set_binary (value, &bin);

		if (!gda_data_model_set_value_at (model, 0, i, value, &error)) {
			g_print ("Could not set key: %s\n", 
				 error && error->message ? error->message : "no detail");
			exit (1);
		}
		gda_value_free (value);

		m_value.size = 100.1;
		strncpy (m_value.name, "blackhole", NAMESIZE);
		
		bin.data = (gpointer) &m_value;
		bin.binary_length = sizeof (Value);
		value = gda_value_new (GDA_TYPE_BINARY);
		gda_value_set_binary (value, &bin);

		if (!gda_data_model_set_value_at (model, 1, i, value, &error)) {
			g_print ("Could not set key: %s\n", 
				 error && error->message ? error->message : "no detail");
			exit (1);
		}
		gda_value_free (value);
	}
	g_object_unref (model);
	return 0;
}
