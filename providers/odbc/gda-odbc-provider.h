/* GNOME DB ODBC Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Michael Lausch <michael@lausch.at>
 *         Nick Gorham <nick@lurcher.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_odbc_provider_h__)
#  define __gda_odbc_provider_h__

#include <libgda/gda-server-provider.h>
#include <sql.h>
#include <sqlext.h>

G_BEGIN_DECLS

#define GDA_TYPE_ODBC_PROVIDER            (gda_odbc_provider_get_type())
#define GDA_ODBC_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ODBC_PROVIDER, GdaOdbcProvider))
#define GDA_ODBC_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ODBC_PROVIDER, GdaOdbcProviderClass))
#define GDA_IS_ODBC_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ODBC_PROVIDER))
#define GDA_IS_ODBC_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ODBC_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_ODBC_HANDLE "GDA_ODBC_ODBCHandle"

typedef struct _GdaOdbcProvider      GdaOdbcProvider;
typedef struct _GdaOdbcProviderClass GdaOdbcProviderClass;

struct _GdaOdbcProvider {
	GdaServerProvider provider;
};

struct _GdaOdbcProviderClass {
	GdaServerProviderClass parent_class;
};

/* 
 * Connection data
 */

typedef struct {
	SQLHANDLE henv;
        SQLHANDLE hdbc;
	SQLHANDLE hstmt;		/* used for metadata calls only */
	SQLCHAR version[ 128 ];
	SQLCHAR db[ 256 ];
} GdaOdbcConnectionData;

GType              gda_odbc_provider_get_type (void);
GdaServerProvider *gda_odbc_provider_new (void);

G_END_DECLS

#endif
