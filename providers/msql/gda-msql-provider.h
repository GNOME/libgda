/* GDA mSQL Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net
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

#ifndef __gda_msql_provider_h__
#define __gda_msql_provider_h__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_MSQL_PROVIDER (gda_msql_provider_get_type()) 

#define GDA_MSQL_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST(obj,GDA_TYPE_MSQL_PROVIDER,GdaMsqlProvider))

#define GDA_MSQL_PROVIDER_CLASS(cl) \
  (G_TYPE_CHECK_CLASS_CAST(cl,GDA_TYPE_MSQL_PROVIDER,GdaMsqlProviderClass))

#define GDA_IS_MSQL_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE(obj,GDA_TYPE_MSQL_PROVIDER))

#define GDA_IS_MSQL_PROVIDER_CLASS(cl) \
  (G_TYPE_CHECK_CLASS_TYPE((cl),GDA_TYPE_MSQL_PROVIDER))

struct _GdaMsqlProvider {
  GdaServerProvider provider;
};

struct _GdaMsqlProviderClass {
  GdaServerProviderClass parent_class;
};

typedef struct _GdaMsqlProvider GdaMsqlProvider;
typedef struct _GdaMsqlProviderClass GdaMsqlProviderClass;

GType gda_msql_provider_get_type();
GdaServerProvider *gda_msql_provider_new();

G_END_DECLS

#endif
