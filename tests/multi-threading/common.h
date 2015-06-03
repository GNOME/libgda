/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef _COMMON_H_
#define _COMMON_H_

gboolean create_sqlite_db (const gchar *dir, const gchar *dbname, const gchar *sqlfile, GError **error);
GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
GdaDataModel *run_sql_select_cursor (GdaConnection *cnc, const gchar *sql);
gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);
gboolean data_models_equal (GdaDataModel *m1, GdaDataModel *m2);

#endif
