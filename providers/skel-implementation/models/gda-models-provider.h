/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_MODELS_PROVIDER_H__
#define __GDA_MODELS_PROVIDER_H__

#include <virtual/gda-vprovider-data-model.h>

#define GDA_TYPE_MODELS_PROVIDER            (gda_models_provider_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaModelsProvider, gda_models_provider, GDA, MODELS_PROVIDER, GdaVproviderDataModel)

struct _GdaModelsProviderClass {
	GdaVproviderDataModelClass parent_class;
};

G_BEGIN_DECLS


G_END_DECLS

#endif
