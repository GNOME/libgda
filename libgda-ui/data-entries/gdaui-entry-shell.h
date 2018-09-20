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

#ifndef __GDAUI_ENTRY_SHELL__
#define __GDAUI_ENTRY_SHELL__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-data-entry.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_SHELL          (gdaui_entry_shell_get_type())
G_DECLARE_DERIVABLE_TYPE (GdauiEntryShell, gdaui_entry_shell, GDAUI, ENTRY_SHELL, GtkBox)
/* struct for the object's class */
struct _GdauiEntryShellClass
{
	GtkBoxClass           parent_class;
};


void            gdaui_entry_shell_pack_entry        (GdauiEntryShell *shell, GtkWidget *entry);
void            gdaui_entry_shell_set_invalid_color (GdauiEntryShell *shell, gdouble red, gdouble green,
						     gdouble blue, gdouble alpha);

/* private */
void           _gdaui_entry_shell_mark_editable     (GdauiEntryShell *shell, gboolean editable);
void           _gdaui_entry_shell_attrs_changed     (GdauiEntryShell *shell, GdaValueAttribute attr);

G_END_DECLS

#endif
