/* GNOME DB library
 *
 * Copyright (C) 1999 - 2009 The Free Software Foundation
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
	GdauiComboPrivate *priv;
};

struct _GdauiComboClass {
	GtkComboBoxClass     parent_class;
};

GType         gdaui_combo_get_type         (void) G_GNUC_CONST;

GtkWidget    *gdaui_combo_new              (void);
GtkWidget    *gdaui_combo_new_with_model   (GdaDataModel *model, gint n_cols, gint *cols_index);

void          gdaui_combo_set_model        (GdauiCombo *combo, GdaDataModel *model, gint n_cols, gint *cols_index);
GdaDataModel *gdaui_combo_get_model        (GdauiCombo *combo);
void          gdaui_combo_add_undef_choice (GdauiCombo *combo, gboolean add_undef_choice);

gboolean      gdaui_combo_set_values       (GdauiCombo *combo, const GSList *values);
GSList       *gdaui_combo_get_values       (GdauiCombo *combo);
gboolean      gdaui_combo_undef_selected   (GdauiCombo *combo);

gboolean      gdaui_combo_set_values_ext   (GdauiCombo *combo, const GSList *values, gint *cols_index);
GSList       *gdaui_combo_get_values_ext   (GdauiCombo *combo, gint n_cols, gint *cols_index);


G_END_DECLS

#endif
