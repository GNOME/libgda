/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __BROWSER_CORE_H_
#define __BROWSER_CORE_H_

#include <libgda/libgda.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_CORE          (browser_core_get_type())
#define BROWSER_CORE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_core_get_type(), BrowserCore)
#define BROWSER_CORE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_core_get_type (), BrowserCoreClass)
#define BROWSER_IS_CORE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_core_get_type ())

typedef struct _BrowserCoreClass BrowserCoreClass;
typedef struct _BrowserCorePrivate BrowserCorePrivate;
typedef GSList *(*BrowserCoreInitFactories) (void);

/* struct for the object's data */
struct _BrowserCore
{
	GObject                object;
	BrowserCorePrivate *priv;
};

/* struct for the object's class */
struct _BrowserCoreClass
{
	GObjectClass            parent_class;

	/* signals */
	void    (*connection_added) (BrowserCore *bcore, BrowserConnection *bcnc);
	void    (*connection_removed) (BrowserCore *bcore, BrowserConnection *bcnc);
};

/**
 * SECTION:browser-core
 * @short_description: Singleton holding the global browser information
 * @title: BrowserCore
 * @stability: Stable
 * @see_also:
 *
 * A single instance of a #BrowserCore is created when the browser is started,
 * accessible using browser_core_get().
 */

GType           browser_core_get_type               (void) G_GNUC_CONST;
gboolean        browser_core_exists                 (void);
BrowserCore    *browser_core_get                    (void);

void            browser_core_take_window            (BrowserWindow *bwin);
GSList         *browser_core_get_windows            (void);
void            browser_core_close_window           (BrowserWindow *bwin);

void            browser_core_take_connection        (BrowserConnection *bcnc);
GSList         *browser_core_get_connections        (void);
void            browser_core_close_connection       (BrowserConnection *bcnc);

void            browser_core_quit                   (void);

BrowserPerspectiveFactory *browser_core_get_default_factory    (void);
BrowserPerspectiveFactory *browser_core_get_factory (const gchar *factory);
void           browser_core_set_default_factory (const gchar *factory);
const GSList  *browser_core_get_factories          (void);

G_END_DECLS

#endif
