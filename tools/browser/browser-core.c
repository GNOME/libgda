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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "browser-core.h"
#include "browser-window.h"
#include "browser-connection.h"

/* 
 * Main static functions 
 */
static void browser_core_class_init (BrowserCoreClass *klass);
static void browser_core_init (BrowserCore *bcore);
static void browser_core_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* pointer to the singleton */
static BrowserCore *_bcore = NULL;

/* signals */
enum
	{
		CNC_ADDED,
		CNC_REMOVED,
		LAST_SIGNAL
	};

static gint browser_core_signals[LAST_SIGNAL] = { 0, 0 };

struct _BrowserCorePrivate {
	GSList         *factories; /* list of PerspectiveFactory pointers, statically compiled */
        BrowserPerspectiveFactory *default_factory; /* used by default, no ref held on it */
        GSList         *connections; /* list of BrowserConnection objects, referenced here */
        GSList         *windows; /* list of BrowserWindow pointers: list of opened windows */
};

GType
browser_core_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (BrowserCoreClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_core_class_init,
			NULL,
			NULL,
			sizeof (BrowserCore),
			0,
			(GInstanceInitFunc) browser_core_init,
			0
		};

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "BrowserCore", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
browser_core_class_init (BrowserCoreClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

        browser_core_signals[CNC_ADDED] =
                g_signal_new ("connection-added",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserCoreClass, connection_added),
                              NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, BROWSER_TYPE_CONNECTION);

        browser_core_signals[CNC_REMOVED] =
                g_signal_new ("connection-removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (BrowserCoreClass, connection_removed),
                              NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, BROWSER_TYPE_CONNECTION);

	klass->connection_added = NULL;
	klass->connection_removed = NULL;

	object_class->dispose = browser_core_dispose;
}

BrowserCoreInitFactories browser_core_init_factories = NULL;

static void
browser_core_init (BrowserCore *bcore)
{
	bcore->priv = g_new0 (BrowserCorePrivate, 1);
	bcore->priv->factories = NULL;

	if (browser_core_init_factories)
		bcore->priv->factories = browser_core_init_factories ();

	/* set default perspective */
	if (bcore->priv->factories)
		bcore->priv->default_factory = (BrowserPerspectiveFactory*) bcore->priv->factories->data; 

	bcore->priv->windows = NULL;
}

/**
 * browser_core_exists
 *
 * Tells if a #BrowserCore has already been created
 *
 * Returns: %TRUE if the #BrowserCore singleton has already been created
 */
gboolean
browser_core_exists (void)
{
	return _bcore ? TRUE : FALSE;
}

/**
 * browser_core_get
 *
 * Returns a #BrowserCore object which holds the browser's configuration. This is
 * a singleton factory.
 *
 * Returns: the #BrowserCore object
 */
BrowserCore*
browser_core_get (void)
{
	if (!_bcore)
		_bcore = BROWSER_CORE (g_object_new (BROWSER_TYPE_CORE, NULL));

	return _bcore;
}

static void
browser_core_dispose (GObject *object)
{
	BrowserCore *bcore;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_CORE (object));

	bcore = BROWSER_CORE (object);
	if (bcore->priv) {
		if (bcore->priv->factories) {
			g_slist_free (bcore->priv->factories);
			bcore->priv->factories = NULL;
		}
		bcore->priv->default_factory = NULL;
		
		if (bcore->priv->windows) {
			g_slist_foreach (bcore->priv->windows, (GFunc) g_object_unref, NULL);
			g_slist_free (bcore->priv->windows);
			bcore->priv->windows = NULL;
		}

		g_free (bcore->priv);
		bcore->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

/**
 * browser_core_take_window
 * @bwin: a #BrowserWindow
 *
 * Makes sure @bwin is handled by the #BrowserCore object, reference to @bwin is stolen here.
 * This method should be called after a #BrowserWindow has been created to have it managed properly.
 */
void
browser_core_take_window (BrowserWindow *bwin)
{
        g_return_if_fail (BROWSER_IS_WINDOW (bwin));

        _bcore = browser_core_get ();
        _bcore->priv->windows = g_slist_append (_bcore->priv->windows, bwin);
}

/**
 * browser_core_close_window
 * @bwin: a #BrowserWindow
 *
 * Requests that @bwin be closed.
 */
void
browser_core_close_window (BrowserWindow *bwin)
{
	g_return_if_fail (BROWSER_IS_WINDOW (bwin));
	_bcore = browser_core_get ();
	g_return_if_fail (g_slist_find (_bcore->priv->windows, bwin));

	_bcore->priv->windows = g_slist_remove (_bcore->priv->windows, bwin);
	gtk_widget_destroy (GTK_WIDGET (bwin));

	if (! _bcore->priv->windows) {
		/* no more windows left => quit */
		g_print ("Closed last window, bye!\n");
		browser_core_quit ();
	}
}

/**
 * browser_core_get_windows
 *
 * Get a list of #BrowserWindow mananged by the browser (windows must have been
 * declared using browser_core_take_window()).
 *
 * Returns: a new list, free it with g_slist_free()
 */
GSList *
browser_core_get_windows (void)
{
	_bcore = browser_core_get ();
	if (_bcore->priv->windows)
		return g_slist_copy (_bcore->priv->windows);
	else
		return NULL;
}

/**
 * browser_core_take_connection
 * @bcnc: a #BrowserConnection
 *
 * Makes sure @bcnc is handled by @dbata, reference to @bcnc is stolen here
 */
void
browser_core_take_connection (BrowserConnection *bcnc)
{
        g_return_if_fail (BROWSER_IS_CONNECTION (bcnc));

        _bcore = browser_core_get ();
        _bcore->priv->connections = g_slist_append (_bcore->priv->connections, bcnc);
	g_signal_emit (_bcore, browser_core_signals [CNC_ADDED], 0, bcnc);
}

/**
 * browser_core_close_connection
 * @bcnc: a #BrowserConnection
 *
 * Requests that @bcnc be closed.
 */
void
browser_core_close_connection (BrowserConnection *bcnc)
{
        g_return_if_fail (g_slist_find (_bcore->priv->connections, bcnc));

        _bcore = browser_core_get ();
        _bcore->priv->connections = g_slist_remove (_bcore->priv->connections, bcnc);
	g_signal_emit (_bcore, browser_core_signals [CNC_REMOVED], 0, bcnc);
        g_object_unref (bcnc);
}

/**
 * browser_core_get_connections
 *
 * Get a list of #BrowserWindow
 *
 * Returns: a new list, free it with g_slist_free()
 */
GSList *
browser_core_get_connections (void)
{
	_bcore = browser_core_get ();
	if (_bcore->priv->connections)
		return g_slist_copy (_bcore->priv->connections);
	else
		return NULL;
}

/**
 * browser_core_get_factory
 * @factory: the name of the requested factory
 *
 * Get a pointer to a #BrowserPerspectiveFactory, from its name
 *
 * Returns: a pointer to the #BrowserPerspectiveFactory, or %NULL if not found
 */
BrowserPerspectiveFactory *
browser_core_get_factory (const gchar *factory)
{
	GSList *list;
	g_return_val_if_fail (factory, NULL);
	_bcore = browser_core_get ();
	for (list = _bcore->priv->factories; list; list = list->next) {
		BrowserPerspectiveFactory *bpf = BROWSER_PERSPECTIVE_FACTORY (list->data);
		if (!g_ascii_strcasecmp (bpf->perspective_name, factory))
			return bpf;
	}
	return NULL;
}

/**
 * browser_core_get_default_factory
 *
 * Get the default #BrowserPerspectiveFactory used when making new #BrowserWindow if none
 * is provided when calling browser_window_new().
 *
 * Returns: the default #BrowserPerspectiveFactory
 */
BrowserPerspectiveFactory *
browser_core_get_default_factory (void)
{
	_bcore = browser_core_get ();
	return _bcore->priv->default_factory;
}

/**
 * browser_core_set_default_factory
 * @factory: the name of a #BrowserPerspectiveFactory
 *
 * Sets the default #BrowserPerspectiveFactory used when making new #BrowserWindow if none
 * is provided when calling browser_window_new().
 */
void
browser_core_set_default_factory (const gchar *factory)
{
	GSList *list;
	gchar *lc2;
	_bcore = browser_core_get ();

	if (!factory)
		return;

	lc2 = g_utf8_strdown (factory, -1);
	for (list = _bcore->priv->factories; list; list = list->next) {
		BrowserPerspectiveFactory *fact = (BrowserPerspectiveFactory*) list->data;
		gchar *lc1;
		lc1 = g_utf8_strdown (fact->perspective_name, -1);

		if (strstr (lc1, lc2)) {
			_bcore->priv->default_factory = fact;
			g_free (lc1);
			break;
		}

		g_free (lc1);
	}
	g_free (lc2);
}

/**
 * browser_core_get_factories
 *
 * Get a list of all the known Perspective factories
 *
 * Returns: a constant list which must not be altered
 */
const GSList *
browser_core_get_factories (void)
{
	_bcore = browser_core_get ();
	return _bcore->priv->factories;
}

/**
 * browser_core_quit
 *
 * Quits the browser after having made some clean-ups
 */
void
browser_core_quit (void)
{
	if (_bcore) {
		g_object_unref (_bcore);
		_bcore = NULL;
	}
        gtk_main_quit();
}
