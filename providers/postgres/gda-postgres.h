/*
 * Copyright (C) 2001 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
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

#ifndef __GDA_POSTGRES_H__
#define __GDA_POSTGRES_H__

/*
 * Provider name
 */
#define POSTGRES_PROVIDER_NAME "PostgreSQL"

#include <libgda/libgda.h>
#include <libgda/gda-connection-private.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <gda-postgres-reuseable.h>

/*
 * Postgres type identification
 */
typedef struct {
        gchar              *name;
        Oid                 oid;
        GType               type;
        gchar              *comments;
        gchar              *owner;
} GdaPostgresTypeOid;

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	GdaPostgresReuseable *reuseable;
	GdaConnection        *cnc;
        PGconn               *pconn;
	gboolean              pconn_is_busy;

	GDateDMY              date_first;
	GDateDMY              date_second;
	GDateDMY              date_third;
	gchar                 date_sep;
} PostgresConnectionData;

#endif
