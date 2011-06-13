/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VIRTUAL_PROVIDER_H__
#define __GDA_VIRTUAL_PROVIDER_H__

/* NOTICE: SQLite must be compiled without the SQLITE_OMIT_VIRTUALTABLE flag */

#include <libgda/sqlite/gda-sqlite-provider.h>

#define GDA_TYPE_VIRTUAL_PROVIDER            (gda_virtual_provider_get_type())
#define GDA_VIRTUAL_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VIRTUAL_PROVIDER, GdaVirtualProvider))
#define GDA_VIRTUAL_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VIRTUAL_PROVIDER, GdaVirtualProviderClass))
#define GDA_IS_VIRTUAL_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VIRTUAL_PROVIDER))
#define GDA_IS_VIRTUAL_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VIRTUAL_PROVIDER))

G_BEGIN_DECLS

typedef struct _GdaVirtualProvider      GdaVirtualProvider;
typedef struct _GdaVirtualProviderClass GdaVirtualProviderClass;

struct _GdaVirtualProvider {
	GdaSqliteProvider  provider;
	/*< private >*/
	void             (*_gda_reserved1) (void);
};

struct _GdaVirtualProviderClass {
	GdaSqliteProviderClass      parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

GType          gda_virtual_provider_get_type          (void) G_GNUC_CONST;

G_END_DECLS

#endif
