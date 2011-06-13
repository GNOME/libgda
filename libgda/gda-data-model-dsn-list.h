/*
 * Copyright (C) 2008 - 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_DATA_MODEL_DSN_LIST_H__
#define __GDA_DATA_MODEL_DSN_LIST_H__

#include "gda-decl.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_DSN_LIST            (gda_data_model_dsn_list_get_type())
#define GDA_DATA_MODEL_DSN_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_DSN_LIST, GdaDataModelDsnList))
#define GDA_DATA_MODEL_DSN_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_DSN_LIST, GdaDataModelDsnListClass))
#define GDA_IS_DATA_MODEL_DSN_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_DSN_LIST))
#define GDA_IS_DATA_MODEL_DSN_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_DSN_LIST))

typedef struct _GdaDataModelDsnList GdaDataModelDsnList;
typedef struct _GdaDataModelDsnListClass GdaDataModelDsnListClass;
typedef struct _GdaDataModelDsnListPrivate GdaDataModelDsnListPrivate;

struct _GdaDataModelDsnList {
	GObject                     object;
	GdaDataModelDsnListPrivate *priv;
};

struct _GdaDataModelDsnListClass {
	GObjectClass                object_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType gda_data_model_dsn_list_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
