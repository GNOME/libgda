/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "browser-favorites.h"
#include <string.h>

static void
dump_favorite (ToolsFavoritesAttributes *f)
{
	g_print ("Favorite: ID=>%d\n", f->id);
	g_print ("          Type=>%d\n", f->type);
	g_print ("          Name=>%s\n", f->name);
	g_print ("          Descr=>%s\n", f->descr);
	g_print ("          Contents=>%s\n", f->contents);
}

static void
dump_favorites_in_db (GdaConnection *cnc)
{
	GdaDataModel *model;
	GError *error = NULL;

	g_print ("\n*********** contents of gda_sql_favorites ***********\n");
	model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), "SELECT * FROM gda_sql_favorites", &error,
					NULL);
	if (!model) {
                g_print ("Could not extract list of favorites: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	gda_data_model_dump (model, NULL);
	g_object_unref (model);

	g_print ("\n************ contents of gda_sql_favorder ***********\n");
	model = gda_meta_store_extract (gda_connection_get_meta_store (cnc), "SELECT * FROM gda_sql_favorder", &error,
					NULL);
	if (!model) {
                g_print ("Could not extract list of favorites: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	gda_data_model_dump (model, NULL);
	g_object_unref (model);
}

static void test1 (GdaConnection *cnc, ToolsFavorites *bfav);
static void test2 (GdaConnection *cnc, ToolsFavorites *bfav);
static void test3 (GdaConnection *cnc, ToolsFavorites *bfav);
int
main (int argc, char *argv[])
{
	ToolsFavorites *bfav;
	GdaConnection *cnc;
	GError *error = NULL;

	gda_init ();

	cnc = gda_connection_open_from_dsn ("SalesTest", NULL,
                                            GDA_CONNECTION_OPTIONS_NONE,
                                            &error);
        if (!cnc) {
                g_print ("Could not open connection: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
	bfav = gda_tools_favorites_new (gda_connection_get_meta_store (cnc));
	test1 (cnc, bfav);
	test2 (cnc, bfav);
	test3 (cnc, bfav);
	g_object_unref (G_OBJECT (bfav));
	g_object_unref (G_OBJECT (cnc));
	
	g_print ("OK!\n");
	return 0;
}

static void
test1 (GdaConnection *cnc, ToolsFavorites *bfav)
{
	gint favid, i;
	GError *error = NULL;
	ToolsFavoritesAttributes fav_array[]= {
		{-1, GDA_TOOLS_FAVORITES_TABLES, "table1", "fav1-descr", "fav1 contents"},
		{-1, GDA_TOOLS_FAVORITES_DIAGRAMS, "diagram1", "fav2-descr", "fav2 contents"}
	};
	for (i = 0; i < G_N_ELEMENTS (fav_array); i++) {
		ToolsFavoritesAttributes *f = (ToolsFavoritesAttributes*) &(fav_array[i]);
		favid = gda_tools_favorites_add (bfav, 0, f, -1, 0, &error);
		if (!favid < 0) {
			g_print ("Could not create favorite: %s\n",
				 error && error->message ? error->message : "No detail");
			exit (1);
		}
	}
	//dump_favorites_in_db (cnc);

	GSList *favlist, *list;
	favlist = gda_tools_favorites_list (bfav, 0, GDA_TOOLS_FAVORITES_TABLES, -1, &error);
	if (!favlist && error) {
		g_print ("Could not list favorites: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	for (i = 0, list = favlist; (i < G_N_ELEMENTS (fav_array)) && list; i++, list = list -> next) {
		ToolsFavoritesAttributes *f1 = (ToolsFavoritesAttributes*) &(fav_array[i]);
		ToolsFavoritesAttributes *f2 = (ToolsFavoritesAttributes*) list->data;
		if (f2->id == 0) {
			g_print ("ID should not be 0:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
		if ((f2->type != f1->type) ||
		    strcmp (f2->contents, f1->contents) ||
		    strcmp (f2->descr, f1->descr) ||
		    strcmp (f2->name, f1->name)) {
			g_print ("Favorites differ:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
	}
	if (list || (i != G_N_ELEMENTS (fav_array))) {
		g_print ("Wrong number of favorites reported\n");
		exit (1);
	}
	gda_tools_favorites_free_list (favlist);	
}

static void
test2 (GdaConnection *cnc, ToolsFavorites *bfav)
{
	gint favid, i;
	GError *error = NULL;
	ToolsFavoritesAttributes fav_array[]= {
		{-1, GDA_TOOLS_FAVORITES_TABLES, "table2", "fav1-descr", "fav1 contents SEPARATE"},
		{-1, GDA_TOOLS_FAVORITES_DIAGRAMS, "diagram2", "fav2-descr", "fav2 contents SEPARATE"},
		{-1, GDA_TOOLS_FAVORITES_TABLES, "table2", "Another description for table2", "fav1 contents SEPARATE"},
	};
	ToolsFavoritesAttributes res_fav_array[]= {
		{-1, GDA_TOOLS_FAVORITES_TABLES, "table2", "Another description for table2", "fav1 contents SEPARATE"},
		{-1, GDA_TOOLS_FAVORITES_DIAGRAMS, "diagram2", "fav2-descr", "fav2 contents SEPARATE"},
	};
	for (i = 0; i < G_N_ELEMENTS (fav_array); i++) {
		ToolsFavoritesAttributes *f = (ToolsFavoritesAttributes*) &(fav_array[i]);
		favid = gda_tools_favorites_add (bfav, 0, f, 1, 0, &error);
		if (!favid < 0) {
			g_print ("Could not create favorite: %s\n",
				 error && error->message ? error->message : "No detail");
			exit (1);
		}
	}
	dump_favorites_in_db (cnc);

	GSList *favlist, *list;
	favlist = gda_tools_favorites_list (bfav, 0, GDA_TOOLS_FAVORITES_TABLES, 1, &error);
	if (!favlist && error) {
		g_print ("Could not list favorites: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	for (i = 0, list = favlist; (i < G_N_ELEMENTS (res_fav_array)) && list; i++, list = list -> next) {
		ToolsFavoritesAttributes *f1 = (ToolsFavoritesAttributes*) &(res_fav_array[i]);
		ToolsFavoritesAttributes *f2 = (ToolsFavoritesAttributes*) list->data;
		if (f2->id == 0) {
			g_print ("ID should not be 0:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
		if ((f2->type != f1->type) ||
		    strcmp (f2->contents, f1->contents) ||
		    strcmp (f2->descr, f1->descr) ||
		    strcmp (f2->name, f1->name)) {
			g_print ("Favorites differ:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
	}
	if (list || (i != G_N_ELEMENTS (res_fav_array))) {
		g_print ("Wrong number of favorites reported\n");
		exit (1);
	}
	gda_tools_favorites_free_list (favlist);	
}

static void
test3 (GdaConnection *cnc, ToolsFavorites *bfav)
{
	gint favid, i;
	GError *error = NULL;
	ToolsFavoritesAttributes fav_array[]= {
		{-1, GDA_TOOLS_FAVORITES_DIAGRAMS, "diagram2", "fav2-descr", "fav2 contents SEPARATE"},
	};
	ToolsFavoritesAttributes res_fav_array[]= {
		{-1, GDA_TOOLS_FAVORITES_TABLES, "table2", "Another description for table2", "fav1 contents SEPARATE"},
	};
	for (i = 0; i < G_N_ELEMENTS (fav_array); i++) {
		ToolsFavoritesAttributes *f = (ToolsFavoritesAttributes*) &(fav_array[i]);
		favid = gda_tools_favorites_delete (bfav, 0, f, &error);
		if (!favid < 0) {
			g_print ("Could not delete favorite: %s\n",
				 error && error->message ? error->message : "No detail");
			exit (1);
		}
	}
	dump_favorites_in_db (cnc);

	GSList *favlist, *list;
	favlist = gda_tools_favorites_list (bfav, 0, GDA_TOOLS_FAVORITES_TABLES, 1, &error);
	if (!favlist && error) {
		g_print ("Could not list favorites: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	for (i = 0, list = favlist; (i < G_N_ELEMENTS (res_fav_array)) && list; i++, list = list -> next) {
		ToolsFavoritesAttributes *f1 = (ToolsFavoritesAttributes*) &(res_fav_array[i]);
		ToolsFavoritesAttributes *f2 = (ToolsFavoritesAttributes*) list->data;
		if (f2->id == 0) {
			g_print ("ID should not be 0:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
		if ((f2->type != f1->type) ||
		    strcmp (f2->contents, f1->contents) ||
		    strcmp (f2->descr, f1->descr) ||
		    strcmp (f2->name, f1->name)) {
			g_print ("Favorites differ:\n");
			dump_favorite (f1);
			dump_favorite (f2);
			exit (1);
		}
	}
	if (list || (i != G_N_ELEMENTS (res_fav_array))) {
		g_print ("Wrong number of favorites reported\n");
		exit (1);
	}
	gda_tools_favorites_free_list (favlist);	
}
