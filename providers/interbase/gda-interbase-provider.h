/* GDA Interbase Provider
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

#if !defined(__gda_interbase_provider_h__)
#  define __gda_interbase_provider_h__

#include <libgda/gda-server-provider.h>
#include <ibase.h>

G_BEGIN_DECLS

#define GDA_TYPE_INTERBASE_PROVIDER            (gda_interbase_provider_get_type())
#define GDA_INTERBASE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_INTERBASE_PROVIDER, GdaInterbaseProvider))
#define GDA_INTERBASE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_INTERBASE_PROVIDER, GdaInterbaseProviderClass))
#define GDA_IS_INTERBASE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_INTERBASE_PROVIDER))
#define GDA_IS_INTERBASE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_INTERBASE_PROVIDER))

typedef struct _GdaInterbaseProvider      GdaInterbaseProvider;
typedef struct _GdaInterbaseProviderClass GdaInterbaseProviderClass;

struct _GdaInterbaseProvider {
	GdaServerProvider provider;
};

struct _GdaInterbaseProviderClass {
	GdaServerProviderClass parent_class;
};

GType              gda_interbase_provider_get_type (void);
GdaServerProvider *gda_interbase_provider_new (void);

G_END_DECLS

#endif
