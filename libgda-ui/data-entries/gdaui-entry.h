/* gdaui-entry.h
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

#ifndef __GDAUI_ENTRY_H__
#define __GDAUI_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY                 (gdaui_entry_get_type ())
#define GDAUI_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_ENTRY, GdauiEntry))
#define GDAUI_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_ENTRY, GdauiEntryClass))
#define GDAUI_IS_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_ENTRY))
#define GDAUI_IS_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_ENTRY))
#define GDAUI_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_ENTRY, GdauiEntryClass))

typedef struct _GdauiEntry        GdauiEntry;
typedef struct _GdauiEntryClass   GdauiEntryClass;
typedef struct _GdauiEntryPrivate GdauiEntryPrivate;

struct _GdauiEntry
{
	GtkEntry                   entry;
	GdauiEntryPrivate *priv;
};

struct _GdauiEntryClass
{
	GtkEntryClass              parent_class;

	/* virtual methods */
	/**
	 * GdauiEntryClass::get_empty_text
	 *
	 * If defined, sould return a text suitable to display EMPTY value, it will be called when
	 * entry was set to NULL and is becomming not NULL
	 *
	 * Returs: a newt string, or %NULL
	 */
	gchar                  *(*get_empty_text) (GdauiEntry *entry);

	/**
	 * GdauiEntryClass::assume_insert
	 * @entry:
	 * @text: the text to be inserted
	 * @text_length: @text's length in bytes (not characters)
	 * @virt_pos: the position where @text is to be inserted
	 * @offset: an offset to add to positions using @virt_pos as reference to call gtk_editable_*()
	 * 
	 * To be defined by children classes to handle insert themselves
	 */
	void                    (*assume_insert) (GdauiEntry *entry, const gchar *text, gint text_length,
						  gint *virt_pos, gint offset);
	/**
	 * GdauiEntryClass::assume_delete
	 * @entry:
	 * @virt_start_pos: the starting position.
	 * @virt_end_pos: the end position (not included in deletion), always > @start_pos
	 * @offset: an offset to add to positions using @virt_start_pos or @virt_end_pos as reference
	 *          to call gtk_editable_*()
	 *
	 * To be defined by children classes to handle delete themselves
	 */
	void                     (*assume_delete) (GdauiEntry *entry, gint virt_start_pos, gint virt_end_pos, gint offset);
};

GType                 gdaui_entry_get_type           (void) G_GNUC_CONST;
GtkWidget            *gdaui_entry_new                (const gchar *prefix, const gchar *suffix);

void                  gdaui_entry_set_max_length     (GdauiEntry *entry, gint max);
void                  gdaui_entry_set_prefix         (GdauiEntry *entry, const gchar *prefix);
void                  gdaui_entry_set_suffix         (GdauiEntry *entry, const gchar *suffix);

void                  gdaui_entry_set_text           (GdauiEntry *entry, const gchar *text);
gchar                *gdaui_entry_get_text           (GdauiEntry *entry);

void                  gdaui_entry_set_width_chars    (GdauiEntry *entry, gint max_width);

/* for sub classes */
void                  _gdaui_entry_block_changes     (GdauiEntry *entry);
void                  _gdaui_entry_unblock_changes   (GdauiEntry *entry);

G_END_DECLS

#endif
