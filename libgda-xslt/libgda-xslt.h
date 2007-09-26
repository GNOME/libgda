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

#ifndef _LIBGDA_XSLT_H
#define _LIBGDA_XSLT_H

#include <glib.h>
#include <libgda/libgda.h>
#include <libxslt/transform.h>

G_BEGIN_DECLS
#define GDA_XSLT_EXTENSION_URI "http://www.gnome-db.org/ns/gda-sql-ext"
struct _GdaXsltExCont
{
	int init;
	GdaConnection *cnc;
	GdaDict *gda_dict;
	GHashTable *query_hash;
	GError *error;
};
typedef struct _GdaXsltExCont GdaXsltExCont;

void           gda_xslt_register              (void);
void           gda_xslt_set_execution_context (xsltTransformContextPtr tcxt,
					       GdaXsltExCont * exec);
GdaXsltExCont *gda_xslt_create_context_simple (GdaConnection * cnc,
					       GError ** error);
int            gda_xslt_finalize_context      (GdaXsltExCont * ctx);

G_END_DECLS

#endif
