/* GDA server library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#if !defined(__gda_server_recordset_model_h__)
#  define __gda_server_recordset_model_h__

#include <libgda/gda-server-recordset.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_RECORDSET_MODEL            (gda_server_recordset_model_get_type())
#define GDA_SERVER_RECORDSET_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_RECORDSET_MODEL, GdaServerRecordsetModel))
#define GDA_SERVER_RECORDSET_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_RECORDSET_MODEL, GdaServerRecordsetModelClass))
#define GDA_IS_SERVER_RECORDSET_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_RECORDSET_MODEL))
#define GDA_IS_SERVER_RECORDSET_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_RECORDSET_MODEL))

typedef struct _GdaServerRecordsetModel        GdaServerRecordsetModel;
typedef struct _GdaServerRecordsetModelClass   GdaServerRecordsetModelClass;
typedef struct _GdaServerRecordsetModelPrivate GdaServerRecordsetModelPrivate;

struct _GdaServerRecordsetModel {
	GdaServerRecordset recset;
	GdaServerRecordsetModelPrivate *priv;
};

struct _GdaServerRecordsetModelClass {
	GdaServerRecordsetClass parent_class;
};

GType               gda_server_recordset_model_get_type (void);
GdaServerRecordset *gda_server_recordset_model_new (GdaServerConnection *cnc, gint n_cols);
void                gda_server_recordset_model_set_field_defined_size (GdaServerRecordsetModel *recset,
								       gint col,
								       glong size);

void gda_server_recordset_model_set_field_name (GdaServerRecordsetModel *recset,
						gint col,
						const gchar *name);
void gda_server_recordset_model_set_field_scale (GdaServerRecordsetModel *recset,
						 gint col,
						 glong scale);
void gda_server_recordset_model_set_field_gdatype (GdaServerRecordsetModel *recset,
						   gint col,
						   GdaType type);
void gda_server_recordset_model_append_row (GdaServerRecordsetModel *recset,
					    const GList *values);

G_END_DECLS

#endif
