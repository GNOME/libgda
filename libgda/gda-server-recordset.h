/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_server_recordset_h__)
#  define __gda_server_recordset_h__

#include <libgda/gda-row.h>
#include <libgda/gda-server-connection.h>
#include <bonobo/bonobo-xobject.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_RECORDSET            (gda_server_recordset_get_type())
#define GDA_SERVER_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_RECORDSET, GdaServerRecordset))
#define GDA_SERVER_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_RECORDSET, GdaServerRecordsetClass))
#define GDA_IS_SERVER_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_RECORDSET))
#define GDA_IS_SERVER_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_RECORDSET))

typedef struct _GdaServerRecordset        GdaServerRecordset;
typedef struct _GdaServerRecordsetClass   GdaServerRecordsetClass;
typedef struct _GdaServerRecordsetPrivate GdaServerRecordsetPrivate;

struct _GdaServerRecordset {
	BonoboXObject object;
	GdaServerRecordsetPrivate *priv;
};

struct _GdaServerRecordsetClass {
	BonoboXObjectClass parent_class;
	POA_GNOME_Database_Recordset__epv epv;
};

typedef GdaRow * (* GdaServerRecordsetFetchFunc) (GdaServerRecordset *recset, gulong rownum);
typedef GdaRowAttributes * (* GdaServerRecordsetDescribeFunc) (GdaServerRecordset *recset);

GType                gda_server_recordset_get_type (void);
GdaServerRecordset  *gda_server_recordset_new (GdaServerConnection *cnc,
					       GdaServerRecordsetFetchFunc fetch_func,
					       GdaServerRecordsetDescribeFunc desc_func);
GdaServerConnection *gda_server_recordset_get_connection (GdaServerRecordset *recset);

G_END_DECLS

#endif
