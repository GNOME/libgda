/* GDA DB Postgres provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#if !defined(__gda_postgres_recordset_h__)
#  define __gda_postgres_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>
#include <libpq-fe.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_RECORDSET            (gda_postgres_recordset_get_type())
#define GDA_POSTGRES_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_RECORDSET, GdaPostgresRecordset))
#define GDA_POSTGRES_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_RECORDSET, GdaPostgresRecordsetClass))
#define GDA_IS_POSTGRES_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_RECORDSET))
#define GDA_IS_POSTGRES_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_POSTGRES_RECORDSET))

typedef struct _GdaPostgresRecordset        GdaPostgresRecordset;
typedef struct _GdaPostgresRecordsetClass   GdaPostgresRecordsetClass;
typedef struct _GdaPostgresRecordsetPrivate GdaPostgresRecordsetPrivate;

struct _GdaPostgresRecordset {
	GdaDataModelHash model;
	GdaPostgresRecordsetPrivate *priv;
};

struct _GdaPostgresRecordsetClass {
	GdaDataModelHashClass parent_class;
};

GType         gda_postgres_recordset_get_type (void);
GdaDataModel *gda_postgres_recordset_new (GdaConnection *cnc, PGresult *pgres);

PGresult     *gda_postgres_recordset_get_pgresult (GdaPostgresRecordset *recset);

G_END_DECLS

#endif

