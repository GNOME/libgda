/*
 * Copyright (C) 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgda/gda-tree.h>
#include "query-favorite-selector.h"
#include "../mgr-favorites.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../support.h"
#include "marshal.h"
#include "../cc-gray-bar.h"
#include "../browser-favorites.h"
#include <gdk/gdkkeysyms.h>
#include "../common/popup-container.h"
#include "query-editor.h"

struct _QueryFavoriteSelectorPrivate {
	BrowserConnection *bcnc;
	GdaTree *tree;
	GtkWidget *treeview;
	guint idle_update_favorites;

	GtkWidget *popup_menu;
	GtkWidget *popup_properties;
	GtkWidget *properties_name;
	GtkWidget *properties_text;
	gint       properties_id;
	gint       properties_position;
	guint      prop_save_timeout;
};

static void query_favorite_selector_class_init (QueryFavoriteSelectorClass *klass);
static void query_favorite_selector_init       (QueryFavoriteSelector *tsel,
				       QueryFavoriteSelectorClass *klass);
static void query_favorite_selector_dispose   (GObject *object);

static void favorites_changed_cb (BrowserFavorites *bfav, QueryFavoriteSelector *tsel);

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint query_favorite_selector_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* columns of the resulting GtkTreeModel */
enum {
	COLUMN_POSITION = 0,
	COLUMN_ICON = 1,
	COLUMN_CONTENTS = 2,
	COLUMN_TYPE = 3,
	COLUMN_ID = 4,
	COLUMN_NAME = 5
};


/*
 * QueryFavoriteSelector class implementation
 */

static void
query_favorite_selector_class_init (QueryFavoriteSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	query_favorite_selector_signals [SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (QueryFavoriteSelectorClass, selection_changed),
                              NULL, NULL,
                              _qe_marshal_VOID__INT_ENUM_STRING, G_TYPE_NONE,
                              3, G_TYPE_INT, G_TYPE_UINT, G_TYPE_STRING);
	klass->selection_changed = NULL;

	object_class->dispose = query_favorite_selector_dispose;
}


static void
query_favorite_selector_init (QueryFavoriteSelector *tsel,	QueryFavoriteSelectorClass *klass)
{
	tsel->priv = g_new0 (QueryFavoriteSelectorPrivate, 1);
	tsel->priv->idle_update_favorites = 0;
	tsel->priv->prop_save_timeout = 0;
}

static void
query_favorite_selector_dispose (GObject *object)
{
	QueryFavoriteSelector *tsel = (QueryFavoriteSelector *) object;

	/* free memory */
	if (tsel->priv) {
		if (tsel->priv->idle_update_favorites != 0)
			g_source_remove (tsel->priv->idle_update_favorites);
		if (tsel->priv->tree)
			g_object_unref (tsel->priv->tree);

		if (tsel->priv->bcnc) {
			g_signal_handlers_disconnect_by_func (browser_connection_get_favorites (tsel->priv->bcnc),
							      G_CALLBACK (favorites_changed_cb), tsel);
			g_object_unref (tsel->priv->bcnc);
		}
		
		if (tsel->priv->popup_properties)
			gtk_widget_destroy (tsel->priv->popup_properties);
		if (tsel->priv->popup_menu)
			gtk_widget_destroy (tsel->priv->popup_menu);
		if (tsel->priv->prop_save_timeout)
			g_source_remove (tsel->priv->prop_save_timeout);

		g_free (tsel->priv);
		tsel->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
query_favorite_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (QueryFavoriteSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_favorite_selector_class_init,
			NULL,
			NULL,
			sizeof (QueryFavoriteSelector),
			0,
			(GInstanceInitFunc) query_favorite_selector_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "QueryFavoriteSelector",
					       &info, 0);
	}
	return type;
}

static gboolean
key_press_event_cb (GtkTreeView *treeview, GdkEventKey *event, QueryFavoriteSelector *tsel)
{
	if (event->keyval == GDK_Delete) {
		GtkTreeModel *model;
		GtkTreeSelection *select;
		GtkTreeIter iter;
		
		select = gtk_tree_view_get_selection (treeview);
		if (gtk_tree_selection_get_selected (select, &model, &iter)) {
			BrowserFavorites *bfav;
			BrowserFavoritesAttributes fav;
			GError *lerror = NULL;

			memset (&fav, 0, sizeof (BrowserFavoritesAttributes));
			gtk_tree_model_get (model, &iter,
					    COLUMN_ID, &(fav.id), -1);
			bfav = browser_connection_get_favorites (tsel->priv->bcnc);
			if (!browser_favorites_delete (bfav, 0, &fav, NULL)) {
				browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*)tsel),
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
selection_changed_cb (GtkTreeView *treeview, GtkTreePath *path,
		      GtkTreeViewColumn *column, QueryFavoriteSelector *tsel)
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
		g_signal_emit (tsel, query_favorite_selector_signals [SELECTION_CHANGED], 0, fav_id, type, str);
		g_free (str);
	}
}

static gboolean
prop_save_timeout (QueryFavoriteSelector *tsel)
{
	BrowserFavorites *bfav;
	BrowserFavoritesAttributes fav;
	GError *error = NULL;

	memset (&fav, 0, sizeof (BrowserFavoritesAttributes));
	fav.id = tsel->priv->properties_id;
	fav.type = BROWSER_FAVORITES_QUERIES;
	fav.name = (gchar*) gtk_entry_get_text (GTK_ENTRY (tsel->priv->properties_name));
	fav.descr = NULL;
	fav.contents = query_editor_get_all_text (QUERY_EDITOR (tsel->priv->properties_text));

	bfav = browser_connection_get_favorites (tsel->priv->bcnc);
	if (! browser_favorites_add (bfav, 0, &fav, ORDER_KEY_QUERIES, tsel->priv->properties_position, &error)) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tsel),
				    _("Could not add favorite: %s"),
				    error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
	}

	tsel->priv->prop_save_timeout = 0;
	return FALSE; /* remove timeout */
}

static void
property_changed_cb (GtkWidget *multiple, QueryFavoriteSelector *tsel)
{
	if (tsel->priv->prop_save_timeout)
		g_source_remove (tsel->priv->prop_save_timeout);
	tsel->priv->prop_save_timeout = g_timeout_add (100, (GSourceFunc) prop_save_timeout, tsel);
}

static void
properties_activated_cb (GtkMenuItem *mitem, QueryFavoriteSelector *tsel)
{
	if (! tsel->priv->popup_properties) {
		GtkWidget *pcont, *vbox, *hbox, *label, *entry, *text, *table;
		gchar *str;
		
		pcont = popup_container_new (GTK_WIDGET (mitem));
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (pcont), vbox);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Favorite's properties"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		hbox = gtk_hbox_new (FALSE, 0); /* HIG */
		gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
		label = gtk_label_new ("      ");
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		
		table = gtk_table_new (2, 2, FALSE);
		gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Name"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
		gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("SQL Code"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., 0.);
		gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
		
		entry = gtk_entry_new ();
		gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
		tsel->priv->properties_name = entry;
		g_signal_connect (entry, "changed",
				  G_CALLBACK (property_changed_cb), tsel);
		
		text = query_editor_new ();
		query_editor_show_tooltip (QUERY_EDITOR (text), FALSE);
		gtk_widget_set_size_request (GTK_WIDGET (text), 400, 300);
		gtk_table_attach_defaults (GTK_TABLE (table), text, 1, 2, 1, 2);
		tsel->priv->properties_text = text;
		g_signal_connect (text, "changed",
				  G_CALLBACK (property_changed_cb), tsel);

		tsel->priv->popup_properties = pcont;
		gtk_widget_show_all (vbox);
	}

	/* adjust contents */
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tsel->priv->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *name, *contents;
		
		gtk_tree_model_get (model, &iter,
				    COLUMN_ID, &(tsel->priv->properties_id),
				    COLUMN_POSITION, &(tsel->priv->properties_position),
				    COLUMN_NAME, &name,
				    COLUMN_CONTENTS, &contents, -1);

		g_signal_handlers_block_by_func (tsel->priv->properties_name,
						 G_CALLBACK (property_changed_cb), tsel);
		gtk_entry_set_text (GTK_ENTRY (tsel->priv->properties_name), name);
		g_signal_handlers_unblock_by_func (tsel->priv->properties_name,
						   G_CALLBACK (property_changed_cb), tsel);
		g_free (name);

		g_signal_handlers_block_by_func (tsel->priv->properties_text,
						 G_CALLBACK (property_changed_cb), tsel);
		query_editor_set_text (QUERY_EDITOR (tsel->priv->properties_text), contents);
		g_signal_handlers_unblock_by_func (tsel->priv->properties_text,
						   G_CALLBACK (property_changed_cb), tsel);
		g_free (contents);

		gtk_widget_show (tsel->priv->popup_properties);
	}
}

static void
do_popup_menu (GtkWidget *widget, GdkEventButton *event, QueryFavoriteSelector *tsel)
{
	int button, event_time;

	if (! tsel->priv->popup_menu) {
		GtkWidget *menu, *mitem;
		
		menu = gtk_menu_new ();
		g_signal_connect (menu, "deactivate", 
				  G_CALLBACK (gtk_widget_hide), NULL);
		
		mitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
		gtk_widget_show (mitem);
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (properties_activated_cb), tsel);

		tsel->priv->popup_menu = menu;
	}
		
	if (event) {
		button = event->button;
		event_time = event->time;
	}
	else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (tsel->priv->popup_menu), NULL, NULL, NULL, NULL, 
			button, event_time);
}


static gboolean
popup_menu_cb (GtkWidget *widget, QueryFavoriteSelector *tsel)
{
	do_popup_menu (widget, NULL, tsel);
	return TRUE;
}

static gboolean
button_press_event_cb (GtkTreeView *treeview, GdkEventButton *event, QueryFavoriteSelector *tsel)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		do_popup_menu ((GtkWidget*) treeview, event, tsel);
		return TRUE;
	}

	return FALSE;
}

static void cell_data_func (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
			    GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);
static gboolean idle_update_favorites (QueryFavoriteSelector *tsel);
static gboolean tree_store_drag_drop_cb (GdauiTreeStore *store, const gchar *path,
					 GtkSelectionData *selection_data, QueryFavoriteSelector *tsel);
static gboolean tree_store_drag_can_drag_cb (GdauiTreeStore *store, const gchar *path,
					     QueryFavoriteSelector *tsel);
static gboolean tree_store_drag_get_cb (GdauiTreeStore *store, const gchar *path,
					GtkSelectionData *selection_data, QueryFavoriteSelector *tsel);
static void trash_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
				    GtkSelectionData *selection_data, guint target_type, guint time,
				    QueryFavoriteSelector *tsel);
/**
 * query_favorite_selector_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
query_favorite_selector_new (BrowserConnection *bcnc)
{
	QueryFavoriteSelector *tsel;
	GdaTreeManager *manager;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	tsel = QUERY_FAVORITE_SELECTOR (g_object_new (QUERY_FAVORITE_SELECTOR_TYPE, NULL));

	tsel->priv->bcnc = g_object_ref (bcnc);
	g_signal_connect (browser_connection_get_favorites (tsel->priv->bcnc), "favorites-changed",
			  G_CALLBACK (favorites_changed_cb), tsel);
	
	/* create tree managers */
	tsel->priv->tree = gda_tree_new ();
	manager = mgr_favorites_new (bcnc, BROWSER_FAVORITES_QUERIES, ORDER_KEY_QUERIES);
        gda_tree_add_manager (tsel->priv->tree, manager);
	g_object_unref (manager);

	/* update the tree's contents */
	if (! gda_tree_update_all (tsel->priv->tree, NULL)) {
		if (tsel->priv->idle_update_favorites == 0)
			tsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites, tsel);
	}

	/* header */
	GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Favorites"));
	label = cc_gray_bar_new (str);
	g_free (str);
	cc_gray_bar_set_icon_from_pixbuf (CC_GRAY_BAR (label), browser_get_pixbuf_icon (BROWSER_ICON_BOOKMARK));
        gtk_box_pack_start (GTK_BOX (tsel), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* tree model */
	GtkTreeModel *model;
	GtkWidget *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	model = gdaui_tree_store_new (tsel->priv->tree, 6,
				      G_TYPE_INT, MGR_FAVORITES_POSITION_ATT_NAME,
				      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, MGR_FAVORITES_CONTENTS_ATT_NAME,
				      G_TYPE_UINT, MGR_FAVORITES_TYPE_ATT_NAME,
				      G_TYPE_INT, MGR_FAVORITES_ID_ATT_NAME,
				      G_TYPE_STRING, MGR_FAVORITES_NAME_ATT_NAME);
	treeview = gtk_tree_view_new_with_model (model);
	tsel->priv->treeview = treeview;
	g_object_unref (model);

	/* icon */
	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COLUMN_ICON);
	g_object_set ((GObject*) renderer, "yalign", 0., NULL);

	/* text */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, (GtkTreeCellDataFunc) cell_data_func,
						 NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	
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
	g_signal_connect (G_OBJECT (treeview), "popup-menu",
			  G_CALLBACK (popup_menu_cb), tsel);
	g_signal_connect (G_OBJECT (treeview), "button-press-event",
			  G_CALLBACK (button_press_event_cb), tsel);

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

static void
cell_data_func (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	gchar *name, *contents;
	gchar *markup, *tmp1, *tmp2;

	gtk_tree_model_get (tree_model, iter,
			    COLUMN_NAME, &name, COLUMN_CONTENTS, &contents, -1);
	tmp1 = g_markup_printf_escaped ("%s", name);
	tmp2 = g_markup_printf_escaped ("%s", contents);
	markup = g_strdup_printf ("%s\n<small>%s</small>", tmp1, tmp2);
	g_object_set ((GObject*) cell, "markup", markup, NULL);
	g_free (tmp1);
	g_free (tmp2);
	g_free (name);
	g_free (contents);
}


static gboolean
idle_update_favorites (QueryFavoriteSelector *tsel)
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
tree_store_drag_drop_cb (GdauiTreeStore *store, const gchar *path, GtkSelectionData *selection_data,
			 QueryFavoriteSelector *tsel)
{
	BrowserFavorites *bfav;
	BrowserFavoritesAttributes fav;
	GError *error = NULL;
	gint pos;

	memset (&fav, 0, sizeof (BrowserFavoritesAttributes));
	fav.id = -1;
	fav.type = BROWSER_FAVORITES_QUERIES;
	fav.name = _("Unnamed query");
	fav.descr = NULL;
	fav.contents = (gchar*) selection_data->data;

	pos = atoi (path);
	g_print ("%s() path => %s, pos: %d\n", __FUNCTION__, path, pos);
	
	bfav = browser_connection_get_favorites (tsel->priv->bcnc);
	if (! browser_favorites_add (bfav, 0, &fav, ORDER_KEY_QUERIES, pos, &error)) {
		browser_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) tsel),
				    _("Could not add favorite: %s"),
				    error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static gboolean
tree_store_drag_can_drag_cb (GdauiTreeStore *store, const gchar *path, QueryFavoriteSelector *tsel)
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
tree_store_drag_get_cb (GdauiTreeStore *store, const gchar *path, GtkSelectionData *selection_data,
			QueryFavoriteSelector *tsel)
{
	GdaTreeNode *node;
	node = gda_tree_get_node (tsel->priv->tree, path, FALSE);
	if (node) {
		const GValue *cvalue;
		cvalue = gda_tree_node_get_node_attribute (node, "fav_contents");
		if (cvalue) {
			const gchar *str;
			str = g_value_get_string (cvalue);
			gtk_selection_data_set (selection_data, selection_data->target, 8,
						(guchar*) str, strlen (str));
			return TRUE;
		}
	}
	return FALSE;
}

static void
favorites_changed_cb (BrowserFavorites *bfav, QueryFavoriteSelector *tsel)
{
	if (! gda_tree_update_all (tsel->priv->tree, NULL)) {
		if (tsel->priv->idle_update_favorites == 0)
			tsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites, tsel);

	}
}
