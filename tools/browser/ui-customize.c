/*
 * Copyright (C) 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include "ui-customize.h"
#include <browser-perspective.h>
#include <browser-page.h>

/*
 * UI customization management (toolbar, top level window header, and top level menu
 */
typedef struct {
	GtkToolbar *toolbar;
	GtkHeaderBar *header;
	GArray *custom_parts;

	BrowserWindow *bwin;
	GActionEntry  *actions;
	guint          n_actions;
} CustomizationData;

static GHashTable *cust_hash = NULL; /* key = a #GObject, value = a #CustomizationData (owned) */
static void
customization_data_free (CustomizationData *cust_data)
{
	g_assert (cust_data);
	if (cust_data->custom_parts) {
		guint i;
		for (i = 0; i < cust_data->custom_parts->len; i++) {
			GWeakRef *wref;
			wref = g_array_index (cust_data->custom_parts, GWeakRef*, i);
			GObject *obj;
			obj = g_weak_ref_get (wref);
			g_weak_ref_clear (wref);
			if (obj) {
				if (GTK_IS_WIDGET (obj))
					gtk_widget_destroy (GTK_WIDGET (obj));
				else
					g_warning ("Unknown type to uncustomize: %s\n",
						   G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (obj)));
				g_object_unref (obj);
			}
			g_free (wref);
		}
		g_array_free (cust_data->custom_parts, TRUE);
	}

	if (cust_data->actions) {
		guint i;
		for (i = 0; i < cust_data->n_actions; i++) {
			GActionEntry *entry;
			entry = &cust_data->actions [i];
			g_action_map_remove_action (G_ACTION_MAP (cust_data->bwin), entry->name);
		}
	}
	g_free (cust_data);
}

/**
 * customization_data_release:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 *
 * Removes any customization done for @object
 */
void
customization_data_release (GObject *object)
{
	g_return_if_fail (G_IS_OBJECT (object));
	if (cust_hash) {
		if (g_hash_table_lookup (cust_hash, object))
			g_hash_table_remove (cust_hash, object);
	}
}

static CustomizationData *
customization_data_get (GObject *object)
{
	if (cust_hash)
		return g_hash_table_lookup (cust_hash, object);
	else
		return NULL;
}

/**
 * customization_data_init:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 * @toolbar: (nullable): a #GtkToolbar
 * @header: (nullable): a #GtkHeaderBar
 *
 * Initializes the customization for @object
 */
void
customization_data_init (GObject *object, GtkToolbar *toolbar, GtkHeaderBar *header)
{
	g_return_if_fail (G_IS_OBJECT (object));
	if (customization_data_get (object)) {
		g_warning ("Customization for %p already exists", object);
		customization_data_release (object);
	}
	if (!cust_hash)
		cust_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) customization_data_free);

	CustomizationData *cust_data;
	cust_data = g_new0 (CustomizationData, 1);
	cust_data->toolbar = toolbar;
	cust_data->header = header;
	cust_data->custom_parts = g_array_new (FALSE, FALSE, sizeof (gpointer));
	g_hash_table_insert (cust_hash, object, cust_data);
}

/**
 * customization_data_exists:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 *
 * Tells if some customization data exists for @object
 */
gboolean
customization_data_exists (GObject *object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	return customization_data_get (object) ? TRUE : FALSE;
}

/**
 * customization_data_add_part:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 * @part: a #GObject object part of the customization
 *
 * Declares that @part is part of the customization (and will be removed when customization_data_release() is called)
 */
void
customization_data_add_part (GObject *object, GObject *part)
{
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (G_IS_OBJECT (part));

	CustomizationData *cust_data;
	cust_data = customization_data_get (object);
	if (!cust_data)
		return;

	GWeakRef *wref;
	wref = g_new0 (GWeakRef, 1);
	g_weak_ref_init (wref, part);
	g_array_append_val (cust_data->custom_parts, wref);
}

/**
 * customization_data_add_actions:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 * @entries: a list of actions
 * @n_entries: the size of @entries
 *
 * Adds some actions (which will be removed when customization_data_release() is called)
 */
void
customization_data_add_actions (GObject *object, GActionEntry *entries, guint n_entries)
{
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (entries);

	CustomizationData *cust_data;
	cust_data = customization_data_get (object);
	if (!cust_data)
		return;

	BrowserWindow *bwin;
	if (IS_BROWSER_PERSPECTIVE (object))
		bwin = browser_perspective_get_window (BROWSER_PERSPECTIVE (object));
	else if (IS_BROWSER_PAGE (object))
		bwin = browser_perspective_get_window (browser_page_get_perspective (BROWSER_PAGE (object)));
	else {
		g_warning ("Unhandled object type for %s", __FUNCTION__);
		return;
	}
	g_action_map_add_action_entries (G_ACTION_MAP (bwin), entries, n_entries, object);

	cust_data->bwin = bwin;
	cust_data->actions = entries;
	cust_data->n_actions = n_entries;
}

/**
 * customization_data_get_toolbar:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 *
 * Retreive the toolbar used in @cust_data
 *
 * Returns: (transfer none): the requested data
 */
GtkToolbar *
customization_data_get_toolbar (GObject *object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);

	CustomizationData *cust_data;
	cust_data = customization_data_get (object);
	if (!cust_data)
		return NULL;
	else
		return cust_data->toolbar;
}

/**
 * customization_data_get_header_bar:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 *
 * Retreive the header bar used in @cust_data
 *
 * Returns: (transfer none): the requested data
 */
GtkHeaderBar *
customization_data_get_header_bar (GObject *object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);

	CustomizationData *cust_data;
	cust_data = customization_data_get (object);
	if (!cust_data)
		return NULL;
	else
		return cust_data->header;
}

/**
 * customization_data_get_action:
 * @object: an object being customized (#BrowserPerspective or #BrowserPage)
 * @action_name: the name of the action to retreive
 *
 * Get a pointer to the #GAction names @action_name.
 *
 * Returns: (transfer none): the requested #Gaction, or %NULL if not found (not existant,
 *          or no customization data for @object)
 */
GAction *
customization_data_get_action (GObject *object, const gchar *action_name)
{
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (action_name && *action_name, NULL);

	if (customization_data_exists (object)) {
		BrowserWindow *bwin;
		if (IS_BROWSER_PERSPECTIVE (object))
			bwin = browser_perspective_get_window (BROWSER_PERSPECTIVE (object));
		else if (IS_BROWSER_PAGE (object))
			bwin = browser_perspective_get_window (browser_page_get_perspective (BROWSER_PAGE (object)));
		else {
			g_warning ("Unhandled object type for %s", __FUNCTION__);
			return NULL;
		}

		return g_action_map_lookup_action (G_ACTION_MAP (bwin), action_name);
	}
	return NULL;
}
