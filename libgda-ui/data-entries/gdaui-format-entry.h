/* gdaui-format-entry.h
 *
 * Copyright (C) 2007 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_FORMAT_ENTRY_H__
#define __GDAUI_FORMAT_ENTRY_H__

#include <gdk/gdk.h>
#include <gtk/gtkentry.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_FORMAT_ENTRY                 (gdaui_format_entry_get_type ())
#define GDAUI_FORMAT_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_FORMAT_ENTRY, GdauiFormatEntry))
#define GDAUI_FORMAT_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_FORMAT_ENTRY, GdauiFormatEntry))
#define GDAUI_IS_FORMAT_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_FORMAT_ENTRY))
#define GDAUI_IS_FORMAT_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_FORMAT_ENTRY))
#define GDAUI_FORMAT_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_FORMAT_ENTRY, GdauiFormatEntry))


typedef struct _GdauiFormatEntry        GdauiFormatEntry;
typedef struct _GdauiFormatEntryClass   GdauiFormatEntryClass;
typedef struct _GdauiFormatEntryPrivate GdauiFormatEntryPrivate;

struct _GdauiFormatEntry
{
	GtkEntry                   entry;
	GdauiFormatEntryPrivate *priv;
};

struct _GdauiFormatEntryClass
{
	GtkEntryClass              parent_class;
};

GType                 gdaui_format_entry_get_type           (void) G_GNUC_CONST;
GtkWidget            *gdaui_format_entry_new                (void);

void                  gdaui_format_entry_set_max_length     (GdauiFormatEntry *entry, gint max);
void                  gdaui_format_entry_set_format         (GdauiFormatEntry *entry, 
								const gchar *format, const gchar *mask, const gchar *completion);
void                  gdaui_format_entry_set_decimal_places (GdauiFormatEntry *entry, gint nb_decimals);
void                  gdaui_format_entry_set_separators     (GdauiFormatEntry *entry, guchar decimal, guchar thousands);
void                  gdaui_format_entry_set_edited_type    (GdauiFormatEntry *entry, GType type);
void                  gdaui_format_entry_set_prefix         (GdauiFormatEntry *entry, const gchar *prefix);
void                  gdaui_format_entry_set_suffix         (GdauiFormatEntry *entry, const gchar *suffix);

void                  gdaui_format_entry_set_text           (GdauiFormatEntry *entry, const gchar *text);
gchar                *gdaui_format_entry_get_text           (GdauiFormatEntry *entry);


G_END_DECLS

#endif
