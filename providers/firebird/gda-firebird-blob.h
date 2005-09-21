/* GDA Firebird Blob
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifndef _GDA_FIREBIRD_BLOB_H_
#define _GDA_FIREBIRD_BLOB_H

#include <libgda/gda-blob.h>
#include <libgda/gda-connection.h>
#include <ibase.h>

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_BLOB            (gda_firebird_blob_get_type())
#define GDA_FIREBIRD_BLOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_BLOB, GdaFirebirdBlob))
#define GDA_FIREBIRD_BLOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_BLOB, GdaFirebirdBlobClass))
#define GDA_IS_FIREBIRD_BLOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_BLOB))
#define GDA_IS_FIREBIRD_BLOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_BLOB))

typedef struct _GdaFirebirdBlob        GdaFirebirdBlob;
typedef struct _GdaFirebirdBlobClass   GdaFirebirdBlobClass;
typedef struct _GdaFirebirdBlobPrivate GdaFirebirdBlobPrivate;

struct _GdaFirebirdBlob {
	GdaBlob                 parent;
	GdaFirebirdBlobPrivate *priv;
};

struct _GdaFirebirdBlobClass {
	GdaBlobClass            parent_class;
};

GType         gda_firebird_blob_get_type (void);
GdaBlob      *gda_firebird_blob_new      (GdaConnection *cnc);
void          gda_firebird_blob_set_id   (GdaFirebirdBlob *blob, const ISC_QUAD *blob_id);

G_END_DECLS

#endif /* _GDA_FIREBIRD_BLOB_H */

