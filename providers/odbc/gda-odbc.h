/* GNOME DB ODBC Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Michael Lausch <michael@lausch.at>
 *         Nick Gorham <nick@lurcher.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_odbc_h__)
#  define __gda_odbc_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <glib-object.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-server-provider.h>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include "gda-odbc-provider.h"

#define GDA_ODBC_PROVIDER_ID "GDA ODBC Provider"

G_BEGIN_DECLS

void gda_odbc_emit_error ( GdaConnection *cnc, SQLHANDLE env, SQLHANDLE con, SQLHANDLE stmt );
GdaValueType odbc_to_gda_type ( int odbc_type );

G_END_DECLS

#endif
