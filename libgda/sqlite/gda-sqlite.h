/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2005 Denis Fortin <denis.fortin@free.fr>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
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

#ifndef __GDA_SQLITE_H__
#define __GDA_SQLITE_H__

#include <glib.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-connection-private.h>
#include <libgda/sqlite/gda-sqlite-provider.h>

#ifdef WITH_BDBSQLITE
  #include <dbsql.h>
  #include "gda-symbols-util.h"
  #define SQLITE3_CALL(x) (s3r->x)
#else
  #include <sqlite3.h>
  #include "gda-symbols-util.h"
  #define SQLITE3_CALL(p,x) (((Sqlite3ApiRoutines*) gda_sqlite_provider_get_api(p))->x)
  #if (SQLITE_VERSION_NUMBER < 3005000)
    typedef sqlite_int64 sqlite3_int64;
  #endif
#endif

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	sqlite3      *connection;
	GWeakRef     provider;
	gchar        *file;
	GHashTable   *types_hash; /* key = type name, value = pointer to a GType */
	GType        *types_array;/* holds GType values, pointed by @types_hash */
} SqliteConnectionData;

extern GHashTable *error_blobs_hash;

#endif
