/*
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_ACCESS_WRAPPER_H__
#define __GDA_DATA_ACCESS_WRAPPER_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_ACCESS_WRAPPER            (gda_data_access_wrapper_get_type())
#define GDA_DATA_ACCESS_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_ACCESS_WRAPPER, GdaDataAccessWrapper))
#define GDA_DATA_ACCESS_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_ACCESS_WRAPPER, GdaDataAccessWrapperClass))
#define GDA_IS_DATA_ACCESS_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_ACCESS_WRAPPER))
#define GDA_IS_DATA_ACCESS_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_ACCESS_WRAPPER))

typedef struct _GdaDataAccessWrapper        GdaDataAccessWrapper;
typedef struct _GdaDataAccessWrapperClass   GdaDataAccessWrapperClass;
typedef struct _GdaDataAccessWrapperPrivate GdaDataAccessWrapperPrivate;

struct _GdaDataAccessWrapper {
	GObject                        object;
	GdaDataAccessWrapperPrivate   *priv;
};

struct _GdaDataAccessWrapperClass {
	GObjectClass                   parent_class;

	/* Padding for future expansion */
	/*< private >*/
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType         gda_data_access_wrapper_get_type    (void) G_GNUC_CONST;
GdaDataModel *gda_data_access_wrapper_new         (GdaDataModel *model);

G_END_DECLS

#endif
