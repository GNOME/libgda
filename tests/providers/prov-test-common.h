/*
 * Copyright (C) 2007 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#define fail(x) g_warning ("%s", x)
#define fail_if(x,y) if (x) g_warning ("%s", y)
#define fail_unless(x,y) if (!(x)) g_warning ("%s", y)

int prov_test_common_setup ();
int prov_test_common_load_data ();
int prov_test_common_check_meta ();
int prov_test_common_check_meta_identifiers (gboolean case_sensitive, gboolean update_all);
int prov_test_common_check_cursor_models ();
int prov_test_common_check_data_select ();
int prov_test_common_clean ();

#endif
