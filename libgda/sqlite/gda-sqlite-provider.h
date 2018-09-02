/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_SQLITE_PROVIDER_H__
#define __GDA_SQLITE_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_SQLITE_PROVIDER            (gda_sqlite_provider_get_type())
#define GDA_SQLITE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SQLITE_PROVIDER, GdaSqliteProvider))
#define GDA_SQLITE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SQLITE_PROVIDER, GdaSqliteProviderClass))
#define GDA_IS_SQLITE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SQLITE_PROVIDER))
#define GDA_IS_SQLITE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SQLITE_PROVIDER))

typedef struct _GdaSqliteProvider      GdaSqliteProvider;
typedef struct _GdaSqliteProviderClass GdaSqliteProviderClass;

struct _GdaSqliteProvider {
	GdaServerProvider provider;
};
struct _GdaSqliteProviderClass {
	GdaServerProviderClass parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};


G_BEGIN_DECLS

GType              gda_sqlite_provider_get_type (void) G_GNUC_CONST;
G_END_DECLS

#endif
