/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
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

#ifndef __GDA_ORACLE_PROVIDER_H__
#define __GDA_ORACLE_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_ORACLE_PROVIDER            (gda_oracle_provider_get_type())
#define GDA_ORACLE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_PROVIDER, GdaOracleProvider))
#define GDA_ORACLE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_PROVIDER, GdaOracleProviderClass))
#define GDA_IS_ORACLE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_PROVIDER))
#define GDA_IS_ORACLE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_PROVIDER))

typedef struct _GdaOracleProvider      GdaOracleProvider;
typedef struct _GdaOracleProviderClass GdaOracleProviderClass;

struct _GdaOracleProvider {
	GdaServerProvider      provider;
};

struct _GdaOracleProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType gda_oracle_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
