/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ORACLE_H__
#define __GDA_ORACLE_H__

/*
 * Provider name
 */
#define ORACLE_PROVIDER_NAME "Oracle"

/* headers necessary for the C or C++ API */
#include <libgda/libgda.h>
#include <libgda/gda-connection-private.h>
#include <oci.h>

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	OCIEnv *henv;
        OCIError *herr;
        OCIServer *hserver;
        OCISvcCtx *hservice;
        OCISession *hsession;
        gchar *schema; /* the same as the username which opened the connection */

	gchar *version;
	guint8 major_version;
	guint8 minor_version;

	gboolean autocommit;
} OracleConnectionData;

#endif
