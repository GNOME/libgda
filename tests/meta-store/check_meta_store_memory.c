/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char ** argv)
{
	GdaMetaStore *store;

	gda_init ();

	/* Clean eveything which might exist in the store */
	store = gda_meta_store_new (NULL);
	common_drop_all_tables (store);
	g_object_unref (store);
	
	/* Test store setup */
	store = gda_meta_store_new (NULL);

	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);

	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);
	
	g_print ("Test Ok.\n");
	
	return EXIT_SUCCESS;
}





