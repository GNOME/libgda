/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VPROVIDER_DATA_MODEL_H__
#define __GDA_VPROVIDER_DATA_MODEL_H__

#include "gda-virtual-provider.h"


/* error reporting */
extern GQuark gda_vprovider_data_model_error_quark (void);
#define GDA_VPROVIDER_DATA_MODEL_ERROR gda_vprovider_data_model_error_quark ()

typedef enum {
	GDA_VPROVIDER_DATA_MODEL_GENERAL_ERROR
} GdaVproviderDataModelError;


#define GDA_TYPE_VPROVIDER_DATA_MODEL            (gda_vprovider_data_model_get_type())

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE (GdaVproviderDataModel, gda_vprovider_data_model, GDA, VPROVIDER_DATA_MODEL, GdaVirtualProvider)

struct _GdaVproviderDataModelClass {
	GdaVirtualProviderClass       parent_class;

	/*< private >*/
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-vprovider-data-model
 * @short_description: Virtual provider for connections based on a list of GdaDataModel
 * @title: GdaVproviderDataModel
 * @stability: Stable
 * @see_also: See also the <link linkend="VirtualIntro">introduction to virtual connections</link>
 *
 * This provider is used to create virtual connections in which each #GdaDataModel data model can be
 * added as a table in the connection. Using gda_virtual_connection_open() with this provider as argument
 * will generate a #GdaVconnectionDataModel connection object, from which data models can be added.
 */

GdaVirtualProvider *gda_vprovider_data_model_new      (void);

G_END_DECLS

#endif
