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


#ifndef __GDAUI_ENTRY_STRING_H_
#define __GDAUI_ENTRY_STRING_H_

#include "gdaui-entry-wrapper.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_STRING          (gdaui_entry_string_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiEntryString, gdaui_entry_string, GDAUI, ENTRY_STRING, GdauiEntryWrapper)
/* struct for the object's class */
struct _GdauiEntryStringClass
{
	GdauiEntryWrapperClass         parent_class;
};

GtkWidget   *gdaui_entry_string_new             (GdaDataHandler *dh, GType type, const gchar *options);


G_END_DECLS

#endif
