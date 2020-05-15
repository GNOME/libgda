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


#ifndef __GDAUI_ENTRY_FILESEL_H_
#define __GDAUI_ENTRY_FILESEL_H_

#include <libgda-ui/data-entries/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_FILESEL          (gdaui_entry_filesel_get_type())

G_DECLARE_FINAL_TYPE(GdauiEntryFilesel, gdaui_entry_filesel, GDAUI, ENTRY_FILESEL, GdauiEntryWrapper)

GtkWidget   *gdaui_entry_filesel_new             (GdaDataHandler *dh, GType type, const gchar *options);

G_END_DECLS

#endif
