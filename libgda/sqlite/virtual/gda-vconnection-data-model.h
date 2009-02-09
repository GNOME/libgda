/* GDA
 * Copyright (C) 2007 - 2009 The GNOME Foundation.
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

#ifndef __GDA_VCONNECTION_DATA_MODEL_H__
#define __GDA_VCONNECTION_DATA_MODEL_H__

#include <virtual/gda-virtual-connection.h>

#define GDA_TYPE_VCONNECTION_DATA_MODEL            (gda_vconnection_data_model_get_type())
#define GDA_VCONNECTION_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VCONNECTION_DATA_MODEL, GdaVconnectionDataModel))
#define GDA_VCONNECTION_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VCONNECTION_DATA_MODEL, GdaVconnectionDataModelClass))
#define GDA_IS_VCONNECTION_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VCONNECTION_DATA_MODEL))
#define GDA_IS_VCONNECTION_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VCONNECTION_DATA_MODEL))

G_BEGIN_DECLS

typedef struct _GdaVconnectionDataModel      GdaVconnectionDataModel;
typedef struct _GdaVconnectionDataModelClass GdaVconnectionDataModelClass;
typedef struct _GdaVconnectionDataModelPrivate GdaVconnectionDataModelPrivate;
typedef struct _GdaVconnectionDataModelSpec  GdaVconnectionDataModelSpec;

typedef GList        *(*GdaVconnectionDataModelCreateColumnsFunc) (GdaVconnectionDataModelSpec *, GError **);
typedef GdaDataModel *(*GdaVconnectionDataModelCreateModelFunc)   (GdaVconnectionDataModelSpec *);
typedef void (*GdaVconnectionDataModelFunc) (GdaDataModel *, const gchar *, gpointer );

struct _GdaVconnectionDataModelSpec {
	GdaDataModel                             *data_model;
	GdaVconnectionDataModelCreateColumnsFunc  create_columns_func;
	GdaVconnectionDataModelCreateModelFunc    create_model_func;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

#define GDA_VCONNECTION_DATA_MODEL_SPEC(x) ((GdaVconnectionDataModelSpec*)(x))

struct _GdaVconnectionDataModel {
	GdaVirtualConnection            connection;
	GdaVconnectionDataModelPrivate *priv;
};

struct _GdaVconnectionDataModelClass {
	GdaVirtualConnectionClass       parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

GType               gda_vconnection_data_model_get_type  (void) G_GNUC_CONST;

gboolean            gda_vconnection_data_model_add       (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelSpec *spec, 
							  GDestroyNotify spec_free_func,
							  const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_add_model (GdaVconnectionDataModel *cnc, 
							  GdaDataModel *model, const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_remove    (GdaVconnectionDataModel *cnc, const gchar *table_name, GError **error);

const gchar        *gda_vconnection_data_model_get_table_name (GdaVconnectionDataModel *cnc, GdaDataModel *model);
GdaDataModel       *gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name);

void                gda_vconnection_data_model_foreach   (GdaVconnectionDataModel *cnc, 
							  GdaVconnectionDataModelFunc func, gpointer data);

G_END_DECLS

#endif
