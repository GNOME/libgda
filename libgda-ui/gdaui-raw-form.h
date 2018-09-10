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

#ifndef __GDAUI_RAW_FORM__
#define __GDAUI_RAW_FORM__

#include <gtk/gtk.h>
#include <libgda/gda-data-model.h>
#include "gdaui-basic-form.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_RAW_FORM          (gdaui_raw_form_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiRawForm, gdaui_raw_form, GDAUI, RAW_FORM, GdauiBasicForm)
/* struct for the object's class */
struct _GdauiRawFormClass
{
	GdauiBasicFormClass parent_class;
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-raw-form
 * @short_description: Form widget to manipulate data in a #GdaDataModel
 * @title: GdauiRawForm
 * @stability: Stable
 * @Image:
 * @see_also: the #GdauiForm widget which uses the #GdauiRawForm and adds decorations such as information about data model size, and features searching.
 *
 * The #GdauiForm widget which uses the #GdauiRawForm and adds decorations such as
 * information about data model size, and features searching.
 */

GtkWidget   *gdaui_raw_form_new                   (GdaDataModel *model);

G_END_DECLS

#endif



