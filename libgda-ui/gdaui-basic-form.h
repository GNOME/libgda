/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_BASIC_FORM__
#define __GDAUI_BASIC_FORM__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_BASIC_FORM          (gdaui_basic_form_get_type())
#define GDAUI_BASIC_FORM(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_basic_form_get_type(), GdauiBasicForm)
#define GDAUI_BASIC_FORM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_basic_form_get_type (), GdauiBasicFormClass)
#define GDAUI_IS_BASIC_FORM(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_basic_form_get_type ())
#define GDAUI_IS_BASIC_FORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDAUI_TYPE_BASIC_FORM))
#define GDAUI_BASIC_FORM_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), GDAUI_TYPE_BASIC_FORM, GdauiBasicFormClass))


typedef struct _GdauiBasicForm      GdauiBasicForm;
typedef struct _GdauiBasicFormClass GdauiBasicFormClass;
typedef struct _GdauiBasicFormPriv  GdauiBasicFormPriv;

/* struct for the object's data */
struct _GdauiBasicForm
{
	GtkVBox             object;

	GdauiBasicFormPriv *priv;
};

/* struct for the object's class */
struct _GdauiBasicFormClass
{
	GtkVBoxClass        parent_class;

	/* signals */
        void       (*holder_changed) (GdauiBasicForm *form, GdaHolder *holder, gboolean is_user_action);
	void       (*activated)      (GdauiBasicForm *form);
	void       (*layout_changed) (GdauiBasicForm *form);
};

/* 
 * Generic widget's methods 
*/
GType             gdaui_basic_form_get_type                 (void) G_GNUC_CONST;
GtkWidget        *gdaui_basic_form_new                      (GdaSet *data_set);
GtkWidget        *gdaui_basic_form_new_in_dialog            (GdaSet *data_set, GtkWindow *parent,
							     const gchar *title, const gchar *header);
GdaSet           *gdaui_basic_form_get_data_set             (GdauiBasicForm *form);
gboolean          gdaui_basic_form_is_valid                 (GdauiBasicForm *form);
gboolean          gdaui_basic_form_has_changed              (GdauiBasicForm *form);
void              gdaui_basic_form_reset                    (GdauiBasicForm *form);
void              gdaui_basic_form_set_as_reference         (GdauiBasicForm *form);

void              gdaui_basic_form_entry_set_visible        (GdauiBasicForm *form, 
							     GdaHolder *holder, gboolean show);
void              gdaui_basic_form_entry_grab_focus         (GdauiBasicForm *form, GdaHolder *holder);
void              gdaui_basic_form_entry_set_editable       (GdauiBasicForm *form, GdaHolder *holder, 
							     gboolean editable);
void              gdaui_basic_form_set_entries_to_default   (GdauiBasicForm *form);

GtkWidget        *gdaui_basic_form_get_entry_widget         (GdauiBasicForm *form, GdaHolder *holder);
GtkWidget        *gdaui_basic_form_get_label_widget         (GdauiBasicForm *form, GdaHolder *holder);

void              gdaui_basic_form_set_layout_from_file     (GdauiBasicForm *form, const gchar *file_name,
							     const gchar *form_name);
GtkWidget        *gdaui_basic_form_get_place_holder         (GdauiBasicForm *form, const gchar *placeholder_id);

typedef enum {
	GDAUI_BASIC_FORM_LABELS,
	GDAUI_BASIC_FORM_ENTRIES
} GdauiBasicFormPart;
void              gdaui_basic_form_add_to_size_group        (GdauiBasicForm *form, GtkSizeGroup *size_group,
							     GdauiBasicFormPart part);
void              gdaui_basic_form_remove_from_size_group   (GdauiBasicForm *form, GtkSizeGroup *size_group,
							     GdauiBasicFormPart part);

void              gdaui_basic_form_set_unknown_color        (GdauiBasicForm *form, gdouble red, gdouble green,
							     gdouble blue, gdouble alpha);

G_END_DECLS

#endif
