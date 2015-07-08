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

#ifndef __BROWSER_PERSPECTIVE_H__
#define __BROWSER_PERSPECTIVE_H__

#include <gtk/gtk.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_PERSPECTIVE_TYPE            (browser_perspective_get_type())
#define BROWSER_PERSPECTIVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, BROWSER_PERSPECTIVE_TYPE, BrowserPerspective))
#define IS_BROWSER_PERSPECTIVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, BROWSER_PERSPECTIVE_TYPE))
#define BROWSER_PERSPECTIVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), BROWSER_PERSPECTIVE_TYPE, BrowserPerspectiveIface))

typedef struct _BrowserPerspectiveIface   BrowserPerspectiveIface;
typedef struct _BrowserPerspective        BrowserPerspective;

/* struct for the interface */
struct _BrowserPerspectiveIface {
	GTypeInterface          g_iface;

	/* virtual table */
	GtkWidget           *(* i_get_notebook) (BrowserPerspective *perspective);

	void                 (* i_customize) (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header);
	void                 (* i_uncustomize) (BrowserPerspective *perspective);
};

/**
 * SECTION:browser-perspective
 * @short_description: A "perspective" in a #BrowserWindow window
 * @title: BrowserPerspective
 * @stability: Stable
 * @see_also:
 *
 * #BrowserPerspective is an interface used by the #BrowserWindow object to switch
 * between the activities ("perspectives"); it requires the #GtkWidget.
 */

GType           browser_perspective_get_type          (void) G_GNUC_CONST;

void            browser_perspective_customize         (BrowserPerspective *perspective,
						       GtkToolbar *toolbar, GtkHeaderBar *header);
void            browser_perspective_uncustomize       (BrowserPerspective *perspective);

BrowserWindow  *browser_perspective_get_window        (BrowserPerspective *perspective);
GtkWidget      *browser_perspective_get_notebook      (BrowserPerspective *perspective);

/* reserved for BrowserPerspective implementations */
GtkWidget      *browser_perspective_create_notebook   (BrowserPerspective *perspective);

/*
 * All perspectives information
 */
typedef struct {
	const gchar          *id;
	const gchar          *perspective_name;
	const gchar          *menu_shortcut;
	BrowserPerspective *(*perspective_create) (BrowserWindow *);
} BrowserPerspectiveFactory;
#define BROWSER_PERSPECTIVE_FACTORY(x) ((BrowserPerspectiveFactory*)(x))

BrowserPerspectiveFactory *browser_get_default_factory (void);
BrowserPerspectiveFactory *browser_get_factory         (const gchar *factory);
void                       browser_set_default_factory (const gchar *factory);
const GSList              *browser_get_factories       (void);

G_END_DECLS

#endif
