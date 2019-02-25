/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 - 2014 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-server-provider-impl.h>

G_DEFINE_TYPE (GdaVirtualProvider, gda_virtual_provider, GDA_TYPE_SQLITE_PROVIDER)

/*
 * GdaVirtualProvider class implementation
 */
static void
gda_virtual_provider_class_init (GdaVirtualProviderClass *klass)
{
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) NULL);
}

static void
gda_virtual_provider_init (G_GNUC_UNUSED GdaVirtualProvider *vprov)
{
}

