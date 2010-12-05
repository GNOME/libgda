/* 
 * Copyright (C) 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config-info.h"
#include <glib/gi18n-lib.h>

GdaDataModel *
config_info_list_all_dsn (void)
{
	return gda_config_list_dsn ();
}

GdaDataModel *
config_info_list_all_providers (void)
{
	GdaDataModel *prov_list, *model;
	gint i, nrows;

	prov_list = gda_config_list_providers ();
	
	model = gda_data_model_array_new_with_g_types (2,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Provider"));
	gda_data_model_set_column_title (model, 1, _("Description"));
	g_object_set_data (G_OBJECT (model), "name", _("Installed providers list"));

	nrows = gda_data_model_get_n_rows (prov_list);
	for (i =0; i < nrows; i++) {
		const GValue *value;
		GList *list = NULL;
		value = gda_data_model_get_value_at (prov_list, 0, i, NULL);
		if (!value)
			goto onerror;
		list = g_list_append (list, gda_value_copy (value));
		value = gda_data_model_get_value_at (prov_list, 1, i, NULL);
		if (!value)
			goto onerror;
		list = g_list_append (list, gda_value_copy (value));
		
		if (gda_data_model_append_values (model, list, NULL) == -1)
			goto onerror;
		
		g_list_foreach (list, (GFunc) gda_value_free, NULL);
		g_list_free (list);
	}
	g_object_unref (prov_list);
	return model;

 onerror:
	g_warning ("Could not obtain the list of database providers");
	g_object_unref (prov_list);
	g_object_unref (model);
	return NULL;
}

GdaDataModel *
config_info_detail_provider (const gchar *provider, GError **error)
{
	GdaDataModel *prov_list, *model = NULL;
	gint i, nrows;

	prov_list = gda_config_list_providers ();
	nrows = gda_data_model_get_n_rows (prov_list);

	for (i = 0; i < nrows; i++) {
		const GValue *value;
		value = gda_data_model_get_value_at (prov_list, 0, i, error);
		if (!value)
			goto onerror;
		
		if (!strcmp (g_value_get_string (value), provider)) {
			gint j;
			model = gda_data_model_array_new_with_g_types (2,
								       G_TYPE_STRING,
								       G_TYPE_STRING);
			gda_data_model_set_column_title (model, 0, _("Attribute"));
			gda_data_model_set_column_title (model, 1, _("Value"));
			g_object_set_data_full (G_OBJECT (model), "name", 
						g_strdup_printf (_("Provider '%s' description"), provider),
						g_free);
			
			for (j = 0; j < 5; j++) {
				GValue *tmpvalue;
				if (gda_data_model_append_row (model, error) == -1) 
					goto onerror;
				
				g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)),
						    gda_data_model_get_column_title (prov_list, j));
				if (! gda_data_model_set_value_at (model, 0, j, tmpvalue, error))
					goto onerror;
				gda_value_free (tmpvalue);
				
				value = gda_data_model_get_value_at (prov_list, j, i, error);
				if (!value ||
				    ! gda_data_model_set_value_at (model, 1, j, value, error))
					goto onerror;
			}
			g_object_unref (prov_list);
			return model;
		}
	}
	g_object_unref (prov_list);
	g_set_error (error, 0, 0,
		     _("Could not find any provider named '%s'"), provider);
	return model;
	
 onerror:
	g_object_unref (prov_list);
	return NULL;
}
