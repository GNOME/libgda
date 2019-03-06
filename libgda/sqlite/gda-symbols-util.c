/*
 * Copyright (C) 2010 Robert Ancell <robert.ancell@canonical.com>
 * Copyright (C) 2010 - 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gda-symbols-util.h"

extern Sqlite3ApiRoutines *s3r;



static GModule *
find_sqlite_in_dir (const gchar *dir_name, const gchar *name_part)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;
	GModule *handle = NULL;
	
#ifdef GDA_DEBUG_NO
	g_print ("Looking for '%s' in %s\n", name_part, dir_name);
#endif
	dir = g_dir_open (dir_name, 0, &err);
	if (err) {
		gda_log_error (err->message);
		g_error_free (err);
		return FALSE;
	}
	
	while ((name = g_dir_read_name (dir))) {
		const gchar *ptr1, *ptr2;
		ptr1 = g_strrstr (name, "." G_MODULE_SUFFIX);
		if (! ptr1)
			continue;
		ptr2 = g_strrstr (name, name_part);
		if (!ptr2)
			continue;
		if (ptr1 < ptr2)
			continue;
		
		gchar *path;
		
		path = g_build_path (G_DIR_SEPARATOR_S, dir_name, name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
		g_free (path);
		if (!handle) {
			/*g_warning (_("Error: %s"), g_module_error ());*/
			continue;
		}

		gpointer func;
		if (g_module_symbol (handle, "sqlite3_open", &func)) {
#ifdef GDA_DEBUG_NO
			path = g_build_path (G_DIR_SEPARATOR_S, dir_name, name, NULL);
			g_print ("'%s' found as: '%s'\n", name_part, path);
			g_free (path);
#endif
			break;
		}
		else {
			g_module_close (handle);
			handle = NULL;
		}

	}
	/* free memory */
	g_dir_close (dir);
	return handle;
}

GModule *
find_sqlite_library (const gchar *name_part)
{
	GModule *handle = NULL;
	const gchar *env;

	/* first use the compile time SEARCH_LIB_PATH */
	if (SEARCH_LIB_PATH) {
		gchar **array;
		gint i;
		array = g_strsplit (SEARCH_LIB_PATH, ":", 0);
		for (i = 0; array[i]; i++) {
			handle = find_sqlite_in_dir (array [i], name_part);
			if (handle)
				break;
		}
		g_strfreev (array);
		if (handle)
			return handle;
	}

	/* then try by 'normal' shared library finding */
	handle = g_module_open (name_part, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (handle) {
		gpointer func;
		if (g_module_symbol (handle, "sqlite3_open", &func)) {
#ifdef GDA_DEBUG_NO
			g_print ("'%s' found using default shared library finding\n", name_part);
#endif
			return handle;
		}

		g_module_close (handle);
		handle = NULL;
	}

	/* lastly, use LD_LIBRARY_PATH */
	env = g_getenv ("LD_LIBRARY_PATH");
	if (env) {
		gchar **array;
		gint i;
		array = g_strsplit (env, ":", 0);
		for (i = 0; array[i]; i++) {
			handle = find_sqlite_in_dir (array [i], name_part);
			if (handle)
				break;
		}
		g_strfreev (array);
		if (handle)
			return handle;
	}

	return NULL;
}

void
load_symbols (GModule *module, Sqlite3ApiRoutines** apilib)
{
	g_assert (module);
	(*apilib) = g_new (Sqlite3ApiRoutines, 1);

	if (! g_module_symbol (module, "sqlite3_bind_blob", (gpointer*) &((*apilib)->sqlite3_bind_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_double", (gpointer*) &((*apilib)->sqlite3_bind_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_int", (gpointer*) &((*apilib)->sqlite3_bind_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_int64", (gpointer*) &((*apilib)->sqlite3_bind_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_null", (gpointer*) &((*apilib)->sqlite3_bind_null)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_text", (gpointer*) &((*apilib)->sqlite3_bind_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_zeroblob", (gpointer*) &((*apilib)->sqlite3_bind_zeroblob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_bytes", (gpointer*) &((*apilib)->sqlite3_blob_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_close", (gpointer*) &((*apilib)->sqlite3_blob_close)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_open", (gpointer*) &((*apilib)->sqlite3_blob_open)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_read", (gpointer*) &((*apilib)->sqlite3_blob_read)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_write", (gpointer*) &((*apilib)->sqlite3_blob_write)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_busy_timeout", (gpointer*) &((*apilib)->sqlite3_busy_timeout)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_changes", (gpointer*) &((*apilib)->sqlite3_changes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_clear_bindings", (gpointer*) &((*apilib)->sqlite3_clear_bindings)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_close", (gpointer*) &((*apilib)->sqlite3_close)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_close_v2", (gpointer*) &((*apilib)->sqlite3_close_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_blob", (gpointer*) &((*apilib)->sqlite3_column_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_bytes", (gpointer*) &((*apilib)->sqlite3_column_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_count", (gpointer*) &((*apilib)->sqlite3_column_count)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_database_name", (gpointer*) &((*apilib)->sqlite3_column_database_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_decltype", (gpointer*) &((*apilib)->sqlite3_column_decltype)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_double", (gpointer*) &((*apilib)->sqlite3_column_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_int", (gpointer*) &((*apilib)->sqlite3_column_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_int64", (gpointer*) &((*apilib)->sqlite3_column_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_name", (gpointer*) &((*apilib)->sqlite3_column_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_origin_name", (gpointer*) &((*apilib)->sqlite3_column_origin_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_table_name", (gpointer*) &((*apilib)->sqlite3_column_table_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_text", (gpointer*) &((*apilib)->sqlite3_column_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_type", (gpointer*) &((*apilib)->sqlite3_column_type)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_config", (gpointer*) &((*apilib)->sqlite3_config)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_create_function", (gpointer*) &((*apilib)->sqlite3_create_function)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_create_module", (gpointer*) &((*apilib)->sqlite3_create_module)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_db_handle", (gpointer*) &((*apilib)->sqlite3_db_handle)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_declare_vtab", (gpointer*) &((*apilib)->sqlite3_declare_vtab)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_errcode", (gpointer*) &((*apilib)->sqlite3_errcode)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_errmsg", (gpointer*) &((*apilib)->sqlite3_errmsg)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_exec", (gpointer*) &((*apilib)->sqlite3_exec)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_extended_result_codes", (gpointer*) &((*apilib)->sqlite3_extended_result_codes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_finalize", (gpointer*) &((*apilib)->sqlite3_finalize)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_free", (gpointer*) &((*apilib)->sqlite3_free)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_free_table", (gpointer*) &((*apilib)->sqlite3_free_table)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_get_table", (gpointer*) &((*apilib)->sqlite3_get_table)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_last_insert_rowid", (gpointer*) &((*apilib)->sqlite3_last_insert_rowid)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_malloc", (gpointer*) &((*apilib)->sqlite3_malloc)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_mprintf", (gpointer*) &((*apilib)->sqlite3_mprintf)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_open", (gpointer*) &((*apilib)->sqlite3_open)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_open_v2", (gpointer*) &((*apilib)->sqlite3_open_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_prepare", (gpointer*) &((*apilib)->sqlite3_prepare)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_prepare_v2", (gpointer*) &((*apilib)->sqlite3_prepare_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_reset", (gpointer*) &((*apilib)->sqlite3_reset)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_blob", (gpointer*) &((*apilib)->sqlite3_result_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_double", (gpointer*) &((*apilib)->sqlite3_result_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_error", (gpointer*) &((*apilib)->sqlite3_result_error)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_int", (gpointer*) &((*apilib)->sqlite3_result_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_int64", (gpointer*) &((*apilib)->sqlite3_result_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_null", (gpointer*) &((*apilib)->sqlite3_result_null)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_text", (gpointer*) &((*apilib)->sqlite3_result_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_step", (gpointer*) &((*apilib)->sqlite3_step)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_table_column_metadata", (gpointer*) &((*apilib)->sqlite3_table_column_metadata)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_threadsafe", (gpointer*) &((*apilib)->sqlite3_threadsafe)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_blob", (gpointer*) &((*apilib)->sqlite3_value_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_bytes", (gpointer*) &((*apilib)->sqlite3_value_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_int", (gpointer*) &((*apilib)->sqlite3_value_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_int64", (gpointer*) &((*apilib)->sqlite3_value_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_double", (gpointer*) &((*apilib)->sqlite3_value_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_text", (gpointer*) &((*apilib)->sqlite3_value_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_type", (gpointer*) &((*apilib)->sqlite3_value_type)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_key", (gpointer*) &((*apilib)->sqlite3_key)))
		(*apilib)->sqlite3_key = NULL;
	if (! g_module_symbol (module, "sqlite3_key_v2", (gpointer*) &((*apilib)->sqlite3_key_v2)))
		(*apilib)->sqlite3_key_v2 = NULL;
	if (! g_module_symbol (module, "sqlite3_rekey", (gpointer*) &((*apilib)->sqlite3_key)))
		(*apilib)->sqlite3_rekey = NULL;

	if (! g_module_symbol (module, "sqlite3_create_collation", (gpointer*) &((*apilib)->sqlite3_create_collation)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_enable_load_extension", (gpointer*) &((*apilib)->sqlite3_enable_load_extension)))
		(*apilib)->sqlite3_enable_load_extension = NULL;
	return;

 onerror:
	g_free ((*apilib));
	apilib = NULL;
	g_module_close (module);	
}

