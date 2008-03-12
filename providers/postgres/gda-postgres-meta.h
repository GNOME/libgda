/* GDA postgres provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_META_H__
#define __GDA_POSTGRES_META_H__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

void     _gda_postgres_provider_meta_init    (GdaServerProvider *provider);
gboolean _gda_postgres_meta_info             (GdaServerProvider *prov, GdaConnection *cnc, 
					      GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_postgres_meta_btypes           (GdaServerProvider *prov, GdaConnection *cnc, 
					      GdaMetaStore *store, GdaMetaContext *context, GError **error);
gboolean _gda_postgres_meta_schemata         (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, 
					      const GValue *catalog_name, const GValue *schema_name_n);
gboolean _gda_postgres_meta_tables_views     (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **,
					      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name_n);
gboolean _gda_postgres_meta_columns          (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **,
					      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name);
gboolean _gda_postgres_meta_constraints_tab  (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **,
					      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name,
					      const GValue *constraint_name_n);
gboolean _gda_postgres_meta_constraints_ref  (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **,
					      const GValue *table_catalog, const GValue *table_schema, const GValue *table_name, 
					      const GValue *constraint_name);


G_END_DECLS

#endif

