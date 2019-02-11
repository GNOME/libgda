/*
 * Copyright (C) 2012 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "base-tool-defines.h"
#include "base-tool-output.h"
#include "base-tool-help.h"
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#ifndef G_OS_WIN32
#include <signal.h>
typedef void (*sighandler_t)(int);
#include <pwd.h>
#else
#include <stdlib.h>
#include <windows.h>
#endif

static GdaSet *
make_options_set_from_string (const gchar *context, GdaSet *options)
{
        GdaSet *expopt = NULL;
        GSList *list, *nlist = NULL;
	if (options) {
		for (list = gda_set_get_holders (options); list; list = list->next) {
			GdaHolder *param = GDA_HOLDER (list->data);
			gchar *val;
			val = g_object_get_data ((GObject*) param, context);
			if (!val)
				continue;

			GdaHolder *nparam;
			const GValue *cvalue2;
			cvalue2 = gda_holder_get_value (param);
			nparam = gda_holder_new (G_VALUE_TYPE (cvalue2), val);
			g_assert (gda_holder_set_value (nparam, cvalue2, NULL));
			nlist = g_slist_append (nlist, nparam);
		}
		if (nlist) {
			expopt = gda_set_new (nlist);
			g_slist_free (nlist);
		}
	}
        return expopt;
}

/**
 * base_tool_output_data_model_to_string:
 */
gchar *
base_tool_output_data_model_to_string (GdaDataModel *model, ToolOutputFormat format, FILE *stream, GdaSet *options)
{
	if (!GDA_IS_DATA_MODEL (model))
		return NULL;

	if (format & BASE_TOOL_OUTPUT_FORMAT_DEFAULT) {
		gchar *tmp;
		GdaSet *local_options;
		gint width;
		base_tool_input_get_size (&width, NULL);
		local_options = gda_set_new_inline (6, "NAME", G_TYPE_BOOLEAN, TRUE,
						    "NULL_AS_EMPTY", G_TYPE_BOOLEAN, TRUE,
						    "MAX_WIDTH", G_TYPE_INT, width,
						    "COLUMN_SEPARATORS", G_TYPE_BOOLEAN, TRUE,
						    "SEPARATOR_LINE", G_TYPE_BOOLEAN, TRUE,
						    "NAMES_ON_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
		if (options)
			gda_set_merge_with_set (local_options, options);
		tmp = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_TABLE, NULL, 0, NULL, 0,
						       local_options);
		g_object_unref (local_options);
		if (GDA_IS_DATA_SELECT (model)) {
			gchar *tmp2, *tmp3;
			gdouble etime;
			g_object_get ((GObject*) model, "execution-delay", &etime, NULL);
			tmp2 = g_strdup_printf ("%s: %.03f s", _("Execution delay"), etime);
			tmp3 = g_strdup_printf ("%s\n%s", tmp, tmp2);
			g_free (tmp);
			g_free (tmp2);
			return tmp3;
		}
		else
			return tmp;
	}
	else if (format & BASE_TOOL_OUTPUT_FORMAT_XML)
		return gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							NULL, 0,
							NULL, 0, NULL);
	else if (format & BASE_TOOL_OUTPUT_FORMAT_CSV) {
		gchar *retval;
		GdaSet *optexp;
		optexp = make_options_set_from_string ("csv", options);
		retval = gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
							  NULL, 0,
							  NULL, 0, optexp);
		if (optexp)
			g_object_unref (optexp);
		return retval;
	}
	else if (format & BASE_TOOL_OUTPUT_FORMAT_HTML) {
		xmlBufferPtr buffer;
		xmlNodePtr top, div, table, node, row_node, col_node, header, meta;
		gint ncols, nrows, i, j;
		gchar *str;

		top = xmlNewNode (NULL, BAD_CAST "html");
		header = xmlNewChild (top, NULL, BAD_CAST "head", NULL);
		meta = xmlNewChild (header, NULL, BAD_CAST "meta", NULL);
		xmlSetProp (meta, BAD_CAST "http-equiv", BAD_CAST "content-type");
		xmlSetProp (meta, BAD_CAST "content", BAD_CAST "text/html; charset=UTF-8");
		div = xmlNewChild (top, NULL, BAD_CAST "body", NULL);
		table = xmlNewChild (div, NULL, BAD_CAST "table", NULL);
		xmlSetProp (table, BAD_CAST "border", BAD_CAST "1");
		
		if (g_object_get_data (G_OBJECT (model), "name"))
			xmlNewTextChild (table, NULL, BAD_CAST "caption", g_object_get_data (G_OBJECT (model), "name"));

		ncols = gda_data_model_get_n_columns (model);
		nrows = gda_data_model_get_n_rows (model);

		row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
		for (j = 0; j < ncols; j++) {
			const gchar *cstr;
			cstr = gda_data_model_get_column_title (model, j);
			col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "th", BAD_CAST cstr);
			xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "center");
		}

		for (i = 0; i < nrows; i++) {
			row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
			xmlSetProp (row_node, BAD_CAST "valign", BAD_CAST "top");
			for (j = 0; j < ncols; j++) {
				const GValue *value;
				value = gda_data_model_get_value_at (model, j, i, NULL);
				if (!value) {
					col_node = xmlNewChild (row_node, NULL, BAD_CAST "td", BAD_CAST "ERROR");
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
				}
				else {
					str = gda_value_stringify (value);
					col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "td", BAD_CAST str);
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
					g_free (str);
				}
			}
		}

		node = xmlNewChild (div, NULL, BAD_CAST "p", NULL);
		str = g_strdup_printf (ngettext ("(%d row)", "(%d rows)", nrows), nrows);
		xmlNodeSetContent (node, BAD_CAST str);
		g_free (str);

		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, top, 0, 1);
		str = g_strdup ((gchar *) xmlBufferContent (buffer));
		xmlBufferFree (buffer);
		xmlFreeNode (top);
		return str;
	}
	else
		TO_IMPLEMENT;

	return NULL;
}


/*
 * REM: if @attr_names is not NULL, then @cols_size will have the same number of items
 *
 * - Optionally called once with @out_max_prefix_size not %NULL and @in_string %NULL => compute
 *   cols_size[x] and *out_max_prefix_size
 * - Called once with @in_string not %NULL and @out_max_prefix_size %NULL
 */
static void
tree_node_to_string (GdaTreeNode *node, gboolean has_parent, gboolean has_next_sibling,
		     const gchar *prefix,
		     gchar **attr_names, guint *cols_size, guint max_prefix_size,
		     guint *out_max_prefix_size, GString *in_string)
{
	gchar *pipe = "|";
	gchar *prefix2 = "|- ";
	gchar *prefix3 = "`- ";

	pipe = "│";
	prefix2 = "├─ ";
	prefix3 = "└─ ";
#define SEP "  "
	const GValue *cvalue;
	gchar *p;
	const gchar *cstr;
	guint i;

	/* prefix */
	if (has_next_sibling)
		p = g_strdup_printf ("%s%s", prefix, prefix2);
	else
		p = g_strdup_printf ("%s%s", prefix, prefix3);
	if (in_string)
		g_string_append (in_string, p);
	i = g_utf8_strlen (p, -1);
	g_free (p);

	/* node name */
	cvalue = gda_tree_node_get_node_attribute (node, GDA_ATTRIBUTE_NAME);
	cstr = cvalue && g_value_get_string (cvalue)? g_value_get_string (cvalue) : "???";
	if (in_string)
		g_string_append (in_string, cstr);

	/* padding */
	if (in_string) {
		for (i = i +  g_utf8_strlen (cstr, -1); i < max_prefix_size; i++)
			g_string_append_c (in_string, ' ');
	}
	else {
		guint size = i;
		if (g_utf8_validate (cstr, -1, NULL))
			size += g_utf8_strlen (cstr, -1);
		else
			size += strlen (cstr);
		*out_max_prefix_size = MAX (size, *out_max_prefix_size);
	}

	/* some node's attributes */
	if (attr_names) {
		for (i = 0; attr_names[i] && *attr_names[i]; i++) {
			guint colsize = 0;
			if (in_string) {
				if (cols_size [i] == 0)
					continue; /* ignore this attribute as it's not set */
				g_string_append (in_string, SEP);
			}

			cvalue = gda_tree_node_get_node_attribute (node, attr_names[i]);
			if (cvalue && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)) {
				gchar *tmp = NULL;
				if (G_VALUE_TYPE (cvalue) == G_TYPE_FLOAT)
					tmp = g_strdup_printf ("%.01f", g_value_get_float (cvalue));
				else {
					GdaDataHandler *dh;
					dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
					if (dh)
						tmp = gda_data_handler_get_str_from_value (dh, cvalue);
					else
						tmp = gda_value_stringify (cvalue);
				}
				if (in_string) {
					gboolean right = FALSE;
					if ((G_VALUE_TYPE (cvalue) == G_TYPE_INT) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_UINT) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_INT64) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_UINT64) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_FLOAT) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_DOUBLE) ||
					    (G_VALUE_TYPE (cvalue) == G_TYPE_CHAR) ||
					    (G_VALUE_TYPE (cvalue) == GDA_TYPE_SHORT) ||
					    (G_VALUE_TYPE (cvalue) == GDA_TYPE_USHORT))
						right = TRUE;

					if (right) {
						/* right align */
						guint j;
						for (j = tmp ? g_utf8_strlen (tmp, -1) : 0; j < cols_size [i]; j++)
							g_string_append_c (in_string, ' ');

						if (tmp) {
							if (g_utf8_strlen (tmp, -1) > cols_size[i])
								tmp [cols_size [i]] = 0;
							g_string_append (in_string, tmp);
						}
					}
					else {
						/* left align */
						if (tmp) {
							if (g_utf8_strlen (tmp, -1) > cols_size[i])
								tmp [cols_size [i]] = 0;
							g_string_append (in_string, tmp);
						}
						guint j;
						for (j = tmp ? g_utf8_strlen (tmp, -1) : 0; j < cols_size [i]; j++)
							g_string_append_c (in_string, ' ');
					}
				}
				else {
					if (tmp) {
						if (g_utf8_validate (tmp, -1, NULL))
							colsize += g_utf8_strlen (tmp, -1);
						else
							colsize += strlen (tmp);
					}
					cols_size [i] = MAX (cols_size [i], colsize);
				}

				g_free (tmp);
			}
			else if (in_string){
				guint j;
				for (j = 0; j < cols_size [i]; j++)
					g_string_append_c (in_string, ' ');
			}
		}
	}
	if (in_string)
		g_string_append_c (in_string, '\n');

	/* children */
	gchar *ch_prefix;
	if (has_next_sibling)
		ch_prefix = g_strdup_printf ("%s%s  ", prefix, pipe);
	else
		ch_prefix = g_strdup_printf ("%s   ", prefix);

	GSList *top, *list;
	top = gda_tree_node_get_children (node);
	for (list = top; list; list = list->next)
		tree_node_to_string (GDA_TREE_NODE (list->data), TRUE, list->next ? TRUE : FALSE,
				     ch_prefix, attr_names, cols_size, max_prefix_size,
				     out_max_prefix_size, in_string);

	g_slist_free (top);
	g_free (ch_prefix);
}

static gchar *
tree_to_string_default (GdaTree *tree, FILE *stream, GdaSet *options)
{
	GString *string;
	GSList *top, *list;
	gchar **attr_names = NULL;
	guint attr_names_size = 0;
	guint *cols_size = NULL;
	guint max_prefix_size = 0;

	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);

	if (options) {
		const GValue *cvalue;
		cvalue = gda_set_get_holder_value (options, "GDA_TREE_COLUMN_NAMES");
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING) && g_value_get_string (cvalue)) {
			attr_names = g_strsplit (g_value_get_string (cvalue), ",", 0);
			if (attr_names) {
				guint i;
				for (i = 0; attr_names [i]; i++)
					g_strstrip (attr_names [i]);
				attr_names_size = i;
			}
		}
	}

	string = g_string_new ("");
	top = gda_tree_get_nodes_in_path (tree, NULL, FALSE);
	if (attr_names) {
		/* determine prefix's max size and columns' max size */
		cols_size = g_new0 (guint, attr_names_size);
		for (list = top; list; list = list->next) {
			tree_node_to_string (GDA_TREE_NODE (list->data), FALSE, list->next ? TRUE : FALSE, "",
					     attr_names, cols_size, 0, &max_prefix_size, NULL);			
		}

		guint i, j;
		for (i = 0; attr_names [i]; i++) {
			guint size = 0;
			if (g_utf8_validate (attr_names[i], -1, NULL))
				size += g_utf8_strlen (attr_names[i], -1);
			else
				size += strlen (attr_names[i]);
			cols_size[i] = MAX (cols_size[i], size);
		}

		/* output column names */
		const gchar *title;
		title = (const gchar*) g_object_get_data ((GObject*) tree, "GDA_TREE_TITLE");
		if (title) {
			g_string_append (string, title);
			for (j = g_utf8_strlen (title, -1); j < max_prefix_size; j++)
				g_string_append_c (string, ' ');
		}
		else {
			for (j =  0; j < max_prefix_size; j++)
				g_string_append_c (string, ' ');
		}

		/* output column values */
		for (i = 0; attr_names [i]; i++) {
			if (cols_size[i] == 0)
				continue;
			g_string_append (string, SEP);
			g_string_append (string, attr_names[i]);
			guint size = 0;
			if (g_utf8_validate (attr_names[i], -1, NULL))
				size += g_utf8_strlen (attr_names[i], -1);
			else
				size += strlen (attr_names[i]);
			for (j =  size; j < cols_size[i]; j++)
				g_string_append_c (string, ' ');
		}
		g_string_append_c (string, '\n');
	}

	for (list = top; list; list = list->next)
		tree_node_to_string (GDA_TREE_NODE (list->data), FALSE, list->next ? TRUE : FALSE, "",
				     attr_names, cols_size, max_prefix_size, NULL, string);
	g_slist_free (top);

	if (attr_names) {
		g_strfreev (attr_names);
		g_free (cols_size);
	}

	return g_string_free (string, FALSE);
}

static gchar *
tree_to_string (GdaTree *tree, ToolOutputFormat format, FILE *stream, GdaSet *options)
{
	if (!GDA_IS_TREE (tree))
		return NULL;

	if (format & BASE_TOOL_OUTPUT_FORMAT_DEFAULT)
		return tree_to_string_default (tree, stream, options);
	else if (format & BASE_TOOL_OUTPUT_FORMAT_XML) {
		TO_IMPLEMENT;
	}
	else if (format & BASE_TOOL_OUTPUT_FORMAT_CSV) {
		TO_IMPLEMENT;
	}
	else if (format & BASE_TOOL_OUTPUT_FORMAT_HTML) {
		TO_IMPLEMENT;
	}
	else
		TO_IMPLEMENT;

	return NULL;
}


/**
 * base_tool_output_result_to_string:
 * @res: a #ToolCommandResult
 * @format: a #ToolOutputFormat format specification
 * @stream: (nullable): a stream which the returned string will be put to, or %NULL
 * @options: (nullable): a #GdaSet containing options, or %NULL
 *
 * Converts @res to a string
 *
 * Returns: (transfer full): a new string
 */
gchar *
base_tool_output_result_to_string (ToolCommandResult *res, ToolOutputFormat format,
				   FILE *stream, GdaSet *options)
{
	switch (res->type) {
	case BASE_TOOL_COMMAND_RESULT_DATA_MODEL:
		return base_tool_output_data_model_to_string (res->u.model, format, stream, options);

	case BASE_TOOL_COMMAND_RESULT_SET: {
		GSList *list;
		GString *string;
		xmlNodePtr node;
		xmlBufferPtr buffer;
		gchar *str;

		if (format & BASE_TOOL_OUTPUT_FORMAT_DEFAULT) {
			string = g_string_new ("");
			for (list = gda_set_get_holders (res->u.set); list; list = list->next) {
				const GValue *value;
				gchar *tmp;
				const gchar *cstr;
				GdaHolder *h;
				h = GDA_HOLDER (list->data);

				cstr = gda_holder_get_id (h);
				value = gda_holder_get_value (h);
				if (!strcmp (cstr, "IMPACTED_ROWS")) {
					g_string_append_printf (string, "%s: ",
								_("Number of rows impacted"));
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "%s", tmp);
					g_free (tmp);
				}
				else if (!strcmp (cstr, "EXEC_DELAY")) {
					g_string_append_printf (string, "%s: ",
								_("Execution delay"));
					gdouble etime;
					etime = g_value_get_double (value);
					g_string_append_printf (string, "%.03f s", etime);
				}
				else {
					tmp = g_markup_escape_text (cstr, -1);
					g_string_append_printf (string, "%s: ", tmp);
					g_free (tmp);
					
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "%s", tmp);
					g_free (tmp);
				}
				g_string_append (string, "\n");
			}
			str = string->str;
			g_string_free (string, FALSE);
			return str;
		}
		else if (format & BASE_TOOL_OUTPUT_FORMAT_XML) {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "parameters");
			for (list = gda_set_get_holders (res->u.set); list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "parameter");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		else if (format & BASE_TOOL_OUTPUT_FORMAT_HTML) {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "ul");
			for (list = gda_set_get_holders (res->u.set); list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "li");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		else if (format & BASE_TOOL_OUTPUT_FORMAT_CSV) {
			string = g_string_new ("");
			for (list = gda_set_get_holders (res->u.set); list; list = list->next) {
				const GValue *value;
				gchar *tmp;
				const gchar *cstr;
				GdaHolder *h;
				h = GDA_HOLDER (list->data);

				cstr = gda_holder_get_id (h);
				value = gda_holder_get_value (h);
				if (!strcmp (cstr, "IMPACTED_ROWS")) {
					g_string_append_printf (string, "\"%s\",",
								_("Number of rows impacted"));
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "\"%s\"", tmp);
					g_free (tmp);
				}
				else if (!strcmp (cstr, "EXEC_DELAY")) {
					g_string_append_printf (string, "\"%s\",",
								_("Execution delay"));
					gdouble etime;
					etime = g_value_get_double (value);
					g_string_append_printf (string, "\"%.03f s\"", etime);
				}
				else {
					tmp = g_markup_escape_text (cstr, -1);
					g_string_append_printf (string, "\"%s\",", tmp);
					g_free (tmp);
					
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "\"%s\"", tmp);
					g_free (tmp);
				}
				g_string_append (string, "\n");
			}
			str = string->str;
			g_string_free (string, FALSE);
			return str;
		}
		else {
			TO_IMPLEMENT;
			return NULL;
		}
	}

	case BASE_TOOL_COMMAND_RESULT_TREE: {
		GdaSet *options2, *merge = NULL;

		options2 = g_object_get_data ((GObject*) res->u.tree, "BASE_TOOL_OUTPUT_OPTIONS");
		if (options && options2) {
			merge = gda_set_copy (options2);
			gda_set_merge_with_set (merge, options);
		}
		gchar *tmp;
		tmp = tree_to_string (res->u.tree, format, stream, merge ? merge : (options ? options : options2));
		if (merge)
			g_object_unref (merge);
		return tmp;
	}

	case BASE_TOOL_COMMAND_RESULT_TXT: {
		xmlNodePtr node;
		xmlBufferPtr buffer;
		gchar *str;

		if ((format & BASE_TOOL_OUTPUT_FORMAT_DEFAULT) ||
		    (format & BASE_TOOL_OUTPUT_FORMAT_CSV))
			return g_strdup (res->u.txt->str);
		else if (format & BASE_TOOL_OUTPUT_FORMAT_XML) {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "txt");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		else if (format & BASE_TOOL_OUTPUT_FORMAT_HTML) {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "p");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		else {
			TO_IMPLEMENT;
			return NULL;
		}
	}

	case BASE_TOOL_COMMAND_RESULT_EMPTY:
		return g_strdup ("");

	case BASE_TOOL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		GString *string = NULL;
		gchar *str;

		for (list = res->u.multiple_results; list; list = list->next) {
			ToolCommandResult *tres = (ToolCommandResult*) list->data;
			gchar *tmp;
			
			tmp = base_tool_output_result_to_string (tres,
								 format & BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM ? TRUE : FALSE,
								 stream, options);
			if (!string)
				string = g_string_new (tmp);
			else {
				g_string_append_c (string, '\n');
				g_string_append (string, tmp);
			}
			g_free (tmp);
		}
		if (string) {
			str = string->str;
			g_string_free (string, FALSE);
		}
		else
			str = g_strdup ("");
		return str;
	}

	case BASE_TOOL_COMMAND_RESULT_HELP: {
		if (format & BASE_TOOL_OUTPUT_FORMAT_XML) {
			xmlBufferPtr buffer;
			gchar *str;
			buffer = xmlBufferCreate ();
			xmlNodeDump (buffer, NULL, res->u.xml_node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			return str;
		}
		else if (format & BASE_TOOL_OUTPUT_FORMAT_HTML) {
			TO_IMPLEMENT;
			return NULL;
		}
		else {
			gint width = -1;
			gboolean term_color;
			if (format & BASE_TOOL_OUTPUT_FORMAT_DEFAULT)
				base_tool_input_get_size (&width, NULL);
			term_color = format & BASE_TOOL_OUTPUT_FORMAT_COLOR_TERM ? TRUE : FALSE;
			return base_tool_help_to_string (res, width, term_color);
		}
		break;
	}

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/*
 * Check that the @arg string can safely be passed to a shell
 * to be executed, i.e. it does not contain dangerous things like "rm -rf *"
 */
// coverity[ +tainted_string_sanitize_content : arg-0 ]
static gboolean
check_shell_argument (const gchar *arg)
{
        const gchar *ptr;
        g_assert (arg);

        /* check for starting spaces */
        for (ptr = arg; *ptr == ' '; ptr++);
        if (!*ptr)
                return FALSE; /* only spaces is not allowed */

        /* check for the rest */
        for (; *ptr; ptr++) {
                if (! g_ascii_isalnum (*ptr) && (*ptr != G_DIR_SEPARATOR))
                        return FALSE;
        }
        return TRUE;
}

/**
 * base_tool_output_output_string:
 * @stream: (nullable): an outout stream, or %NULL
 * @str: a string
 *
 * "Outputs" @str to @stream, or to stdout if @stream is %NULL
 */
void
base_tool_output_output_string (FILE *stream, const gchar *str)
{
	FILE *to_stream;
        gboolean append_nl = FALSE;
        gint length;
        static gint force_no_pager = -1;

        if (!str)
                return;

        if (force_no_pager < 0) {
                /* still unset... */
                if (getenv (BASE_TOOL_NO_PAGER))
                        force_no_pager = 1;
                else
                        force_no_pager = 0;
        }

        length = strlen (str);
        if (*str && (str[length - 1] != '\n'))
                append_nl = TRUE;

        if (stream)
                to_stream = stream;
        else
                to_stream = stdout;

        if (!force_no_pager && isatty (fileno (to_stream))) {
                /* use pager */
                FILE *pipe;
                const char *pager;
#ifndef G_OS_WIN32
                sighandler_t phandler;
#endif
                pager = getenv ("PAGER");
                if (!pager)
                        pager = "more";
                if (!check_shell_argument (pager)) {
                        g_warning ("Invalid PAGER value: must only contain alphanumeric characters");
                        return;
                }
                else
                        pipe = popen (pager, "w");
#ifndef G_OS_WIN32
                phandler = signal (SIGPIPE, SIG_IGN);
#endif
                if (append_nl)
                        g_fprintf (pipe, "%s\n", str);
                else
                        g_fprintf (pipe, "%s", str);
                pclose (pipe);
#ifndef G_OS_WIN32
                signal (SIGPIPE, phandler);
#endif
        }
        else {
                if (append_nl)
                        g_fprintf (to_stream, "%s\n", str);
                else
                        g_fprintf (to_stream, "%s", str);
        }
}

/*
 * color output handling
 */
gchar *
base_tool_output_color_string (ToolOutputColor color, gboolean color_term, const char *fmt, ...)
{
	va_list argv;
	gchar *tmp, *res;

        va_start (argv, fmt);
	tmp = g_strdup_vprintf (fmt, argv);
        va_end (argv);
	res = g_strdup_printf ("%s%s%s", base_tool_output_color_s (color, color_term),
			       tmp,
			       base_tool_output_color_s (BASE_TOOL_COLOR_RESET, color_term));
	g_free (tmp);
	return res;
}

void
base_tool_output_color_append_string (ToolOutputColor color, gboolean color_term, GString *string, const char *fmt, ...)
{
	va_list argv;
	g_string_append (string, base_tool_output_color_s (color, color_term));
        va_start (argv, fmt);
	g_string_append_vprintf (string, fmt, argv);
        va_end (argv);
	g_string_append (string, base_tool_output_color_s (BASE_TOOL_COLOR_RESET, color_term));
}

void
base_tool_output_color_print (ToolOutputColor color, gboolean color_term, const char *fmt, ...)
{
	va_list argv;
	g_print ("%s", base_tool_output_color_s (color, color_term));
        va_start (argv, fmt);
	g_vprintf (fmt, argv);
        va_end (argv);
	g_print ("%s", base_tool_output_color_s (BASE_TOOL_COLOR_RESET, color_term));
}

const gchar *
base_tool_output_color_s (ToolOutputColor color, gboolean color_term)
{
#ifndef G_OS_WIN32
	if (color_term) {
		switch (color) {
		case BASE_TOOL_COLOR_NORMAL:
			return "";
		case BASE_TOOL_COLOR_RESET:
			return "\033[m";
		case BASE_TOOL_COLOR_BOLD:
			return "\033[1m";
		case BASE_TOOL_COLOR_RED:
			return "\033[31m";
		default:
			g_assert_not_reached();
		}
	}
#endif
	return "";
}
