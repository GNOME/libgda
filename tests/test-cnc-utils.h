/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#ifndef __TEST_CNC_UTIL_H__
#define __TEST_CNC_UTIL_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

GdaConnection *test_cnc_setup_connection (const gchar *provider, const gchar *dbname, GError **error);
GdaConnection *test_cnc_open_connection (const gchar *provider, const gchar *dbname, GError **error);
gboolean       test_cnc_setup_db_structure (GdaConnection *cnc, const gchar *schema_file, GError **error);
gboolean       test_cnc_setup_db_contents (GdaConnection *cnc, const gchar *data_file, GError **error);
gboolean       test_cnc_clean_connection (GdaConnection *cnc, GError **error);

gboolean       test_cnc_load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *full_file, GError **error);

gchar         *test_prov_name_upcase (const gchar *prov_name);
gchar         *test_random_string(const gchar *prefix, gint ncharacters);

typedef struct {
    GdaQuarkList *quark_list;
    gchar *dbname;
} CreateDBObject;

void test_create_db_object_free (CreateDBObject *obj);
CreateDBObject *test_create_db_object_new ();

CreateDBObject * test_create_database (const gchar *prov_id);

#endif
