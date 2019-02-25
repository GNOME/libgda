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

#ifndef __GDA_VIRTUAL_PROVIDER_H__
#define __GDA_VIRTUAL_PROVIDER_H__

/* NOTICE: SQLite must be compiled without the SQLITE_OMIT_VIRTUALTABLE flag */

#include <libgda/sqlite/gda-sqlite-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_VIRTUAL_PROVIDER            (gda_virtual_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaVirtualProvider, gda_virtual_provider, GDA, VIRTUAL_PROVIDER, GdaSqliteProvider)

struct _GdaVirtualProviderClass {
	GdaSqliteProviderClass      parent_class;

	/*< private >*/
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

/**
 * SECTION:gda-virtual-provider
 * @short_description: Base class for all virtual provider objects
 * @title: GdaVirtualProvider
 * @stability: Stable
 * @see_also: #GdaVirtualConnection
 *
 * This is a base virtual class for all virtual providers implementations.
 */

G_END_DECLS

#endif
