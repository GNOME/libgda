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


#ifndef __GDAUI_ENTRY_DATE_H_
#define __GDAUI_ENTRY_DATE_H_

#include "gdaui-entry-common-time.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_DATE          (gdaui_entry_date_get_type())
#define GDAUI_ENTRY_DATE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_date_get_type(), GdauiEntryDate)
#define GDAUI_ENTRY_DATE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_date_get_type (), GdauiEntryDateClass)
#define GDAUI_IS_ENTRY_DATE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_date_get_type ())


typedef struct _GdauiEntryDate GdauiEntryDate;
typedef struct _GdauiEntryDateClass GdauiEntryDateClass;
typedef struct _GdauiEntryDatePrivate GdauiEntryDatePrivate;


/* struct for the object's data */
struct _GdauiEntryDate
{
	GdauiEntryCommonTime           object;
};

/* struct for the object's class */
struct _GdauiEntryDateClass
{
	GdauiEntryCommonTimeClass      parent_class;
};

GType        gdaui_entry_date_get_type        (void) G_GNUC_CONST;
GtkWidget   *gdaui_entry_date_new             (GdaDataHandler *dh);


G_END_DECLS

#endif
