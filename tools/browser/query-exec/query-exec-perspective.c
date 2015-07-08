/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "query-exec-perspective.h"
#include "../browser-window.h"
#include "../browser-page.h"
#include "query-console-page.h"
#include "../ui-support.h"
#include "../ui-customize.h"
#include "query-favorite-selector.h"
#include "query-editor.h"
#include <libgda/gda-debug-macros.h>

/* 
 * Main static functions 
 */
static void query_exec_perspective_class_init (QueryExecPerspectiveClass *klass);
static void query_exec_perspective_init (QueryExecPerspective *stmt);
static void query_exec_perspective_dispose (GObject *object);

static void query_exec_perspective_grab_focus (GtkWidget *widget);
static void query_exec_new_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data);

/* BrowserPerspective interface */
static void                 query_exec_perspective_perspective_init (BrowserPerspectiveIface *iface);
static GtkWidget           *query_exec_perspective_get_notebook (BrowserPerspective *perspective);
static void                 query_exec_perspective_customize (BrowserPerspective *perspective,
							      GtkToolbar *toolbar, GtkHeaderBar *header);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _QueryExecPerspectivePrivate {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
	TConnection *tcnc;
};

GType
query_exec_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
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
		
		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "QueryExecPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_mutex_unlock (&registering);
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
	iface->i_get_notebook = query_exec_perspective_get_notebook;
	iface->i_customize = query_exec_perspective_customize;
        iface->i_uncustomize = NULL;
}

static void
query_exec_perspective_init (QueryExecPerspective *perspective)
{
	perspective->priv = g_new0 (QueryExecPerspectivePrivate, 1);
	perspective->priv->favorites_shown = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
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
	TConnection *tcnc;
	BrowserPerspective *bpers;
	QueryExecPerspective *perspective;
	gboolean fav_supported;

	bpers = (BrowserPerspective*) g_object_new (TYPE_QUERY_EXEC_PERSPECTIVE, NULL);
	perspective = (QueryExecPerspective*) bpers;

	perspective->priv->bwin = bwin;
	tcnc = browser_window_get_connection (bwin);
	perspective->priv->tcnc = g_object_ref (tcnc);
	fav_supported = t_connection_get_favorites (tcnc) ? TRUE : FALSE;

	/* contents */
	GtkWidget *paned, *nb, *wid;
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	if (fav_supported) {
		wid = query_favorite_selector_new (tcnc);
		g_signal_connect (wid, "selection-changed",
				  G_CALLBACK (fav_selection_changed_cb), bpers);
		gtk_paned_pack1 (GTK_PANED (paned), wid, FALSE, TRUE);
		gtk_paned_set_position (GTK_PANED (paned), DEFAULT_FAVORITES_SIZE);
		perspective->priv->favorites = wid;
	}

	nb = browser_perspective_create_notebook (bpers);
	perspective->priv->notebook = nb;
	gtk_paned_pack2 (GTK_PANED (paned), nb, TRUE, TRUE);

	GtkWidget *page, *tlabel, *button;

	page = query_console_page_new (tcnc);
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

	return bpers;
}

static void
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id,
			  G_GNUC_UNUSED TFavoritesType fav_type, const gchar *selection,
			  QueryExecPerspective *perspective)
{
	GtkNotebook *nb;
	GtkWidget *page;

	nb = GTK_NOTEBOOK (perspective->priv->notebook);
	page = gtk_notebook_get_nth_page (nb, gtk_notebook_get_current_page (nb));
	if (!page) {
		query_exec_new_cb (NULL, NULL, BROWSER_PERSPECTIVE (perspective));
		page = gtk_notebook_get_nth_page (nb, gtk_notebook_get_current_page (nb));
		if (!page)
			return;
	}
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
		if (customization_data_exists (object))
			customization_data_release (object);

		if (perspective->priv->tcnc)
			g_object_unref (perspective->priv->tcnc);

		g_free (perspective->priv);
		perspective->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
query_exec_new_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a QueryExecPerspective */
	GtkWidget *page, *tlabel, *button;
	QueryExecPerspective *perspective;
	TConnection *tcnc;
	gint i;

	perspective = QUERY_EXEC_PERSPECTIVE (data);
	tcnc = perspective->priv->tcnc;

	page = query_console_page_new (tcnc);
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
favorites_toggle_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a QueryExecPerspective */
	QueryExecPerspective *perspective;
	perspective = QUERY_EXEC_PERSPECTIVE (data);
	if (!perspective->priv->favorites)
		return;

	perspective->priv->favorites_shown = g_variant_get_boolean (state);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
	g_simple_action_set_state (action, state);
}

static GtkWidget *
query_exec_perspective_get_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_QUERY_EXEC_PERSPECTIVE (perspective), NULL);
	return QUERY_EXEC_PERSPECTIVE (perspective)->priv->notebook;
}

static GActionEntry win_entries[] = {
        { "show-favorites", NULL, NULL, "true", favorites_toggle_cb },
        { "query-exec-new", query_exec_new_cb, NULL, NULL, NULL },
};

static void
query_exec_perspective_customize (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_print ("%s ()\n", __FUNCTION__);

	customization_data_init (G_OBJECT (perspective), toolbar, header);

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

	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "tab-new-symbolic");
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("New query execution tab"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.query-exec-new");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));
}
