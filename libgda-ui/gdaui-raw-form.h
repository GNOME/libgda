/*
 * Copyright (C) 2002 - 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GDAUI_RAW_FORM__
#define __GDAUI_RAW_FORM__

#include <gtk/gtk.h>
#include <libgda/gda-data-model.h>
#include "gdaui-basic-form.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_RAW_FORM          (gdaui_raw_form_get_type())
#define GDAUI_RAW_FORM(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_raw_form_get_type(), GdauiRawForm)
#define GDAUI_RAW_FORM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_raw_form_get_type (), GdauiRawFormClass)
#define GDAUI_IS_RAW_FORM(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_raw_form_get_type ())


typedef struct _GdauiRawForm      GdauiRawForm;
typedef struct _GdauiRawFormClass GdauiRawFormClass;
typedef struct _GdauiRawFormPriv  GdauiRawFormPriv;

/* struct for the object's data */
struct _GdauiRawForm
{
	GdauiBasicForm      object;

	GdauiRawFormPriv   *priv;
};

/* struct for the object's class */
struct _GdauiRawFormClass
{
	GdauiBasicFormClass parent_class;
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

GType        gdaui_raw_form_get_type              (void) G_GNUC_CONST;
GtkWidget   *gdaui_raw_form_new                   (GdaDataModel *model);

G_END_DECLS

#endif



