/*
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#ifndef __CUSTOM_ENTRY_PASSWORD_H_
#define __CUSTOM_ENTRY_PASSWORD_H_

#include <libgda-ui/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define CUSTOM_ENTRY_PASSWORD_TYPE          (custom_entry_password_get_type())
#define CUSTOM_ENTRY_PASSWORD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, custom_entry_password_get_type(), CustomEntryPassword)
#define CUSTOM_ENTRY_PASSWORD_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, custom_entry_password_get_type (), CustomEntryPasswordClass)
#define IS_CUSTOM_ENTRY_PASSWORD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, custom_entry_password_get_type ())


typedef struct _CustomEntryPassword CustomEntryPassword;
typedef struct _CustomEntryPasswordClass CustomEntryPasswordClass;
typedef struct _CustomEntryPasswordPrivate CustomEntryPasswordPrivate;


/* struct for the object's data */
struct _CustomEntryPassword
{
	GdauiEntryWrapper              object;
	CustomEntryPasswordPrivate    *priv;
};

/* struct for the object's class */
struct _CustomEntryPasswordClass
{
	GdauiEntryWrapperClass         parent_class;
};

GType        custom_entry_password_get_type (void) G_GNUC_CONST;
GtkWidget   *custom_entry_password_new      (GdaDataHandler *dh, GType type, const gchar *options);


G_END_DECLS

#endif
