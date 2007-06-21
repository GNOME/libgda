/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Laurent Sansonetti <lrz@gnome.org>
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

#if !defined(__gda_bdb_recordset_h__)
#  define __gda_bdb_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>
#include <db.h>

#define GDA_TYPE_BDB_RECORDSET            (gda_bdb_recordset_get_type())
#define GDA_BDB_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_BDB_RECORDSET, GdaBdbRecordset))
#define GDA_BDB_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_BDB_RECORDSET, GdaBdbRecordsetClass))
#define GDA_IS_BDB_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_BDB_RECORDSET))
#define GDA_IS_BDB_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_BDB_RECORDSET))

typedef struct _GdaBdbRecordset        GdaBdbRecordset;
typedef struct _GdaBdbRecordsetClass   GdaBdbRecordsetClass;
typedef struct _GdaBdbRecordsetPrivate GdaBdbRecordsetPrivate;

struct _GdaBdbRecordset {
	GdaDataModelHash model;
	GdaBdbRecordsetPrivate *priv;
};

struct _GdaBdbRecordsetClass {
	GdaDataModelHashClass parent_class;
};

G_BEGIN_DECLS

GType         gda_bdb_recordset_get_type (void) G_GNUC_CONST;
GdaDataModel *gda_bdb_recordset_new (GdaConnection *cnc, DB *dbp);

G_END_DECLS

#endif /* __gda_bdb_recordset_h__ */
