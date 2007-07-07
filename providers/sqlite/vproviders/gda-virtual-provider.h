/* GDA virtual provider (based on SQLite)
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifndef __GDA_VIRTUAL_PROVIDER_H__
#define __GDA_VIRTUAL_PROVIDER_H__

/* NOTICE: SQLite must be compiled without the SQLITE_OMIT_VIRTUALTABLE flag */

#include "../gda-sqlite-provider.h"

#define GDA_TYPE_VIRTUAL_PROVIDER            (gda_virtual_provider_get_type())
#define GDA_VIRTUAL_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VIRTUAL_PROVIDER, GdaVirtualProvider))
#define GDA_VIRTUAL_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VIRTUAL_PROVIDER, GdaVirtualProviderClass))
#define GDA_IS_VIRTUAL_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VIRTUAL_PROVIDER))
#define GDA_IS_VIRTUAL_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VIRTUAL_PROVIDER))

G_BEGIN_DECLS

typedef struct _GdaVirtualProvider      GdaVirtualProvider;
typedef struct _GdaVirtualProviderClass GdaVirtualProviderClass;
typedef struct _GdaVirtualProviderPrivate GdaVirtualProviderPrivate;

struct _GdaVirtualProvider {
	GdaSqliteProvider          provider;
	GdaVirtualProviderPrivate *priv;
};

struct _GdaVirtualProviderClass {
	GdaSqliteProviderClass      parent_class;
};

GType          gda_virtual_provider_get_type          (void) G_GNUC_CONST;

G_END_DECLS

#endif
