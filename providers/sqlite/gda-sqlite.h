/* GDA SQLite provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Carlos Perelló Marín <carlos@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDA_SQLITE_H__
#define __GDA_SQLITE_H__

#include <glib/gmacros.h>
#include <glib/gtypes.h>
#include <libgda/gda-value.h>
#include <libgda/gda-connection.h>

#ifdef HAVE_SQLITE
#include <sqlite3.h>
#else
#include "sqlite-src/sqlite3.h"
#endif

#define GDA_SQLITE_PROVIDER_ID    "GDA SQLite provider"
#define OBJECT_DATA_SQLITE_HANDLE "GDA_Sqlite_SqliteHandle"

typedef struct {
	sqlite3_stmt *stmt;
	gint          ncols;
	gint          nrows;
	GType *types; /* array of ncols types */
	int          *sqlite_types; /* array of ncols types */
	int          *cols_size; /* array of ncols types */
} SQLITEresult;

typedef struct {
	sqlite3    *connection;
	gchar      *file;
	GHashTable *types; /* key = type name, value = GType */
} SQLITEcnc;


void gda_sqlite_update_types_hash (SQLITEcnc *scnc);

void gda_sqlite_free_result (SQLITEresult *sres);
void gda_sqlite_free_cnc    (SQLITEcnc *scnc);

#endif
