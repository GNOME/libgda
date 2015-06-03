/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef __GDAUI_ENTRY_IMPORT_H_
#define __GDAUI_ENTRY_IMPORT_H_

#include <libgda-ui/data-entries/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_IMPORT          (gdaui_entry_import_get_type())
#define GDAUI_ENTRY_IMPORT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_import_get_type(), GdauiEntryImport)
#define GDAUI_ENTRY_IMPORT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_import_get_type (), GdauiEntryImportClass)
#define GDAUI_IS_ENTRY_IMPORT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_import_get_type ())


typedef struct _GdauiEntryImport GdauiEntryImport;
typedef struct _GdauiEntryImportClass GdauiEntryImportClass;
typedef struct _GdauiEntryImportPrivate GdauiEntryImportPrivate;


/* struct for the object's data */
struct _GdauiEntryImport
{
	GdauiEntryWrapper              object;
	GdauiEntryImportPrivate         *priv;
};

/* struct for the object's class */
struct _GdauiEntryImportClass
{
	GdauiEntryWrapperClass         parent_class;
};

GType        gdaui_entry_import_get_type        (void) G_GNUC_CONST;
GtkWidget   *gdaui_entry_import_new             (GType type);


G_END_DECLS

#endif
