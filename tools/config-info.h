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

#ifndef __GDA_CONFIG_INFO__
#define __GDA_CONFIG_INFO__

#include <libgda/libgda.h>

gchar        *config_info_compute_dict_directory (void);
gchar        *config_info_compute_dict_file_name (GdaDsnInfo *info, const gchar *cnc_string);
void          config_info_update_meta_store_properties (GdaMetaStore *mstore, GdaConnection *rel_cnc);

GdaDataModel *config_info_list_all_dsn (void);
GdaDataModel *config_info_list_all_providers (void);
GdaDataModel *config_info_detail_provider (const gchar *provider, GError **error);
GdaDataModel *config_info_list_data_files (GError **error);
gchar        *config_info_purge_data_files (const gchar *criteria, GError **error);

#endif
