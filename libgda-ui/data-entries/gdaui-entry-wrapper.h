/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_ENTRY_WRAPPER__
#define __GDAUI_ENTRY_WRAPPER__

#include <gtk/gtk.h>
#include "gdaui-entry-shell.h"
#include <libgda-ui/gdaui-data-entry.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_WRAPPER          (gdaui_entry_wrapper_get_type())
#define GDAUI_ENTRY_WRAPPER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_wrapper_get_type(), GdauiEntryWrapper)
#define GDAUI_ENTRY_WRAPPER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_wrapper_get_type (), GdauiEntryWrapperClass)
#define GDAUI_IS_ENTRY_WRAPPER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_wrapper_get_type ())


typedef struct _GdauiEntryWrapper      GdauiEntryWrapper;
typedef struct _GdauiEntryWrapperClass GdauiEntryWrapperClass;
typedef struct _GdauiEntryWrapperPriv  GdauiEntryWrapperPriv;


/* struct for the object's data */
struct _GdauiEntryWrapper
{
	GdauiEntryShell        object;

	GdauiEntryWrapperPriv  *priv;
};

/* struct for the object's class */
struct _GdauiEntryWrapperClass
{
	GdauiEntryShellClass   parent_class;

	/* pure virtual functions */
	GtkWidget        *(*create_entry)     (GdauiEntryWrapper *wrapper);
	void              (*real_set_value)   (GdauiEntryWrapper *wrapper, const GValue *value);
	GValue           *(*real_get_value)   (GdauiEntryWrapper *wrapper);
	void              (*connect_signals)  (GdauiEntryWrapper *wrapper, GCallback modify_cb, GCallback activate_cb);
	gboolean          (*can_expand)       (GdauiEntryWrapper *wrapper, gboolean horiz);
	void              (*set_editable)     (GdauiEntryWrapper *wrapper, gboolean editable);

	gboolean          (*value_is_equal_to)(GdauiEntryWrapper *wrapper, const GValue *value);
	gboolean          (*value_is_null)    (GdauiEntryWrapper *wrapper);
	gboolean          (*is_valid)         (GdauiEntryWrapper *wrapper); /* not used yet */
	void              (*grab_focus)       (GdauiEntryWrapper *wrapper);
};


GType           gdaui_entry_wrapper_get_type           (void) G_GNUC_CONST;
void            gdaui_entry_wrapper_contents_changed   (GdauiEntryWrapper *wrapper);
void            gdaui_entry_wrapper_contents_activated (GdauiEntryWrapper *wrapper);

G_END_DECLS

#endif
