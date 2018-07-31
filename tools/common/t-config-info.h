/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __T_CONFIG_INFO__
#define __T_CONFIG_INFO__

#include <libgda/libgda.h>

void          t_config_info_modify_argv (char *argvi);

GFile        *t_config_info_compute_dict_directory (void);
GFile        *t_config_info_compute_dict_file_name (GdaDsnInfo *info, const gchar *cnc_string);
void          t_config_info_update_meta_store_properties (GdaMetaStore *mstore, GdaConnection *rel_cnc);

GdaDataModel *t_config_info_list_all_dsn (void);
GdaDataModel *t_config_info_list_all_providers (void);
GdaDataModel *t_config_info_detail_provider (const gchar *provider, GError **error);
GdaDataModel *t_config_info_detail_dsn (const gchar *dsn, GError **error);
GdaDataModel *t_config_info_list_data_files (GError **error);
gchar        *t_config_info_purge_data_files (const gchar *criteria, GError **error);

#endif
