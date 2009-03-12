/*
 * gda-types.h
 * Copyright (C) 2007 Sebastien Granjoux  <seb.sfo@free.fr>
 *               2008 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GDA_TYPES_H__
#define __GDA_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* Boxed type for GError will not be done in GLib see bug #300610 */
#define GDA_TYPE_ERROR (_gda_error_get_type())
GType   _gda_error_get_type (void) G_GNUC_CONST;

/* Pointer type for GSList (not a boxed type!) */
#define GDA_TYPE_SLIST (_gda_slist_get_type())
GType   _gda_slist_get_type (void) G_GNUC_CONST;

/* Pointer type for GdaMetaContext (not a boxed type!) */
#define GDA_TYPE_META_CONTEXT (_gda_meta_context_get_type())
GType   _gda_meta_context_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif
