/* GDA Xbase Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_xbase_database_h__)
#  define __gda_xbase_database_h__

#include <libgda/gda-connection.h>

G_BEGIN_DECLS

typedef struct _GdaXbaseDatabase GdaXbaseDatabase;

GdaXbaseDatabase *gda_xbase_database_open (GdaConnection *cnc, const gchar *filename);
GdaXbaseDatabase *gda_xbase_database_create (GdaConnection *cnc, const gchar *filename);
void              gda_xbase_database_close (GdaXbaseDatabase *xdb);

gboolean          gda_xbase_database_delete_all_records (GdaXbaseDatabase *xdb);

G_END_DECLS

#endif
