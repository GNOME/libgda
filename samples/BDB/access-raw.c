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

	gda_init ("bdb test", "3.1.1", argc, argv);
	if (! g_file_test (DATABASE, G_FILE_TEST_EXISTS)) {
		g_print ("File '%s' done not exist\n", DATABASE);
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

		c_value= gda_data_model_get_value_at (model, 0, i);
		bin = gda_value_get_binary (c_value);
		key = (Key *)bin->data;
		g_print ("color/type = %s/%d\n", key->color, key->type);

		c_value= gda_data_model_get_value_at (model, 1, i);
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
