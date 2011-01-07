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

GdaDataModel *
config_info_list_data_files (GError **error)
{
	const gchar *name;
	gchar *confdir;
	GdaDataModel *model;
	GDir *dir;

	/* open directory */
	confdir = config_info_compute_dict_directory ();
	dir = g_dir_open (confdir, 0, error);
	if (!dir) {
		g_free (confdir);
		return NULL;
	}

	/* create model */
	model = gda_data_model_array_new (5);
	gda_data_model_set_column_name (model, 0, _("File name"));
	gda_data_model_set_column_name (model, 1, _("DSN"));
	gda_data_model_set_column_name (model, 2, _("Last used"));
	gda_data_model_set_column_name (model, 3, _("Provider"));
	gda_data_model_set_column_name (model, 4, _("Connection string"));

	while ((name = g_dir_read_name (dir))) {
		GValue *value;
		gint row;
		gchar *copy, *dsn, *fname;
		GdaDsnInfo *dsninfo;

		if (! g_str_has_suffix (name, ".db"))
			continue;
		if (! g_str_has_prefix (name, "gda-sql-"))
			continue;

		fname = g_build_filename (confdir, name, NULL);
		copy = g_strdup (name);

		row = gda_data_model_append_row (model, NULL);

		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), name);
		gda_data_model_set_value_at (model, 0, row, value, NULL);
		gda_value_free (value);

		dsn = copy + 8;
		dsn [strlen (dsn) - 3] = 0;
		dsninfo = gda_config_get_dsn_info (dsn);
		if (dsninfo) {
			g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), dsn);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);
		}

		/* Open the file */
		GdaMetaStore *store;
		gchar *attvalue;
		store = gda_meta_store_new_with_file (fname);
		if (gda_meta_store_get_attribute_value (store, "last-used", &attvalue, NULL)) {
			value = gda_value_new_from_string (attvalue, G_TYPE_DATE);
			g_free (attvalue);
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);
		}
		if (gda_meta_store_get_attribute_value (store, "cnc-provider", &attvalue, NULL)) {
			g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), attvalue);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}
		if (gda_meta_store_get_attribute_value (store, "cnc-string", &attvalue, NULL)) {
			g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), attvalue);
			gda_data_model_set_value_at (model, 4, row, value, NULL);
			gda_value_free (value);
		}
		g_object_unref (store);

		g_free (copy);
		g_free (fname);
	}
	
	g_free (confdir);
	g_dir_close (dir);

	return model;
}






gchar *
config_info_compute_dict_directory (void)
{
	gchar *confdir;

	confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "libgda", NULL);
	if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
		g_free (confdir);
		confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".libgda", NULL);
	}

	return confdir;
}

static void
compute_dict_file_name_foreach_cb (const gchar *key, G_GNUC_UNUSED const gchar *value, GSList **list)
{
	if (!*list) 
		*list = g_slist_prepend (NULL, (gpointer) key);
	else
		*list = g_slist_insert_sorted (*list, (gpointer) key, (GCompareFunc) strcmp);
}

gchar *
config_info_compute_dict_file_name (GdaDsnInfo *info, const gchar *cnc_string)
{
	gchar *filename = NULL;
	gchar *confdir;

	confdir = config_info_compute_dict_directory ();
	if (info) {		
		filename = g_strdup_printf ("%s%sgda-sql-%s.db", 
					    confdir, G_DIR_SEPARATOR_S,
					    info->name);
	}
	else {
#if GLIB_CHECK_VERSION(2,16,0)
		GdaQuarkList *ql;
		GSList *list, *sorted_list = NULL;
		GString *string = NULL;
		ql = gda_quark_list_new_from_string (cnc_string);

		gda_quark_list_foreach (ql, (GHFunc) compute_dict_file_name_foreach_cb, &sorted_list);
		for (list = sorted_list; list; list = list->next) {
			const gchar *value;
			gchar *evalue;

			if (!string)
				string = g_string_new ("");
			else
				g_string_append_c (string, ',');

			value = gda_quark_list_find (ql, (gchar *) list->data);
			evalue = gda_rfc1738_encode (value);
			g_string_append_printf (string, ",%s=%s", (gchar *) list->data, evalue);
			g_free (evalue);
		}
		gda_quark_list_free (ql);

		if (string) {
			gchar *chname;
			chname = g_compute_checksum_for_string (G_CHECKSUM_SHA1, string->str, -1);
			g_string_free (string, TRUE);
			filename = g_strdup_printf ("%s%sgda-sql-%s.db", 
					    confdir, G_DIR_SEPARATOR_S,
					    chname);
			g_free (chname);
		}
#endif
	}

	g_free (confdir);
	return filename;
}

void
config_info_update_meta_store_properties (GdaMetaStore *mstore, GdaConnection *rel_cnc)
{
	GDate *date;
	GValue *dvalue;
	gchar *tmp;
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	g_value_take_boxed ((dvalue = gda_value_new (G_TYPE_DATE)), date);
	tmp = gda_value_stringify (dvalue);
	gda_value_free (dvalue);
	gda_meta_store_set_attribute_value (mstore, "last-used", tmp, NULL);
	g_free (tmp);
	
	gda_meta_store_set_attribute_value (mstore, "cnc-string",
					    gda_connection_get_cnc_string (rel_cnc), NULL);
	gda_meta_store_set_attribute_value (mstore, "cnc-provider",
					    gda_connection_get_provider_name (rel_cnc), NULL);
}
