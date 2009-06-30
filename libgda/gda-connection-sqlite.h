/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
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
