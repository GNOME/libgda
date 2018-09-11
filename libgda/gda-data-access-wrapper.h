/*
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
G_DECLARE_DERIVABLE_TYPE(GdaDataAccessWrapper, gda_data_access_wrapper, GDA, DATA_ACCESS_WRAPPER, GObject)

struct _GdaDataAccessWrapperClass {
	GObjectClass                   parent_class;

	/*< private >*/
	/* Padding for future expansion */
	/*< private >*/
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-data-access-wrapper
 * @short_description: Offers a random access on top of a cursor-only access data model
 * @title: GdaDataAccessWrapper
 * @stability: Stable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataAccessWrapper object simply wraps another #GdaDataModel data model object
 * and allows data to be accessed in a random way while remaining memory efficient as much as possible.
 */

GdaDataModel *gda_data_access_wrapper_new         (GdaDataModel *model);
gboolean      gda_data_access_wrapper_set_mapping (GdaDataAccessWrapper *wrapper,
						   const gint *mapping, gint mapping_size);

G_END_DECLS

#endif
