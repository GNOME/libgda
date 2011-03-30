/*
 * gda-types.c
 * Copyright (C) 2007 Sebastien Granjoux  <seb.sfo@free.fr>
 *               2008 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-types.h"

GType
_gda_slist_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		g_static_mutex_lock (&registering);
                if (type == 0)
			type = g_pointer_type_register_static ("GdaSList");
		g_static_mutex_unlock (&registering);
	}

	return type;
}

GType
_gda_meta_context_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		g_static_mutex_lock (&registering);
                if (type == 0)
			type = g_pointer_type_register_static ("GdaMetaContext");
		g_static_mutex_unlock (&registering);
	}

	return type;
}
