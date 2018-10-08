/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "ui-support.h"
#include "ui-customize.h"
#include <string.h>

#include "schema-browser/perspective-main.h"
#include "query-exec/perspective-main.h"
#include "data-manager/perspective-main.h"
#ifdef HAVE_LDAP
  #include "ldap-browser/perspective-main.h"
#endif
#include "dummy-perspective/perspective-main.h"

static GRecMutex init_rmutex;
#define MUTEX_LOCK() g_rec_mutex_lock(&init_rmutex)
#define MUTEX_UNLOCK() g_rec_mutex_unlock(&init_rmutex)
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
		
		MUTEX_LOCK();
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "BrowserPerspective", &info, 0);
			g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
		}
		MUTEX_UNLOCK();
	}
	return type;
}

static void
browser_perspective_class_init (G_GNUC_UNUSED gpointer g_class)
{
	static gboolean initialized = FALSE;

	MUTEX_LOCK();
	if (! initialized) {
		initialized = TRUE;
	}
	MUTEX_UNLOCK();
}

/**
 * browser_perspective_customize:
 * @perspective: an object implementing the #BrowserPerspective interface
 * @toolbar: (nullable):
 * @header: (nullable):
 *
 * Add optional custom UI elements to @toolbar, @header and @menu. any call to the
 * browser_perspective_uncustomize() function will undo all the customizations to
 * these elements
 */
void
browser_perspective_customize (BrowserPerspective *perspective, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_return_if_fail (IS_BROWSER_PERSPECTIVE (perspective));
	if (BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_customize)
		(BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_customize) (perspective, toolbar, header);

	/* Current BrowserPage */
	GtkNotebook *nb;
	nb = (GtkNotebook*) browser_perspective_get_notebook (perspective);
	if (nb) {
		gint current_index;
		current_index = gtk_notebook_get_current_page (nb);
		if (current_index >= 0) {
			GtkWidget *current_page;
			current_page = gtk_notebook_get_nth_page (nb, current_index);
			if (current_page && IS_BROWSER_PAGE (current_page))
				browser_page_customize (BROWSER_PAGE (current_page), toolbar, header);
		}
	}
}

/**
 * browser_perspective_uncustomize:
 * @perspective: an object implementing the #BrowserPerspective interface
 *
 * Remove any optional custom UI elements  which have been added
 * when browser_perspective_customize() was called.
 */
void
browser_perspective_uncustomize (BrowserPerspective *perspective)
{
	g_return_if_fail (IS_BROWSER_PERSPECTIVE (perspective));

	/* Current BrowserPage */
	GtkNotebook *nb;
	nb = (GtkNotebook*) browser_perspective_get_notebook (perspective);
	if (nb) {
		gint current_index;
		current_index = gtk_notebook_get_current_page (nb);
		if (current_index >= 0) {
			GtkWidget *current_page;
			current_page = gtk_notebook_get_nth_page (nb, current_index);
			if (current_page && IS_BROWSER_PAGE (current_page))
				browser_page_uncustomize (BROWSER_PAGE (current_page));
		}
	}

	if (BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_uncustomize)
		(BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_uncustomize) (perspective);
	else {
		g_print ("Default browser_perspective_uncustomize for %s\n",
			 G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (perspective)));
		if (customization_data_exists (G_OBJECT (perspective)))
			customization_data_release (G_OBJECT (perspective));
	}
}

/**
 * browser_perspective_get_window:
 * @perspective: an object implementing the #BrowserPerspective interface
 *
 * Returns: (transfer none): the #BrowserWindow @perspective is in
 */
BrowserWindow *
browser_perspective_get_window (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (perspective), NULL);
	return (BrowserWindow*) ui_find_parent_widget (GTK_WIDGET (perspective), BROWSER_TYPE_WINDOW);
}

/**
 * browser_perspective_get_notebook:
 * @perspective: an object implementing the #BrowserPerspective interface
 *
 * Returns: (transfer none): the #GtkWidget which acts a a notebook for #BrowserPage widgets
 */
GtkWidget *
browser_perspective_get_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (perspective), NULL);
	if (BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_get_notebook)
		return (BROWSER_PERSPECTIVE_GET_CLASS (perspective)->i_get_notebook) (perspective);
	else
		return NULL;
}

static void notebook_switch_page_cb (GtkNotebook *nb, GtkWidget *page, gint pagenb, BrowserPerspective *bpers);
static void notebook_remove_page_cb (GtkNotebook *nb, GtkWidget *page, gint pagenb, BrowserPerspective *bpers);
static void notebook_destroy_cb (GtkNotebook *nb, BrowserPerspective *bpers);

/**
 * browser_perspective_create_notebook:
 * @perspective: an object implementing the #BrowserPerspective interface
 *
 * Creates a #GtkNotebook to hold #BrowserPage widgets in each page. It handles the customization (header and tool bars)
 * when pages are toggles.
 *
 * Returns: (transfer full):  a new #GtkNotebook
 */
GtkWidget *
browser_perspective_create_notebook (BrowserPerspective *perspective)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (perspective), NULL);

	GtkWidget *nb;
	nb = gtk_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (nb), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (nb));

	g_signal_connect (nb, "destroy",
			  G_CALLBACK (notebook_destroy_cb), perspective);
	g_signal_connect (nb, "switch-page",
			  G_CALLBACK (notebook_switch_page_cb), perspective);
	g_signal_connect (nb, "page-removed",
			  G_CALLBACK (notebook_remove_page_cb), perspective);

	return nb;
}

static void
notebook_destroy_cb (GtkNotebook *nb, BrowserPerspective *bpers)
{
	g_signal_handlers_disconnect_by_func (nb,
					      G_CALLBACK (notebook_switch_page_cb), bpers);
	g_signal_handlers_disconnect_by_func (nb,
					      G_CALLBACK (notebook_remove_page_cb), bpers);
	g_print ("==== %s\n", __FUNCTION__);
}

static void
notebook_switch_page_cb (GtkNotebook *nb, GtkWidget *page, gint pagenb, BrowserPerspective *bpers)
{
	if (customization_data_exists (G_OBJECT (bpers))) {
		gint current_index;
		current_index = gtk_notebook_get_current_page (nb);
		g_print ("\tNotebook, current page %d switching to %d\n", current_index, pagenb);
		if (current_index >= 0) {
			GtkWidget *current_page;
			current_page = gtk_notebook_get_nth_page (nb, current_index);
			if (current_page && IS_BROWSER_PAGE (current_page))
				browser_page_uncustomize (BROWSER_PAGE (current_page));
		}
		if (pagenb >= 0) {
			GtkWidget *next_page;
			next_page = gtk_notebook_get_nth_page (nb, pagenb);
			if (next_page && IS_BROWSER_PAGE (next_page))
				browser_page_customize (BROWSER_PAGE (next_page),
							customization_data_get_toolbar (G_OBJECT (bpers)),
							customization_data_get_header_bar (G_OBJECT (bpers)));
		}
	}
}

static void
notebook_remove_page_cb (GtkNotebook *nb, GtkWidget *page, gint pagenb, BrowserPerspective *bpers)
{
	if (customization_data_exists (G_OBJECT (bpers))) {
		gint current_index;
		GtkWidget *current_page = NULL;
		current_index = gtk_notebook_get_current_page (nb);
		if (current_index >= 0)
			current_page = gtk_notebook_get_nth_page (nb, current_index);

		g_print ("\tNotebook, removing page %d, current page is now %d\n", pagenb, current_index);

		/* REM: we need to uncustomize _both_ the removed page and the new page, and customize
		 * again the new page to avoid discrepancies */
		if (current_page && IS_BROWSER_PAGE (current_page))
			browser_page_uncustomize (BROWSER_PAGE (current_page));
		if (page && IS_BROWSER_PAGE (page))
			browser_page_uncustomize (BROWSER_PAGE (page));

		if (current_page && IS_BROWSER_PAGE (current_page))
			browser_page_customize (BROWSER_PAGE (current_page),
						customization_data_get_toolbar (G_OBJECT (bpers)),
						customization_data_get_header_bar (G_OBJECT (bpers)));
	}
}




/*
 * All perspectives information
 */
GSList *factories = NULL; /* list of PerspectiveFactory pointers, statically compiled */
BrowserPerspectiveFactory *default_factory; /* used by default, no ref held on it */

static void
_factories_init (void)
{
	static gboolean init_done = FALSE;
	if (G_LIKELY (init_done))
		return;

	/* initialize factories */
	BrowserPerspectiveFactory *pers;

	pers = schema_browser_perspective_get_factory ();
	factories = g_slist_append (factories, pers);

	pers = query_exec_perspective_get_factory ();
	factories = g_slist_append (factories, pers);

	pers = data_manager_perspective_get_factory ();
	factories = g_slist_append (factories, pers);

#ifdef HAVE_LDAP
	pers = ldap_browser_perspective_get_factory ();
	factories = g_slist_append (factories, pers);
#endif

	pers = dummy_perspective_get_factory ();
	factories = g_slist_append (factories, pers);

	if (factories)
                default_factory = BROWSER_PERSPECTIVE_FACTORY (factories->data);

	init_done = TRUE;
}

/**
 * browser_get_factory
 * @factory_id: the ID of the requested factory
 *
 * Get a pointer to a #BrowserPerspectiveFactory, from its name
 *
 * Returns: a pointer to the #BrowserPerspectiveFactory, or %NULL if not found
 */
BrowserPerspectiveFactory *
browser_get_factory (const gchar *factory_id)
{
        g_return_val_if_fail (factory_id, NULL);
	_factories_init ();

	GSList *list;
        for (list = factories; list; list = list->next) {
                BrowserPerspectiveFactory *bpf = BROWSER_PERSPECTIVE_FACTORY (list->data);
                if (!g_ascii_strcasecmp (bpf->id, factory_id))
                        return bpf;
        }
        return NULL;
}

/**
 * browser_get_default_factory
 *
 * Get the default #BrowserPerspectiveFactory used when making new #BrowserWindow if none
 * is provided when calling browser_window_new().
 *
 * Returns: the default #BrowserPerspectiveFactory
 */
BrowserPerspectiveFactory *
browser_get_default_factory (void)
{
	_factories_init ();
        return default_factory;
}

/**
 * browser_set_default_factory
 * @factory: the name of a #BrowserPerspectiveFactory
 *
 * Sets the default #BrowserPerspectiveFactory used when making new #BrowserWindow if none
 * is provided when calling browser_window_new().
 */
void
browser_set_default_factory (const gchar *factory)
{
        GSList *list;
        gchar *lc2;

        if (!factory)
                return;

	_factories_init ();

        lc2 = g_utf8_strdown (factory, -1);
        for (list = factories; list; list = list->next) {
                BrowserPerspectiveFactory *fact = BROWSER_PERSPECTIVE_FACTORY (list->data);
                gchar *lc1;
                lc1 = g_utf8_strdown (fact->perspective_name, -1);

                if (strstr (lc1, lc2)) {
                        default_factory = fact;
                        g_free (lc1);
                        break;
                }

                g_free (lc1);
        }
        g_free (lc2);
}

/**
 * browser_get_factories
 *
 * Get a list of all the known Perspective factories
 *
 * Returns: a constant list which must not be altered
 */
const GSList *
browser_get_factories (void)
{
	_factories_init ();

        return factories;
}
