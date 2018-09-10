/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_PROVIDER_SELECTOR_H__
#define __GDAUI_PROVIDER_SELECTOR_H__

#include <libgda-ui/gdaui-combo.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_PROVIDER_SELECTOR            (gdaui_provider_selector_get_type())
G_DECLARE_DERIVABLE_TYPE (GdauiProviderSelector, gdaui_provider_selector, GDAUI, PROVIDER_SELECTOR, GdauiCombo)

struct _GdauiProviderSelectorClass {
	GdauiComboClass     parent_class;
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-provider-selector
 * @short_description: Select a database provider from a combo box
 * @title: GdauiProviderSelector
 * @stability: Stable
 * @Image: vi-provider-selector.png
 * @see_also:
 */

GtkWidget         *gdaui_provider_selector_new              (void);

GdaServerProvider *gdaui_provider_selector_get_provider_obj (GdauiProviderSelector *selector);
const gchar       *gdaui_provider_selector_get_provider     (GdauiProviderSelector *selector);
gboolean           gdaui_provider_selector_set_provider     (GdauiProviderSelector *selector, const gchar *provider);

G_END_DECLS

#endif
