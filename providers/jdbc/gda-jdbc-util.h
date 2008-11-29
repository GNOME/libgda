/* GDA postgres provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_UTIL_H__
#define __GDA_POSTGRES_UTIL_H__

#include "gda-jdbc.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

GdaConnectionEvent *_gda_jdbc_make_error   (GdaConnection *cnc, gint error_code, gchar *sql_state, GError *error);
JNIEnv             *_gda_jdbc_get_jenv     (gboolean *out_needs_detach, GError **error);
void                _gda_jdbc_release_jenv (gboolean needs_detach);

int                 _gda_jdbc_gtype_to_proto_type (GType type);

G_END_DECLS

#endif

