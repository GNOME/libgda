/* GDA SQLite provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Carlos Perelló Marín <carlos@gnome-db.org>
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

#if !defined(__gda_sqlite_h__)
#  define __gda_sqlite_h__

#include <glib/gmacros.h>
#include <bonobo/bonobo-i18n.h>
#include <config.h>
#include <libgda/gda-server.h>
#include <gda-sqlite-provider.h>
#include <sqlite.h>

#define GDA_SQLITE_COMPONENT_FACTORY_ID "OAFIID:GNOME_Database_SQLite_ComponentFactory"
#define GDA_SQLITE_PROVIDER_ID          "OAFIID:GNOME_Database_SQLite_Provider"

typedef struct {
	gint ncols;
	gint nrows;
	gchar **data;
} SQLITE_Recordset;

#endif
