/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DSN_SELECTOR_H__
#define __GDAUI_DSN_SELECTOR_H__

#include <libgda-ui/gdaui-combo.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DSN_SELECTOR            (_gdaui_dsn_selector_get_type())
#define GDAUI_DSN_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DSN_SELECTOR, GdauiDsnSelector))
#define GDAUI_DSN_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_DSN_SELECTOR, GdauiDsnSelectorClass))
#define GDAUI_IS_DSN_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DSN_SELECTOR))
#define GDAUI_IS_DSN_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DSN_SELECTOR))

typedef struct _GdauiDsnSelector        GdauiDsnSelector;
typedef struct _GdauiDsnSelectorClass   GdauiDsnSelectorClass;
typedef struct _GdauiDsnSelectorPrivate GdauiDsnSelectorPrivate;

struct _GdauiDsnSelector {
	GdauiCombo                      combo;
	GdauiDsnSelectorPrivate *priv;
};

struct _GdauiDsnSelectorClass {
	GdauiComboClass                 parent_class;
};

GType      _gdaui_dsn_selector_get_type (void) G_GNUC_CONST;
GtkWidget *_gdaui_dsn_selector_new      (void);
gchar     *_gdaui_dsn_selector_get_dsn  (GdauiDsnSelector *selector);
void       _gdaui_dsn_selector_set_dsn  (GdauiDsnSelector *selector, const gchar *dsn);

G_END_DECLS

#endif
