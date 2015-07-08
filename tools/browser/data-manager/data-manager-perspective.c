/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include "data-manager-perspective.h"
#include "data-console.h"
#include "../ui-customize.h"
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
static GtkWidget           *data_manager_perspective_get_notebook (BrowserPerspective *perspective);
static void                 data_manager_perspective_customize (BrowserPerspective *perspective,
								GtkToolbar *toolbar, GtkHeaderBar *header);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _DataManagerPerspectivePriv {
	GtkWidget *notebook;
	GtkWidget *favorites;
	gboolean favorites_shown;
	BrowserWindow *bwin;
        TConnection *tcnc;
};

GType
data_manager_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
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
		
		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (GTK_TYPE_BOX, "DataManagerPerspective", &info, 0);
			g_type_add_interface_static (type, BROWSER_PERSPECTIVE_TYPE, &perspective_info);
		}
		g_mutex_unlock (&registering);
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
	iface->i_get_notebook = data_manager_perspective_get_notebook;
	iface->i_customize = data_manager_perspective_customize;
        iface->i_uncustomize = NULL;
}


static void
data_manager_perspective_init (DataManagerPerspective *perspective)
{
	perspective->priv = g_new0 (DataManagerPerspectivePriv, 1);
	perspective->priv->favorites_shown = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (perspective), GTK_ORIENTATION_VERTICAL);
}

static void fav_selection_changed_cb (GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
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
	TConnection *tcnc;
	BrowserPerspective *bpers;
	DataManagerPerspective *perspective;
	gboolean fav_supported;

	bpers = (BrowserPerspective*) g_object_new (TYPE_DATA_MANAGER_PERSPECTIVE, NULL);
	perspective = (DataManagerPerspective*) bpers;
	perspective->priv->bwin = bwin;
        tcnc = browser_window_get_connection (bwin);
        perspective->priv->tcnc = g_object_ref (tcnc);
	fav_supported = t_connection_get_favorites (tcnc) ? TRUE : FALSE;

	/* contents */
        GtkWidget *paned, *nb, *wid;
        paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	if (fav_supported) {
		wid = data_favorite_selector_new (tcnc);
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
	page = data_console_new (tcnc);
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
		page = data_console_new_with_fav_id (perspective->priv->tcnc, fav_id);
	else
		page = data_console_new (perspective->priv->tcnc);
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
fav_selection_changed_cb (G_GNUC_UNUSED GtkWidget *widget, gint fav_id, TFavoritesType fav_type,
                          const gchar *selection, DataManagerPerspective *perspective)
{
	/* find or create page for this favorite */
        GtkWidget *page_contents;
	gint current_page, npages, i;
	DataConsole *page_to_reuse = NULL;

	if (fav_type != T_FAVORITES_DATA_MANAGERS)
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
		
		if (IS_DATA_CONSOLE (page_contents) &&
		    data_console_is_unused (DATA_CONSOLE (page_contents)))
			page_to_reuse = DATA_CONSOLE (page_contents);
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
manager_new_cb (G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *state, gpointer data)
{
	/* @data is a DataManagerPerspective */
	BrowserPerspective *bpers;
	bpers = BROWSER_PERSPECTIVE (data);
	add_new_data_console (bpers, -1);
}

static void
favorites_toggle_cb (GSimpleAction *action, GVariant *state, gpointer data)
{
	/* @data is a DataManagerPerspective */
	DataManagerPerspective *perspective;
	perspective = DATA_MANAGER_PERSPECTIVE (data);

	if (! perspective->priv->favorites)
		return;
	perspective->priv->favorites_shown = g_variant_get_boolean (state);
	if (perspective->priv->favorites_shown)
		gtk_widget_show (perspective->priv->favorites);
	else
		gtk_widget_hide (perspective->priv->favorites);
	g_simple_action_set_state (action, state);
}

static GtkWidget *
data_manager_perspective_get_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_DATA_MANAGER_PERSPECTIVE (perspective), NULL);
	return DATA_MANAGER_PERSPECTIVE (perspective)->priv->notebook;
}

static GActionEntry win_entries[] = {
        { "show-favorites", NULL, NULL, "true", favorites_toggle_cb },
        { "data-manager-new", manager_new_cb, NULL, NULL, NULL },
};

static void
data_manager_perspective_customize (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header)
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
	gtk_widget_set_tooltip_text (GTK_WIDGET (titem), _("New data manager tab"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.data-manager-new");
	gtk_widget_show (GTK_WIDGET (titem));
	customization_data_add_part (G_OBJECT (perspective), G_OBJECT (titem));
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
		if (! IS_DATA_CONSOLE (page) || !data_console_is_unused (DATA_CONSOLE (page)))
			page = NULL;
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

