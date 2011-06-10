/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDAUI_ENTRY_NONE_H_
#define __GDAUI_ENTRY_NONE_H_

#include "gdaui-entry-wrapper.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_NONE          (gdaui_entry_none_get_type())
#define GDAUI_ENTRY_NONE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_none_get_type(), GdauiEntryNone)
#define GDAUI_ENTRY_NONE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_none_get_type (), GdauiEntryNoneClass)
#define GDAUI_IS_ENTRY_NONE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_none_get_type ())


typedef struct _GdauiEntryNone GdauiEntryNone;
typedef struct _GdauiEntryNoneClass GdauiEntryNoneClass;
typedef struct _GdauiEntryNonePrivate GdauiEntryNonePrivate;


/* struct for the object's data */
struct _GdauiEntryNone
{
	GdauiEntryWrapper            object;
	GdauiEntryNonePrivate       *priv;
};

/* struct for the object's class */
struct _GdauiEntryNoneClass
{
	GdauiEntryWrapperClass       parent_class;
};

GType        gdaui_entry_none_get_type        (void) G_GNUC_CONST;
GtkWidget   *gdaui_entry_none_new             (GType type);


G_END_DECLS

#endif
