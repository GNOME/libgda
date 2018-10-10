/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef _SQL_EXSLT_BACKEND_H
#define _SQL_EXSLT_BACKEND_H

#include <glib.h>
#include <libgda/libgda.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>

G_BEGIN_DECLS

#define GDA_XSLT_FUNC_GETVALUE       "getvalue"
#define GDA_XSLT_FUNC_GETXMLVALUE    "getxmlvalue"
#define GDA_XSLT_FUNC_GETNODESET     "getnodeset"
#define GDA_XSLT_FUNC_CHECKIF        "checkif"
#define GDA_XSLT_ELEM_SECTION        "section"
#define GDA_XSLT_ELEM_INTERNAL_QUERY      "query"
#define GDA_XSLT_ELEM_INTERNAL_TEMPLATE   "template"

/* error reporting */
extern GQuark gda_xslt_error_quark (void);
#define GDA_XSLT_ERROR gda_xslt_error_quark ()

typedef enum {
	GDA_XSLT_GENERAL_ERROR
} GdaXsltError;

struct _GdaXsltIntCont
{
	int         init;
	GHashTable *result_sets;

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
};

typedef struct _GdaXsltIntCont GdaXsltIntCont;

void *_gda_xslt_extension_init (xsltTransformContextPtr ctxt,
			       const xmlChar * URI);
void _gda_xslt_extension_shutdown (xsltTransformContextPtr ctxt,
				  const xmlChar * URI, void *data);

/* elements backend */
int _gda_xslt_bk_section (GdaXsltExCont * exec, GdaXsltIntCont * pdata,
			 xsltTransformContextPtr ctxt, xmlNodePtr node,
			 xmlNodePtr inst, xsltStylePreCompPtr comp);

/* functions backend */
xmlXPathObjectPtr _gda_xslt_bk_fun_getvalue (xmlChar * set, xmlChar * name,
					    GdaXsltExCont * exec,
					    GdaXsltIntCont * pdata,
					    int getXml);
xmlXPathObjectPtr _gda_xslt_bk_fun_getnodeset (xmlChar * set,
					      GdaXsltExCont * exec,
					      GdaXsltIntCont * pdata);
xmlXPathObjectPtr _gda_xslt_bk_fun_checkif (xmlChar * setname,
					   xmlChar * sql_condition,
					   GdaXsltExCont * exec,
					   GdaXsltIntCont * pdata);

G_END_DECLS

#endif 
