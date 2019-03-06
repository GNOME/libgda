/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_SQLCIPHER_PROVIDER_H__
#define __GDA_SQLCIPHER_PROVIDER_H__

#include <libgda/sqlite/gda-sqlite-provider.h>


G_BEGIN_DECLS

#define GDA_TYPE_SQLCIPHER_PROVIDER            (gda_sqlcipher_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaSqlcipherProvider, gda_sqlcipher_provider, GDA, SQLCIPHER_PROVIDER, GdaSqliteProvider)

struct _GdaSqlcipherProviderClass {
	GdaSqliteProviderClass parent_class;
};

G_END_DECLS

#endif
