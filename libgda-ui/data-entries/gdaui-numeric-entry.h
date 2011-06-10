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

#ifndef __GDAUI_NUMERIC_ENTRY_H__
#define __GDAUI_NUMERIC_ENTRY_H__

#include "gdaui-entry.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_NUMERIC_ENTRY                 (gdaui_numeric_entry_get_type ())
#define GDAUI_NUMERIC_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_NUMERIC_ENTRY, GdauiNumericEntry))
#define GDAUI_NUMERIC_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_NUMERIC_ENTRY, GdauiNumericEntry))
#define GDAUI_IS_NUMERIC_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_NUMERIC_ENTRY))
#define GDAUI_IS_NUMERIC_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_NUMERIC_ENTRY))
#define GDAUI_NUMERIC_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_NUMERIC_ENTRY, GdauiNumericEntry))


typedef struct _GdauiNumericEntry        GdauiNumericEntry;
typedef struct _GdauiNumericEntryClass   GdauiNumericEntryClass;
typedef struct _GdauiNumericEntryPrivate GdauiNumericEntryPrivate;

struct _GdauiNumericEntry
{
	GdauiEntry                  entry;
	GdauiNumericEntryPrivate *priv;
};

struct _GdauiNumericEntryClass
{
	GdauiEntryClass             parent_class;
};

GType                 gdaui_numeric_entry_get_type           (void) G_GNUC_CONST;
GtkWidget            *gdaui_numeric_entry_new                (GType type);
GValue               *gdaui_numeric_entry_get_value          (GdauiNumericEntry *entry);

G_END_DECLS

#endif
