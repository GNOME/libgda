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

#ifndef __GDAUI_FORM__
#define __GDAUI_FORM__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_FORM          (gdaui_form_get_type())
#define GDAUI_FORM(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_form_get_type(), GdauiForm)
#define GDAUI_FORM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_form_get_type (), GdauiFormClass)
#define GDAUI_IS_FORM(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_form_get_type ())
#define GDAUI_IS_FORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDAUI_TYPE_FORM))

typedef struct _GdauiForm      GdauiForm;
typedef struct _GdauiFormClass GdauiFormClass;
typedef struct _GdauiFormPriv  GdauiFormPriv;

/* struct for the object's data */
struct _GdauiForm
{
	GtkBox             object;

	GdauiFormPriv     *priv;
};

/* struct for the object's class */
struct _GdauiFormClass
{
	GtkBoxClass       parent_class;
};

/**
 * SECTION:gdaui-form
 * @short_description: Form widget to manipulate data in a #GdaDataModel, with decorations
 * @title: GdauiForm
 * @stability: Stable
 * @Image:
 * @see_also: The #GdauiRawForm widget which is used by the #GdaForm widget.
 */

GType             gdaui_form_get_type            (void) G_GNUC_CONST;

GtkWidget        *gdaui_form_new                 (GdaDataModel *model);

G_END_DECLS

#endif



