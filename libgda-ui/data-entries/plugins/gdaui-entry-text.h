/* gdaui-entry-text.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef __GDAUI_ENTRY_TEXT_H_
#define __GDAUI_ENTRY_TEXT_H_

#include <libgda-ui/data-entries/gdaui-entry-wrapper.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_ENTRY_TEXT          (gdaui_entry_text_get_type())
#define GDAUI_ENTRY_TEXT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_entry_text_get_type(), GdauiEntryText)
#define GDAUI_ENTRY_TEXT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_entry_text_get_type (), GdauiEntryTextClass)
#define GDAUI_IS_ENTRY_TEXT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_entry_text_get_type ())


typedef struct _GdauiEntryText GdauiEntryText;
typedef struct _GdauiEntryTextClass GdauiEntryTextClass;
typedef struct _GdauiEntryTextPrivate GdauiEntryTextPrivate;


/* struct for the object's data */
struct _GdauiEntryText
{
	GdauiEntryWrapper              object;
	GdauiEntryTextPrivate         *priv;
};

/* struct for the object's class */
struct _GdauiEntryTextClass
{
	GdauiEntryWrapperClass         parent_class;
};

GType        gdaui_entry_text_get_type        (void) G_GNUC_CONST;
GtkWidget   *gdaui_entry_text_new             (GdaDataHandler *dh, GType type);


G_END_DECLS

#endif
