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
	GtkActionGroup      *(* i_get_actions_group) (BrowserPage *page);
	const gchar         *(* i_get_actions_ui) (BrowserPage *page);
	GtkWidget           *(* i_get_tab_label) (BrowserPage *page, GtkWidget **out_close_button);
};

GType               browser_page_get_type          (void) G_GNUC_CONST;

GtkActionGroup     *browser_page_get_actions_group (BrowserPage *page);
const gchar        *browser_page_get_actions_ui    (BrowserPage *page);

BrowserPerspective *browser_page_get_perspective   (BrowserPage *page);
GtkWidget          *browser_page_get_tab_label     (BrowserPage *page, GtkWidget **out_close_button);

G_END_DECLS

#endif
