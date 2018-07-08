/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_COMBO_H__
#define __GDAUI_COMBO_H__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_COMBO            (gdaui_combo_get_type())
#define GDAUI_COMBO(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_COMBO, GdauiCombo))
#define GDAUI_COMBO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_COMBO, GdauiComboClass))
#define GDAUI_IS_COMBO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_COMBO))
#define GDAUI_IS_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_COMBO))

typedef struct _GdauiCombo        GdauiCombo;
typedef struct _GdauiComboClass   GdauiComboClass;
typedef struct _GdauiComboPrivate GdauiComboPrivate;

struct _GdauiCombo {
	GtkComboBox          object;
	GdauiComboPrivate   *priv;
};

struct _GdauiComboClass {
	GtkComboBoxClass     parent_class;
};

/**
 * SECTION:gdaui-combo
 * @short_description: Combo box to choose from the contents of a #GdaDataModel
 * @title: GdauiCombo
 * @stability: Stable
 * @Image: vi-combo.png
 * @see_also:
 */

GType         gdaui_combo_get_type         (void) G_GNUC_CONST;

GtkWidget    *gdaui_combo_new              (void);
GtkWidget    *gdaui_combo_new_with_model   (GdaDataModel *model, gint n_cols, gint *cols_index);

void					gdaui_combo_set_data				 (GdauiCombo *combo, GdaDataModel *model, gint n_cols, gint *cols_index);
void          gdaui_combo_add_null         (GdauiCombo *combo, gboolean add_null);
gboolean      gdaui_combo_is_null_selected (GdauiCombo *combo);


/* private API */
gboolean      _gdaui_combo_set_selected     (GdauiCombo *combo, const GSList *values);
GSList       *_gdaui_combo_get_selected     (GdauiCombo *combo);
gboolean      _gdaui_combo_set_selected_ext (GdauiCombo *combo, const GSList *values, gint *cols_index);
GSList       *_gdaui_combo_get_selected_ext (GdauiCombo *combo, gint n_cols, gint *cols_index);


G_END_DECLS

#endif
