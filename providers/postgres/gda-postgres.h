/* GDA postgres provider
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#ifndef __GDA_POSTGRES_H__
#define __GDA_POSTGRES_H__

/*
 * Provider name
 */
#define POSTGRES_PROVIDER_NAME "PostgreSQL"

#include <libgda/libgda.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>

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
	GdaConnection      *cnc;
        PGconn             *pconn;
	gboolean            pconn_is_busy;
        gint                ntypes;
        GdaPostgresTypeOid *type_data;
        GHashTable         *h_table;

        /* Version of the backend to which we are connected */
        gchar              *version;
        gfloat              version_float;
        gchar              *short_version;

        /* Internal data types not returned */
        gchar              *avoid_types;
        gchar              *avoid_types_oids;
        gchar              *any_type_oid; /* oid for the 'any' data type, used to fetch aggregates and functions */
} PostgresConnectionData;

#endif
