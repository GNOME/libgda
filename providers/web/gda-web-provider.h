/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2019 Daniel Espinosa <malerba@gmail.com>
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

#ifndef __GDA_WEB_PROVIDER_H__
#define __GDA_WEB_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_WEB_PROVIDER            (gda_web_provider_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaWebProvider, gda_web_provider, GDA, WEB_PROVIDER, GdaServerProvider)

struct _GdaWebProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS


G_END_DECLS

#endif
