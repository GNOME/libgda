/* 
 * Copyright (C) 2009 The GNOME Foundation.
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

#include "browser-page.h"
#include "browser-perspective.h"

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;
static void browser_page_class_init (gpointer g_class);

GType
browser_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserPageIface),
			(GBaseInitFunc) browser_page_class_init,
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
			type = g_type_register_static (G_TYPE_INTERFACE, "BrowserPage", &info, 0);
			g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
		}
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}

static void
browser_page_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	g_static_rec_mutex_lock (&init_mutex);
	if (! initialized) {
		initialized = TRUE;
	}
	g_static_rec_mutex_unlock (&init_mutex);
}

/**
 * browser_page_get_actions_group
 * @page: an object implementing the #BrowserPage interface
 *
 * Get the #GtkActionGroup from a @page to represent its specific actions.
 *
 * Returns: a new #GtkActionGroup
 */
GtkActionGroup *
browser_page_get_actions_group (BrowserPage *page)
{
	g_return_val_if_fail (IS_BROWSER_PAGE (page), NULL);
	
	if (BROWSER_PAGE_GET_CLASS (page)->i_get_actions_group)
		return (BROWSER_PAGE_GET_CLASS (page)->i_get_actions_group) (page);
	else
		return NULL;
}

/**
 * browser_page_get_actions_ui
 * @page: an object implementing the #BrowserPage interface
 *
 * Get the UI definition from @page to represent how its specific actions (obtained
 * using browser_page_get_actions_group()) are to be integrated in a #BrowserWindow's menu
 * and toolbar.
 *
 * Returns: a read-only string
 */
const gchar *
browser_page_get_actions_ui (BrowserPage *page)
{
	g_return_val_if_fail (IS_BROWSER_PAGE (page), NULL);
	
	if (BROWSER_PAGE_GET_CLASS (page)->i_get_actions_ui)
		return (BROWSER_PAGE_GET_CLASS (page)->i_get_actions_ui) (page);
	else
		return NULL;
}

/**
 * browser_page_get_perspective
 * @page: an object implementing the #BrowserPage interface
 *
 * Finds the BrowserPerspective in which @page is. Note that the #BrowserPerspective may
 * have changed since a previous call as users are allowed to do some drag and drop between
 * browser's windows which contain different #BrowserPerspective objects.
 *
 * Returns: the #BrowserPerspective
 */
BrowserPerspective *
browser_page_get_perspective (BrowserPage *page)
{
	GtkWidget *wid;
	for (wid = gtk_widget_get_parent (GTK_WIDGET (page)); wid; wid = gtk_widget_get_parent (wid))
		if (IS_BROWSER_PERSPECTIVE (wid))
			return BROWSER_PERSPECTIVE (wid);
	return NULL;
}

/**
 * browser_page_get_tab_label
 * @page: an object implementing the #BrowserPage interface
 * @out_close_button: a place holder to hold a pointer to a close button
 *
 * Get a new widget to be packed in a notebook's tab label.
 *
 * If @out_close_button is not %NULL, then the implementation may decide to add
 * a button to close the tab; if @out_close_button is %NULL, then it should not add
 * any close button.
 *
 * Returns: a new #GtkWidget, or %NULL
 */
GtkWidget *
browser_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	g_return_val_if_fail (IS_BROWSER_PAGE (page), NULL);
	
	if (out_close_button)
		*out_close_button = NULL;
	if (BROWSER_PAGE_GET_CLASS (page)->i_get_tab_label)
		return (BROWSER_PAGE_GET_CLASS (page)->i_get_tab_label) (page, out_close_button);
	else
		return NULL;	
}
