/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDA_THREAD_PROVIDER_H__
#define __GDA_THREAD_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_THREAD_PROVIDER            (_gda_thread_provider_get_type())
#define GDA_THREAD_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_THREAD_PROVIDER, GdaThreadProvider))
#define GDA_THREAD_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_THREAD_PROVIDER, GdaThreadProviderClass))
#define GDA_IS_THREAD_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_THREAD_PROVIDER))
#define GDA_IS_THREAD_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_THREAD_PROVIDER))

typedef struct _GdaThreadProvider      GdaThreadProvider;
typedef struct _GdaThreadProviderClass GdaThreadProviderClass;
typedef struct _GdaThreadProviderPrivate GdaThreadProviderPrivate;

struct _GdaThreadProvider {
	GdaServerProvider provider;
	GdaThreadProviderPrivate *priv;
};

struct _GdaThreadProviderClass {
	GdaServerProviderClass parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

G_BEGIN_DECLS

GType              _gda_thread_provider_get_type (void) G_GNUC_CONST;
GdaConnection     *_gda_thread_provider_handle_virtual_connection (GdaThreadProvider *provider, GdaConnection *sub_cnc);

G_END_DECLS

#endif
