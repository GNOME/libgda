/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __BROWSER_WINDOW_H_
#define __BROWSER_WINDOW_H_

#include <gtk/gtk.h>
#include "decl.h"
#include <common/t-connection.h>
#include "browser-perspective.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_WINDOW          (browser_window_get_type())
#define BROWSER_WINDOW(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_window_get_type(), BrowserWindow)
#define BROWSER_WINDOW_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_window_get_type (), BrowserWindowClass)
#define BROWSER_IS_WINDOW(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_window_get_type ())

typedef struct _BrowserWindow BrowserWindow;
typedef struct _BrowserWindowClass BrowserWindowClass;
typedef struct _BrowserWindowPrivate BrowserWindowPrivate;

/* struct for the object's data */
struct _BrowserWindow
{
	GtkApplicationWindow  object;
	BrowserWindowPrivate *priv;
};

/* struct for the object's class */
struct _BrowserWindowClass
{
	GtkApplicationWindowClass parent_class;
	
	/* signals */
	void                (*fullscreen_changed) (BrowserWindow *bwin, gboolean fullscreen);
	void                (*meta_updated) (BrowserWindow *bwin, GdaConnection *cnc);
};

/**
 * SECTION:browser-window
 * @short_description: A top level browser window
 * @title: BrowserWindow
 * @stability: Stable
 * @see_also:
 *
 * Each top level browser window is represented by a #BrowserWindow object, and uses
 * a single #BrowserConnection connection object.
 */

GType               browser_window_get_type               (void) G_GNUC_CONST;
BrowserWindow      *browser_window_new                    (TConnection *tcnc, BrowserPerspectiveFactory *factory);
TConnection        *browser_window_get_connection         (BrowserWindow *bwin);

guint               browser_window_push_status            (BrowserWindow *bwin, const gchar *context,
							   const gchar *text, gboolean auto_clear);
void                browser_window_pop_status             (BrowserWindow *bwin, const gchar *context);
void                browser_window_show_notice            (BrowserWindow *bwin, GtkMessageType type,
							   const gchar *context, const gchar *text);
void                browser_window_show_notice_printf     (BrowserWindow *bwin, GtkMessageType type,
							   const gchar *context, const gchar *format, ...);

BrowserPerspective *browser_window_change_perspective     (BrowserWindow *bwin, const gchar *perspective);

void                browser_window_set_fullscreen         (BrowserWindow *bwin, gboolean fullscreen);
gboolean            browser_window_is_fullscreen          (BrowserWindow *bwin);

G_END_DECLS

#endif
