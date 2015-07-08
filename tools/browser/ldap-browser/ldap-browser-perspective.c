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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "ldap-browser-perspective.h"
#include "../browser-window.h"
#include "ldap-entries-page.h"
#include "ldap-classes-page.h"
#include "ldap-search-page.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "../browser-page.h"
#include "ldap-favorite-selector.h"

/* 
 * Main static functions 
 */
static void ldap_browser_perspective_class_init (LdapBrowserPerspectiveClass *klass);
static void ldap_browser_perspective_init (LdapBrowserPerspective *stmt);
static void ldap_browser_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 ldap_browser_perspective_perspective_init (BrowserPerspectiveIface *iface);
static GtkWidget           *ldap_browser_perspective_get_notebook (BrowserPerspective *perspective);
static void                 ldap_browser_perspective_customize (BrowserPerspective *perspective,
								GtkToolbar *toolbar, GtkHeaderBar *header);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _LdapBrowserPerspectivePrivate {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
};

GType
ldap_browser_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (LdapBrowserPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ldap_browser_perspective_class_init,
			NULL,
			NULL,
			sizeof (LdapBrowserPerspective),
			0,
			(GInstanceInitFunc) ldap_browser_perspective_init,
			0
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) ldap_browser_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "LdapBrowserPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
ldap_browser_perspective_class_init (LdapBrowserPerspectiveClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ldap_browser_perspective_dispose;
}

static void
ldap_browser_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_get_notebook = ldap_browser_perspective_get_notebook;
	iface->i_customize = ldap_browser_perspective_customize;
        iface->i_uncustomize = NULL;
}


static void
ldap_browser_perspective_init (LdapBrowserPerspective *perspective)
{
	perspective->priv = g_new0 (LdapBrowserPerspectivePrivate, 1);
	perspective->priv->favorites_shown = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void
close_button_clicked_cb (G_GNUC_UNUSED GtkWidget *wid, GtkWidget *page_widget)
{
	gtk_widget_destroy (page_widget);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
				      const gchar *selection, LdapBrowserPerspective *bpers);
/**
 * ldap_browser_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
ldap_browser_perspective_new (BrowserWindow *bwin)
{
	TConnection *tcnc;
	BrowserPerspective *bpers;
	LdapBrowserPerspective *perspective;

	bpers = (BrowserPerspective*) g_object_new (TYPE_LDAP_BROWSER_PERSPECTIVE, NULL);
	perspective = (LdapBrowserPerspective*) bpers;

	perspective->priv->bwin = bwin;

	/* contents */
	GtkWidget *paned, *wid, *nb, *button, *tlabel;
	tcnc = browser_window_get_connection (bwin);
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	wid = ldap_favorite_selector_new (tcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (fav_selection_changed_cb), bpers);
	gtk_paned_add1 (GTK_PANED (paned), wid);
	gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
	perspective->priv->favorites = wid;

	nb = browser_perspective_create_notebook (bpers);
	perspective->priv->notebook = nb;
	gtk_paned_add2 (GTK_PANED (paned), nb);

	wid = ldap_entries_page_new (tcnc, NULL);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (wid), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), wid);

	gtk_notebook_append_page (GTK_NOTEBOOK (nb), wid, tlabel);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), wid, TRUE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (nb), "ldap-browser");

	gtk_box_pack_start (GTK_BOX (bpers), paned, TRUE, TRUE, 0);
	gtk_widget_show_all (paned);

	if (!perspective->priv->favorites_shown)
		gtk_widget_hide (perspective->priv->favorites);

	return bpers;
}

static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gint fav_id,
			  TFavoritesType fav_type,
			  const gchar *selection, LdapBrowserPerspective *bpers)
{
	if (fav_type == T_FAVORITES_LDAP_DN) {
		ldap_browser_perspective_display_ldap_entry (bpers, selection);
	}
	if (fav_type == T_FAVORITES_LDAP_CLASS) {
		ldap_browser_perspective_display_ldap_class (bpers, selection);
	}
#ifdef GDA_DEBUG_NO
	g_print ("Reacted to selection fav_id=>%d type=>%u, contents=>%s\n", fav_id, fav_type, selection);	
#endif
}

static void
ldap_browser_perspective_dispose (GObject *object)
{
	LdapBrowserPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LDAP_BROWSER_PERSPECTIVE (object));

	perspective = LDAP_BROWSER_PERSPECTIVE (object);
	if (perspective->priv) {
		if (customization_data_exists (object))
			customization_data_release (object);

		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
favorites_toggle_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a LdapBrowserPerspective */
	LdapBrowserPerspective *perspective;
	perspective = LDAP_BROWSER_PERSPECTIVE (data);
	perspective->priv->favorites_shown = g_variant_get_boolean (state);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
	g_simple_action_set_state (action, state);
}

static void
ldab_ldap_entries_page_add_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a LdapBrowserPerspective */
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        TConnection *tcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (data);
        tcnc = browser_window_get_connection (perspective->priv->bwin);

        page = ldap_entries_page_new (tcnc, NULL);
        gtk_widget_show (page);
        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), page);

        i = gtk_notebook_append_page (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook), i);

        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (perspective->priv->notebook), page, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
                                         TRUE);

        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
        gtk_notebook_set_menu_label (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);

        gtk_widget_grab_focus (page);
}

static void
ldab_ldap_classes_page_add_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a LdapBrowserPerspective */
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        TConnection *tcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (data);
        tcnc = browser_window_get_connection (perspective->priv->bwin);

        page = ldap_classes_page_new (tcnc, NULL);
        gtk_widget_show (page);
        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), page);

        i = gtk_notebook_append_page (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook), i);

        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (perspective->priv->notebook), page, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
                                         TRUE);

        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
        gtk_notebook_set_menu_label (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);

        gtk_widget_grab_focus (page);
}

static void
ldab_search_add_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a LdapBrowserPerspective */
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        TConnection *tcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (data);
        tcnc = browser_window_get_connection (perspective->priv->bwin);

	i = gtk_notebook_get_current_page (GTK_NOTEBOOK (perspective->priv->notebook));
	GtkWidget *child;
	const gchar *dn = NULL;
	child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (perspective->priv->notebook), i);
	if (IS_LDAP_ENTRIES_PAGE (child))
		dn = ldap_entries_page_get_current_dn (LDAP_ENTRIES_PAGE (child));

        page = ldap_search_page_new (tcnc, dn);
        gtk_widget_show (page);
        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), page);

        i = gtk_notebook_append_page (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook), i);

        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (perspective->priv->notebook), page, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
                                         TRUE);

        tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
        gtk_notebook_set_menu_label (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);

        gtk_widget_grab_focus (page);
}

static GtkWidget *
ldap_browser_perspective_get_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_LDAP_BROWSER_PERSPECTIVE (perspective), NULL);
	return LDAP_BROWSER_PERSPECTIVE (perspective)->priv->notebook;
}

static GActionEntry win_entries[] = {
        { "show-favorites", NULL, NULL, "true", favorites_toggle_cb },
        { "entries-page-new", ldab_ldap_entries_page_add_cb, NULL, NULL, NULL },
	{ "classes-page-new", ldab_ldap_classes_page_add_cb, NULL, NULL, NULL },
	{ "search-page-new", ldab_search_add_cb, NULL, NULL, NULL },
};

static void
ldap_browser_perspective_customize (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	/* add perspective's actions */
	customization_data_add_actions (G_OBJECT (perspective), win_entries, G_N_ELEMENTS (win_entries));

	/* add to toolbar */
	GtkToolItem *titem;
	titem = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "user-bookmarks-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("Show favorites"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.show-favorites");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));

	GdkPixbuf *pix;
	GtkWidget *img;
	pix = ui_get_pixbuf_icon (UI_ICON_LDAP_ORGANIZATION);
	img = gtk_image_new_from_pixbuf (pix);
	gtk_widget_show (img);
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (titem), img);
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("New LDAP entries tab"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.entries-page-new");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));

	pix = ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_STRUCTURAL);
	img = gtk_image_new_from_pixbuf (pix);
	gtk_widget_show (img);
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (titem), img);
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("New LDAP classes tab"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.classes-page-new");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "edit-find");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("New LDAP search tab"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.search-page-new");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));
}

/**
 * ldap_browser_perspective_display_ldap_entry
 *
 * Display (and create if necessary) a new page for the LDAP entry's properties
 */
void
ldap_browser_perspective_display_ldap_entry (LdapBrowserPerspective *bpers, const gchar *dn)
{
	g_return_if_fail (IS_LDAP_BROWSER_PERSPECTIVE (bpers));

	gint ntabs, i;
	ntabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (bpers->priv->notebook));
	i = gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook));
	for (; i < ntabs; i++) {
		GtkWidget *child;
		child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		if (IS_LDAP_ENTRIES_PAGE (child)) {
			ldap_entries_page_set_current_dn (LDAP_ENTRIES_PAGE (child), dn);
			gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
			return;
		}
	}

	/* open a new page */
	GtkWidget *ti;
	ti = ldap_entries_page_new (browser_window_get_connection (bpers->priv->bwin), dn);
	if (ti) {
		GtkWidget *close_btn;
		GtkWidget *tab_label;
		gint i;
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), &close_btn);
		i = gtk_notebook_append_page (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), ti);
		
		gtk_widget_show (ti);
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);
		
		gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						  TRUE);
		gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						 TRUE);
	}
}

/**
 * ldap_browser_perspective_display_ldap_class
 *
 * Display (and create if necessary) a new page for the LDAP class's properties
 */
void
ldap_browser_perspective_display_ldap_class (LdapBrowserPerspective *bpers, const gchar *classname)
{
	g_return_if_fail (IS_LDAP_BROWSER_PERSPECTIVE (bpers));

	gint ntabs, i;
	ntabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (bpers->priv->notebook));
	i = gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook));
	for (; i < ntabs; i++) {
		GtkWidget *child;
		child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		if (IS_LDAP_CLASSES_PAGE (child)) {
			ldap_classes_page_set_current_class (LDAP_CLASSES_PAGE (child), classname);
			gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
			return;
		}
	}

	/* open a new page */
	GtkWidget *ti;
	ti = ldap_classes_page_new (browser_window_get_connection (bpers->priv->bwin), classname);
	if (ti) {
		GtkWidget *close_btn;
		GtkWidget *tab_label;
		gint i;
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), &close_btn);
		i = gtk_notebook_append_page (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), ti);
		
		gtk_widget_show (ti);
		
		tab_label = browser_page_get_tab_label (BROWSER_PAGE (ti), NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook), ti, tab_label);
		
		gtk_notebook_set_current_page (GTK_NOTEBOOK (bpers->priv->notebook), i);
		gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						  TRUE);
		gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (bpers->priv->notebook), ti,
						 TRUE);
	}
}
