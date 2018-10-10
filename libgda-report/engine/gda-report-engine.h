/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_REPORT_ENGINE_H__
#define __GDA_REPORT_ENGINE_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-statement.h>

G_BEGIN_DECLS


/* error reporting */
extern GQuark gda_report_engine_error_quark (void);
#define GDA_REPORT_ENGINE_ERROR gda_report_engine_error_quark ()

typedef enum {
	GDA_REPORT_ENGINE_GENERAL_ERROR
} GdaReportEngineError;


#define GDA_TYPE_REPORT_ENGINE            (gda_report_engine_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaReportEngine, gda_report_engine, GDA, REPORT_ENGINE, GObject)

struct _GdaReportEngineClass {
	GObjectClass            parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-report-engine
 * @short_description: Low level report generator based on XML
 * @title: GdaReportEngine
 * @stability: Stable
 * @see_also:
 *
 * The #GdaReportEngine object generates a report based on a specifications file in any XML format. It browses
 * through the XML file and replaces the parts of it which are &lt;gda_report...&gt; action nodes. The generated
 * XML file may then need to be processed using other post-processor tools depending on the XML file format.
 *
 * The following "action" tags known are:
 * <table frame="all">
 *   <tgroup cols="4" colsep="1" rowsep="1" align="justify">
 *     <thead>
 *       <row>
 *         <entry>Node name</entry>
 *         <entry>Description</entry>
 *         <entry>Attributes</entry>
 *         <entry>&lt;gda_report...&gt; sub nodes</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>&lt;gda_report_section&gt;</entry>
 *         <entry>
 *	    <para>Starts a section which runs a SELECT query to generate a data model (#GdaDataModel).
 *	    </para>
 *	    <para>
 *	      A parameter named &quot;&lt;query_name&gt;|?nrows&quot; is created and is available in any sub node
 *	      of the &lt;gda_report_section&gt; node, which contains the number of rows in the section's data model.
 *	    </para>
 *	  </entry>
 *         <entry>
 *	    <itemizedlist>
 *	      <listitem><para>"query_name": the name of a SELECT #GdaStatement to be run; using this attribute
 *		  implies that a GdaStatement has
 *		  already been created and that the query has been declared to the GdaReportEngine object
 *		  with the "query_name "name. 
 *		  To define a query within the report spec., add a &lt;gda_report_query&gt;
 *		  sub node instead
 *	      </para></listitem>
 *	      <listitem><para>"cnc_name": name of the connection to use (the #GdaConnection object has
 *		  already been created and has been declared to the GdaReportEngine object with the "cnc_name" name)
 *	      </para></listitem>
 *	    </itemizedlist>
 *	  </entry>
 *         <entry>
 *	    <itemizedlist>
 *	      <listitem><para>&lt;gda_report_query&gt; to define the SELECT query which will be run. It is also
 *	      possible to use a named query using the "query_name" attribute of the &lt;gda_report_section&gt; node
 *	      </para></listitem>
 *	      <listitem><para>&lt;gda_report_if_empty_section&gt; to define the contents by which the &lt;gda_report_section&gt; 
 *		  node is replaced if the data model for the section contains no row.
 *	      </para></listitem>
 *	    </itemizedlist>
 *	  </entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_iter&gt;</entry>
 *         <entry>
 *	    <para>Repeats is list of children nodes for each row in the section's data model. Note that is that data
 *	      model has no row, then the contents of the &lt;gda_report_iter&gt; node will not appear at all in the
 *	      final result.
 *	    </para>
 *	    <para>
 *	      For each column of the data model, several parameters are created, named 
 *	      &lt;query_name&gt;|@&lt;column_name&gt; (such as &quot;customers|@@name&quot; for the &quot;name&quot; column)
 *	      and &lt;query_name&gt;|#&lt;column_number&gt;,
 *	      (such as &quot;customers|##0&quot; for the first column). 
 *	      Those parameters can be used in any child node (recursively),
 *	      and have their value updated for each row of the data model.
 *	      For more information about the parameter's syntax, 
 *	      see the <link linkend="SQL_queries">GDA SQL query syntax</link>.
 *	    </para>
 *	  </entry>
 *         <entry/>
 *         <entry/>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_query&gt;</entry>
 *         <entry>Specifies the SQL of the SELECT query in a &lt;gda_report_section&gt; section. The SQL should be
 *	  in the contents of that node</entry>
 *         <entry><itemizedlist>
 *	      <listitem><para>"query_name": name of the query</para></listitem>
 *	      <listitem><para>"cnc_name": name of the connection to use (the #GdaConnection object has
 *		  already been created and has been declared to the GdaReportEngine object with the "cnc_name" name)
 *	      </para></listitem>
 *	    </itemizedlist>
 *	  </entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_param_value&gt;</entry>
 *         <entry>Replace the node with the value of a parameter. The parameter can either by defined globally
 *	  (and declared to the GdaReportEngine), or defined as part of a section</entry>
 *         <entry>
 *	    <itemizedlist>
 *	      <listitem><para>"param_name" specifies the name of the parameter</para></listitem>
 *	      <listitem><para>"converter" optionnally specifies a conversion to apply to the parameter's
 *		  contents (for now only "richtext::docbook" to convert text
 *		  in <ulink url="http://txt2tags.org/">rich text format</ulink> to
 *		  the DocBook syntax)</para></listitem>
 *	    </itemizedlist>
 *	  </entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_expr&gt;</entry>
 *         <entry>Replace the node with the evaluation of an expression. The expression can be any SQLite valid expression,
 *	  and can contain any reference to any parameter. For example the node:
 *	    <programlisting><![CDATA[
 *<gda_report_expr>
 *##customers|@col IS NULL
 *</gda_report_expr>]]></programlisting>
 *	    will return a TRUE value if the parameter named &quot;customers|@@col&quot; is not NULL. For more
 *	    information about the parameter's syntax, see the <link linkend="SQL_queries">GDA SQL query syntax</link>.
 *	  </entry>
 *         <entry></entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_if&gt;</entry>
 *         <entry>Creates an "IF THEN ELSE" flow control, based on the evaluation of an expression (in the same way as for the
 *	  &lt;gda_report_expr&gt; node). If the expression evaluates as TRUE, then the &lt;gda_report_if&gt; node will
 *	  be replaced by the children nodes of the &lt;gda_report_if_true&gt; node if it exists, and otherwise by 
 *	    the children nodes of the &lt;gda_report_if_false&gt; node if it exists.</entry>
 *         <entry>"expr" to define the expression to evaluate</entry>
 *         <entry>&lt;gda_report_if_true&gt; and &lt;gda_report_if_false&gt;</entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_if_true&gt;</entry>
 *         <entry>Defines the contents to use when the expression of a &lt;gda_report_if&gt; is evaluated as TRUE</entry>
 *         <entry></entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>&lt;gda_report_if_false&gt;</entry>
 *         <entry>Defines the contents to use when the expression of a &lt;gda_report_if&gt; is evaluated as FALSE</entry>
 *         <entry></entry>
 *         <entry></entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </table>
 */

GdaReportEngine *gda_report_engine_new             (xmlNodePtr spec_node);
GdaReportEngine *gda_report_engine_new_from_string (const gchar *spec_string);
GdaReportEngine *gda_report_engine_new_from_file   (const gchar *spec_file_name);

void             gda_report_engine_declare_object  (GdaReportEngine *engine, GObject *object, const gchar *obj_name);
GObject         *gda_report_engine_find_declared_object (GdaReportEngine *engine, GType obj_type, const gchar *obj_name);

xmlNodePtr       gda_report_engine_run_as_node     (GdaReportEngine *engine, GError **error);
xmlDocPtr        gda_report_engine_run_as_doc      (GdaReportEngine *engine, GError **error);

G_END_DECLS

#endif
