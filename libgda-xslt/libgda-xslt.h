/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef _LIBGDA_XSLT_H
#define _LIBGDA_XSLT_H

#include <glib.h>
#include <libgda/libgda.h>
#include <libxslt/transform.h>

G_BEGIN_DECLS
#define GDA_XSLT_EXTENSION_URI "http://www.gnome-db.org/ns/gda-sql-ext-v4"
struct _GdaXsltExCont
{
	int            init;
	GdaConnection *cnc;
	GHashTable    *query_hash;
	GError        *error;

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
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
