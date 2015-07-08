/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __BROWSER_PAGE_H__
#define __BROWSER_PAGE_H__

#include <gtk/gtk.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_PAGE_TYPE            (browser_page_get_type())
#define BROWSER_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, BROWSER_PAGE_TYPE, BrowserPage))
#define IS_BROWSER_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, BROWSER_PAGE_TYPE))
#define BROWSER_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), BROWSER_PAGE_TYPE, BrowserPageIface))

/* struct for the interface */
struct _BrowserPageIface {
	GTypeInterface           g_iface;

	/* virtual table */
	void                 (* i_customize) (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
        void                 (* i_uncustomize) (BrowserPage *page);

	GtkWidget           *(* i_get_tab_label) (BrowserPage *page, GtkWidget **out_close_button);
};

/**
 * SECTION:browser-page
 * @short_description: A "page" within a #BrowserPerspective widget
 * @title: BrowserPage
 * @stability: Stable
 * @see_also:
 *
 * Some widgets packed within a #BrowserPerspective can implement
 * the #BrowserPage interface to specify their specific actions and menu and
 * toolbar customizations. When they are packed in a notebook, they can also
 * request some specific tab labels.
 */

GType               browser_page_get_type          (void) G_GNUC_CONST;

void                browser_page_customize         (BrowserPage *page, GtkToolbar *toolbar, GtkHeaderBar *header);
void                browser_page_uncustomize       (BrowserPage *page);


BrowserPerspective *browser_page_get_perspective   (BrowserPage *page);
GtkWidget          *browser_page_get_tab_label     (BrowserPage *page, GtkWidget **out_close_button);

G_END_DECLS

#endif
