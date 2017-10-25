/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_MDB_PROVIDER_H__
#define __GDA_MDB_PROVIDER_H__

#include <virtual/gda-vprovider-data-model.h>

#define GDA_TYPE_MDB_PROVIDER            (gda_mdb_provider_get_type())
#define GDA_MDB_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MDB_PROVIDER, GdaMdbProvider))
#define GDA_MDB_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MDB_PROVIDER, GdaMdbProviderClass))
#define GDA_IS_MDB_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_MDB_PROVIDER))
#define GDA_IS_MDB_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_MDB_PROVIDER))

typedef struct _GdaMdbProvider      GdaMdbProvider;
typedef struct _GdaMdbProviderClass GdaMdbProviderClass;

struct _GdaMdbProvider {
	GdaVproviderDataModel      provider;
};

struct _GdaMdbProviderClass {
	GdaVproviderDataModelClass parent_class;
};

G_BEGIN_DECLS

GType              gda_mdb_provider_get_type (void) G_GNUC_CONST;
GdaServerProvider *gda_mdb_provider_new      (void);

G_END_DECLS

#endif
