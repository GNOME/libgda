/* GDA MySQL provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_mysql_h__)
#  define __gda_mysql_h__

#include <glib/gmacros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-value.h>
#include "gda-mysql-provider.h"
#include <mysql.h>
#include <mysql_com.h>

#define GDA_MYSQL_PROVIDER_ID          "GDA MySQL provider"

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaConnectionEvent     *gda_mysql_make_error (MYSQL *handle);
GdaValueType  gda_mysql_type_to_gda (enum enum_field_types mysql_type, gboolean is_unsigned);
gchar        *gda_mysql_type_from_gda (const GdaValueType type);

G_END_DECLS

#endif
