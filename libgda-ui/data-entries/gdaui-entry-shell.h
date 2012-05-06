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

#ifndef __GDAUI_ENTRY_SHELL__
#define __GDAUI_ENTRY_SHELL__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-data-entry.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_SHELL          (gdaui_entry_shell_get_type())
#define GDAUI_ENTRY_SHELL(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_shell_get_type(), GdauiEntryShell)
#define GDAUI_ENTRY_SHELL_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_shell_get_type (), GdauiEntryShellClass)
#define GDAUI_IS_ENTRY_SHELL(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_shell_get_type ())

/*
 * Very simple object wrapper for the widgets that will be used to display
 * the data.
 */

typedef struct _GdauiEntryShell      GdauiEntryShell;
typedef struct _GdauiEntryShellClass GdauiEntryShellClass;
typedef struct _GdauiEntryShellPriv  GdauiEntryShellPriv;

/* struct for the object's data */
struct _GdauiEntryShell
{
	GtkViewport         object;

	GdauiEntryShellPriv  *priv;
};

/* struct for the object's class */
struct _GdauiEntryShellClass
{
	GtkViewportClass    parent_class;
};


GType           gdaui_entry_shell_get_type      (void) G_GNUC_CONST;
void            gdaui_entry_shell_pack_entry    (GdauiEntryShell *shell, GtkWidget *main_widget);
void            gdaui_entry_shell_refresh       (GdauiEntryShell *shell);
void            gdaui_entry_shell_set_unknown   (GdauiEntryShell *shell, gboolean unknown);
void            gdaui_entry_shell_set_ucolor    (GdauiEntryShell *shell, gdouble red, gdouble green,
						 gdouble blue, gdouble alpha);


G_END_DECLS

#endif
