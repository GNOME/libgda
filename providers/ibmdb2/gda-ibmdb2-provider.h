/* GNOME DB IBMDB2 Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS: 
 *         Holger Thon <holger.thon@gnome-db.org>
 *	   Sergey N. Belinsky <sergey_be@mail.ru>
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

#if !defined(__gda_ibmdb2_provider_h__)
#  define __gda_ibmdb2_provider_h__

#include <libgda/gda-server-provider.h>
#include <sqlcli1.h>


#define GDA_TYPE_IBMDB2_PROVIDER            (gda_ibmdb2_provider_get_type())
#define GDA_IBMDB2_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_IBMDB2_PROVIDER, GdaIBMDB2Provider))
#define GDA_IBMDB2_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_IBMDB2_PROVIDER, GdaIBMDB2ProviderClass))
#define GDA_IS_IBMDB2_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_IBMDB2_PROVIDER))
#define GDA_IS_IBMDB2_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_IBMDB2_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_IBMDB2_HANDLE "GDA_IBMDB2_IBMDB2Handle"


typedef struct _GdaIBMDB2Provider      	GdaIBMDB2Provider;
typedef struct _GdaIBMDB2ProviderClass 	GdaIBMDB2ProviderClass;
typedef struct _GdaIBMDB2ConnectionData GdaIBMDB2ConnectionData;

struct _GdaIBMDB2Provider {
	GdaServerProvider provider;
};

struct _GdaIBMDB2ProviderClass {
	GdaServerProviderClass parent_class;
};

struct _GdaIBMDB2ConnectionData {
	SQLHANDLE henv;
	SQLHANDLE hdbc;
	SQLHANDLE hstmt; /* Only for schema */
	gchar    *database;
	gboolean GetInfo_supported;
	gchar    *server_version;
	SQLRETURN rc;
	SQLCHAR sql_state[5];
	SQLINTEGER native_error;
        SQLCHAR error_msg[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT size_error_msg;
};

G_BEGIN_DECLS

GType              gda_ibmdb2_provider_get_type (void);
GdaServerProvider *gda_ibmdb2_provider_new (void);

G_END_DECLS

#endif

