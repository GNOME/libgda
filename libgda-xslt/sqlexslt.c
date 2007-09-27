/* GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Pawe³ Cesar Sanjuan Szklarz <paweld2@gmail.com>
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
 * FIXME: ADD COMMENT
 */
void
gda_xslt_register (void)
{
	static int init = 0;
	if (!init) {
		int init_res;
		init = 1;
		init_res =
			xsltRegisterExtModule (GDA_XSLT_EXTENSION_URI,
					       gda_xslt_extension_init,
					       gda_xslt_extension_shutdown);
		if (init_res != 0) {
			g_error ("error, xsltRegisterExtModule = [%d]\n",
				 init_res);
		}
	}
}

/**
 * gda_xslt_set_execution_context
 *
 * FIXME: ADD COMMENT
 */
void
gda_xslt_set_execution_context (xsltTransformContextPtr tcxt,
				GdaXsltExCont * exec)
{
	tcxt->_private = (void *) exec;
}

/**
 * gda_xslt_create_context_simple
 *
 * FIXME: ADD COMMENT
 *
 * Returns:
 */
GdaXsltExCont *
gda_xslt_create_context_simple (GdaConnection * cnc, GError ** error)
{
	GdaXsltExCont *local = NULL;
	GdaDict *gda_dict = NULL;

	gda_dict = gda_dict_new ();
	gda_dict_set_connection (gda_dict, cnc);

	local = (GdaXsltExCont *) g_new0 (GdaXsltExCont, 1);
	local->init = 1;
	local->cnc = cnc;
	local->error = NULL;
	local->gda_dict = gda_dict;
	local->query_hash =
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	return local;
}

/**
 * gda_xslt_finalize_context
 * 
 * FIXME: ADD COMMENT
 *
 * Returns:
 */
int
gda_xslt_finalize_context (GdaXsltExCont * ctx)
{
	g_free (ctx);
	return 0;
}
