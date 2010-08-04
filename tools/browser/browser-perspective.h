/*
 * Copyright (C) 2009 - 2010 The GNOME Foundation.
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

#ifndef __BROWSER_PERSPECTIVE_H__
#define __BROWSER_PERSPECTIVE_H__

#include <gtk/gtk.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_PERSPECTIVE_TYPE            (browser_perspective_get_type())
#define BROWSER_PERSPECTIVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, BROWSER_PERSPECTIVE_TYPE, BrowserPerspective))
#define IS_BROWSER_PERSPECTIVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, BROWSER_PERSPECTIVE_TYPE))
#define BROWSER_PERSPECTIVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), BROWSER_PERSPECTIVE_TYPE, BrowserPerspectiveIface))

/* struct for the interface */
struct _BrowserPerspectiveIface {
	GTypeInterface           g_iface;

	/* virtual table */
	GtkActionGroup      *(* i_get_actions_group) (BrowserPerspective *perspective);
	const gchar         *(* i_get_actions_ui) (BrowserPerspective *perspective);
	void                 (* i_get_current_customization) (BrowserPerspective *perspective,
							      GtkActionGroup **out_agroup,
							      const gchar **out_ui);
	void                 (* i_page_tab_label_change) (BrowserPerspective *perspective, BrowserPage *page);
};

GType           browser_perspective_get_type          (void) G_GNUC_CONST;

GtkActionGroup *browser_perspective_get_actions_group (BrowserPerspective *perspective);
const gchar    *browser_perspective_get_actions_ui    (BrowserPerspective *perspective);
void            browser_perspective_get_current_customization (BrowserPerspective *perspective,
							       GtkActionGroup **out_agroup,
							       const gchar **out_ui);
void            browser_perspective_page_tab_label_change (BrowserPerspective *perspective,
							   BrowserPage *page);

G_END_DECLS

#endif
