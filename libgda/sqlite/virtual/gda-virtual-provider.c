/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <sqlite3.h>
#include "gda-virtual-provider.h"

#define PARENT_TYPE GDA_TYPE_SQLITE_PROVIDER
#define CLASS(obj) (GDA_VIRTUAL_PROVIDER_CLASS (G_OBJECT_GET_CLASS (obj)))

static void gda_virtual_provider_class_init (GdaVirtualProviderClass *klass);
static void gda_virtual_provider_init       (GdaVirtualProvider *prov, GdaVirtualProviderClass *klass);
static void gda_virtual_provider_finalize   (GObject *object);
static GObjectClass *parent_class = NULL;

static gboolean gda_virtual_provider_close_connection (GdaServerProvider *prov,
						       GdaConnection *cnc);

/*
 * GdaVirtualProvider class implementation
 */
static void
gda_virtual_provider_class_init (GdaVirtualProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *prov_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	object_class->finalize = gda_virtual_provider_finalize;
	prov_class->close_connection = gda_virtual_provider_close_connection;
}

static void
gda_virtual_provider_init (G_GNUC_UNUSED GdaVirtualProvider *vprov, G_GNUC_UNUSED GdaVirtualProviderClass *klass)
{
}

static void
gda_virtual_provider_finalize (GObject *object)
{
	GdaVirtualProvider *prov = (GdaVirtualProvider *) object;

	g_return_if_fail (GDA_IS_VIRTUAL_PROVIDER (prov));

	/* chain to parent class */
	parent_class->finalize (object);
}

static gboolean
gda_virtual_provider_close_connection (GdaServerProvider *prov, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_VIRTUAL_PROVIDER (prov), FALSE);
	return GDA_SERVER_PROVIDER_CLASS (parent_class)->close_connection (prov, cnc);
}

GType
gda_virtual_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVirtualProviderClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_virtual_provider_class_init,
				NULL, NULL,
				sizeof (GdaVirtualProvider),
				0,
				(GInstanceInitFunc) gda_virtual_provider_init,
				0
			};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaVirtualProvider", &info, G_TYPE_FLAG_ABSTRACT);
		g_static_mutex_unlock (&registering);
		}
	}

	return type;
}
