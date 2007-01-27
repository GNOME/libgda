/* GDA SQLite provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Carlos Perelló Marín <carlos@gnome-db.org>
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

#if !defined(__gda_sqlite_provider_h__)
#  define __gda_sqlite_provider_h__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_SQLITE_PROVIDER            (gda_sqlite_provider_get_type())
#define GDA_SQLITE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SQLITE_PROVIDER, GdaSqliteProvider))
#define GDA_SQLITE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SQLITE_PROVIDER, GdaSqliteProviderClass))
#define GDA_IS_SQLITE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SQLITE_PROVIDER))
#define GDA_IS_SQLITE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SQLITE_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

typedef struct _GdaSqliteProvider      GdaSqliteProvider;
typedef struct _GdaSqliteProviderClass GdaSqliteProviderClass;

struct _GdaSqliteProvider {
	GdaServerProvider provider;
};

struct _GdaSqliteProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType              gda_sqlite_provider_get_type (void);
GdaServerProvider *gda_sqlite_provider_new (void);

G_END_DECLS

#endif
