/* GDA Firebird Blob
 * Copyright (C) 1998-2004 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
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

/* Firebird Blob public interface */

GdaBlob		*gda_firebird_blob_new (GdaConnection *cnc);
void		 gda_firebird_blob_set_id (GdaBlob *self, const ISC_QUAD *blob_id);

#endif /* _GDA_FIREBIRD_BLOB_H */

