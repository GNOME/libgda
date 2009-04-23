/* GDA oracle provider
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
