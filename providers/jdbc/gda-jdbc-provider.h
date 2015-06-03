/*
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2004 Jeronimo Albi <jeronimoalbi@yahoo.com.ar>
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_JDBC_PROVIDER_H__
#define __GDA_JDBC_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_JDBC_PROVIDER            (gda_jdbc_provider_get_type())
#define GDA_JDBC_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_JDBC_PROVIDER, GdaJdbcProvider))
#define GDA_JDBC_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_JDBC_PROVIDER, GdaJdbcProviderClass))
#define GDA_IS_JDBC_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_JDBC_PROVIDER))
#define GDA_IS_JDBC_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_JDBC_PROVIDER))

typedef struct _GdaJdbcProvider      GdaJdbcProvider;
typedef struct _GdaJdbcProviderClass GdaJdbcProviderClass;

struct _GdaJdbcProvider {
	GdaServerProvider      provider;
	gchar                 *jdbc_driver;
	GValue                *jprov_obj; /* JAVA GdaJProvider object */
};

struct _GdaJdbcProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType              gda_jdbc_provider_get_type (void) G_GNUC_CONST;
GdaServerProvider *gda_jdbc_provider_new (const gchar *jdbc_driver, GError **error);

G_END_DECLS

#endif
