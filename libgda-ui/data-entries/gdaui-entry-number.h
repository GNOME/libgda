/* gdaui-entry-number.h
 *
 * Copyright (C) 2009 Vivien Malerba
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


#ifndef __GDAUI_ENTRY_NUMBER_H_
#define __GDAUI_ENTRY_NUMBER_H_

#include "gdaui-entry-wrapper.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_NUMBER          (gdaui_entry_number_get_type())
#define GDAUI_ENTRY_NUMBER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_number_get_type(), GdauiEntryNumber)
#define GDAUI_ENTRY_NUMBER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_number_get_type (), GdauiEntryNumberClass)
#define GDAUI_IS_ENTRY_NUMBER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_number_get_type ())


typedef struct _GdauiEntryNumber GdauiEntryNumber;
typedef struct _GdauiEntryNumberClass GdauiEntryNumberClass;
typedef struct _GdauiEntryNumberPrivate GdauiEntryNumberPrivate;


/* struct for the object's data */
struct _GdauiEntryNumber
{
	GdauiEntryWrapper              object;
	GdauiEntryNumberPrivate       *priv;
};

/* struct for the object's class */
struct _GdauiEntryNumberClass
{
	GdauiEntryWrapperClass         parent_class;
};

GType        gdaui_entry_number_get_type        (void) G_GNUC_CONST;
GtkWidget   *gdaui_entry_number_new             (GdaDataHandler *dh, GType type, const gchar *options);


gboolean     gdaui_entry_number_is_type_numeric (GType type);

G_END_DECLS

#endif
