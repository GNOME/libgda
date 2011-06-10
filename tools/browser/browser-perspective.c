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

#include "browser-perspective.h"
#include "browser-page.h"
#include "browser-window.h"
#include "support.h"

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;
static void browser_perspective_class_init (gpointer g_class);

GType
browser_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserPerspectiveIface),
			(GBaseInitFunc) browser_perspective_class_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL,
			0
		};
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "BrowserPerspective", &info, 0);
			g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
		}
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}

static void
browser_perspective_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	g_static_rec_mutex_lock (&init_mutex);
	if (! initialized) {
		initialized = TRUE;
	}
	g_static_rec_mutex_unlock (&init_mutex);
}

/**
 * browser_perspective_get_actions_group
 * @pers: an object implementing the #BrowserPerspective interface
 *
 * Get the #GtkActionGroup from a @pers to represent its specific actions.
 *
 * Returns: a new #GtkActionGroup
 */
GtkActionGroup *
browser_perspective_get_actions_group (BrowserPerspective *pers)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (pers), NULL);
	
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_group)
		return (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_group) (pers);
	else
		return NULL;
}

/**
 * browser_perspective_get_actions_ui
 * @pers: an object implementing the #BrowserPerspective interface
 *
 * Get the UI definition from a perspective to represent how its specific actions (obtained
 * using browser_perspective_get_actions_group()) are to be integrated in a #BrowserWindow's menu
 * and toolbar.
 *
 * Returns: a read-only string
 */
const gchar *
browser_perspective_get_actions_ui (BrowserPerspective *pers)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (pers), NULL);
	
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_ui)
		return (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_ui) (pers);
	else
		return NULL;
}

/**
 * browser_perspective_get_current_customization
 * @pers: an object implementing the #BrowserPerspective interface
 * @out_agroup: (transfer full): a place to store the returned GtkActionGroup, not %NULL
 * @out_ui: (transfer none): a place to store the returned UI string, not %NULL
 * 
 * Rem: *@out_agroup is a new object and should be unref'ed when not needed anymore
 */
void
browser_perspective_get_current_customization (BrowserPerspective *pers,
					       GtkActionGroup **out_agroup,
					       const gchar **out_ui)
{
	g_return_if_fail (IS_BROWSER_PERSPECTIVE (pers));
	g_return_if_fail (out_agroup);
	g_return_if_fail (out_ui);

	*out_agroup = NULL;
	*out_ui = NULL;
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_current_customization)
		(BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_current_customization) (pers,
										     out_agroup,
										     out_ui);
}

/**
 * browser_perspective_page_tab_label_change
 * @pers: an object implementing the #BrowserPerspective interface
 * @page: an object implementing the #BrowserPage interface
 *
 * When @pers organizes its contents as pages in a notebook, each page may
 * request that the tab's label may be changed, and the purpose of this method
 * is to request that @pers update the tab's label associated to @page.
 */
void
browser_perspective_page_tab_label_change (BrowserPerspective *pers, BrowserPage *page)
{
	g_return_if_fail (IS_BROWSER_PERSPECTIVE (pers));
	g_return_if_fail (IS_BROWSER_PAGE (page));

	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_page_tab_label_change)
		(BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_page_tab_label_change) (pers, page);
}

/**
 * browser_perspective_get_window:
 * @pers: an object implementing the #BrowserPerspective interface
 *
 * Returns: (transfer none): the #BrowserWindow @perspective is in
 */
BrowserWindow *
browser_perspective_get_window (BrowserPerspective *pers)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (pers), NULL);
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_window)
		return (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_window) (pers);
	else
		return (BrowserWindow*) browser_find_parent_widget (GTK_WIDGET (pers),
								    BROWSER_PERSPECTIVE_TYPE);
}

static void nb_page_added_or_removed_cb (GtkNotebook *nb, GtkWidget *child, guint page_num,
					 BrowserPerspective *perspective);
static void nb_switch_page_cb (GtkNotebook *nb, GtkWidget *page, gint page_num,
			       BrowserPerspective *perspective);
static void adapt_notebook_for_fullscreen (BrowserPerspective *perspective);;
static void fullscreen_changed_cb (BrowserWindow *bwin, gboolean fullscreen, BrowserPerspective *perspective);

/**
 * browser_perspective_declare_notebook:
 * @pers: an object implementing the #BrowserPerspective interface
 * @nb: a #GtkNotebook
 *
 * Internally used by browser's perspectives to declare they internally use a notebook
 * which state is modified when switching to and from fullscreen
 */
void
browser_perspective_declare_notebook (BrowserPerspective *perspective, GtkNotebook *nb)
{
	BrowserWindow *bwin;
	g_return_if_fail (IS_BROWSER_PERSPECTIVE (perspective));
	g_return_if_fail (! nb || GTK_IS_NOTEBOOK (nb));
	
	bwin = browser_perspective_get_window (perspective);
	if (!bwin)
		return;

	GtkNotebook *onb;
	onb = g_object_get_data (G_OBJECT (perspective), "fullscreen_nb");
	if (onb) {
		g_signal_handlers_disconnect_by_func (onb,
						      G_CALLBACK (nb_page_added_or_removed_cb),
						      perspective);
		g_signal_handlers_disconnect_by_func (onb,
						      G_CALLBACK (nb_switch_page_cb),
						      perspective);
		g_signal_handlers_disconnect_by_func (bwin,
						      G_CALLBACK (fullscreen_changed_cb), perspective);

	}

	g_object_set_data (G_OBJECT (perspective), "fullscreen_nb", nb);
	if (nb) {
		g_signal_connect (bwin, "fullscreen-changed",
				  G_CALLBACK (fullscreen_changed_cb), perspective);
		g_signal_connect (nb, "page-added",
				  G_CALLBACK (nb_page_added_or_removed_cb), perspective);
		g_signal_connect (nb, "page-removed",
				  G_CALLBACK (nb_page_added_or_removed_cb), perspective);
		g_signal_connect (nb, "switch-page",
				  G_CALLBACK (nb_switch_page_cb), perspective);
		adapt_notebook_for_fullscreen (perspective);
	}
}

static void
nb_switch_page_cb (GtkNotebook *nb, GtkWidget *page, gint page_num, BrowserPerspective *perspective)
{
	GtkWidget *page_contents;
	GtkActionGroup *actions = NULL;
	const gchar *ui = NULL;
	BrowserWindow *bwin;

	page_contents = gtk_notebook_get_nth_page (nb, page_num);
	if (IS_BROWSER_PAGE (page_contents)) {
		actions = browser_page_get_actions_group (BROWSER_PAGE (page_contents));
		ui = browser_page_get_actions_ui (BROWSER_PAGE (page_contents));
	}

	bwin = browser_perspective_get_window (perspective);
	if (bwin)
		browser_window_customize_perspective_ui (bwin, perspective, actions, ui);
	if (actions)
		g_object_unref (actions);
}

static void
nb_page_added_or_removed_cb (GtkNotebook *nb, GtkWidget *child, guint page_num,
			     BrowserPerspective *perspective)
{
	adapt_notebook_for_fullscreen (perspective);
	if (gtk_notebook_get_n_pages (nb) == 0) {
		BrowserWindow *bwin;
		bwin = browser_perspective_get_window (perspective);
		if (!bwin)
			return;
		browser_window_customize_perspective_ui (bwin,
							 BROWSER_PERSPECTIVE (perspective),
							 NULL, NULL);
	}
}

static void
fullscreen_changed_cb (G_GNUC_UNUSED BrowserWindow *bwin, gboolean fullscreen,
		       BrowserPerspective *perspective)
{
	adapt_notebook_for_fullscreen (perspective);
}

static void
adapt_notebook_for_fullscreen (BrowserPerspective *perspective)
{
	gboolean showtabs = TRUE;
	gboolean fullscreen;
	BrowserWindow *bwin;
	GtkNotebook *nb;

	bwin = browser_perspective_get_window (perspective);
	if (!bwin)
		return;
	nb = g_object_get_data (G_OBJECT (perspective), "fullscreen_nb");
	if (!nb)
		return;

	fullscreen = browser_window_is_fullscreen (bwin);
	if (fullscreen && gtk_notebook_get_n_pages (nb) == 1)
		showtabs = FALSE;
	gtk_notebook_set_show_tabs (nb, showtabs);
}
