/* GDA Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_postgres_h__)
#  define __gda_postgres_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-server-provider.h>
#include "gda-postgres-provider.h"
#include "gda-postgres-recordset.h"

#define GDA_POSTGRES_PROVIDER_ID          "GDA PostgreSQL provider"

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaError *gda_postgres_make_error (PGconn *pconn, PGresult *pg_res);
void gda_postgres_set_value (GdaValue *value, 
			     GdaValueType type, 
			     const gchar *thevalue,
			     gboolean isNull);

GdaValueType gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, 
					   gint ntypes,
					   Oid postgres_type);

GdaValueType gda_postgres_type_name_to_gda (GHashTable *h_table,
					    const gchar *name);

const gchar *gda_data_type_to_string (GdaValueType type);

G_END_DECLS

#endif

