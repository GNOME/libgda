/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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
#define GDA_MODELS_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MODELS_PROVIDER, GdaModelsProvider))
#define GDA_MODELS_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MODELS_PROVIDER, GdaModelsProviderClass))
#define GDA_IS_MODELS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_MODELS_PROVIDER))
#define GDA_IS_MODELS_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_MODELS_PROVIDER))

typedef struct _GdaModelsProvider      GdaModelsProvider;
typedef struct _GdaModelsProviderClass GdaModelsProviderClass;

struct _GdaModelsProvider {
	GdaVproviderDataModel      provider;
};

struct _GdaModelsProviderClass {
	GdaVproviderDataModelClass parent_class;
};

G_BEGIN_DECLS

GType gda_models_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
