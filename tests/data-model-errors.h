/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __DATA_MODEL_ERRORS_H__
#define __DATA_MODEL_ERRORS_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define TYPE_DATA_MODEL_ERRORS            (data_model_errors_get_type())
#define DATA_MODEL_ERRORS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_DATA_MODEL_ERRORS, DataModelErrors))
#define DATA_MODEL_ERRORS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, TYPE_DATA_MODEL_ERRORS, DataModelErrorsClass))
#define IS_DATA_MODEL_ERRORS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, TYPE_DATA_MODEL_ERRORS))
#define IS_DATA_MODEL_ERRORS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_DATA_MODEL_ERRORS))

typedef struct _DataModelErrors        DataModelErrors;
typedef struct _DataModelErrorsClass   DataModelErrorsClass;
typedef struct _DataModelErrorsPrivate DataModelErrorsPrivate;

struct _DataModelErrors {
	GObject                 object;
	DataModelErrorsPrivate *priv;
};

struct _DataModelErrorsClass {
	GObjectClass            parent_class;
};

GType         data_model_errors_get_type     (void) G_GNUC_CONST;
GdaDataModel *data_model_errors_new          (void);

G_END_DECLS

#endif
