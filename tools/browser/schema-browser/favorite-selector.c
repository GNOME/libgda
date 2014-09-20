/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <libgda/gda-tree.h>
#include "favorite-selector.h"
#include "../mgr-favorites.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../ui-support.h"
#include "marshal.h"
#include "../gdaui-bar.h"
#include <gdk/gdkkeysyms.h>

struct _FavoriteSelectorPrivate {
	TConnection *tcnc;
	GdaTree *tree;
	GtkWidget *treeview;
	guint idle_update_favorites;
};

static void favorite_selector_class_init (FavoriteSelectorClass *klass);
static void favorite_selector_init       (FavoriteSelector *tsel,
				       FavoriteSelectorClass *klass);
static void favorite_selector_dispose   (GObject *object);

static void favorites_changed_cb (TFavorites *bfav, FavoriteSelector *tsel);

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint favorite_selector_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* columns of the resulting GtkTreeModel */
enum {
	COLUMN_MARKUP = 0,
	COLUMN_ICON = 1,
	COLUMN_CONTENTS = 2,
	COLUMN_TYPE = 3,
	COLUMN_ID = 4
};


/*
 * FavoriteSelector class implementation
 */

static void
favorite_selector_class_init (FavoriteSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	favorite_selector_signals [SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (FavoriteSelectorClass, selection_changed),
                              NULL, NULL,
                              _sb_marshal_VOID__INT_ENUM_STRING, G_TYPE_NONE,
                              3, G_TYPE_INT, G_TYPE_UINT, G_TYPE_STRING);
	klass->selection_changed = NULL;

	object_class->dispose = favorite_selector_dispose;
}


static void
favorite_selector_init (FavoriteSelector *tsel,	G_GNUC_UNUSED FavoriteSelectorClass *klass)
{
	tsel->priv = g_new0 (FavoriteSelectorPrivate, 1);
	tsel->priv->idle_update_favorites = 0;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (tsel), GTK_ORIENTATION_VERTICAL);
}

static void
favorite_selector_dispose (GObject *object)
{
	FavoriteSelector *tsel = (FavoriteSelector *) object;

	/* free memory */
	if (tsel->priv) {
		if (tsel->priv->idle_update_favorites != 0)
			g_source_remove (tsel->priv->idle_update_favorites);
		if (tsel->priv->tree)
			g_object_unref (tsel->priv->tree);

		if (tsel->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (t_connection_get_favorites (tsel->priv->tcnc),
							      G_CALLBACK (favorites_changed_cb), tsel);
			g_object_unref (tsel->priv->tcnc);
		}

		g_free (tsel->priv);
		tsel->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
favorite_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (FavoriteSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) favorite_selector_class_init,
			NULL,
			NULL,
			sizeof (FavoriteSelector),
			0,
			(GInstanceInitFunc) favorite_selector_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "FavoriteSelector",
					       &info, 0);
	}
	return type;
}

static gboolean
key_press_event_cb (GtkTreeView *treeview, GdkEventKey *event, FavoriteSelector *tsel)
{
	if (event->keyval == GDK_KEY_Delete) {
		GtkTreeModel *model;
		GtkTreeSelection *select;
		GtkTreeIter iter;
		
		select = gtk_tree_view_get_selection (treeview);
		if (gtk_tree_selection_get_selected (select, &model, &iter)) {
			TFavorites *bfav;
			TFavoritesAttributes fav;
			GError *lerror = NULL;

			memset (&fav, 0, sizeof (TFavoritesAttributes));
			gtk_tree_model_get (model, &iter,
					    COLUMN_ID, &(fav.id), -1);
			bfav = t_connection_get_favorites (tsel->priv->tcnc);
			if (!t_favorites_delete (bfav, 0, &fav, NULL)) {
				ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*)tsel),
					       _("Could not remove favorite: %s"),
					       lerror && lerror->message ? lerror->message : _("No detail"));
				if (lerror)
					g_error_free (lerror);
			}
		}
		
		return TRUE;
	}
	return FALSE; /* not handled */
}


static void
selection_changed_cb (GtkTreeView *treeview, G_GNUC_UNUSED GtkTreePath *path,
		      G_GNUC_UNUSED GtkTreeViewColumn *column, FavoriteSelector *tsel)
{
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	
	select = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gchar *str;
		guint type;
		gint fav_id;
		gtk_tree_model_get (model, &iter,
				    COLUMN_ID, &fav_id,
				    COLUMN_TYPE, &type,
				    COLUMN_CONTENTS, &str, -1);
		g_signal_emit (tsel, favorite_selector_signals [SELECTION_CHANGED], 0, fav_id, type, str);
		g_free (str);
	}
}

static gboolean idle_update_favorites (FavoriteSelector *tsel);
static gboolean tree_store_drag_drop_cb (GdauiTreeStore *store, const gchar *path,
					 GtkSelectionData *selection_data, FavoriteSelector *tsel);
static gboolean tree_store_drag_can_drag_cb (GdauiTreeStore *store, const gchar *path,
					     FavoriteSelector *tsel);
static gboolean tree_store_drag_get_cb (GdauiTreeStore *store, const gchar *path,
					GtkSelectionData *selection_data, FavoriteSelector *tsel);

/**
 * favorite_selector_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
favorite_selector_new (TConnection *tcnc)
{
	FavoriteSelector *tsel;
	GdaTreeManager *manager;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	tsel = FAVORITE_SELECTOR (g_object_new (FAVORITE_SELECTOR_TYPE, NULL));

	tsel->priv->tcnc = g_object_ref (tcnc);
	g_signal_connect (t_connection_get_favorites (tsel->priv->tcnc), "favorites-changed",
			  G_CALLBACK (favorites_changed_cb), tsel);
	
	/* create tree managers */
	tsel->priv->tree = gda_tree_new ();
	manager = mgr_favorites_new (tcnc, T_FAVORITES_TABLES | T_FAVORITES_DIAGRAMS,
				     ORDER_KEY_SCHEMA);
        gda_tree_add_manager (tsel->priv->tree, manager);
	g_object_unref (manager);

	/* update the tree's contents */
	if (tsel->priv->idle_update_favorites == 0)
		tsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites,
								tsel);

	/* header */
	GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Favorites"));
	label = gdaui_bar_new (str);
	g_free (str);
	gdaui_bar_set_icon_from_pixbuf (GDAUI_BAR (label), ui_get_pixbuf_icon (UI_ICON_BOOKMARK));
        gtk_box_pack_start (GTK_BOX (tsel), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* tree model */
	GtkTreeModel *model;
	GtkWidget *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	model = gdaui_tree_store_new (tsel->priv->tree, 5,
				      G_TYPE_STRING, "markup",
				      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, MGR_FAVORITES_CONTENTS_ATT_NAME,
				      G_TYPE_UINT, MGR_FAVORITES_TYPE_ATT_NAME,
				      G_TYPE_INT, MGR_FAVORITES_ID_ATT_NAME);
	treeview = ui_make_tree_view (model);
	tsel->priv->treeview = treeview;
	g_object_unref (model);

	/* icon */
	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COLUMN_ICON);

	/* text */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "markup", COLUMN_MARKUP);

	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	
	/* scrolled window packing */
	GtkWidget *sw;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), treeview);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);	

	gtk_box_pack_start (GTK_BOX (tsel), sw, TRUE, TRUE, 0);
	gtk_widget_show_all (sw);
	g_signal_connect (G_OBJECT (treeview), "row-activated",
			  G_CALLBACK (selection_changed_cb), tsel);
	g_signal_connect (G_OBJECT (treeview), "key-press-event",
			  G_CALLBACK (key_press_event_cb), tsel);

	/* DnD */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (treeview), dbo_table, G_N_ELEMENTS (dbo_table),
					      GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview), GDK_BUTTON1_MASK,
						dbo_table, G_N_ELEMENTS (dbo_table),
						GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect (model, "drag-drop",
			  G_CALLBACK (tree_store_drag_drop_cb), tsel);
	g_signal_connect (model, "drag-can-drag",
			  G_CALLBACK (tree_store_drag_can_drag_cb), tsel);
	g_signal_connect (model, "drag-get",
			  G_CALLBACK (tree_store_drag_get_cb), tsel);

	return (GtkWidget*) tsel;
}

static gboolean
idle_update_favorites (FavoriteSelector *tsel)
{
	gboolean done;
	done = gda_tree_update_all (tsel->priv->tree, NULL);
	if (done)
		tsel->priv->idle_update_favorites = 0;
	else
		tsel->priv->idle_update_favorites = g_timeout_add_seconds (1, (GSourceFunc) idle_update_favorites,
									   tsel);
	return FALSE;
}

static gboolean
tree_store_drag_drop_cb (G_GNUC_UNUSED GdauiTreeStore *store, const gchar *path,
			 GtkSelectionData *selection_data, FavoriteSelector *tsel)
{
	TFavorites *bfav;
	TFavoritesAttributes fav;
	GError *error = NULL;
	gint pos;

	memset (&fav, 0, sizeof (TFavoritesAttributes));
	fav.id = -1;
	fav.type = T_FAVORITES_TABLES;
	fav.name = NULL;
	fav.descr = NULL;
	fav.contents = (gchar*) gtk_selection_data_get_data (selection_data);

	pos = atoi (path);
	/*g_print ("%s() path => %s, pos: %d\n", __FUNCTION__, path, pos);*/
	
	bfav = t_connection_get_favorites (tsel->priv->tcnc);
	if (! t_favorites_add (bfav, 0, &fav, ORDER_KEY_SCHEMA, pos, &error)) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tsel),
				    _("Could not add favorite: %s"),
				    error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
tree_store_drag_can_drag_cb (G_GNUC_UNUSED GdauiTreeStore *store, const gchar *path, FavoriteSelector *tsel)
{
	GdaTreeNode *node;
	node = gda_tree_get_node (tsel->priv->tree, path, FALSE);
	if (node) {
		const GValue *cvalue;
		cvalue = gda_tree_node_get_node_attribute (node, "fav_contents");
		if (cvalue)
			return TRUE;
	}
	return FALSE;
}

static gboolean
tree_store_drag_get_cb (G_GNUC_UNUSED GdauiTreeStore *store, const gchar *path,
			GtkSelectionData *selection_data, FavoriteSelector *tsel)
{
	GdaTreeNode *node;
	node = gda_tree_get_node (tsel->priv->tree, path, FALSE);
	if (node) {
		const GValue *cvalue;
		cvalue = gda_tree_node_get_node_attribute (node, "fav_contents");
		if (cvalue) {
			const gchar *str;
			str = g_value_get_string (cvalue);
			gtk_selection_data_set (selection_data,
						gtk_selection_data_get_target (selection_data), 8,
						(guchar*) str, strlen (str));
			return TRUE;
		}
	}
	return FALSE;
}

static void
favorites_changed_cb (G_GNUC_UNUSED TFavorites *bfav, FavoriteSelector *tsel)
{
	if (! gda_tree_update_all (tsel->priv->tree, NULL)) {
		if (tsel->priv->idle_update_favorites == 0)
			tsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites, tsel);

	}
}
