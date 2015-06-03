/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib.h>

#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>

#include "libgda-xslt.h"
#include "sql_backend.h"

/**
 * gda_xslt_register
 *
 * Register the xslt extension to libxslt
 */
void
gda_xslt_register (void)
{
	static GMutex init_mutex;
	static int init = 0;

	g_mutex_lock (&init_mutex);
	if (!init) {
		int init_res;
		init = 1;
		init_res =
			xsltRegisterExtModule (BAD_CAST GDA_XSLT_EXTENSION_URI,
					       _gda_xslt_extension_init,
					       _gda_xslt_extension_shutdown);
		if (init_res != 0) {
			g_error ("error, xsltRegisterExtModule = [%d]\n",
				 init_res);
		}
	}
	g_mutex_unlock (&init_mutex);
}

/**
 * gda_xslt_set_execution_context
 *
 * Set the internal gda-xslt context to the xsltTransformationContext.
 */
void
gda_xslt_set_execution_context (xsltTransformContextPtr tcxt,
				GdaXsltExCont * exec)
{
	tcxt->_private = (void *) exec;
}

/**
 * gda_xslt_create_context_simple
 * @cnc: a #GdaConnection
 * @error: a place to store errors, or %NULL
 *
 * create a gda-xslt context with the Gdaconnection and not predefined queries
 *
 * Returns:
 */
GdaXsltExCont *
gda_xslt_create_context_simple (GdaConnection *cnc, G_GNUC_UNUSED GError **error)
{
	GdaXsltExCont *local = NULL;

	local = (GdaXsltExCont *) g_new0 (GdaXsltExCont, 1);
	local->init = 1;
	local->cnc = cnc;
	local->error = NULL;
	local->query_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	return local;
}

/**
 * gda_xslt_finalize_context
 * 
 * clear the gda-xslt context
 *
 * Returns:
 */
int
gda_xslt_finalize_context (GdaXsltExCont * ctx)
{
	g_free (ctx);
	return 0;
}
