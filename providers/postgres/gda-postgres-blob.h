/* GDA DB Postgres Blob
 * Copyright (C) 2005 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#if !defined(__gda_postgres_blob_h__)
#  define __gda_postgres_blob_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>
#include <libpq-fe.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_BLOB            (gda_postgres_blob_get_type())
#define GDA_POSTGRES_BLOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_BLOB, GdaPostgresBlob))
#define GDA_POSTGRES_BLOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_BLOB, GdaPostgresBlobClass))
#define GDA_IS_POSTGRES_BLOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_BLOB))
#define GDA_IS_POSTGRES_BLOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_POSTGRES_BLOB))

typedef struct _GdaPostgresBlob        GdaPostgresBlob;
typedef struct _GdaPostgresBlobClass   GdaPostgresBlobClass;
typedef struct _GdaPostgresBlobPrivate GdaPostgresBlobPrivate;

struct _GdaPostgresBlob {
	GdaBlob                 parent;
	GdaPostgresBlobPrivate *priv;
};

struct _GdaPostgresBlobClass {
	GdaBlobClass            parent_class;
};

GType         gda_postgres_blob_get_type (void);
GdaBlob      *gda_postgres_blob_new      (GdaConnection *cnc);
void          gda_postgres_blob_set_id   (GdaPostgresBlob *blob, gint value);

G_END_DECLS

#endif

