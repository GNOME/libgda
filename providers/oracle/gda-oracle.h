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
#endif

#include <glib/gmacros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-value.h>
#include "gda-oracle-provider.h"
#include "gda-oracle-recordset.h"
#include <oci.h>

#define GDA_ORACLE_PROVIDER_ID          "GDA Oracle provider"
#define ORA_NAME_BUFFER_SIZE		30

#define gda_oracle_check_result(result, cnc, priv_data, type, msg) \
    (((result) == OCI_SUCCESS || (result) == OCI_SUCCESS_WITH_INFO) \
    ? TRUE : gda_oracle_handle_error(result, cnc, priv_data,       \
				     type, msg, __FILE__, __LINE__))
    
G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaConnectionEvent *gda_oracle_make_error (dvoid *hndlp, ub4 type,
				 const gchar *file, gint line);

void gda_oracle_set_value (GdaValue *value, 
				GdaOracleValue *thevalue,
				GdaConnection *cnc);

gchar *gda_oracle_value_to_sql_string (GdaValue *value);

GdaValueType  oracle_sqltype_to_gda_type (const ub2 sqltype);

gchar *oracle_sqltype_to_string (const ub2 sqltype);

GdaOracleValue *gda_value_to_oracle_value (const GdaValue *value);

gboolean gda_oracle_handle_error (gint result, 
				  GdaConnection *cnc, 
				  GdaOracleConnectionData *priv_data,
				  ub4 type, 
				  const gchar *msg,
				  const gchar *file,
				  gint line);


G_END_DECLS

#endif
