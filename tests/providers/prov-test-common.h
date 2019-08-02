/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#ifndef __PROV_TEST_COMMON_H__
#define __PROV_TEST_COMMON_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>
#include "prov-test-util.h"

int prov_test_common_setup (void);
GdaConnection *prov_test_common_create_extra_connection (void);
int prov_test_common_load_data (void);
int prov_test_common_check_meta_full (void);
int prov_test_common_check_meta_partial (void);
int prov_test_common_check_meta_partial2 (void);
int prov_test_common_check_meta_partial3 (void);
int prov_test_common_check_meta_identifiers (gboolean case_sensitive, gboolean update_all);
int prov_test_common_check_cursor_models (void);
int prov_test_common_check_data_select (void);
int prov_test_common_check_timestamp (void);
int prov_test_common_check_date (void);
int prov_test_common_clean (void);
int prov_test_common_check_bigint (void);
int prov_test_common_values (void);
int priv_test_common_simultaneos_connections (void);

#endif
