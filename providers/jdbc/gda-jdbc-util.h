/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dj@starfire-programming.net>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2005 - 2008 Vivien Malerba <malerba@gnome-db.org>
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

