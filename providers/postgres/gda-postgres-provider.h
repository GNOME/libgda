/* GNOME DB Postgres Provider
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

#if !defined(__gda_postgres_provider_h__)
#  define __gda_postgres_provider_h__

#include <libgda/gda-server-provider.h>
#include <libpq-fe.h>

#define GDA_TYPE_POSTGRES_PROVIDER            (gda_postgres_provider_get_type())
#define GDA_POSTGRES_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_PROVIDER, GdaPostgresProvider))
#define GDA_POSTGRES_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_PROVIDER, GdaPostgresProviderClass))
#define GDA_IS_POSTGRES_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_PROVIDER))
#define GDA_IS_POSTGRES_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_POSTGRES_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_POSTGRES_HANDLE "GDA_Postgres_PostgresHandle"


typedef struct _GdaPostgresProvider      GdaPostgresProvider;
typedef struct _GdaPostgresProviderClass GdaPostgresProviderClass;

struct _GdaPostgresProvider {
	GdaServerProvider provider;
};

struct _GdaPostgresProviderClass {
	GdaServerProviderClass parent_class;
};

// Connection data.
typedef struct {
	gchar *name;
	Oid oid;
	GdaValueType type;
} GdaPostgresTypeOid;

typedef struct {
	PGconn *pconn;
	gint ntypes;
	GdaPostgresTypeOid *type_data;
	GHashTable *h_table;
	gchar *version;
} GdaPostgresConnectionData;


G_BEGIN_DECLS

GType              gda_postgres_provider_get_type (void);
GdaServerProvider *gda_postgres_provider_new (void);

G_END_DECLS

#endif

