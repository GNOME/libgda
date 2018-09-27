/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_FORMATTED_ENTRY_H__
#define __GDAUI_FORMATTED_ENTRY_H__

#include "gdaui-entry.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_FORMATTED_ENTRY                 (gdaui_formatted_entry_get_type ())
G_DECLARE_DERIVABLE_TYPE (GdauiFormattedEntry, gdaui_formatted_entry, GDAUI, FORMATTED_ENTRY, GdauiEntry)
struct _GdauiFormattedEntryClass
{
	GdauiEntryClass             parent_class;
};

GtkWidget            *gdaui_formatted_entry_new                (const gchar *format, const gchar *mask);
gchar                *gdaui_formatted_entry_get_text           (GdauiFormattedEntry *entry);

typedef void (*GdauiFormattedEntryInsertFunc) (GdauiFormattedEntry *entry, gunichar insert_char,
					       gint virt_pos, gpointer data);
void                  gdaui_formatted_entry_set_insert_func    (GdauiFormattedEntry *entry,
								GdauiFormattedEntryInsertFunc insert_func,
								gpointer data);

G_END_DECLS

#endif
