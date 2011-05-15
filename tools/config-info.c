/* 
 * Copyright (C) 2010 - 2011 The GNOME Foundation.
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
#include <glib/gstdio.h>

/*
 * Replace @argvi's contents with the connection name
 */
void
config_info_modify_argv (char *argvi)
{
	GString *string;
	gchar *prov, *cncparams, *user, *pass;
	size_t size;
	string = g_string_new ("");
	gda_connection_string_split (argvi, &cncparams, &prov, &user, &pass);
	g_free (user);
	g_free (pass);
	if (prov) {
		g_string_append (string, prov);
		g_free (prov);
	}
	if (cncparams) {
		g_string_append (string, cncparams);
		g_free (cncparams);
	}
	size = MIN (strlen (string->str), strlen (argvi));
	strncpy (argvi, string->str, size);
	g_string_free (string, TRUE);
	if (size < strlen (argvi))
		memset (argvi + size, 0, strlen (argvi) - size);
}

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
	GdaProviderInfo *pinfo;
	pinfo = gda_config_get_provider_info (provider);
	if (! pinfo) {
		g_set_error (error, 0, 0,
			     _("Could not find provider '%s'"), provider);
		return NULL;
	}

	GdaDataModel *model;
	GValue *tmpvalue = NULL;
	gint row;

	/* define data model */
	model = gda_data_model_array_new_with_g_types (2,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Attribute"));
	gda_data_model_set_column_title (model, 1, _("Value"));
	g_object_set_data_full (G_OBJECT (model), "name", 
				g_strdup_printf (_("Provider '%s' description"), provider),
				g_free);
	/* Provider name */
	row = 0;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Provider"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), pinfo->id);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	/* Provider's description */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Description"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), pinfo->description);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	/* DSN parameters */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("DSN parameters"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	if (pinfo->dsn_params) {
		GSList *list;
		GString *string = NULL;
		for (list = pinfo->dsn_params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			if (string) {
				g_string_append (string, ",\n");
				g_string_append (string, gda_holder_get_id (holder));
			}
			else
				string = g_string_new (gda_holder_get_id (holder));
			g_string_append (string, ": ");

			gchar *descr, *name;
			GType type;
			g_object_get (holder, "name", &name, "description", &descr,
				      "g-type", &type, NULL);
			if (name) {
				g_string_append (string, name); 
				g_free (name);
			}
			if (descr) {
				if (name)
					g_string_append (string, ". ");
				g_string_append (string, descr); 
				g_free (descr);
			}
			g_string_append_printf (string, " (%s)", gda_g_type_to_string (type));
		}
		if (string) {
			g_value_take_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), string->str);
			g_string_free (string, FALSE);
			if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
				goto onerror;
			gda_value_free (tmpvalue);
		}
	}

	/* Provider's authentication */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Authentication"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	if (pinfo->auth_params) {
		GSList *list;
		GString *string = NULL;
		for (list = pinfo->auth_params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			if (string) {
				g_string_append (string, ",\n");
				g_string_append (string, gda_holder_get_id (holder));
			}
			else
				string = g_string_new (gda_holder_get_id (holder));

			gchar *str;
			GType type;
			g_object_get (holder, "description", &str, "g-type", &type, NULL);
			if (str) {
				g_string_append_printf (string, ": %s", str); 
				g_free (str);
			}
			g_string_append_printf (string, " (%s)", gda_g_type_to_string (type)); 
		}
		if (string) {
			g_value_take_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), string->str);
			g_string_free (string, FALSE);
			if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
				goto onerror;
			gda_value_free (tmpvalue);
		}
	}

	/* Provider's DLL file */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("File"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), pinfo->location);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	return model;
 onerror:
	if (tmpvalue)
		gda_value_free (tmpvalue);
	g_object_unref (model);
	return NULL;
}

static void
ql_foreach_cb (const gchar *name, const gchar *value, GString *string)
{
	if (*string->str)
		g_string_append (string, "\n");
	g_string_append_printf (string, "%s: %s", name, value);
}

GdaDataModel *
config_info_detail_dsn (const gchar *dsn, GError **error)
{
	GdaDsnInfo *info = NULL;
	if (dsn && *dsn)
		info = gda_config_get_dsn_info (dsn);
	if (!info) {
		g_set_error (error, 0, 0,
			     _("Could not find data source '%s'"), dsn);
		return NULL;
	}

	GdaDataModel *model;
	GValue *tmpvalue = NULL;
	gint row;

	/* define data model */
	model = gda_data_model_array_new_with_g_types (2,
						       G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (model, 0, _("Attribute"));
	gda_data_model_set_column_title (model, 1, _("Value"));
	g_object_set_data_full (G_OBJECT (model), "name", 
				g_strdup_printf (_("DSN '%s' description"), dsn),
				g_free);
	/* DSN name */
	row = 0;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("DSN name"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), info->name);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	/* provider */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Provider"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), info->provider);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	/* description */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Description"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), info->description);
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	/* CNC prameters */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Parameters"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	if (info->cnc_string) {
		GString *string;
		GdaQuarkList *ql;
		string = g_string_new ("");
		ql = gda_quark_list_new_from_string (info->cnc_string);
		gda_quark_list_foreach (ql, (GHFunc) ql_foreach_cb, string);
		gda_quark_list_free (ql);

		g_value_take_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), string->str);
		g_string_free (string, FALSE);
		if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
			goto onerror;
		gda_value_free (tmpvalue);
	}

	/* authentication */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("Authentication"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	if (info->auth_string) {
		GString *string;
		GdaQuarkList *ql;
		string = g_string_new ("");
		ql = gda_quark_list_new_from_string (info->auth_string);
		gda_quark_list_foreach (ql, (GHFunc) ql_foreach_cb, string);
		gda_quark_list_free (ql);

		g_value_take_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), string->str);
		g_string_free (string, FALSE);
		if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
			goto onerror;
		gda_value_free (tmpvalue);
	}

	/* system wide? */
	row++;
	if (gda_data_model_append_row (model, error) == -1) 
		goto onerror;
	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)), _("System DSN?"));
	if (! gda_data_model_set_value_at (model, 0, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	g_value_set_string ((tmpvalue = gda_value_new (G_TYPE_STRING)),
			    info->is_system ? _("Yes") : _("No"));
	if (! gda_data_model_set_value_at (model, 1, row, tmpvalue, error))
		goto onerror;
	gda_value_free (tmpvalue);

	return model;
 onerror:
	if (tmpvalue)
		gda_value_free (tmpvalue);
	g_object_unref (model);
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
		else {
			gchar *ptr;
			gint i;
			for (i = 0, ptr = dsn; *ptr; ptr++, i++) {
				if (((*ptr < 'a') && (*ptr > 'z')) &&
				    ((*ptr < '0') && (*ptr > '9')))
					break;
			}
			if (*ptr || (i != 40)) {
				/* this used to be a DSN which has been removed */
				g_value_take_string ((value = gda_value_new (G_TYPE_STRING)),
						     g_strdup_printf (_("(%s)"), dsn));
				gda_data_model_set_value_at (model, 1, row, value, NULL);
				gda_value_free (value);
			}
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

typedef enum {
	PURGE_ALL,
	PURGE_NON_DSN,
	PURGE_NON_EXIST_DSN,
	
	PURGE_LIST_ONLY,

	PURGE_UNKNOWN
} PurgeCriteria;

static PurgeCriteria
parse_criteria (const gchar *criteria)
{
	if (!g_ascii_strcasecmp (criteria, "all"))
		return PURGE_ALL;
	else if (!g_ascii_strcasecmp (criteria, "non-dsn"))
		return PURGE_NON_DSN;
	else if (!g_ascii_strcasecmp (criteria, "non-exist-dsn"))
		return PURGE_NON_EXIST_DSN;
	else if (!g_ascii_strcasecmp (criteria, "list-only"))
		return PURGE_LIST_ONLY;
	else
		return PURGE_UNKNOWN;
}

gchar *
config_info_purge_data_files (const gchar *criteria, GError **error)
{
	PurgeCriteria cri = PURGE_UNKNOWN;
	GString *string = NULL;
	gchar **array;
	gint i;
	gboolean list_only = FALSE;

	array = g_strsplit_set (criteria, ",:", 0);
	for (i = 0; array[i]; i++) {
		PurgeCriteria tmpcri;
		tmpcri = parse_criteria (array[i]);
		if (tmpcri == PURGE_LIST_ONLY)
			list_only = TRUE;
		else if (tmpcri != PURGE_UNKNOWN)
			cri = tmpcri;
	}
	g_strfreev (array);
	if (cri == PURGE_UNKNOWN) {
		g_set_error (error, 0, 0, "Unknown criteria '%s'", criteria);
		return NULL;
	}

	const gchar *name;
	gchar *confdir;
	GDir *dir;
	GString *errstring = NULL;

	/* open directory */
	confdir = config_info_compute_dict_directory ();
	dir = g_dir_open (confdir, 0, error);
	if (!dir) {
		g_free (confdir);
		return NULL;
	}
	while ((name = g_dir_read_name (dir))) {
		gchar *copy, *dsn, *fname;
		gboolean to_remove = FALSE;
		
		if (! g_str_has_suffix (name, ".db"))
			continue;
		if (! g_str_has_prefix (name, "gda-sql-"))
			continue;

		fname = g_build_filename (confdir, name, NULL);
		copy = g_strdup (name);

		dsn = copy + 8;
		dsn [strlen (dsn) - 3] = 0;

		if (cri == PURGE_ALL)
			to_remove = TRUE;
		else if ((cri == PURGE_NON_DSN) || (cri == PURGE_NON_EXIST_DSN)) {
			GdaDsnInfo *dsninfo;
			dsninfo = gda_config_get_dsn_info (dsn);
			if (! dsninfo) {
				to_remove = TRUE;
				if (cri == PURGE_NON_DSN) {
					gchar *ptr;
					gint i;
					for (i = 0, ptr = dsn; *ptr; ptr++, i++) {
						if (((*ptr < 'a') && (*ptr > 'z')) &&
						    ((*ptr < '0') && (*ptr > '9')))
							break;
					}
					if (*ptr || (i != 40))
						/* this used to be a DSN which has been removed */
						to_remove = FALSE;
				}
			}
		}		
		
		if (to_remove) {
			if ((! list_only) && g_unlink (fname)) {
				if (! errstring)
					errstring = g_string_new ("");

				g_string_append_c (errstring, '\n');
				g_string_append (errstring, _("Failed to remove: "));
				g_string_append (errstring, fname);
			}
			else {
				if (! string)
					string = g_string_new (fname);
				else {
					g_string_append_c (string, '\n');
					g_string_append (string, fname);
				}
			}
		}
		
		g_free (copy);
		g_free (fname);
	}
	g_free (confdir);
	g_dir_close (dir);

	if (errstring) {
		g_set_error (error, 0, 0, "%s", errstring->str);
		g_string_free (errstring, TRUE);
	}

	if (string)
		return g_string_free (string, FALSE);
	else {
		if (!errstring)
			return g_strdup_printf (_("No file to purge!"));
		else
			return NULL;
	}
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
