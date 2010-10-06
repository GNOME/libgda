/* GNOME DB library
 * Copyright (C) 1999 - 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <string.h>
#include <libgda/gda-config.h>
#include <libgda-ui/gdaui-provider-selector.h>
#include <libgda-ui/gdaui-combo.h>

struct _GdauiProviderSelectorPrivate {
	gint dummy;
};

static void gdaui_provider_selector_class_init (GdauiProviderSelectorClass *klass);
static void gdaui_provider_selector_init       (GdauiProviderSelector *selector,
						GdauiProviderSelectorClass *klass);
static void gdaui_provider_selector_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/* column to display */
static gint cols[] = {0};

/*
 * Private functions
 */

static void
show_providers (GdauiProviderSelector *selector)
{
	GdaDataModel *model;
	GSList *list;
	GValue *tmpval;

	model = gda_config_list_providers ();
	gdaui_combo_set_model (GDAUI_COMBO (selector), model, 1, cols);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "SQLite");
	list = g_slist_append (NULL, tmpval);
	_gdaui_combo_set_selected_ext (GDAUI_COMBO (selector), list, cols);
	gda_value_free ((GValue *)(list->data));
	g_slist_free (list);
	g_object_unref (model);
}

/*
 * GdauiProviderSelector class implementation
 */

static void
gdaui_provider_selector_class_init (GdauiProviderSelectorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_provider_selector_finalize;
}

static void
gdaui_provider_selector_init (GdauiProviderSelector *selector,
			      G_GNUC_UNUSED GdauiProviderSelectorClass *klass)
{
	g_return_if_fail (GDAUI_IS_PROVIDER_SELECTOR (selector));

	selector->priv = g_new0 (GdauiProviderSelectorPrivate, 1);
	show_providers (selector);
}

static void
gdaui_provider_selector_finalize (GObject *object)
{
	GdauiProviderSelector *selector = (GdauiProviderSelector *) object;

	g_return_if_fail (GDAUI_IS_PROVIDER_SELECTOR (selector));

	g_free (selector->priv);
	selector->priv = NULL;

	parent_class->finalize (object);
}

GType
gdaui_provider_selector_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiProviderSelectorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_provider_selector_class_init,
			NULL,
			NULL,
			sizeof (GdauiProviderSelector),
			0,
			(GInstanceInitFunc) gdaui_provider_selector_init,
			0
		};
		type = g_type_register_static (GDAUI_TYPE_COMBO, "GdauiProviderSelector", &info, 0);
	}
	return type;
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
 * @provider: (allow-none): the provider to be selected, or %NULL for the default (SQLite)
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
