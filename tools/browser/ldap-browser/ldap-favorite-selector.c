/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <gtk/gtk.h>
#include <libgda/gda-tree.h>
#include "ldap-favorite-selector.h"
#include "../mgr-favorites.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../dnd.h"
#include "../ui-support.h"
#include "ldap-marshal.h"
#include "../gdaui-bar.h"
#include <gdk/gdkkeysyms.h>
#include <libgda-ui/internal/popup-container.h>

struct _LdapFavoriteSelectorPrivate {
	TConnection *tcnc;
	GdaTree *tree;
	GtkWidget *treeview;
	guint idle_update_favorites;

	GtkWidget *popup_menu;
	GtkWidget *popup_properties;
	GtkWidget *properties_name;
	GtkWidget *properties_descr;
	gint       properties_id;
	gint       properties_position;
	guint      prop_save_timeout;
};

static void ldap_favorite_selector_class_init (LdapFavoriteSelectorClass *klass);
static void ldap_favorite_selector_init       (LdapFavoriteSelector *fsel,
					       LdapFavoriteSelectorClass *klass);
static void ldap_favorite_selector_dispose   (GObject *object);

static void favorites_changed_cb (TFavorites *bfav, LdapFavoriteSelector *fsel);

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint ldap_favorite_selector_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* columns of the resulting GtkTreeModel */
enum {
	COLUMN_ID = 0,
	COLUMN_NAME = 1,
	COLUMN_ICON = 2,
	COLUMN_MARKUP = 3,
	COLUMN_POSITION = 4,
	COLUMN_DESCR = 5,
	COLUMN_FAVTYPE = 6,
	COLUMN_LAST
};


/*
 * LdapFavoriteSelector class implementation
 */

static void
ldap_favorite_selector_class_init (LdapFavoriteSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	ldap_favorite_selector_signals [SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (LdapFavoriteSelectorClass, selection_changed),
                              NULL, NULL,
                              _ldap_marshal_VOID__INT_ENUM_STRING, G_TYPE_NONE,
                              3, G_TYPE_INT, G_TYPE_UINT, G_TYPE_STRING);
	klass->selection_changed = NULL;

	object_class->dispose = ldap_favorite_selector_dispose;
}


static void
ldap_favorite_selector_init (LdapFavoriteSelector *fsel, G_GNUC_UNUSED LdapFavoriteSelectorClass *klass)
{
	fsel->priv = g_new0 (LdapFavoriteSelectorPrivate, 1);
	fsel->priv->idle_update_favorites = 0;
	fsel->priv->prop_save_timeout = 0;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (fsel), GTK_ORIENTATION_VERTICAL);
}

static void
ldap_favorite_selector_dispose (GObject *object)
{
	LdapFavoriteSelector *fsel = (LdapFavoriteSelector *) object;

	/* free memory */
	if (fsel->priv) {
		if (fsel->priv->idle_update_favorites != 0)
			g_source_remove (fsel->priv->idle_update_favorites);
		if (fsel->priv->prop_save_timeout)
			g_source_remove (fsel->priv->prop_save_timeout);

		if (fsel->priv->tree)
			g_object_unref (fsel->priv->tree);

		if (fsel->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (t_connection_get_favorites (fsel->priv->tcnc),
							      G_CALLBACK (favorites_changed_cb), fsel);
			g_object_unref (fsel->priv->tcnc);
		}
		
		if (fsel->priv->popup_properties)
			gtk_widget_destroy (fsel->priv->popup_properties);
		if (fsel->priv->popup_menu)
			gtk_widget_destroy (fsel->priv->popup_menu);

		g_free (fsel->priv);
		fsel->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
ldap_favorite_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (LdapFavoriteSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ldap_favorite_selector_class_init,
			NULL,
			NULL,
			sizeof (LdapFavoriteSelector),
			0,
			(GInstanceInitFunc) ldap_favorite_selector_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "LdapFavoriteSelector",
					       &info, 0);
	}
	return type;
}

static void
favorite_delete_selected (LdapFavoriteSelector *fsel)
{
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->priv->treeview));
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		TFavorites *bfav;
		TFavoritesAttributes fav;
		GError *lerror = NULL;
		
		memset (&fav, 0, sizeof (TFavoritesAttributes));
		gtk_tree_model_get (model, &iter,
				    COLUMN_ID, &(fav.id), -1);
		bfav = t_connection_get_favorites (fsel->priv->tcnc);
		if (!t_favorites_delete (bfav, 0, &fav, NULL)) {
			ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*)fsel),
					    _("Could not remove favorite: %s"),
					    lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
	}
}

static gboolean
key_press_event_cb (G_GNUC_UNUSED GtkTreeView *treeview, GdkEventKey *event, LdapFavoriteSelector *fsel)
{
	if (event->keyval == GDK_KEY_Delete) {
		favorite_delete_selected (fsel);
		return TRUE;
	}
	return FALSE; /* not handled */
}


static void
selection_changed_cb (GtkTreeView *treeview, G_GNUC_UNUSED GtkTreePath *path,
		      G_GNUC_UNUSED GtkTreeViewColumn *column, LdapFavoriteSelector *fsel)
{
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	
	select = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gchar *str;
		gint fav_id;
		TFavoritesType fav_type;
		gtk_tree_model_get (model, &iter,
				    COLUMN_ID, &fav_id,
				    COLUMN_FAVTYPE, &fav_type,
				    COLUMN_NAME, &str, -1);
		g_signal_emit (fsel, ldap_favorite_selector_signals [SELECTION_CHANGED], 0, fav_id,
			       fav_type, str);
		g_free (str);
	}
}

static gboolean
prop_save_timeout (LdapFavoriteSelector *fsel)
{
	TFavorites *bfav;
	TFavoritesAttributes fav;
	GError *error = NULL;
	gboolean allok;

	bfav = t_connection_get_favorites (fsel->priv->tcnc);

	memset (&fav, 0, sizeof (TFavoritesAttributes));
	fav.id = fsel->priv->properties_id;
	fav.type = T_FAVORITES_LDAP_DN;
	fav.name = (gchar*) gtk_entry_get_text (GTK_ENTRY (fsel->priv->properties_name));
	fav.descr = (gchar*) gtk_entry_get_text (GTK_ENTRY (fsel->priv->properties_descr));
	fav.contents = (gchar*) gtk_entry_get_text (GTK_ENTRY (fsel->priv->properties_name));

	allok = t_favorites_add (bfav, 0, &fav, ORDER_KEY_LDAP,
				       fsel->priv->properties_position, &error);
	if (! allok) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) fsel),
				    _("Could not add favorite: %s"),
				    error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
	}

	fsel->priv->prop_save_timeout = 0;
	return FALSE; /* remove timeout */
}

static void
property_changed_cb (G_GNUC_UNUSED GtkWidget *multiple, LdapFavoriteSelector *fsel)
{
	if (fsel->priv->prop_save_timeout)
		g_source_remove (fsel->priv->prop_save_timeout);
	fsel->priv->prop_save_timeout = g_timeout_add (200, (GSourceFunc) prop_save_timeout, fsel);
}

static void
properties_activated_cb (GtkMenuItem *mitem, LdapFavoriteSelector *fsel)
{
	if (! fsel->priv->popup_properties) {
		GtkWidget *pcont, *vbox, *hbox, *label, *entry, *grid;
		gchar *str;
		
		pcont = popup_container_new (GTK_WIDGET (mitem));
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_add (GTK_CONTAINER (pcont), vbox);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Favorite's properties"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
		gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
		label = gtk_label_new ("      ");
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		
		grid = gtk_grid_new ();
		gtk_box_pack_start (GTK_BOX (hbox), grid, TRUE, TRUE, 0);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Name"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Description"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
		
		entry = gtk_entry_new ();
		gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);
		gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
		fsel->priv->properties_name = entry;

		entry = gtk_entry_new ();
		gtk_widget_set_size_request (entry, 200, -1);
		gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
		fsel->priv->properties_descr = entry;
		g_signal_connect (entry, "changed",
				  G_CALLBACK (property_changed_cb), fsel);

		fsel->priv->popup_properties = pcont;
		gtk_widget_show_all (vbox);
	}

	/* adjust contents */
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (fsel->priv->treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *name, *descr;
		
		gtk_tree_model_get (model, &iter,
				    COLUMN_ID, &(fsel->priv->properties_id),
				    COLUMN_POSITION, &(fsel->priv->properties_position),
				    COLUMN_NAME, &name,
				    COLUMN_DESCR, &descr, -1);

		if (name) {
			gtk_entry_set_text (GTK_ENTRY (fsel->priv->properties_name), name);
			g_free (name);
		}

		g_signal_handlers_block_by_func (fsel->priv->properties_descr,
						 G_CALLBACK (property_changed_cb), fsel);
		gtk_entry_set_text (GTK_ENTRY (fsel->priv->properties_descr), descr ? descr : "");
		g_signal_handlers_unblock_by_func (fsel->priv->properties_descr,
						   G_CALLBACK (property_changed_cb), fsel);
		g_free (descr);

		gtk_widget_show (fsel->priv->popup_properties);
	}
}

static void
delete_activated_cb (G_GNUC_UNUSED GtkMenuItem *mitem, LdapFavoriteSelector *fsel)
{
	favorite_delete_selected (fsel);
}

static void
do_popup_menu (G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, LdapFavoriteSelector *fsel)
{
	if (! fsel->priv->popup_menu) {
		GtkWidget *menu, *mitem;
		
		menu = gtk_menu_new ();
		g_signal_connect (menu, "deactivate", 
				  G_CALLBACK (gtk_widget_hide), NULL);
		
		mitem = gtk_menu_item_new_with_label (_("_Properties"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
		gtk_widget_show (mitem);
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (properties_activated_cb), fsel);

		mitem = gtk_menu_item_new_with_label (_("_Delete"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
		gtk_widget_show (mitem);
		g_signal_connect (mitem, "activate",
				  G_CALLBACK (delete_activated_cb), fsel);

		fsel->priv->popup_menu = menu;
	}

	gtk_menu_popup_at_pointer (GTK_MENU (fsel->priv->popup_menu), NULL);
}


static gboolean
popup_menu_cb (GtkWidget *widget, LdapFavoriteSelector *fsel)
{
	do_popup_menu (widget, NULL, fsel);
	return TRUE;
}

static gboolean
button_press_event_cb (GtkTreeView *treeview, GdkEventButton *event, LdapFavoriteSelector *fsel)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		do_popup_menu ((GtkWidget*) treeview, event, fsel);
		return TRUE;
	}

	return FALSE;
}

static gboolean idle_update_favorites (LdapFavoriteSelector *fsel);
static gboolean tree_store_drag_drop_cb (GdauiTreeStore *store, const gchar *path,
					 GtkSelectionData *selection_ldap, LdapFavoriteSelector *fsel);
static gboolean tree_store_drag_can_drag_cb (GdauiTreeStore *store, const gchar *path,
					     LdapFavoriteSelector *fsel);
static gboolean tree_store_drag_get_cb (GdauiTreeStore *store, const gchar *path,
					GtkSelectionData *selection_ldap, LdapFavoriteSelector *fsel);

/**
 * ldap_favorite_selector_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
ldap_favorite_selector_new (TConnection *tcnc)
{
	LdapFavoriteSelector *fsel;
	GdaTreeManager *manager;
	gchar *signame;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	fsel = LDAP_FAVORITE_SELECTOR (g_object_new (LDAP_FAVORITE_SELECTOR_TYPE, NULL));

	fsel->priv->tcnc = g_object_ref (tcnc);
	signame = g_strdup_printf ("favorites-changed::%s",
				   t_favorites_type_to_string (T_FAVORITES_LDAP_DN));
	g_signal_connect (t_connection_get_favorites (fsel->priv->tcnc), signame,
			  G_CALLBACK (favorites_changed_cb), fsel);
	g_free (signame);
	signame = g_strdup_printf ("favorites-changed::%s",
				   t_favorites_type_to_string (T_FAVORITES_LDAP_CLASS));
	g_signal_connect (t_connection_get_favorites (fsel->priv->tcnc), signame,
			  G_CALLBACK (favorites_changed_cb), fsel);
	g_free (signame);
	
	/* create tree managers */
	fsel->priv->tree = gda_tree_new ();
	manager = mgr_favorites_new (tcnc, T_FAVORITES_LDAP_DN, ORDER_KEY_LDAP);
        gda_tree_add_manager (fsel->priv->tree, manager);
	g_object_unref (manager);
	manager = mgr_favorites_new (tcnc, T_FAVORITES_LDAP_CLASS, ORDER_KEY_LDAP);
        gda_tree_add_manager (fsel->priv->tree, manager);
	g_object_unref (manager);

	/* update the tree's contents */
	if (! gda_tree_update_all (fsel->priv->tree, NULL)) {
		if (fsel->priv->idle_update_favorites == 0)
			fsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites, fsel);
	}

	/* header */
	GtkWidget *label;
	gchar *str;
	str = g_strdup_printf ("<b>%s</b>", _("Favorites"));
	label = gdaui_bar_new (str);
	g_free (str);
	gdaui_bar_set_icon_from_pixbuf (GDAUI_BAR (label), ui_get_pixbuf_icon (UI_ICON_BOOKMARK));
        gtk_box_pack_start (GTK_BOX (fsel), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* tree model & tree view */
	GtkTreeModel *model;
	GtkWidget *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	model = gdaui_tree_store_new (fsel->priv->tree, COLUMN_LAST,
				      G_TYPE_INT, MGR_FAVORITES_ID_ATT_NAME,
				      G_TYPE_STRING, MGR_FAVORITES_CONTENTS_ATT_NAME,
				      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, "markup",
				      G_TYPE_INT, MGR_FAVORITES_ID_ATT_NAME,
				      G_TYPE_STRING, "descr",
				      G_TYPE_UINT, MGR_FAVORITES_TYPE_ATT_NAME);

	treeview = ui_make_tree_view (model);
	fsel->priv->treeview = treeview;
	g_object_unref (model);

	g_signal_connect (G_OBJECT (treeview), "row-activated",
			  G_CALLBACK (selection_changed_cb), fsel);
	g_signal_connect (G_OBJECT (treeview), "key-press-event",
			  G_CALLBACK (key_press_event_cb), fsel);
	g_signal_connect (G_OBJECT (treeview), "popup-menu",
			  G_CALLBACK (popup_menu_cb), fsel);
	g_signal_connect (G_OBJECT (treeview), "button-press-event",
			  G_CALLBACK (button_press_event_cb), fsel);

	/* icon */
	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COLUMN_ICON);
	g_object_set ((GObject*) renderer, "yalign", 0., NULL);

	/* text */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "markup", COLUMN_MARKUP);
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

	gtk_box_pack_start (GTK_BOX (fsel), sw, TRUE, TRUE, 0);
	gtk_widget_show_all (sw);

	/* DnD */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (treeview), dbo_table, G_N_ELEMENTS (dbo_table),
					      GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview), GDK_BUTTON1_MASK,
						dbo_table, G_N_ELEMENTS (dbo_table),
						GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect (model, "drag-drop",
			  G_CALLBACK (tree_store_drag_drop_cb), fsel);
	g_signal_connect (model, "drag-can-drag",
			  G_CALLBACK (tree_store_drag_can_drag_cb), fsel);
	g_signal_connect (model, "drag-get",
			  G_CALLBACK (tree_store_drag_get_cb), fsel);

	return (GtkWidget*) fsel;
}

static gboolean
idle_update_favorites (LdapFavoriteSelector *fsel)
{
	gboolean done;
	g_print ("%s()\n", __FUNCTION__);
	done = gda_tree_update_all (fsel->priv->tree, NULL);
	if (done)
		fsel->priv->idle_update_favorites = 0;
	else
		fsel->priv->idle_update_favorites = g_timeout_add_seconds (1, (GSourceFunc) idle_update_favorites,
									   fsel);
	return FALSE;
}

static gboolean
tree_store_drag_drop_cb (G_GNUC_UNUSED GdauiTreeStore *store, const gchar *path,
			 GtkSelectionData *selection_ldap, LdapFavoriteSelector *fsel)
{
	TFavorites *bfav;
	TFavoritesAttributes fav;
	GError *error = NULL;
	gint pos;
	gboolean retval = TRUE;
	gint id;
	bfav = t_connection_get_favorites (fsel->priv->tcnc);

	id = t_favorites_find (bfav, 0, (gchar*) gtk_selection_data_get_data (selection_ldap),
				     &fav, NULL);
	if (id < 0) {
		memset (&fav, 0, sizeof (TFavoritesAttributes));
		fav.id = -1;
		fav.type = T_FAVORITES_LDAP_DN;
		fav.name = (gchar*) gtk_selection_data_get_data (selection_ldap);
		fav.descr = NULL;
		fav.contents = (gchar*) gtk_selection_data_get_data (selection_ldap);
	}

	pos = atoi (path);
	/*g_print ("%s() path => %s, pos: %d\n", __FUNCTION__, path, pos);*/
	
	if (! t_favorites_add (bfav, 0, &fav, ORDER_KEY_LDAP, pos, &error)) {
		ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) fsel),
				    _("Could not add favorite: %s"),
				    error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
		retval = FALSE;
	}
	
	if (id >= 0)
		t_favorites_reset_attributes (&fav);

	return retval;
}

static gboolean
tree_store_drag_can_drag_cb (G_GNUC_UNUSED GdauiTreeStore *store, const gchar *path,
			     LdapFavoriteSelector *fsel)
{
	GdaTreeNode *node;
	node = gda_tree_get_node (fsel->priv->tree, path, FALSE);
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
			GtkSelectionData *selection_ldap, LdapFavoriteSelector *fsel)
{
	GdaTreeNode *node;
	node = gda_tree_get_node (fsel->priv->tree, path, FALSE);
	if (node) {
		const GValue *cvalue;
		cvalue = gda_tree_node_get_node_attribute (node, "fav_contents");
		if (cvalue) {
			const gchar *str;
			str = g_value_get_string (cvalue);
			gtk_selection_data_set (selection_ldap, gtk_selection_data_get_target (selection_ldap),
						8, (guchar*) str, strlen (str));
			return TRUE;
		}
	}
	return FALSE;
}

static void
favorites_changed_cb (G_GNUC_UNUSED TFavorites *bfav, LdapFavoriteSelector *fsel)
{
	if (! gda_tree_update_all (fsel->priv->tree, NULL)) {
		if (fsel->priv->idle_update_favorites == 0)
			fsel->priv->idle_update_favorites = g_idle_add ((GSourceFunc) idle_update_favorites, fsel);

	}
}
