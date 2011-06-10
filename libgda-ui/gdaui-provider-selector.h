/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#define GDAUI_PROVIDER_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_PROVIDER_SELECTOR, GdauiProviderSelector))
#define GDAUI_PROVIDER_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_PROVIDER_SELECTOR, GdauiProviderSelectorClass))
#define GDAUI_IS_PROVIDER_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_PROVIDER_SELECTOR))
#define GDAUI_IS_PROVIDER_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_PROVIDER_SELECTOR))

typedef struct _GdauiProviderSelector        GdauiProviderSelector;
typedef struct _GdauiProviderSelectorClass   GdauiProviderSelectorClass;
typedef struct _GdauiProviderSelectorPrivate GdauiProviderSelectorPrivate;

struct _GdauiProviderSelector {
	GdauiCombo                    parent;
	GdauiProviderSelectorPrivate *priv;
};

struct _GdauiProviderSelectorClass {
	GdauiComboClass               parent_class;
};

/**
 * SECTION:gdaui-provider-selector
 * @short_description: Select a database provider from a combo box
 * @title: GdauiProviderSelector
 * @stability: Stable
 * @Image: vi-provider-selector.png
 * @see_also:
 */

GType              gdaui_provider_selector_get_type         (void) G_GNUC_CONST;
GtkWidget         *gdaui_provider_selector_new              (void);

GdaServerProvider *gdaui_provider_selector_get_provider_obj (GdauiProviderSelector *selector);
const gchar       *gdaui_provider_selector_get_provider     (GdauiProviderSelector *selector);
gboolean           gdaui_provider_selector_set_provider     (GdauiProviderSelector *selector, const gchar *provider);

G_END_DECLS

#endif
