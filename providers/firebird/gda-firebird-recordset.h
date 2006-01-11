/* GDA FireBird Provider
 * Copyright (C) 1998 - 2004 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_firebird_recordset_h__)
#  define __gda_firebird_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-connection.h>
#include <ibase.h>

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_RECORDSET            (gda_firebird_recordset_get_type())
#define GDA_FIREBIRD_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordset))
#define GDA_FIREBIRD_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordsetClass))
#define GDA_IS_FIREBIRD_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_RECORDSET))
#define GDA_IS_FIREBIRD_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_RECORDSET))

typedef struct _GdaFirebirdRecordset        GdaFirebirdRecordset;
typedef struct _GdaFirebirdRecordsetClass   GdaFirebirdRecordsetClass;
typedef struct _GdaFirebirdRecordsetPrivate GdaFirebirdRecordsetPrivate;

struct _GdaFirebirdRecordset {
	GdaDataModelRow model;
	GdaFirebirdRecordsetPrivate *priv;
};

struct _GdaFirebirdRecordsetClass {
	GdaDataModelRowClass parent_class;
};

typedef struct {
	gchar *dbname, *server_version;
	isc_db_handle handle;
	ISC_STATUS status[20];
	gchar dpb_buffer[128];
	gshort dpb_length;
} GdaFirebirdConnection;


GType			gda_firebird_recordset_get_type (void);
GdaFirebirdRecordset 	*gda_firebird_recordset_new (GdaConnection *cnc,
				 		     isc_tr_handle *ftr,
						     const gchar *sql);

G_END_DECLS

#endif
