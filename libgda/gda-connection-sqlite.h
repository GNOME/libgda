/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_CONNECTION_SQLITE_H_
#define __GDA_CONNECTION_SQLITE_H_

#include <libgda/gda-decl.h>

G_BEGIN_DECLS

/*
 * Opens a connection to an SQLite database. This function is intended to be used
 * internally when objects require an SQLite connection, for example for the GdaMetaStore
 * object
 */
GdaConnection *_gda_open_internal_sqlite_connection (const gchar *cnc_string);

G_END_DECLS

#endif
