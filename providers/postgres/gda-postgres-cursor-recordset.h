/* GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifndef __GDA_POSTGRES_CURSOR_RECORDSET_H__
#define __GDA_POSTGRES_CURSOR_RECORDSET_H__

#include <libgda/gda-data-model.h>
#include <libpq-fe.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_CURSOR_RECORDSET            (gda_postgres_cursor_recordset_get_type())
#define GDA_POSTGRES_CURSOR_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_CURSOR_RECORDSET, GdaPostgresCursorRecordset))
#define GDA_POSTGRES_CURSOR_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_CURSOR_RECORDSET, GdaPostgresCursorRecordsetClass))
#define GDA_IS_POSTGRES_CURSOR_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_POSTGRES_CURSOR_RECORDSET))
#define GDA_IS_POSTGRES_CURSOR_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_POSTGRES_CURSOR_RECORDSET))

typedef struct _GdaPostgresCursorRecordset        GdaPostgresCursorRecordset;
typedef struct _GdaPostgresCursorRecordsetClass   GdaPostgresCursorRecordsetClass;
typedef struct _GdaPostgresCursorRecordsetPrivate GdaPostgresCursorRecordsetPrivate;

struct _GdaPostgresCursorRecordset {
	GdaObject                  object;
	GdaPostgresCursorRecordsetPrivate *priv;
};

struct _GdaPostgresCursorRecordsetClass {
	GdaObjectClass             parent_class;
};

GType         gda_postgres_cursor_recordset_get_type     (void) G_GNUC_CONST;
GdaDataModel *gda_postgres_cursor_recordset_new          (GdaConnection *cnc, const gchar *cursor_name, gint chunk_size);


G_END_DECLS

#endif
