/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
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

#include <libxml/xpathInternals.h>
#include <libxslt/variables.h>

#include <libgda/libgda.h>

#include "libgda-xslt.h"
#include "sql_backend.h"
#include <libxml/xpathInternals.h>
#include <libgda/gda-debug-macros.h>

#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-type.h>

#include <sql-parser/gda-sql-parser.h>

/* module error */
GQuark gda_xslt_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_xslt_error");
        return quark;
}

static GHashTable *data_handlers = NULL;	/* key = GType, value = GdaDataHandler obj */
static xmlChar *value_to_xmlchar (const GValue * value);



static void look_predefined_query_by_name (GdaXsltExCont * exec,
					   const char *query_name,
					   GdaStatement ** query);

static void set_resultset_value (GdaXsltIntCont * pdata,
				 const char *resultset_name,
				 GObject * result, GError ** error);
static int get_resultset_col_value (GdaXsltIntCont * pdata,
				    const char *resultset_name,
				    const char *colname, char **outvalue,
				    GError ** error);
static int get_resultset_nodeset (GdaXsltIntCont * pdata,
				  const char *resultset_name,
				  xmlXPathObjectPtr * nodeset,
				  GError ** error);
static int _utility_data_model_to_nodeset (GdaDataModel * model,
					   xmlXPathObjectPtr * nodeset,
					   GError ** error);
static int gda_xslt_bk_internal_query (GdaXsltExCont * exec,
				       GdaXsltIntCont * pdata,
				       xsltTransformContextPtr ctxt,
				       xmlNodePtr query_node);

int
_gda_xslt_holder_set_value (GdaHolder *param, xsltTransformContextPtr ctxt)
{
	GType gdatype;
	gchar *name;
	GValue *value;
	xmlChar *xvalue;
	xmlXPathObjectPtr xsltvar;

	/* information from libgda */
	gdatype = gda_holder_get_g_type (param);
	name = gda_text_to_alphanum (gda_holder_get_id (param));

	/* information from libxslt and libxml */
	xsltvar = xsltVariableLookup (ctxt, (const xmlChar *) name, NULL);
	xvalue = xmlXPathCastToString (xsltvar);

	value = g_new0 (GValue, 1);
	if (!gda_value_set_from_string (value, (gchar *) xvalue, gdatype)) {
		/* error */
		g_free (value);
		g_free (xvalue);
		return -1;
	}
	else {
		gint retval = 0;
		if (! gda_holder_set_value (param, value, NULL))
			retval = -1;
		g_free (xvalue);
		gda_value_free (value);
		return retval;
	}
}

int
_gda_xslt_bk_section (GdaXsltExCont * exec, GdaXsltIntCont * pdata,
		     xsltTransformContextPtr ctxt, xmlNodePtr node,
		     xmlNodePtr inst, G_GNUC_UNUSED xsltStylePreCompPtr comp)
{
	xmlNode *cur_node = NULL;
	xmlNode *query_node = NULL;
	xmlNode *template_node = NULL;
	int res;

	for (cur_node = inst->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE &&
		    xmlStrEqual (cur_node->ns->href,
				 BAD_CAST GDA_XSLT_EXTENSION_URI)) {
			printf ("found element in sql namespace: name[%s]\n",
				cur_node->name);
			if (xmlStrEqual
			    (cur_node->name,
			     BAD_CAST GDA_XSLT_ELEM_INTERNAL_QUERY)) {
				query_node = cur_node;
			}
			else if (xmlStrEqual
				 (cur_node->name,
				  BAD_CAST GDA_XSLT_ELEM_INTERNAL_TEMPLATE)) {
				template_node = cur_node;
			}
		}
	}
	if (query_node == NULL) {
		g_set_error (&(exec->error), 0, 0,
			     "%s", "no query node in section node");
		return -1;
	}
	/* set the query context */
	res = gda_xslt_bk_internal_query (exec, pdata, ctxt, query_node);
	if (res < 0) {
		printf ("sql_bk_internal_query res [%d]\n", res);
		return -1;
	}
	/* run the template */
	if (template_node) {
		/* template node must have only
		   xml comments and ONE xsl:call-template nodes */
		for (cur_node = template_node->children; cur_node;
		     cur_node = cur_node->next) {
			if (IS_XSLT_ELEM (cur_node)) {
				if (IS_XSLT_NAME (cur_node, "call-template")) {
					xsltElemPreCompPtr info =
						(xsltElemPreCompPtr)
						cur_node->psvi;
					if (info != NULL) {
						xsltCallTemplate
							(ctxt, node,
							 cur_node, info);
					}
					else {
						printf ("the xsltStylePreCompPtr is empthy\n");
						return -1;
					}
				}
				else {
#ifdef GDA_DEBUG_NO
					printf ("only one call-template node is allowed on sql:template node\n");
#endif
					return -1;
				}
			}
			else if (cur_node->type != XML_COMMENT_NODE) {
#ifdef GDA_DEBUG_NO
				printf ("only one xsl:call-template or comment node is allowed on sql:template node\n");
#endif
				return -1;
			}
		}
	}
	/* unset the query context */
// TODO, delete the query result information and the query if it was created inline
	return 0;

}

xmlXPathObjectPtr
_gda_xslt_bk_fun_getnodeset (xmlChar * set, GdaXsltExCont * exec,
			    GdaXsltIntCont * pdata)
{
	xmlXPathObjectPtr nodeset;
	int res;
#ifdef GDA_DEBUG_NO
	printf ("running function:_gda_xslt_bk_fun_getnodeset\n");
#endif
	res = get_resultset_nodeset (pdata, (gchar*) set, &nodeset, &(exec->error));
	if (res < 0 || nodeset == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "_gda_xslt_bk_fun_getnodeset error\n");
		return NULL;
	}
	return nodeset;
}

xmlXPathObjectPtr
_gda_xslt_bk_fun_getvalue (xmlChar * set, xmlChar * name, GdaXsltExCont * exec,
			  GdaXsltIntCont * pdata,int getXml)
{
	xmlXPathObjectPtr value;
	int res;
	xmlDocPtr sqlxmldoc;
	xmlNodePtr rootnode;
	xmlNodePtr copyrootnode;
#ifdef GDA_DEBUG_NO
	g_print ("running function:_gda_xslt_bk_fun_getvalue (getxml [%d])", getXml);
#endif
	char *strvalue;
	res = get_resultset_col_value (pdata, (const char *) set,
				       (const char *) name, &strvalue,
				       &(exec->error));
	if (res < 0) {
		xsltGenericError (xsltGenericErrorContext,
				  "_gda_xslt_bk_fun_getvalue: internal error on get_resultset_col_value\n");
		return NULL;
	}

	if( getXml) {
		sqlxmldoc = xmlParseDoc((const xmlChar *)strvalue);
		if (sqlxmldoc == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					"_gda_xslt_bk_fun_getvalue: xmlParseDoc fauld\n");
			return NULL;
		}
		rootnode = xmlDocGetRootElement(sqlxmldoc);
		copyrootnode = xmlCopyNode(rootnode,1);
		if (copyrootnode == NULL) {
			xsltGenericError (xsltGenericErrorContext,
					"_gda_xslt_bk_fun_getvalue: get or copy of root node fauld\n");
			return NULL;
		}
		value = (xmlXPathObjectPtr) xmlXPathNewNodeSet (copyrootnode);
		xmlFreeDoc(sqlxmldoc );
	} else {
		value = (xmlXPathObjectPtr) xmlXPathNewCString ((const char *)
								strvalue);
	}

	if (value == NULL) {
		xsltGenericError (xsltGenericErrorContext,
				  "_gda_xslt_bk_fun_getvalue: internal error\n");
		return NULL;
	}
	return value;
}

xmlXPathObjectPtr
_gda_xslt_bk_fun_checkif (G_GNUC_UNUSED xmlChar * setname, G_GNUC_UNUSED xmlChar * sql_condition,
			 G_GNUC_UNUSED GdaXsltExCont * exec, G_GNUC_UNUSED GdaXsltIntCont * pdata)
{
	return xmlXPathNewBoolean (1);
}



/* utility functions */
static void
set_resultset_value (GdaXsltIntCont * pdata, const char *resultset_name,
		     GObject * result, G_GNUC_UNUSED GError ** error)
{
#ifdef GDA_DEBUG_NO
	g_print ("new resultset: name[%s]", resultset_name);
#endif
	g_hash_table_insert (pdata->result_sets, g_strdup (resultset_name),
			     (gpointer) result);
}

static int
get_resultset_col_value (GdaXsltIntCont * pdata, const char *resultset_name,
			 const char *colname, char **outvalue,
			 GError ** error)
{
	gpointer orig_key = NULL;
	gpointer value = NULL;
	GObject *result;
#ifdef GDA_DEBUG_NO
	g_print ("searching resultset[%s]", resultset_name);
#endif

	if (g_hash_table_lookup_extended
	    (pdata->result_sets, resultset_name, &orig_key, &value)) {
#ifdef GDA_DEBUG_NO
		g_print ("resultset found [%p]", value);
#endif
		result = (GObject *) value;
	}
	else {
#ifdef GDA_DEBUG_NO
		g_print ("no resultset found");
#endif
		*outvalue = NULL;
		return -1;
	}

	if (!GDA_IS_DATA_MODEL (result)) {
#ifdef GDA_DEBUG_NO
		g_print ("this is not a data model, returning NULL");
#endif
		*outvalue = NULL;
		return -1;
	}
	gint col_index;
	col_index = gda_data_model_get_column_index (GDA_DATA_MODEL (result), colname);
	if (col_index < 0) {
#ifdef GDA_DEBUG_NO
		g_print ("no column found by name [%s]", colname);
#endif
		*outvalue = NULL;
		return -1;
	}
	const GValue *db_value;
	db_value = gda_data_model_get_value_at (GDA_DATA_MODEL (result), col_index, 0, error);
	if (db_value == NULL) {
#ifdef GDA_DEBUG_NO
		g_print ("no value found on col_index [%d]", col_index);
#endif
		*outvalue = NULL;
		return -1;
	}
	gchar *gvalue_string;
	gvalue_string = (gchar*) value_to_xmlchar (db_value);
	if (gvalue_string == NULL) {
#ifdef GDA_DEBUG_NO
		g_print ("faild to stringify gvalue");
#endif
		*outvalue = NULL;
		return -1;
	}
	*outvalue = g_strdup (gvalue_string);
	g_free (gvalue_string);
	return 0;
}

static int
get_resultset_nodeset (GdaXsltIntCont * pdata, const char *resultset_name,
		       xmlXPathObjectPtr * nodeset, GError ** error)
{
	gpointer orig_key = NULL;
	gpointer value = NULL;
	GObject *result;
	int res;
#ifdef GDA_DEBUG_NO
	g_print ("searching resultset[%s]", resultset_name);
#endif

	if (g_hash_table_lookup_extended
	    (pdata->result_sets, resultset_name, &orig_key, &value)) {
#ifdef GDA_DEBUG_NO
		g_print ("resultset found [%p]\n", value);
#endif
		result = (GObject *) value;
	}
	else {
#ifdef GDA_DEBUG_NO
		g_print ("no resultset found\n");
#endif
		g_set_error (error, GDA_XSLT_ERROR, GDA_XSLT_GENERAL_ERROR,
			     "no resultset found for name [%s]\n",
			     resultset_name);
		*nodeset = NULL;
		return -1;
	}
// Dont check if this is a Data Model, because saving the result we alreally check
	res = _utility_data_model_to_nodeset (GDA_DATA_MODEL (result),
					      nodeset, error);
	if (res < 0) {
#ifdef GDA_DEBUG_NO
		g_print ("_utility_data_model_to_nodeset fault");
#endif
		*nodeset = NULL;
		return -1;
	}
	return 0;
}

/* -------------------------------------------------- */
static int
_utility_data_model_to_nodeset (GdaDataModel * model,
				xmlXPathObjectPtr * nodeset, GError ** error)
{
	gint nrows, i;
	gint *rcols, rnb_cols;
	gchar **col_ids = NULL;
	gint c;

	xmlNodePtr mainnode;
	mainnode = xmlNewNode (NULL, (xmlChar *) "resultset");
	if (mainnode == NULL) {
#ifdef GDA_DEBUG_NO
		g_print ("xmlNewNode return NULL\n");
#endif
		g_set_error (error, GDA_XSLT_ERROR, GDA_XSLT_GENERAL_ERROR, "%s", "xmlNewNode return NULL\n");
		return -1;
	}
	/* compute columns */
	rnb_cols = gda_data_model_get_n_columns (model);
	rcols = g_new (gint, rnb_cols);
	for (i = 0; i < rnb_cols; i++)
		rcols[i] = i;

	col_ids = g_new0 (gchar *, rnb_cols);
	for (c = 0; c < rnb_cols; c++) {
		GdaColumn *column;
		const gchar *name;
		column = gda_data_model_describe_column (model, rcols[c]);
		name = gda_column_get_name (column);
		if (name)
			col_ids[c] = g_strdup (name);
		else
			col_ids[c] = g_strdup_printf ("_%d", c);
	}

	/* add the model data to the XML output */
	nrows = gda_data_model_get_n_rows (model);
	if (nrows > 0) {
		xmlNodePtr row, field;
		gint r, c;
		for (r = 0; r < nrows; r++) {
			row = xmlNewChild (mainnode, NULL, (xmlChar *) "row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GValue *value;
				xmlChar *str = NULL;
				gboolean isnull = FALSE;

				value = (GValue *) gda_data_model_get_value_at (model, rcols[c], r, error);
				if (!value) {
					for (c = 0; c < rnb_cols; c++)
						g_free (col_ids[c]);
					g_free (col_ids);
					g_free (rcols);
					return -1;
				}
				if (gda_value_is_null ((GValue *) value))
					isnull = TRUE;
				else 
					str = value_to_xmlchar (value);
				field = xmlNewTextChild (row, NULL, (xmlChar *) "column", (xmlChar *) str);
				xmlSetProp (field, (xmlChar *) "name", (xmlChar *) col_ids[c]);
				if (isnull)
					xmlSetProp (field, (xmlChar *) "isnull", (xmlChar *) "true");
				g_free (str);
			}
		}
	}
	for (c = 0; c < rnb_cols; c++)
		g_free (col_ids[c]);
	g_free (col_ids);
	g_free (rcols);

	*nodeset = (xmlXPathObjectPtr) xmlXPathNewNodeSet (mainnode);

	return 0;
}

static void
look_predefined_query_by_name (GdaXsltExCont * exec, const char *query_name,
			       GdaStatement ** query)
{
	gpointer orig_key = NULL;
	gpointer value = NULL;
#ifdef GDA_DEBUG_NO
	g_print ("searching predefined query name[%s]", query_name);
#endif

	if (g_hash_table_lookup_extended
	    (exec->query_hash, query_name, &orig_key, &value)) {
#ifdef GDA_DEBUG_NO
		g_print ("query found [%p]", value);
#endif
		*query = (GdaStatement *) value;
	}
	else {
#ifdef GDA_DEBUG_NO
		g_print ("no predefined query found");
#endif
		*query = NULL;
	}
}

static int
gda_xslt_bk_internal_query (GdaXsltExCont * exec, GdaXsltIntCont * pdata,
			    xsltTransformContextPtr ctxt,
			    xmlNodePtr query_node)
{
	GdaStatement *query = NULL;
	GdaSet *params;
	int ret = 0;
	GdaDataModel *resQuery;
	GSList *plist;
	int predefined = 0;

	xmlNodePtr sqltxt_node = NULL;
	xmlChar *query_name;

#ifdef GDA_DEBUG_NO
	g_print ("running function:gda_xslt_bk_internal_query \n");
#endif

	query_name = xmlGetProp (query_node, (const xmlChar *) "name");
	if (query_name == NULL) {
		g_set_error (&(exec->error), 0, 0,
			     "%s", "the query element is not correct, no 'name' attribute\n");
		return -1;
	}

	look_predefined_query_by_name (exec, (gchar*) query_name, &(query));
	if (!query) {
		/* the query is not predefined */
		/* get the xml text node with the sql */
		sqltxt_node = query_node->children;
		if (sqltxt_node == NULL || sqltxt_node->type != XML_TEXT_NODE) {
			g_set_error (&(exec->error), 0, 0,
				     "%s", "the query element is not correct, it have not a first text children\n");
			return -1;
		}
#ifdef GDA_DEBUG_NO
		printf ("query_content[%s]\n", XML_GET_CONTENT (sqltxt_node));
#endif
		/* create the query */
		GdaSqlParser *parser;
		parser = gda_connection_create_parser (exec->cnc);
		query = gda_sql_parser_parse_string (parser, (gchar*) XML_GET_CONTENT (sqltxt_node), NULL, &(exec->error));
		g_object_unref (parser);
		if (!query) {
#ifdef GDA_DEBUG_NO
			g_print ("gda_query_new_from_sql:error [%s]\n",
				 exec->error && exec->error->message ? exec->error->
				 message : "No detail");
#endif
			return -1;
		}
	}
	else {
		/* the query is predefined */
		predefined = 1;
	}

	/* find the parameters on xsltcontext */
	if (! gda_statement_get_parameters (query, &params, &(exec->error)))
		return -1;

	if (params != NULL) {
		plist = gda_set_get_holders (params);
		while (plist && ret == 0) {
			ret = _gda_xslt_holder_set_value (GDA_HOLDER (plist->data), ctxt);
			plist = g_slist_next (plist);
		}
	}

	/* run the query */
	resQuery = gda_connection_statement_execute_select (exec->cnc, query, params, &(exec->error));
	if (!resQuery) {
#ifdef GDA_DEBUG_NO
		g_print ("gda_query_execute:error [%s]\n",
			 exec->error
			 && exec->error->message ? exec->error->
			 message : "No detail");
#endif
		return -1;
	}

	/* save the result to the internal context */
	set_resultset_value (pdata, (const char *) query_name, (GObject*) resQuery, &(exec->error));

	/* free the parameters */
	if (params)
		g_object_unref (params);
	/* free the query if not predefined */
	if (!predefined && query)
		g_object_unref (query);
	xmlFree (query_name);
	return 0;
}


/* from libgda */
static gboolean
gtype_equal (gconstpointer a, gconstpointer b)
{
	return (GType) a == (GType) b ? TRUE : FALSE;
}

static xmlChar *
value_to_xmlchar (const GValue * value)
{
	GdaDataHandler *dh;
	gchar *str;

	if (!value || gda_value_is_null (value))
		return (BAD_CAST "");
	else if ((G_VALUE_TYPE (value) == GDA_TYPE_BINARY) ||
		 (G_VALUE_TYPE (value) == GDA_TYPE_BLOB)) {
		TO_IMPLEMENT;
		return (BAD_CAST "Binary data");
	}
	if (!data_handlers) {
		/* initialize the internal data handlers */
		data_handlers =
			g_hash_table_new_full (g_direct_hash, gtype_equal,
					       NULL, (GDestroyNotify)
					       g_object_unref);

		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_INT64,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) G_TYPE_UINT64,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) G_TYPE_BOOLEAN,
				     gda_handler_boolean_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_DATE,
				     gda_handler_time_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) G_TYPE_DOUBLE,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_INT,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) GDA_TYPE_NUMERIC,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_FLOAT,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) GDA_TYPE_SHORT,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) GDA_TYPE_USHORT,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) G_TYPE_STRING,
				     gda_handler_string_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) GDA_TYPE_TIME,
				     gda_handler_time_new ());
		g_hash_table_insert (data_handlers,
				     (gpointer) G_TYPE_DATE_TIME,
				     gda_handler_time_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_CHAR,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_UCHAR,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_ULONG,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_LONG,
				     gda_handler_numerical_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_GTYPE,
				     gda_handler_type_new ());
		g_hash_table_insert (data_handlers, (gpointer) G_TYPE_UINT,
				     gda_handler_numerical_new ());
	}

	dh = g_hash_table_lookup (data_handlers,
				  (gpointer) G_VALUE_TYPE (value));
	if (dh)
		str = gda_data_handler_get_str_from_value (dh, value);
	else
		str = gda_value_stringify (value);
	return (BAD_CAST (str ? str : ""));
}
