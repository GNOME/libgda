/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org>
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

#if !defined(__gda_bdb_provider_h__)
#  define __gda_bdb_provider_h__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_BDB_PROVIDER            (gda_bdb_provider_get_type())
#define GDA_BDB_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_BDB_PROVIDER, GdaBdbProvider))
#define GDA_BDB_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_BDB_PROVIDER, GdaBdbProviderClass))
#define GDA_IS_BDB_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_BDB_PROVIDER))
#define GDA_IS_BDB_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_BDB_PROVIDER))

#define PARENT_TYPE 			GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_BDB_HANDLE 		"GDA_BDB_BDBHandle"

typedef struct _GdaBdbProvider      GdaBdbProvider;
typedef struct _GdaBdbProviderClass GdaBdbProviderClass;

struct _GdaBdbProvider {
	GdaServerProvider provider;
};

struct _GdaBdbProviderClass {
	GdaServerProviderClass parent_class;
};

typedef struct {
	DB *dbp;
	gchar *dbname;
	gchar *dbver;
} GdaBdbConnectionData;

G_BEGIN_DECLS

GType              gda_bdb_provider_get_type (void);
GdaServerProvider *gda_bdb_provider_new (void);

G_END_DECLS

#endif /* __gda_bdb_provider_h__ */
