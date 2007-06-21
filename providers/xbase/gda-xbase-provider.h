/* GDA Xbase Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_xbase_provider_h__)
#  define __gda_xbase_provider_h__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_XBASE_PROVIDER            (gda_xbase_provider_get_type())
#define GDA_XBASE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XBASE_PROVIDER, GdaXbaseProvider))
#define GDA_XBASE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XBASE_PROVIDER, GdaXbaseProviderClass))
#define GDA_IS_XBASE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XBASE_PROVIDER))
#define GDA_IS_XBASE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XBASE_PROVIDER))

typedef struct _GdaXbaseProvider      GdaXbaseProvider;
typedef struct _GdaXbaseProviderClass GdaXbaseProviderClass;

struct _GdaXbaseProvider {
	GdaServerProvider provider;
};

struct _GdaXbaseProviderClass {
	GdaServerProviderClass parent_class;
};

GType              gda_xbase_provider_get_type (void) G_GNUC_CONST;
GdaServerProvider *gda_xbase_provider_new (void);

void               gda_xbase_provider_make_error (GdaConnection *cnc);

G_END_DECLS

#endif
