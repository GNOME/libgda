/* GDA Postgres Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
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

#ifndef __GDA_POSTGRES_H__
#define __GDA_POSTGRES_H__

#include <glib/gmacros.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider.h>
#include "gda-postgres-provider.h"
#include "gda-postgres-recordset.h"
#include "gda-postgres-cursor-recordset.h"

#define GDA_POSTGRES_PROVIDER_ID "GDA PostgreSQL provider"

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaConnectionEvent *gda_postgres_make_error (GdaConnection *cnc, PGconn *pconn, PGresult *pg_res);
void gda_postgres_set_value (GdaConnection *cnc,
			     GValue *value, 
			     GType type, 
			     const gchar *thevalue,
			     gboolean isNull,
			     gint length);


GType gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, 
					   gint ntypes,
					   Oid postgres_type);

GType gda_postgres_type_name_to_gda (GHashTable *h_table,
					    const gchar *name);

const gchar *gda_data_type_to_string (GType type);
gchar *gda_postgres_value_to_sql_string (GValue *value);
gboolean gda_postgres_check_transaction_started (GdaConnection *cnc);

/*
 * For recordset implementations
 */
void gda_postgres_recordset_describe_column (GdaDataModel *model, GdaConnection *cnc, PGresult *pg_res, 
					     GdaPostgresTypeOid *type_data, gint ntypes, const gchar *table_name,
					     gint col);
gchar *gda_postgres_guess_table_name (GdaConnection *cnc, PGresult *pg_res);
GType *gda_postgres_get_column_types (PGresult *pg_res, GdaPostgresTypeOid *type_data, gint ntypes);
G_END_DECLS

#endif

