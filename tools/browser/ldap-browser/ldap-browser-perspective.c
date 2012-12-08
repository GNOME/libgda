/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "../support.h"
#include "../browser-page.h"
#include "ldap-favorite-selector.h"
#include "../browser-stock-icons.h"

/* 
 * Main static functions 
 */
static void ldap_browser_perspective_class_init (LdapBrowserPerspectiveClass *klass);
static void ldap_browser_perspective_init (LdapBrowserPerspective *stmt);
static void ldap_browser_perspective_dispose (GObject *object);

/* BrowserPerspective interface */
static void                 ldap_browser_perspective_perspective_init (BrowserPerspectiveIface *iface);
static BrowserWindow       *ldap_browser_perspective_get_window (BrowserPerspective *perspective);
static GtkActionGroup      *ldap_browser_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *ldap_browser_perspective_get_actions_ui (BrowserPerspective *perspective);
static void                 ldap_browser_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page);
static void                 ldap_browser_perspective_get_current_customization (BrowserPerspective *perspective,
										GtkActionGroup **out_agroup,
										const gchar **out_ui);

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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "LdapBrowserPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
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
	iface->i_get_window = ldap_browser_perspective_get_window;
	iface->i_get_actions_group = ldap_browser_perspective_get_actions_group;
	iface->i_get_actions_ui = ldap_browser_perspective_get_actions_ui;
	iface->i_page_tab_label_change = ldap_browser_perspective_page_tab_label_change;
	iface->i_get_current_customization = ldap_browser_perspective_get_current_customization;
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

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, ToolsFavoritesType fav_type,
				      const gchar *selection, LdapBrowserPerspective *bpers);
/**
 * ldap_browser_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
ldap_browser_perspective_new (BrowserWindow *bwin)
{
	BrowserConnection *bcnc;
	BrowserPerspective *bpers;
	LdapBrowserPerspective *perspective;

	bpers = (BrowserPerspective*) g_object_new (TYPE_LDAP_BROWSER_PERSPECTIVE, NULL);
	perspective = (LdapBrowserPerspective*) bpers;

	perspective->priv->bwin = bwin;

	/* contents */
	GtkWidget *paned, *wid, *nb, *button, *tlabel;
	bcnc = browser_window_get_connection (bwin);
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	wid = ldap_favorite_selector_new (bcnc);
	g_signal_connect (wid, "selection-changed",
			  G_CALLBACK (fav_selection_changed_cb), bpers);
	gtk_paned_add1 (GTK_PANED (paned), wid);
	gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
	perspective->priv->favorites = wid;

	nb = gtk_notebook_new ();
	perspective->priv->notebook = nb;
	gtk_paned_add2 (GTK_PANED (paned), nb);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));

	wid = ldap_entries_page_new (bcnc, NULL);
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

	browser_perspective_declare_notebook (bpers, GTK_NOTEBOOK (perspective->priv->notebook));

	return bpers;
}

static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gint fav_id,
			  ToolsFavoritesType fav_type,
			  const gchar *selection, LdapBrowserPerspective *bpers)
{
	if (fav_type == GDA_TOOLS_FAVORITES_LDAP_DN) {
		ldap_browser_perspective_display_ldap_entry (bpers, selection);
	}
	if (fav_type == GDA_TOOLS_FAVORITES_LDAP_CLASS) {
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
		browser_perspective_declare_notebook ((BrowserPerspective*) perspective, NULL);
		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
favorites_toggle_cb (GtkToggleAction *action, BrowserPerspective *bpers)
{
	LdapBrowserPerspective *perspective;
	perspective = LDAP_BROWSER_PERSPECTIVE (bpers);
	perspective->priv->favorites_shown = gtk_toggle_action_get_active (action);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
}

static void
ldab_ldap_entries_page_add_cb (G_GNUC_UNUSED GtkAction *action, BrowserPerspective *bpers)
{
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        BrowserConnection *bcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (bpers);
        bcnc = browser_window_get_connection (perspective->priv->bwin);

        page = ldap_entries_page_new (bcnc, NULL);
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
ldab_ldap_classes_page_add_cb (G_GNUC_UNUSED GtkAction *action, BrowserPerspective *bpers)
{
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        BrowserConnection *bcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (bpers);
        bcnc = browser_window_get_connection (perspective->priv->bwin);

        page = ldap_classes_page_new (bcnc, NULL);
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
ldab_search_add_cb (G_GNUC_UNUSED GtkAction *action, BrowserPerspective *bpers)
{
        GtkWidget *page, *tlabel, *button;
        LdapBrowserPerspective *perspective;
        BrowserConnection *bcnc;
        gint i;

        perspective = LDAP_BROWSER_PERSPECTIVE (bpers);
        bcnc = browser_window_get_connection (perspective->priv->bwin);

	i = gtk_notebook_get_current_page (GTK_NOTEBOOK (perspective->priv->notebook));
	GtkWidget *child;
	const gchar *dn = NULL;
	child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (perspective->priv->notebook), i);
	if (IS_LDAP_ENTRIES_PAGE (child))
		dn = ldap_entries_page_get_current_dn (LDAP_ENTRIES_PAGE (child));

        page = ldap_search_page_new (bcnc, dn);
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

static const GtkToggleActionEntry ui_toggle_actions [] =
	{
		{ "LdapToolsFavoritesShow", NULL, N_("_Show favorites"), "F9", N_("Show or hide favorites"), G_CALLBACK (favorites_toggle_cb), FALSE }
	};

static GtkActionEntry ui_actions[] = {
	{ "LDAP", NULL, N_("_LDAP"), NULL, N_("LDAP"), NULL },
        { "LdapLdapEntriesPageNew", BROWSER_STOCK_LDAP_ENTRIES, N_("_New LDAP entries browser"), "<control>T", N_("Open a new LDAP entries browser"),
          G_CALLBACK (ldab_ldap_entries_page_add_cb)},
        { "LdapLdapClassesPageNew", NULL, N_("_New LDAP classes browser"), "<control>C", N_("Open a new LDAP classes browser"),
	  G_CALLBACK (ldab_ldap_classes_page_add_cb)},
        { "LdapSearchNew", GTK_STOCK_FIND, N_("_New LDAP search"), "<control>G", N_("Open a new LDAP search form"),
	  G_CALLBACK (ldab_search_add_cb)},
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
	"    <menu name='Display' action='Display'>"
	"      <menuitem name='LdapToolsFavoritesShow' action='LdapToolsFavoritesShow'/>"
        "    </menu>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='LDAP' action='LDAP'>"
        "        <menuitem name='LdapSearchNew' action= 'LdapSearchNew'/>"
        "        <menuitem name='LdapLdapEntriesPageNew' action= 'LdapLdapEntriesPageNew'/>"
        "        <menuitem name='LdapLdapClassesPageNew' action= 'LdapLdapClassesPageNew'/>"
        "      </menu>"
        "    </placeholder>"
        "  </menubar>"
	"  <toolbar name='ToolBar'>"
        "    <separator/>"
        "    <toolitem action='LdapSearchNew'/>"
        "    <toolitem action='LdapLdapEntriesPageNew'/>"
        "  </toolbar>"
        "</ui>";

static GtkActionGroup *
ldap_browser_perspective_get_actions_group (BrowserPerspective *bpers)
{
	GtkActionGroup *agroup;
	agroup = gtk_action_group_new ("LdapBrowserActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
	gtk_action_group_add_toggle_actions (agroup, ui_toggle_actions, G_N_ELEMENTS (ui_toggle_actions),
					     bpers);
	GtkAction *action;
	action = gtk_action_group_get_action (agroup, "LdapToolsFavoritesShow");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      LDAP_BROWSER_PERSPECTIVE (bpers)->priv->favorites_shown);	

	return agroup;
}

static const gchar *
ldap_browser_perspective_get_actions_ui (G_GNUC_UNUSED BrowserPerspective *bpers)
{
	return ui_actions_info;
}

static void
ldap_browser_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page)
{
	LdapBrowserPerspective *bpers;
	GtkWidget *tab_label;
	GtkWidget *close_btn;
	
	bpers = LDAP_BROWSER_PERSPECTIVE (perspective);
	tab_label = browser_page_get_tab_label (page, &close_btn);
	if (tab_label) {
		gtk_notebook_set_tab_label (GTK_NOTEBOOK (bpers->priv->notebook),
					    GTK_WIDGET (page), tab_label);
		g_signal_connect (close_btn, "clicked",
				  G_CALLBACK (close_button_clicked_cb), page);
		
		tab_label = browser_page_get_tab_label (page, NULL);
		gtk_notebook_set_menu_label (GTK_NOTEBOOK (bpers->priv->notebook),
					     GTK_WIDGET (page), tab_label);
	}
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

static void
ldap_browser_perspective_get_current_customization (BrowserPerspective *perspective,
						    GtkActionGroup **out_agroup,
						    const gchar **out_ui)
{
	LdapBrowserPerspective *bpers;
	GtkWidget *page_contents;

	bpers = LDAP_BROWSER_PERSPECTIVE (perspective);
	page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook),
						   gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook)));
	if (IS_BROWSER_PAGE (page_contents)) {
		*out_agroup = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		*out_ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
}

static BrowserWindow *
ldap_browser_perspective_get_window (BrowserPerspective *perspective)
{
	LdapBrowserPerspective *bpers;
	bpers = LDAP_BROWSER_PERSPECTIVE (perspective);
	return bpers->priv->bwin;
}
