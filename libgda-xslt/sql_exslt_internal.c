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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <libxml/xpathInternals.h>

#include <libgda-xslt.h>
#include "sql_backend.h"


static void gda_xslt_getnodeset_function (xmlXPathParserContextPtr ctxt,
					  int nargs);
static void gda_xslt_checkif_function (xmlXPathParserContextPtr ctxt,
				       int nargs);
static void gda_xslt_getvalue_function (xmlXPathParserContextPtr ctxt,
					int nargs);
static void gda_xslt_section_element (xsltTransformContextPtr tctxt,
				      xmlNodePtr node, xmlNodePtr inst,
				      xsltStylePreCompPtr comp);

void
gda_xslt_free_hashvalue (gpointer data)
{
	g_free (data);
}

void *
gda_xslt_extension_init (xsltTransformContextPtr ctxt, const xmlChar * URI)
{
	int res;
	GdaXsltIntCont *data;
#ifdef GDA_DEBUG_NO
	g_print ("gda_xslt_extension_init");
#endif
	if (!URI || strcmp (URI, GDA_XSLT_EXTENSION_URI)) {
#ifdef GDA_DEBUG_NO
		g_print ("called for another URI, exit");
#endif
		return NULL;
	}

	data = calloc (1, sizeof (GdaXsltIntCont));
	if (data == NULL) {
#ifdef GDA_DEBUG_NO
		g_print ("no memory");
#endif
		return NULL;
	}
#ifdef GDA_DEBUG_NO
	g_print ("initialize result_sets hash");
#endif
	data->result_sets =
		g_hash_table_new_full (g_str_hash, g_str_equal,
				       gda_xslt_free_hashvalue,
				       gda_xslt_free_hashvalue);

	res = xsltRegisterExtFunction (ctxt,
				       (const xmlChar *)
				       GDA_XSLT_FUNC_GETVALUE, URI,
				       gda_xslt_getvalue_function);
	res |= xsltRegisterExtFunction (ctxt,
					(const xmlChar *)
					GDA_XSLT_FUNC_CHECKIF, URI,
					gda_xslt_checkif_function);
	res |= xsltRegisterExtFunction (ctxt,
					(const xmlChar *)
					GDA_XSLT_FUNC_GETNODESET, URI,
					gda_xslt_getnodeset_function);
	if (res != 0) {
		g_error ("failed to xsltRegisterExtFunction = [%d]", res);
	}

	res = xsltRegisterExtElement (ctxt,
				      (const xmlChar *) GDA_XSLT_ELEM_SECTION,
				      URI,
				      (xsltTransformFunction)
				      gda_xslt_section_element);
	if (res != 0) {
		g_error ("failed to xsltRegisterExtElement = [%d]", res);
	}
	return data;
}

void
gda_xslt_extension_shutdown (xsltTransformContextPtr ctxt,
			     const xmlChar * URI, void *data)
{
	GdaXsltIntCont *p_data = (GdaXsltIntCont *) data;
	if (p_data) {
		free (p_data);
	}
}

static void
gda_xslt_getnodeset_function (xmlXPathParserContextPtr ctxt, int nargs)
{
	GdaXsltIntCont *data;
	xsltTransformContextPtr tctxt;
	xmlXPathObjectPtr setname, nodeset;
	GdaXsltExCont *execc;
	if (nargs != 1) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: invalid number of arguments\n");
		return;
	}
	tctxt = xsltXPathGetTransformContext (ctxt);
	if (tctxt == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "sqlxslt: failed to get the transformation context\n");
		return;
	}
	execc = (GdaXsltExCont *) tctxt->_private;
	data = (GdaXsltIntCont *) xsltGetExtData (tctxt,
						  GDA_XSLT_EXTENSION_URI);
	if (data == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "PEIMP_xslt_normal_latex_escape: failed to get module data\n");
		return;
	}
	setname = valuePop (ctxt);
	if (setname == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "PEIMP_xslt_normal_latex_escape: internal error\n");
		return;
	}

	if (setname->type != XPATH_STRING) {
		valuePush (ctxt, setname);
		xmlXPathStringFunction (ctxt, 1);
		setname = valuePop (ctxt);
		if (setname == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "PEIMP_xslt_normal_latex_escape: internal error\n");
			return;
		}
	}
	nodeset =
		gda_xslt_bk_fun_getnodeset (setname->stringval, execc, data);
	if (nodeset == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "exsltDynMapFunctoin: ret == NULL\n");
		return;
	}
	valuePush (ctxt, nodeset);
}

static void
gda_xslt_checkif_function (xmlXPathParserContextPtr ctxt, int nargs)
{
	GdaXsltIntCont *data;
	xsltTransformContextPtr tctxt;
	xmlXPathObjectPtr setname, sql, value;
	GdaXsltExCont *execc;
	if (nargs != 2) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_checkif_function: invalid number of arguments\n");
		return;
	}
	tctxt = xsltXPathGetTransformContext (ctxt);
	if (tctxt == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_checkif_function: failed to get the transformation context\n");
		return;
	}
	execc = (GdaXsltExCont *) tctxt->_private;
	data = (GdaXsltIntCont *) xsltGetExtData (tctxt,
						  GDA_XSLT_EXTENSION_URI);
	if (data == NULL || execc == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_checkif_function: failed to get module internal data\n");
		return;
	}
	sql = valuePop (ctxt);
	setname = valuePop (ctxt);
	if (sql == NULL || setname == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_checkif_function: internal error\n");
		return;
	}

	if (sql->type != XPATH_STRING) {
		valuePush (ctxt, sql);
		xmlXPathStringFunction (ctxt, 1);
		sql = valuePop (ctxt);
		if (sql == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "sqlxslt: internal error. sql parameter is not a string\n");
			return;
		}
	}
	if (setname->type != XPATH_STRING) {
		valuePush (ctxt, setname);
		xmlXPathStringFunction (ctxt, 1);
		setname = valuePop (ctxt);
		if (setname == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "sqlxslt: internal error. Setname parameter is not a string\n");
			return;
		}
	}
	value = gda_xslt_bk_fun_checkif (setname->stringval, sql->stringval,
					 execc, data);
	if (value == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "sqlxslt: internal error. Empty value\n");
		return;
	}
	valuePush (ctxt, value);
}

static void
gda_xslt_getvalue_function (xmlXPathParserContextPtr ctxt, int nargs)
{
	GdaXsltIntCont *data;
	xsltTransformContextPtr tctxt;
	xmlXPathObjectPtr set, name, value;
	GdaXsltExCont *execc;

	if (nargs != 2) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: invalid number of arguments\n");
		return;
	}
	tctxt = xsltXPathGetTransformContext (ctxt);
	if (tctxt == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: failed to get the transformation context\n");
		return;
	}

	execc = (GdaXsltExCont *) tctxt->_private;
	data = (GdaXsltIntCont *) xsltGetExtData (tctxt,
						  GDA_XSLT_EXTENSION_URI);
	if (data == NULL || execc == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: failed to get module internal data\n");
		return;
	}
	name = valuePop (ctxt);
	set = valuePop (ctxt);
	if (name == NULL || set == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: internal error\n");
		return;
	}

	if (name->type != XPATH_STRING) {
		valuePush (ctxt, name);
		xmlXPathStringFunction (ctxt, 1);
		name = valuePop (ctxt);
		if (name == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "gda_xslt_getvalue_function: internal error. Name parameter is not a string\n");
			return;
		}
	}
	if (set->type != XPATH_STRING) {
		valuePush (ctxt, set);
		xmlXPathStringFunction (ctxt, 1);
		set = valuePop (ctxt);
		if (set == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "gda_xslt_getvalue_function: internal error. Set parameter is not a string\n");
			return;
		}
	}

	value = gda_xslt_bk_fun_getvalue (set->stringval, name->stringval,
					  execc, data);
	if (value == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: internal error. Empty value\n");
		return;
	}
	valuePush (ctxt, value);
}

static void
gda_xslt_section_element (xsltTransformContextPtr tctxt,
			  xmlNodePtr node,
			  xmlNodePtr inst, xsltStylePreCompPtr comp)
{
	int res;
	GdaXsltIntCont *data;
	GdaXsltExCont *execc;

//   gda_xslt_dump_element(tctxt,node,inst,comp);
	if (tctxt == NULL || node == NULL || inst == NULL
	    || tctxt->insert == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_section_element: bad input date\n");
		tctxt->state = XSLT_STATE_STOPPED;
		return;
	}

	execc = (GdaXsltExCont *) tctxt->_private;
	data = (GdaXsltIntCont *) xsltGetExtData (tctxt,
						  GDA_XSLT_EXTENSION_URI);
	if (data == NULL || execc == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_section_element: failed to get module internal data\n");
		tctxt->state = XSLT_STATE_STOPPED;
		return;
	}
	res = gda_xslt_bk_section (execc, data, tctxt, node, inst, comp);
	if (res < 0) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_section_element: execute query backend\n");
		tctxt->state = XSLT_STATE_STOPPED;
		return;
	}
	return;
}
