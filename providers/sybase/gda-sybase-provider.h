/* GDA Sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *  Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *  Holger Thon <holger.thon@gnome-db.org>
 *      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	        Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#if !defined(__gda_sybase_provider_h__)
#  define __gda_sybase_provider_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <libgda/gda-server-provider.h>

#include <ctpublic.h>
#include <cspublic.h>

G_BEGIN_DECLS

#define GDA_TYPE_SYBASE_PROVIDER            (gda_sybase_provider_get_type())
#define GDA_SYBASE_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SYBASE_PROVIDER, GdaSybaseProvider))
#define GDA_SYBASE_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SYBASE_PROVIDER, GdaSybaseProviderClass))
#define GDA_IS_SYBASE_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SYBASE_PROVIDER))
#define GDA_IS_SYBASE_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SYBASE_PROVIDER))

typedef struct _GdaSybaseProvider      GdaSybaseProvider;
typedef struct _GdaSybaseProviderClass GdaSybaseProviderClass;

struct _GdaSybaseProvider {
	GdaServerProvider provider;
};

struct _GdaSybaseProviderClass {
	GdaServerProviderClass parent_class;
};

typedef struct _GdaSybaseConnectionData {
	CS_CONTEXT     *context;
	CS_CONNECTION  *connection;
} GdaSybaseConnectionData;

GType              gda_sybase_provider_get_type (void);
GdaServerProvider *gda_sybase_provider_new (void);

G_END_DECLS

#endif
