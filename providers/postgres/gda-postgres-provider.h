/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 - 2008 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_POSTGRES_PROVIDER_H__
#define __GDA_POSTGRES_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_POSTGRES_PROVIDER            (gda_postgres_provider_get_type())
#define GDA_POSTGRES_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_PROVIDER, GdaPostgresProvider))
#define GDA_POSTGRES_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_PROVIDER, GdaPostgresProviderClass))
#define GDA_IS_POSTGRES_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_PROVIDER))
#define GDA_IS_POSTGRES_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_POSTGRES_PROVIDER))

typedef struct _GdaPostgresProvider      GdaPostgresProvider;
typedef struct _GdaPostgresProviderClass GdaPostgresProviderClass;

struct _GdaPostgresProvider {
	GdaServerProvider      provider;
};

struct _GdaPostgresProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType gda_postgres_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
