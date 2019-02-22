/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <libgda/gda-config.h>
#include <libgda-ui/gdaui-provider-selector.h>
#include <libgda-ui/gdaui-combo.h>

typedef struct {
	gint dummy;
} GdauiProviderSelectorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiProviderSelector, gdaui_provider_selector, GDAUI_TYPE_COMBO)

static void gdaui_provider_selector_show       (GtkWidget *widget);


/* column to display */
static gint cols[] = {0};

/*
 * Private functions
 */

static void
show_providers (GdauiProviderSelector *selector)
{
	GdaDataModel *model;

	model = gda_config_list_providers ();
	gdaui_combo_set_data (GDAUI_COMBO (selector), model, 1, cols);
	g_object_unref (model);
}

static void
gdaui_provider_selector_show (GtkWidget *widget)
{
	GSList *list;
	GValue *tmpval;
	GdauiProviderSelector *selector;

	selector = (GdauiProviderSelector *) widget;
	GTK_WIDGET_CLASS (gdaui_provider_selector_parent_class)->show (widget);
	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SQLite");
	list = g_slist_append (NULL, tmpval);
	_gdaui_combo_set_selected_ext (GDAUI_COMBO (selector), list, cols);
	gda_value_free ((GValue *)(list->data));
	g_slist_free (list);
}

/*
 * GdauiProviderSelector class implementation
 */

static void
gdaui_provider_selector_class_init (GdauiProviderSelectorClass *klass)
{
	GTK_WIDGET_CLASS (klass)->show = gdaui_provider_selector_show;
}

static void
gdaui_provider_selector_init (GdauiProviderSelector *selector)
{
	show_providers (selector);
}

/**
 * gdaui_provider_selector_new:
 *
 * Create a new #GdauiProviderSelector widget.
 *
 * Returns: (transfer full): the newly created widget.
 */
GtkWidget *
gdaui_provider_selector_new (void)
{
	GdauiProviderSelector *selector;

	selector = g_object_new (GDAUI_TYPE_PROVIDER_SELECTOR, NULL);
	return GTK_WIDGET (selector);
}

/**
 * gdaui_provider_selector_get_provider:
 * @selector: a #GdauiProviderSelector widget
 *
 * Get the selected provider.
 *
 * Returns: (transfer none): the selected provider, or %NULL if no provider is selected
 *
 * Since: 4.2
 */
const gchar *
gdaui_provider_selector_get_provider (GdauiProviderSelector *selector)
{
	GSList *list;
	const gchar *str;

	g_return_val_if_fail (GDAUI_IS_PROVIDER_SELECTOR (selector), NULL);
	list = _gdaui_combo_get_selected (GDAUI_COMBO (selector));
	if (list && list->data) {
		str = g_value_get_string ((GValue *)(list->data));
		g_slist_free (list);
		return str;
	}
	else
		return NULL;
}

/**
 * gdaui_provider_selector_set_provider:
 * @selector: a #GdauiProviderSelector widget
 * @provider: (nullable): the provider to be selected, or %NULL for the default (SQLite)
 *
 * Forces @selector to be set on @provider
 *
 * Returns: %TRUE if @provider has been selected
 *
 * Since: 4.2
 */
gboolean
gdaui_provider_selector_set_provider (GdauiProviderSelector *selector, const gchar *provider)
{
	GSList *list;
	gboolean retval;
	GValue *tmpval;

	g_return_val_if_fail (GDAUI_IS_PROVIDER_SELECTOR (selector), FALSE);

	if (provider && *provider)
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), provider);
	else
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SQLite");

	list = g_slist_append (NULL, tmpval);
	retval = _gdaui_combo_set_selected_ext (GDAUI_COMBO (selector), list, cols);
	gda_value_free ((GValue *)(list->data));
	g_slist_free (list);

	return retval;
}

/**
 * gdaui_provider_selector_get_provider_obj:
 * @selector: a #GdauiProviderSelector widget
 *
 * Get the selected provider as a #GdaServerProvider object
 *
 * Returns: (transfer none): a #GdaServerProvider or %NULL if an error occurred
 *
 * Since: 4.2
 */
GdaServerProvider *
gdaui_provider_selector_get_provider_obj (GdauiProviderSelector *selector)
{
	const gchar *pname;

	g_return_val_if_fail (GDAUI_IS_PROVIDER_SELECTOR (selector), NULL);

	pname = gdaui_provider_selector_get_provider (selector);
	if (pname)
		return gda_config_get_provider (pname, NULL);
	else
		return NULL;
}
