/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
static void gda_xslt_getxmlvalue_function (xmlXPathParserContextPtr ctxt,
					int nargs);
static void gda_xslt_section_element (xsltTransformContextPtr tctxt,
				      xmlNodePtr node, xmlNodePtr inst,
				      xsltStylePreCompPtr comp);

void *
_gda_xslt_extension_init (xsltTransformContextPtr ctxt, const xmlChar * URI)
{
	int res;
	GdaXsltIntCont *data;
#ifdef GDA_DEBUG_NO
	g_print ("_gda_xslt_extension_init");
#endif
	if (!URI || strcmp ((gchar*) URI, GDA_XSLT_EXTENSION_URI)) {
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
				       g_free,
				       NULL);

	res = xsltRegisterExtFunction (ctxt,
				       (const xmlChar *)
				       GDA_XSLT_FUNC_GETVALUE, URI,
				       gda_xslt_getvalue_function);
	res = xsltRegisterExtFunction (ctxt,
				       (const xmlChar *)
				       GDA_XSLT_FUNC_GETXMLVALUE, URI,
				       gda_xslt_getxmlvalue_function);
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
_gda_xslt_extension_shutdown (G_GNUC_UNUSED xsltTransformContextPtr ctxt,
			     G_GNUC_UNUSED const xmlChar * URI, void *data)
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
				  "gda_xslt_getnodeset_function: invalid number of arguments\n");
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
						  BAD_CAST GDA_XSLT_EXTENSION_URI);
	if (data == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "sqlxslt: failed to get module data\n");
		return;
	}
	setname = valuePop (ctxt);
	if (setname == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "sqlxslt: internal error\n");
		return;
	}

	if (setname->type != XPATH_STRING) {
		valuePush (ctxt, setname);
		xmlXPathStringFunction (ctxt, 1);
		setname = valuePop (ctxt);
		if (setname == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "sqlxslt: internal error\n");
			return;
		}
	}
	nodeset =
		_gda_xslt_bk_fun_getnodeset (setname->stringval, execc, data);
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
						  BAD_CAST GDA_XSLT_EXTENSION_URI);
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
	value = _gda_xslt_bk_fun_checkif (setname->stringval, sql->stringval,
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
						  BAD_CAST GDA_XSLT_EXTENSION_URI);
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

	value = _gda_xslt_bk_fun_getvalue (set->stringval, name->stringval,
					  execc, data,0);
	if (value == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getvalue_function: internal error. Empty value\n");
		return;
	}
	valuePush (ctxt, value);
}

static void
gda_xslt_getxmlvalue_function (xmlXPathParserContextPtr ctxt, int nargs)
{
	GdaXsltIntCont *data;
	xsltTransformContextPtr tctxt;
	xmlXPathObjectPtr set, name, value;
	GdaXsltExCont *execc;

	if (nargs != 2) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getxmlvalue_function: invalid number of arguments\n");
		return;
	}
	tctxt = xsltXPathGetTransformContext (ctxt);
	if (tctxt == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getxmlvalue_function: failed to get the transformation context\n");
		return;
	}

	execc = (GdaXsltExCont *) tctxt->_private;
	data = (GdaXsltIntCont *) xsltGetExtData (tctxt,
						  BAD_CAST GDA_XSLT_EXTENSION_URI);
	if (data == NULL || execc == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getxmlvalue_function: failed to get module internal data\n");
		return;
	}
	name = valuePop (ctxt);
	set = valuePop (ctxt);
	if (name == NULL || set == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getxmlvalue_function: internal error\n");
		return;
	}

	if (name->type != XPATH_STRING) {
		valuePush (ctxt, name);
		xmlXPathStringFunction (ctxt, 1);
		name = valuePop (ctxt);
		if (name == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "gda_xslt_getxmlvalue_function: internal error. Name parameter is not a string\n");
			return;
		}
	}
	if (set->type != XPATH_STRING) {
		valuePush (ctxt, set);
		xmlXPathStringFunction (ctxt, 1);
		set = valuePop (ctxt);
		if (set == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					  "gda_xslt_getxmlvalue_function: internal error. Set parameter is not a string\n");
			return;
		}
	}

	value = _gda_xslt_bk_fun_getvalue (set->stringval, name->stringval,
					  execc, data,1);
	if (value == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_getxmlvalue_function: internal error. Empty value\n");
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
						  BAD_CAST GDA_XSLT_EXTENSION_URI);
	if (data == NULL || execc == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_section_element: failed to get module internal data\n");
		tctxt->state = XSLT_STATE_STOPPED;
		return;
	}
	res = _gda_xslt_bk_section (execc, data, tctxt, node, inst, comp);
	if (res < 0) {
		xsltGenericError (xsltGenericErrorContext,
				  "gda_xslt_section_element: execute query backend\n");
		tctxt->state = XSLT_STATE_STOPPED;
		return;
	}
	return;
}
