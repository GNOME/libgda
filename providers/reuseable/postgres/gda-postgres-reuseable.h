/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_REUSEABLE_H__
#define __GDA_POSTGRES_REUSEABLE_H__

#include "../gda-provider-reuseable.h"
#include "gda-postgres-meta.h"

G_BEGIN_DECLS

/*
 * Specific structure for PostgreSQL
 *
 * Note: each GdaPostgresTypeOid is managed by the types_oid_hash hash table, and not by the types_dbtype_hash
 * even though it's also referenced there.
 */
typedef struct {
	GdaProviderReuseable parent;
	
	gfloat               version_float;
	GHashTable          *types_oid_hash; /* key = an unsigned int * (for OID) , value = a #GdaPostgresTypeOid */
	GHashTable          *types_dbtype_hash; /* key = a gchar *, value = a #GdaPostgresTypeOid */

	/* Internal data types hidden from the exterior */
        gchar               *avoid_types; /* may be %NULL */
        gchar               *avoid_types_oids;
        gchar               *any_type_oid; /* oid for the 'any' data type, used to fetch aggregates and functions, may be %NULL */
} GdaPostgresReuseable;

/*
 * Reuseable implementation
 */
GdaProviderReuseable *_gda_postgres_reuseable_new_data (void);
void _gda_postgres_reuseable_reset_data (GdaProviderReuseable *rdata);
GType _gda_postgres_reuseable_get_g_type (GdaConnection *cnc, GdaProviderReuseable *rdata, const gchar *db_type);
GdaSqlReservedKeywordsFunc _gda_postgres_reuseable_get_reserved_keywords_func (GdaProviderReuseable *rdata);
GdaSqlParser *_gda_postgres_reuseable_create_parser (GdaProviderReuseable *rdata);

/*
 * entry point
 */
GdaProviderReuseableOperations *_gda_postgres_reuseable_get_ops (void);

/*
 * Specific API
 */
gboolean            _gda_postgres_compute_version (GdaConnection *cnc, GdaPostgresReuseable *rdata, GError **error);
void                _gda_postgres_compute_types (GdaConnection *cnc, GdaPostgresReuseable *rdata);

GType               _gda_postgres_type_oid_to_gda (GdaConnection *cnc, GdaPostgresReuseable *rdata,
						   unsigned int postgres_oid);

#ifdef GDA_DEBUG
void                _gda_postgres_test_keywords (void);
#endif

G_END_DECLS

#endif

