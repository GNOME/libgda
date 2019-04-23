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
#include "ldap-classes-page.h"
#include "classes-view.h"
#include "class-properties.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "../gdaui-bar.h"
#include "../browser-page.h"
#include "../browser-window.h"
#include "common/t-connection.h"
#include <providers/ldap/gda-ldap-connection.h>
#include "mgr-ldap-classes.h"
#include <libgda-ui/gdaui-tree-store.h>
#include <libgda/gda-debug-macros.h>

typedef struct {
	gchar *classname;
	GtkTreeRowReference *reference;
} HistoryItem;

static void
history_item_free (HistoryItem *item)
{
	g_free (item->classname);
	gtk_tree_row_reference_free (item->reference);
	g_free (item);
}

struct _LdapClassesPagePrivate {
	TConnection *tcnc;

	GtkWidget *classes_view;
	GtkWidget *entry_props;

	GArray *history_items; /* array of @HistoryItem */
	guint history_max_len; /* max allowed length of @history_items */
	gint current_hitem; /* index in @history_items, or -1 */
	gboolean add_hist_item;
};

static void ldap_classes_page_class_init (LdapClassesPageClass *klass);
static void ldap_classes_page_init       (LdapClassesPage *ebrowser, LdapClassesPageClass *klass);
static void ldap_classes_page_dispose   (GObject *object);

/* BrowserPage interface */
static void                 ldap_classes_page_page_init (BrowserPageIface *iface);
static void                 ldap_classes_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
static GtkWidget           *ldap_classes_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static GObjectClass *parent_class = NULL;


/*
 * LdapClassesPage class implementation
 */

static void
ldap_classes_page_class_init (LdapClassesPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ldap_classes_page_dispose;
}

static void
ldap_classes_page_page_init (BrowserPageIface *iface)
{
	iface->i_customize = ldap_classes_customize;
	iface->i_uncustomize = NULL;
	iface->i_get_tab_label = ldap_classes_page_get_tab_label;
}

static void
ldap_classes_page_init (LdapClassesPage *ebrowser, G_GNUC_UNUSED LdapClassesPageClass *klass)
{
	ebrowser->priv = g_new0 (LdapClassesPagePrivate, 1);
	ebrowser->priv->history_items = g_array_new (FALSE, FALSE, sizeof (HistoryItem*));
	ebrowser->priv->history_max_len = 20;
	ebrowser->priv->add_hist_item = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (ebrowser), GTK_ORIENTATION_VERTICAL);
}

static void
ldap_classes_page_dispose (GObject *object)
{
	LdapClassesPage *ebrowser = (LdapClassesPage *) object;

	/* free memory */
	if (ebrowser->priv) {
		if (ebrowser->priv->tcnc)
			g_object_unref (ebrowser->priv->tcnc);
		if (ebrowser->priv->history_items) {
			guint i;
			for (i = 0; i < ebrowser->priv->history_items->len; i++) {
				HistoryItem *hitem = g_array_index (ebrowser->priv->history_items,
								    HistoryItem*, i);
				history_item_free (hitem);
			}
			g_array_free (ebrowser->priv->history_items, TRUE);
		}
		g_free (ebrowser->priv);
		ebrowser->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
ldap_classes_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (LdapClassesPageClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ldap_classes_page_class_init,
			NULL,
			NULL,
			sizeof (LdapClassesPage),
			0,
			(GInstanceInitFunc) ldap_classes_page_init,
			0
		};

		static GInterfaceInfo page_info = {
                        (GInterfaceInitFunc) ldap_classes_page_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "LdapClassesPage", &info, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_info);
	}
	return type;
}

static gchar *
ldap_classes_page_to_selection (G_GNUC_UNUSED LdapClassesPage *ebrowser)
{
	const gchar *current_classname;
	current_classname = classes_view_get_current_class (CLASSES_VIEW (ebrowser->priv->classes_view));
	if (current_classname)
		return g_strdup (current_classname);
	else
		return NULL;
}

static void
source_drag_data_get_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint browser, G_GNUC_UNUSED guint time, LdapClassesPage *ebrowser)
{
	switch (browser) {
	case TARGET_KEY_VALUE: {
		gchar *str;
		str = ldap_classes_page_to_selection (ebrowser);
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8, (guchar*) str,
					strlen (str));
		g_free (str);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gtk_selection_data_set_text (selection_data, ldap_classes_page_get_current_class (ebrowser), -1);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

static void
update_history_actions (LdapClassesPage *ebrowser)
{
	if (!customization_data_exists (G_OBJECT (ebrowser)))
		return;

	gboolean is_first = TRUE;
	gboolean is_last = TRUE;
	const gchar *current_classname;
	ebrowser->priv->current_hitem = -1;
	current_classname = ldap_classes_page_get_current_class (ebrowser);
	if (current_classname) {
		guint i;
		for (i = 0; i < ebrowser->priv->history_items->len; i++) {
			HistoryItem *hitem;
			hitem = g_array_index (ebrowser->priv->history_items, HistoryItem*, i);
			if (!strcmp (hitem->classname, current_classname)) {
				if (i != 0)
					is_first = FALSE;
				if (i+1 < ebrowser->priv->history_items->len)
					is_last = FALSE;
				ebrowser->priv->current_hitem = (gint) i;
				break;
			}
		}
	}

	GAction *action;
        action = customization_data_get_action (G_OBJECT (ebrowser), "ClassBack");
        if (action)
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_first);
	action = customization_data_get_action (G_OBJECT (ebrowser), "ClassForward");
        if (action)
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_last);
}

static void
selection_changed_cb (GtkTreeSelection *sel, LdapClassesPage *ebrowser)
{
	const gchar *current_classname;
	current_classname = classes_view_get_current_class (CLASSES_VIEW (ebrowser->priv->classes_view));
	class_properties_set_class (CLASS_PROPERTIES (ebrowser->priv->entry_props), current_classname);

	if (!current_classname)
		return;

	GAction *action;
	action = customization_data_get_action (G_OBJECT (ebrowser), "AddToFav");
	if (action) {
		current_classname = ldap_classes_page_get_current_class (ebrowser);
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
					     (current_classname && *current_classname) ? TRUE : FALSE);
	}

	GtkTreeModel *model;
	GtkTreeIter iter;
	if (ebrowser->priv->add_hist_item && gtk_tree_selection_get_selected (sel, &model, &iter)) {
		GtkTreePath *path;
		HistoryItem *hitem;
		hitem = g_new (HistoryItem, 1);
		hitem->classname = g_strdup (current_classname);
		
		path = gtk_tree_model_get_path (model, &iter);
		if (path) {
			hitem->reference = gtk_tree_row_reference_new (model, path);
			gtk_tree_path_free (path);
		}
		else
			hitem->reference = NULL;

		if (ebrowser->priv->current_hitem >= 0) {
			guint i;
			for (i = (guint) ebrowser->priv->current_hitem + 1;
			     i < ebrowser->priv->history_items->len; ) {
				HistoryItem *tmphitem;
				tmphitem = g_array_index (ebrowser->priv->history_items, HistoryItem*, i);
				history_item_free (tmphitem);
				g_array_remove_index (ebrowser->priv->history_items, i);
			}
		}
		g_array_append_val (ebrowser->priv->history_items, hitem);
		if (ebrowser->priv->history_items->len > ebrowser->priv->history_max_len) {
			hitem = g_array_index (ebrowser->priv->history_items, HistoryItem*, 0);
			history_item_free (hitem);
			g_array_remove_index (ebrowser->priv->history_items, 0);
		}
		ebrowser->priv->current_hitem = ebrowser->priv->history_items->len -1;
	}
	update_history_actions (ebrowser);
}

static void
open_classname_requested_cb (G_GNUC_UNUSED ClassProperties *eprop, const gchar *classname, LdapClassesPage *ebrowser)
{
	ldap_classes_page_set_current_class (ebrowser, classname);
}

/**
 * ldap_classes_page_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
ldap_classes_page_new (TConnection *tcnc, const gchar *classname)
{
	LdapClassesPage *ebrowser;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	ebrowser = LDAP_CLASSES_PAGE (g_object_new (LDAP_CLASSES_PAGE_TYPE, NULL));
	ebrowser->priv->tcnc = T_CONNECTION (g_object_ref ((GObject*) tcnc));

	/* header */
        GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>", _("LDAP classes browser"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (ebrowser), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	g_signal_connect (label, "drag-data-get",
			  G_CALLBACK (source_drag_data_get_cb), ebrowser);

	/* VPaned widget */
	GtkWidget *hp;
	hp = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (ebrowser), hp, TRUE, TRUE, 0);

	/* tree */
	GtkWidget *vbox, *hview, *sw;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
	gtk_paned_add1 (GTK_PANED (hp), vbox);
	
	str = g_strdup_printf ("<b>%s:</b>", _("LDAP classes"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hview = classes_view_new (tcnc, NULL);
	ebrowser->priv->classes_view = hview;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), hview);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

	/* tree selection */
	GtkTreeSelection *sel;
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ebrowser->priv->classes_view));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
	g_signal_connect (sel, "changed",
			  G_CALLBACK (selection_changed_cb), ebrowser);

	/* details */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
	gtk_paned_add2 (GTK_PANED (hp), vbox);

	str = g_strdup_printf ("<b>%s:</b>", _("LDAP class's properties"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	GtkWidget *props;
	props = class_properties_new (tcnc);
	gtk_box_pack_start (GTK_BOX (vbox), props, TRUE, TRUE, 0);
	ebrowser->priv->entry_props = props;
	g_signal_connect (props, "open-class",
			  G_CALLBACK (open_classname_requested_cb), ebrowser);

	gtk_paned_set_position (GTK_PANED (hp), 250);

	gtk_widget_show_all (hp);

	if (classname)
		classes_view_set_current_class (CLASSES_VIEW (ebrowser->priv->classes_view), classname);

	return (GtkWidget*) ebrowser;
}

/**
 * ldap_classes_page_get_current_class:
 */
const gchar *
ldap_classes_page_get_current_class (LdapClassesPage *ldap_classes_page)
{
	g_return_val_if_fail (IS_LDAP_CLASSES_PAGE (ldap_classes_page), NULL);
	return classes_view_get_current_class (CLASSES_VIEW (ldap_classes_page->priv->classes_view));
}

/**
 * ldap_classes_page_set_current_class:
 */
void
ldap_classes_page_set_current_class (LdapClassesPage *ldap_classes_page, const gchar *classname)
{
	g_return_if_fail (IS_LDAP_CLASSES_PAGE (ldap_classes_page));
	classes_view_set_current_class (CLASSES_VIEW (ldap_classes_page->priv->classes_view), classname);
}


/*
 * UI actions
 */
static void
action_add_to_fav_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	LdapClassesPage *ebrowser;
	ebrowser = LDAP_CLASSES_PAGE (data);

	TFavorites *bfav;
        TFavoritesAttributes fav;
        GError *error = NULL;

	classes_view_get_current_class (CLASSES_VIEW (ebrowser->priv->classes_view));
        memset (&fav, 0, sizeof (TFavoritesAttributes));
        fav.id = -1;
        fav.type = T_FAVORITES_LDAP_CLASS;
        fav.name = ldap_classes_page_to_selection (ebrowser);
        fav.descr = NULL;
        fav.contents = ldap_classes_page_to_selection (ebrowser);

        bfav = t_connection_get_favorites (ebrowser->priv->tcnc);
        if (! t_favorites_add (bfav, 0, &fav, ORDER_KEY_LDAP, G_MAXINT, &error)) {
                ui_show_error ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) ebrowser),
                                    _("Could not add favorite: %s"),
                                    error && error->message ? error->message : _("No detail"));
                if (error)
                        g_error_free (error);
        }
	g_free (fav.contents);
}

static void
action_class_back_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	LdapClassesPage *ebrowser;
	ebrowser = LDAP_CLASSES_PAGE (data);

	ebrowser->priv->add_hist_item = FALSE;
	if (ebrowser->priv->current_hitem > 0) {
		HistoryItem *hitem = NULL;
		gboolean use_dn = TRUE;
		hitem = g_array_index (ebrowser->priv->history_items, HistoryItem*,
				       ebrowser->priv->current_hitem - 1);
		if (hitem->reference) {
			if (gtk_tree_row_reference_valid (hitem->reference)) {
				GtkTreeSelection *sel;
				GtkTreePath *path;
				path = gtk_tree_row_reference_get_path (hitem->reference);
				sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ebrowser->priv->classes_view));
				gtk_tree_selection_select_path (sel, path);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (ebrowser->priv->classes_view),
							      path, NULL, TRUE, 0.5, 0.5);
				gtk_tree_path_free (path);
				use_dn = FALSE;
			}
			else {
				gtk_tree_row_reference_free (hitem->reference);
				hitem->reference = NULL;
			}
		}
		if (use_dn)
			classes_view_set_current_class (CLASSES_VIEW (ebrowser->priv->classes_view),
							hitem->classname);
	}
	ebrowser->priv->add_hist_item = TRUE;
}

static void
action_class_forward_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	LdapClassesPage *ebrowser;
	ebrowser = LDAP_CLASSES_PAGE (data);

	ebrowser->priv->add_hist_item = FALSE;
	if ((ebrowser->priv->current_hitem >=0) &&
	    ((guint) ebrowser->priv->current_hitem < ebrowser->priv->history_items->len)) {
		HistoryItem *hitem = NULL;
		gboolean use_dn = TRUE;
		hitem = g_array_index (ebrowser->priv->history_items, HistoryItem*,
				       ebrowser->priv->current_hitem + 1);
		if (hitem->reference) {
			if (gtk_tree_row_reference_valid (hitem->reference)) {
				GtkTreeSelection *sel;
				GtkTreePath *path;
				path = gtk_tree_row_reference_get_path (hitem->reference);
				sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (ebrowser->priv->classes_view));
				gtk_tree_selection_select_path (sel, path);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (ebrowser->priv->classes_view),
							      path, NULL, TRUE, 0.5, 0.5);
				gtk_tree_path_free (path);
				use_dn = FALSE;
			}
			else {
				gtk_tree_row_reference_free (hitem->reference);
				hitem->reference = NULL;
			}
		}
		if (use_dn)
			classes_view_set_current_class (CLASSES_VIEW (ebrowser->priv->classes_view),
							hitem->classname);
	}
	ebrowser->priv->add_hist_item = TRUE;
}

static GActionEntry win_entries[] = {
        { "AddToFav", action_add_to_fav_cb, NULL, NULL, NULL },
	{ "ClassBack", action_class_back_cb, NULL, NULL, NULL },
	{ "ClassForward", action_class_forward_cb, NULL, NULL, NULL },
};

static void
ldap_classes_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (page), toolbar, header);

	/* add perspective's actions */
	customization_data_add_actions (G_OBJECT (page), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "bookmark-new-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Add class to Favorites"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.AddToFav");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "go-previous-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Move back to previous LDAP class"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.ClassBack");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "go-next-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Move to next LDAP class"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.ClassForward");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (page), G_OBJECT (titem));
}

static GtkWidget *
ldap_classes_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	const gchar *tab_name;
	GdkPixbuf *classes_pixbuf;

	classes_pixbuf = ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_STRUCTURAL);
	tab_name = _("LDAP classes");
	return ui_make_tab_label_with_pixbuf (tab_name,
						   classes_pixbuf,
						   out_close_button ? TRUE : FALSE, out_close_button);
}
