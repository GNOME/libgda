/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include "browser-page.h"
#include "browser-perspective.h"
#include "ui-customize.h"

static GRecMutex init_rmutex;
#define MUTEX_LOCK() g_rec_mutex_lock(&init_rmutex)
#define MUTEX_UNLOCK() g_rec_mutex_unlock(&init_rmutex)
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
		
		MUTEX_LOCK();
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "BrowserPage", &info, 0);
			g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
		}
		MUTEX_UNLOCK();
	}
	return type;
}

static void
browser_page_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	MUTEX_LOCK();
	if (! initialized) {
		initialized = TRUE;
	}
	MUTEX_UNLOCK();
}

/**
 * browser_page_customize:
 * @page: an object implementing the #BrowserPage interface
 * @toolbar: (nullable):
 * @header: (nullable):
 *
 * Add optional custom UI elements to @toolbar, @header and @menu. any call to the
 * browser_page_uncustomize() function will undo all the customizations to
 * these elements.
 */
void
browser_page_customize (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_return_if_fail (IS_BROWSER_PAGE (page));
	if (BROWSER_PAGE_GET_CLASS (page)->i_customize)
		(BROWSER_PAGE_GET_CLASS (page)->i_customize) (page, toolbar, header);
}

/**
 * browser_page_uncustomize:
 * @page: an object implementing the #BrowserPage interface
 *
 * Remove any optional custom UI elements which have been added
 * when browser_page_customize() was called.
 */
void
browser_page_uncustomize (BrowserPage *page)
{
	g_return_if_fail (IS_BROWSER_PAGE (page));
	if (BROWSER_PAGE_GET_CLASS (page)->i_uncustomize)
		(BROWSER_PAGE_GET_CLASS (page)->i_uncustomize) (page);
	else {
		g_print ("Default browser_page_uncustomize for %s\n", G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (page)));
		if (customization_data_exists (G_OBJECT (page)))
			customization_data_release (G_OBJECT (page));
	}
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
