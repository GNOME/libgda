/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include "data-manager-perspective.h"
#include "data-console.h"
#include "../browser-favorites.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "data-favorite-selector.h"

/* 
 * Main static functions 
 */
static void data_manager_perspective_class_init (DataManagerPerspectiveClass *klass);
static void data_manager_perspective_init (DataManagerPerspective *stmt);
static void data_manager_perspective_dispose (GObject *object);
static void data_manager_perspective_grab_focus (GtkWidget *widget);

/* BrowserPerspective interface */
static void                 data_manager_perspective_perspective_init (BrowserPerspectiveIface *iface);
static BrowserWindow       *data_manager_perspective_get_window (BrowserPerspective *perspective);
static GtkActionGroup      *data_manager_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *data_manager_perspective_get_actions_ui (BrowserPerspective *perspective);
static void                 data_manager_perspective_get_current_customization (BrowserPerspective *perspective,
										GtkActionGroup **out_agroup,
										const gchar **out_ui);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _DataManagerPerspectivePriv {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
        BrowserConnection *bcnc;
};

GType
data_manager_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (DataManagerPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_manager_perspective_class_init,
			NULL,
			NULL,
			sizeof (DataManagerPerspective),
			0,
			(GInstanceInitFunc) data_manager_perspective_init,
			0
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) data_manager_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "DataManagerPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
data_manager_perspective_class_init (DataManagerPerspectiveClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	GTK_WIDGET_CLASS (klass)->grab_focus = data_manager_perspective_grab_focus;
	object_class->dispose = data_manager_perspective_dispose;
}

static void
data_manager_perspective_grab_focus (GtkWidget *widget)
{
	GtkNotebook *nb;

        nb = GTK_NOTEBOOK (DATA_MANAGER_PERSPECTIVE (widget)->priv->notebook);
        gtk_widget_grab_focus (gtk_notebook_get_nth_page (nb,
                                                          gtk_notebook_get_current_page (nb)));
}

static void
data_manager_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_get_window = data_manager_perspective_get_window;
	iface->i_get_actions_group = data_manager_perspective_get_actions_group;
	iface->i_get_actions_ui = data_manager_perspective_get_actions_ui;
	iface->i_get_current_customization = data_manager_perspective_get_current_customization;
}


static void
data_manager_perspective_init (DataManagerPerspective *perspective)
{
	perspective->priv = g_new0 (DataManagerPerspectivePriv, 1);
	perspective->priv->favorites_shown = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, BrowserFavoritesType fav_type,
                                      const gchar *selection, DataManagerPerspective *perspective);
static void close_button_clicked_cb (GtkWidget *wid, GtkWidget *page_widget);


/**
 * data_manager_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
data_manager_perspective_new (BrowserWindow *bwin)
{
	BrowserConnection *bcnc;
	BrowserPerspective *bpers;
	DataManagerPerspective *perspective;
	gboolean fav_supported;

	bpers = (BrowserPerspective*) g_object_new (TYPE_DATA_MANAGER_PERSPECTIVE, NULL);
	perspective = (DataManagerPerspective*) bpers;
	perspective->priv->bwin = bwin;
        bcnc = browser_window_get_connection (bwin);
        perspective->priv->bcnc = g_object_ref (bcnc);
	fav_supported = browser_connection_get_favorites (bcnc) ? TRUE : FALSE;

	/* contents */
        GtkWidget *paned, *nb, *wid;
        paned = gtk_hpaned_new ();
	if (fav_supported) {
		wid = data_favorite_selector_new (bcnc);
		g_signal_connect (wid, "selection-changed",
				  G_CALLBACK (fav_selection_changed_cb), bpers);
		gtk_paned_pack1 (GTK_PANED (paned), wid, FALSE, TRUE);
		gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
		perspective->priv->favorites = wid;
	}

	nb = gtk_notebook_new ();
        perspective->priv->notebook = nb;
        gtk_paned_pack2 (GTK_PANED (paned), nb, TRUE, TRUE);
        gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
        gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));

	GtkWidget *page, *tlabel, *button;
	page = data_console_new (bcnc);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), page);

        gtk_notebook_append_page (GTK_NOTEBOOK (nb), page, tlabel);

        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), page, TRUE);
        gtk_notebook_set_group_name (GTK_NOTEBOOK (nb), "data-manager");
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
                                         TRUE);

	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
        gtk_notebook_set_menu_label (GTK_NOTEBOOK (nb), page, tlabel);

	gtk_box_pack_start (GTK_BOX (bpers), paned, TRUE, TRUE, 0);
	gtk_widget_show_all (paned);

	if (perspective->priv->favorites && !perspective->priv->favorites_shown)
		gtk_widget_hide (perspective->priv->favorites);

	gtk_widget_grab_focus (page);

	browser_perspective_declare_notebook (bpers, GTK_NOTEBOOK (perspective->priv->notebook));

	return bpers;
}

static DataConsole *
add_new_data_console (BrowserPerspective *bpers, gint fav_id)
{
	GtkWidget *page, *tlabel, *button;
	DataManagerPerspective *perspective;
	gint page_nb;

	perspective = DATA_MANAGER_PERSPECTIVE (bpers);
	if (fav_id >= 0)
		page = data_console_new_with_fav_id (perspective->priv->bcnc, fav_id);
	else
		page = data_console_new (perspective->priv->bcnc);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
        g_signal_connect (button, "clicked",
                          G_CALLBACK (close_button_clicked_cb), page);

        page_nb = gtk_notebook_append_page (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);
	gtk_widget_show (page);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook), page_nb);

        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (perspective->priv->notebook), page,
                                         TRUE);

	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), NULL);
        gtk_notebook_set_menu_label (GTK_NOTEBOOK (perspective->priv->notebook), page, tlabel);

	return DATA_CONSOLE (page);
}

static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id, BrowserFavoritesType fav_type,
                          const gchar *selection, DataManagerPerspective *perspective)
{
	/* find or create page for this favorite */
        GtkWidget *page_contents;
	gint current_page, npages, i;
	DataConsole *page_to_reuse = NULL;

	if (fav_type != BROWSER_FAVORITES_DATA_MANAGERS)
		return;

	/* change current page if possible */
	npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (perspective->priv->notebook));
	for (i = 0; i < npages; i++) {
		page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (perspective->priv->notebook),
							   i);
		
		if (IS_DATA_CONSOLE (page_contents)) {
			gchar *text;
			text = data_console_get_text (DATA_CONSOLE (page_contents));
			if (text && selection && !strcmp (text, selection)) {
				gtk_notebook_set_current_page (GTK_NOTEBOOK (perspective->priv->notebook),
							       i);
				return;
			}
		}
	}

	/* create a new page, or reuse the current empty one */
	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (perspective->priv->notebook));
	if (current_page >= 0) {
		page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (perspective->priv->notebook),
							   current_page);
		
		if (IS_DATA_CONSOLE (page_contents)) {
			gchar *text;
			text = data_console_get_text (DATA_CONSOLE (page_contents));
			if (!text || !*text)
				page_to_reuse = DATA_CONSOLE (page_contents);
		}
	}

	if (! page_to_reuse)
		page_to_reuse = add_new_data_console ((BrowserPerspective*) perspective, fav_id);
	
	data_console_set_text (page_to_reuse, selection);
	data_console_set_fav_id (page_to_reuse, fav_id, NULL);
	gtk_widget_grab_focus ((GtkWidget*) page_to_reuse);
}

static void
close_button_clicked_cb (G_GNUC_UNUSED GtkWidget *wid, GtkWidget *page_widget)
{
        gtk_widget_destroy (page_widget);
}

static void
data_manager_perspective_dispose (GObject *object)
{
	DataManagerPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_MANAGER_PERSPECTIVE (object));

	perspective = DATA_MANAGER_PERSPECTIVE (object);
	if (perspective->priv) {
		browser_perspective_declare_notebook ((BrowserPerspective*) perspective, NULL);
                if (perspective->priv->bcnc)
                        g_object_unref (perspective->priv->bcnc);

                g_free (perspective->priv);
                perspective->priv = NULL;
        }

	/* parent class */
	parent_class->dispose (object);
}

static void
manager_new_cb (G_GNUC_UNUSED GtkAction *action, BrowserPerspective *bpers)
{
	add_new_data_console (bpers, -1);
}

static void
favorites_toggle_cb (GtkToggleAction *action, BrowserPerspective *bpers)
{
	DataManagerPerspective *perspective;
	perspective = DATA_MANAGER_PERSPECTIVE (bpers);

	if (! perspective->priv->favorites)
		return;
	perspective->priv->favorites_shown = gtk_toggle_action_get_active (action);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
}

static const GtkToggleActionEntry ui_toggle_actions [] =
{
        { "DataManagerFavoritesShow", NULL, N_("_Show favorites"), "F9", N_("Show or hide favorites"), G_CALLBACK (favorites_toggle_cb), FALSE}
};

static GtkActionEntry ui_actions[] = {
        { "DataManagerMenu", NULL, N_("_Manager"), NULL, N_("Manager"), NULL },
        { "NewDataManager", GTK_STOCK_NEW, N_("_New data manager"), "<control>T", N_("New data manager"),
          G_CALLBACK (manager_new_cb)},
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
        "    <menu name='Display' action='Display'>"
        "      <menuitem name='DataManagerFavoritesShow' action='DataManagerFavoritesShow'/>"
        "    </menu>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='Data manager' action='DataManagerMenu'>"
        "        <menuitem name='NewDataManager' action= 'NewDataManager'/>"
        "      </menu>"
	"    </placeholder>"
        "  </menubar>"
        "  <toolbar name='ToolBar'>"
        "    <separator/>"
        "    <toolitem action='NewDataManager'/>"
        "  </toolbar>"
        "</ui>";

static GtkActionGroup *
data_manager_perspective_get_actions_group (BrowserPerspective *perspective)
{
	GtkActionGroup *agroup;
	DataManagerPerspective *bpers;
	bpers = DATA_MANAGER_PERSPECTIVE (perspective);
	agroup = gtk_action_group_new ("DataManagerActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
	gtk_action_group_add_toggle_actions (agroup, ui_toggle_actions,
					     G_N_ELEMENTS (ui_toggle_actions), bpers);

	GtkAction *action;
	action = gtk_action_group_get_action (agroup, "DataManagerFavoritesShow");
	if (bpers->priv->favorites)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      bpers->priv->favorites_shown);
	else
		gtk_action_set_sensitive (GTK_ACTION (action), FALSE);

	return agroup;
}

static const gchar *
data_manager_perspective_get_actions_ui (G_GNUC_UNUSED BrowserPerspective *bpers)
{
	return ui_actions_info;
}

/**
 * data_manager_perspective_new_tab
 * @dmp: a #DataManagerPerspective perspective
 * @xml_spec: the XML specifications to use
 *
 * Make @dmp create a new page (unless the current one is empty)
 */
void
data_manager_perspective_new_tab (DataManagerPerspective *dmp, const gchar *xml_spec)
{
	gint current;
	GtkWidget *page = NULL;

	g_return_if_fail (IS_DATA_MANAGER_PERSPECTIVE (dmp));
	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (dmp->priv->notebook));
	if (current >= 0) {
		page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (dmp->priv->notebook), current);
		if (! IS_DATA_CONSOLE (page))
			page = NULL;
		else {
			gchar *text;
			text = data_console_get_text (DATA_CONSOLE (page));
			if (text && *text)
				page = NULL;
			g_free (text);
		}
	}

	if (!page) {
		add_new_data_console (BROWSER_PERSPECTIVE (dmp), -1);
		current = gtk_notebook_get_current_page (GTK_NOTEBOOK (dmp->priv->notebook));
		page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (dmp->priv->notebook), current);
		g_assert (IS_DATA_CONSOLE (page));
	}

	data_console_set_text (DATA_CONSOLE (page), xml_spec); 
	data_console_execute (DATA_CONSOLE (page));
	gtk_widget_grab_focus (page);
}

static void
data_manager_perspective_get_current_customization (BrowserPerspective *perspective,
						    GtkActionGroup **out_agroup,
						    const gchar **out_ui)
{
	DataManagerPerspective *bpers;
	GtkWidget *page_contents;

	bpers = DATA_MANAGER_PERSPECTIVE (perspective);
	page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook),
						   gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook)));
	if (IS_BROWSER_PAGE (page_contents)) {
		*out_agroup = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		*out_ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
}

static BrowserWindow *
data_manager_perspective_get_window (BrowserPerspective *perspective)
{
	DataManagerPerspective *bpers;
	bpers = DATA_MANAGER_PERSPECTIVE (perspective);
	return bpers->priv->bwin;
}
