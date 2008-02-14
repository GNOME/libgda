/* GDA capi provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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

#ifndef __GDA_CAPI_PROVIDER_H__
#define __GDA_CAPI_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_CAPI_PROVIDER            (gda_capi_provider_get_type())
#define GDA_CAPI_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CAPI_PROVIDER, GdaCapiProvider))
#define GDA_CAPI_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CAPI_PROVIDER, GdaCapiProviderClass))
#define GDA_IS_CAPI_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_CAPI_PROVIDER))
#define GDA_IS_CAPI_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_CAPI_PROVIDER))

typedef struct _GdaCapiProvider      GdaCapiProvider;
typedef struct _GdaCapiProviderClass GdaCapiProviderClass;

struct _GdaCapiProvider {
	GdaServerProvider      provider;
};

struct _GdaCapiProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType gda_capi_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
