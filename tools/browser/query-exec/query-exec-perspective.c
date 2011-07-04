/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "query-exec-perspective.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "query-console-page.h"
#include "../browser-stock-icons.h"
#include "../support.h"
#include "query-favorite-selector.h"
#include "query-editor.h"

/* 
 * Main static functions 
 */
static void query_exec_perspective_class_init (QueryExecPerspectiveClass *klass);
static void query_exec_perspective_init (QueryExecPerspective *stmt);
static void query_exec_perspective_dispose (GObject *object);

static void query_exec_perspective_grab_focus (GtkWidget *widget);

/* BrowserPerspective interface */
static void                 query_exec_perspective_perspective_init (BrowserPerspectiveIface *iface);
static BrowserWindow       *query_exec_perspective_get_window (BrowserPerspective *perspective);
static GtkActionGroup      *query_exec_perspective_get_actions_group (BrowserPerspective *perspective);
static const gchar         *query_exec_perspective_get_actions_ui (BrowserPerspective *perspective);
static void                 query_exec_perspective_get_current_customization (BrowserPerspective *perspective,
									      GtkActionGroup **out_agroup,
									      const gchar **out_ui);
static void                 query_exec_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _QueryExecPerspectivePrivate {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
	BrowserConnection *bcnc;
};

GType
query_exec_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (QueryExecPerspectiveClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_exec_perspective_class_init,
			NULL,
			NULL,
			sizeof (QueryExecPerspective),
			0,
			(GInstanceInitFunc) query_exec_perspective_init,
			0
		};

		static GInterfaceInfo perspective_info = {
                        (GInterfaceInitFunc) query_exec_perspective_perspective_init,
			NULL,
                        NULL
                };
		
		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "QueryExecPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
query_exec_perspective_class_init (QueryExecPerspectiveClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = query_exec_perspective_dispose;
	GTK_WIDGET_CLASS (object_class)->grab_focus = query_exec_perspective_grab_focus;
}

static void
query_exec_perspective_grab_focus (GtkWidget *widget)
{
	GtkNotebook *nb;

	nb = GTK_NOTEBOOK (QUERY_EXEC_PERSPECTIVE (widget)->priv->notebook);
	gtk_widget_grab_focus (gtk_notebook_get_nth_page (nb,
							  gtk_notebook_get_current_page (nb)));
}
static void
query_exec_perspective_perspective_init (BrowserPerspectiveIface *iface)
{
	iface->i_get_window = query_exec_perspective_get_window;
	iface->i_get_actions_group = query_exec_perspective_get_actions_group;
	iface->i_get_actions_ui = query_exec_perspective_get_actions_ui;
	iface->i_get_current_customization = query_exec_perspective_get_current_customization;
	iface->i_page_tab_label_change = query_exec_perspective_page_tab_label_change;
}

static void
query_exec_perspective_init (QueryExecPerspective *perspective)
{
	perspective->priv = g_new0 (QueryExecPerspectivePrivate, 1);
	perspective->priv->favorites_shown = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, BrowserFavoritesType fav_type,
                                      const gchar *selection, QueryExecPerspective *perspective);
static void close_button_clicked_cb (GtkWidget *wid, GtkWidget *page_widget);

/**
 * query_exec_perspective_new
 *
 * Creates new #BrowserPerspective widget which 
 */
BrowserPerspective *
query_exec_perspective_new (BrowserWindow *bwin)
{
	BrowserConnection *bcnc;
	BrowserPerspective *bpers;
	QueryExecPerspective *perspective;
	gboolean fav_supported;

	bpers = (BrowserPerspective*) g_object_new (TYPE_QUERY_EXEC_PERSPECTIVE, NULL);
	perspective = (QueryExecPerspective*) bpers;

	perspective->priv->bwin = bwin;
	bcnc = browser_window_get_connection (bwin);
	perspective->priv->bcnc = g_object_ref (bcnc);
	fav_supported = browser_connection_get_favorites (bcnc) ? TRUE : FALSE;

	/* contents */
	GtkWidget *paned, *nb, *wid;
	paned = gtk_hpaned_new ();
	if (fav_supported) {
		wid = query_favorite_selector_new (bcnc);
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

	page = query_console_page_new (bcnc);
	tlabel = browser_page_get_tab_label (BROWSER_PAGE (page), &button);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (close_button_clicked_cb), page);

	gtk_notebook_append_page (GTK_NOTEBOOK (nb), page, tlabel);

	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb), page, TRUE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (nb), "query-exec");
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

static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id,
			  G_GNUC_UNUSED BrowserFavoritesType fav_type, const gchar *selection,
			  QueryExecPerspective *perspective)
{
	GtkNotebook *nb;
	GtkWidget *page;

	nb = GTK_NOTEBOOK (perspective->priv->notebook);
	page = gtk_notebook_get_nth_page (nb, gtk_notebook_get_current_page (nb));
	if (!page)
		return;
	if (IS_QUERY_CONSOLE_PAGE_PAGE (page)) {
		query_console_page_set_text (QUERY_CONSOLE_PAGE (page), selection, fav_id);
		gtk_widget_grab_focus (page);
	}
	else {
		TO_IMPLEMENT;
	}
}

static void
close_button_clicked_cb (G_GNUC_UNUSED GtkWidget *wid, GtkWidget *page_widget)
{
	gtk_widget_destroy (page_widget);
}


static void
query_exec_perspective_dispose (GObject *object)
{
	QueryExecPerspective *perspective;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_EXEC_PERSPECTIVE (object));

	perspective = QUERY_EXEC_PERSPECTIVE (object);
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
query_exec_add_cb (G_GNUC_UNUSED GtkAction *action, BrowserPerspective *bpers)
{
	GtkWidget *page, *tlabel, *button;
	QueryExecPerspective *perspective;
	BrowserConnection *bcnc;
	gint i;

	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	bcnc = perspective->priv->bcnc;

	page = query_console_page_new (bcnc);
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
favorites_toggle_cb (GtkToggleAction *action, BrowserPerspective *bpers)
{
	QueryExecPerspective *perspective;
	perspective = QUERY_EXEC_PERSPECTIVE (bpers);
	if (!perspective->priv->favorites)
		return;

	perspective->priv->favorites_shown = gtk_toggle_action_get_active (action);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
}

static const GtkToggleActionEntry ui_toggle_actions [] =
{
        { "QueryExecFavoritesShow", NULL, N_("_Show favorites"), "F9", N_("Show or hide favorites"), G_CALLBACK (favorites_toggle_cb), FALSE }
};

static GtkActionEntry ui_actions[] = {
        { "QueryExecMenu", NULL, N_("_Query"), NULL, N_("Query"), NULL },
        { "QueryExecItem1", STOCK_CONSOLE, N_("_New editor"), "<control>T", N_("Open a new query editor"),
          G_CALLBACK (query_exec_add_cb)},
};

static const gchar *ui_actions_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
	"    <menu name='Display' action='Display'>"
	"      <menuitem name='QueryExecFavoritesShow' action='QueryExecFavoritesShow'/>"
        "    </menu>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='QueryExec' action='QueryExecMenu'>"
        "        <menuitem name='QueryExecItem1' action= 'QueryExecItem1'/>"
        "      </menu>"
	"    </placeholder>"
        "  </menubar>"
        "  <toolbar name='ToolBar'>"
        "    <separator/>"
        "    <toolitem action='QueryExecItem1'/>"
        "  </toolbar>"
        "</ui>";

static GtkActionGroup *
query_exec_perspective_get_actions_group (BrowserPerspective *perspective)
{
	QueryExecPerspective *bpers;
	GtkActionGroup *agroup;
	bpers = QUERY_EXEC_PERSPECTIVE (perspective);
	agroup = gtk_action_group_new ("QueryExecActions");
	gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), bpers);
	
	gtk_action_group_add_toggle_actions (agroup, ui_toggle_actions,
					     G_N_ELEMENTS (ui_toggle_actions),
					     bpers);
	GtkAction *action;
	action = gtk_action_group_get_action (agroup, "QueryExecFavoritesShow");
	if (bpers->priv->favorites)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      bpers->priv->favorites_shown);
	else
		gtk_action_set_sensitive (GTK_ACTION (action), FALSE);
	
	return agroup;
}

static const gchar *
query_exec_perspective_get_actions_ui (G_GNUC_UNUSED BrowserPerspective *perspective)
{
	return ui_actions_info;
}

static void
query_exec_perspective_page_tab_label_change (BrowserPerspective *perspective, BrowserPage *page)
{
	QueryExecPerspective *bpers;
	GtkWidget *tab_label;
	GtkWidget *close_btn;
	
	bpers = QUERY_EXEC_PERSPECTIVE (perspective);
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

static void
query_exec_perspective_get_current_customization (BrowserPerspective *perspective,
						  GtkActionGroup **out_agroup,
						  const gchar **out_ui)
{
	QueryExecPerspective *bpers;
	GtkWidget *page_contents;

	bpers = QUERY_EXEC_PERSPECTIVE (perspective);
	page_contents = gtk_notebook_get_nth_page (GTK_NOTEBOOK (bpers->priv->notebook),
						   gtk_notebook_get_current_page (GTK_NOTEBOOK (bpers->priv->notebook)));
	if (IS_BROWSER_PAGE (page_contents)) {
		*out_agroup = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		*out_ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}
}

static BrowserWindow *
query_exec_perspective_get_window (BrowserPerspective *perspective)
{
	QueryExecPerspective *bpers;
	bpers = QUERY_EXEC_PERSPECTIVE (perspective);
	return bpers->priv->bwin;
}
