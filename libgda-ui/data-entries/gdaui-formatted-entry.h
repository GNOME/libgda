/* gdaui-formatted-entry.h
 *
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDAUI_FORMATTED_ENTRY_H__
#define __GDAUI_FORMATTED_ENTRY_H__

#include "gdaui-entry.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_FORMATTED_ENTRY                 (gdaui_formatted_entry_get_type ())
#define GDAUI_FORMATTED_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_FORMATTED_ENTRY, GdauiFormattedEntry))
#define GDAUI_FORMATTED_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_FORMATTED_ENTRY, GdauiFormattedEntry))
#define GDAUI_IS_FORMATTED_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_FORMATTED_ENTRY))
#define GDAUI_IS_FORMATTED_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_FORMATTED_ENTRY))
#define GDAUI_FORMATTED_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_FORMATTED_ENTRY, GdauiFormattedEntry))


typedef struct _GdauiFormattedEntry        GdauiFormattedEntry;
typedef struct _GdauiFormattedEntryClass   GdauiFormattedEntryClass;
typedef struct _GdauiFormattedEntryPrivate GdauiFormattedEntryPrivate;

struct _GdauiFormattedEntry
{
	GdauiEntry                  entry;
	GdauiFormattedEntryPrivate *priv;
};

struct _GdauiFormattedEntryClass
{
	GdauiEntryClass             parent_class;
};

GType                 gdaui_formatted_entry_get_type           (void) G_GNUC_CONST;
GtkWidget            *gdaui_formatted_entry_new                (const gchar *format, const gchar *mask);
gchar                *gdaui_formatted_entry_get_text           (GdauiFormattedEntry *entry);

typedef void (*GdauiFormattedEntryInsertFunc) (GdauiFormattedEntry *entry, gunichar insert_char,
					       gint virt_pos, gpointer data);
void                  gdaui_formatted_entry_set_insert_func    (GdauiFormattedEntry *entry,
								GdauiFormattedEntryInsertFunc insert_func,
								gpointer data);

G_END_DECLS

#endif
