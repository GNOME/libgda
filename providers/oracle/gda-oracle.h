/* GDA Oracle provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
 *
 * Borrowed from gda-mysql.h, written by:
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

#if !defined(__gda_oracle_h__)
#  define __gda_oracle_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-value.h>
#include "gda-oracle-provider.h"
#include <oci.h>

#define GDA_ORACLE_PROVIDER_ID          "GDA Oracle provider"
#define ORA_NAME_BUFFER_SIZE		30

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaError *gda_oracle_make_error (GdaOracleConnectionData *handle);
void gda_oracle_set_value (GdaValue *value, 
				GdaValueType type, 
				gconstpointer thevalue, 
				gboolean isNull,
				gint defined_size);
gchar *gda_oracle_value_to_sql_string (GdaValue *value);

G_END_DECLS

#endif
