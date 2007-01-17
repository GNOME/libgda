/* GNOME DB Oracle Provider
 * Copyright (C) 2000 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Tim Coleman <tim@timcoleman.com>
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

#ifndef __GDA_ORACLE_PROVIDER_H__
#define __GDA_ORACLE_PROVIDER_H__

#include <libgda/gda-server-provider.h>
#include <oci.h>

#define GDA_TYPE_ORACLE_PROVIDER            (gda_oracle_provider_get_type())
#define GDA_ORACLE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_PROVIDER, GdaOracleProvider))
#define GDA_ORACLE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_PROVIDER, GdaOracleProviderClass))
#define GDA_IS_ORACLE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_PROVIDER))
#define GDA_IS_ORACLE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_PROVIDER))

typedef struct _GdaOracleProvider      GdaOracleProvider;
typedef struct _GdaOracleProviderClass GdaOracleProviderClass;

struct _GdaOracleProvider {
	GdaServerProvider provider;
};

struct _GdaOracleProviderClass {
	GdaServerProviderClass parent_class;
};

/*
 * Connection data
 */
typedef struct {
	OCIEnv *henv;
	OCIError *herr;
	OCIServer *hserver;
	OCISvcCtx *hservice;
	OCISession *hsession;
        gchar *schema; /* the same as the username which opened the connection */
} GdaOracleConnectionData;


G_BEGIN_DECLS

GType                gda_oracle_provider_get_type (void);
GdaServerProvider   *gda_oracle_provider_new (void);


G_END_DECLS

#endif

/*
  Local Variables:
  mode:C
  c-basic-offset: 8
  End:
*/
