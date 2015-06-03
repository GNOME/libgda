/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __CUSTOM_DATA_MODEL_H__
#define __CUSTOM_DATA_MODEL_H__

#include <libgda/gda-data-model-bdb.h>

#define TYPE_CUSTOM_DATA_MODEL            (custom_data_model_get_type())
#define CUSTOM_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VPROVIDER_DATA_MODEL, CustomDataModel))
#define CUSTOM_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VPROVIDER_DATA_MODEL, CustomDataModelClass))
#define IS_CUSTOM_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VPROVIDER_DATA_MODEL))
#define IS_CUSTOM_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VPROVIDER_DATA_MODEL))

G_BEGIN_DECLS

typedef struct _CustomDataModel      CustomDataModel;
typedef struct _CustomDataModelClass CustomDataModelClass;
typedef struct _CustomDataModelPrivate CustomDataModelPrivate;

struct _CustomDataModel {
	GdaDataModelBdb         base;
	CustomDataModelPrivate *priv;
};

struct _CustomDataModelClass {
	GdaDataModelBdbClass    parent_class;
};

GType         custom_data_model_get_type (void) G_GNUC_CONST;
GdaDataModel *custom_data_model_new      (void);

G_END_DECLS

#endif
