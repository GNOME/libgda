/* 
 * Copyright (C) 2007 - 2009 The GNOME Foundation.
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

/* must be included AS-IS in .c file */

static void
compute_dict_file_name_foreach_cb (const gchar *key, G_GNUC_UNUSED const gchar *value, GSList **list)
{
	if (!*list) 
		*list = g_slist_prepend (NULL, (gpointer) key);
	else
		*list = g_slist_insert_sorted (*list, (gpointer) key, (GCompareFunc) strcmp);
}

static gchar *
compute_dict_file_name (GdaDsnInfo *info, const gchar *cnc_string)
{
	gchar *filename = NULL;
	gchar *confdir;

	confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "libgda", NULL);
	if (!g_file_test (confdir, G_FILE_TEST_EXISTS)) {
		g_free (confdir);
		confdir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".libgda", NULL);
	}

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
