/*
 * Copyright (C) 2006 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "html.h"
#include <glib/gi18n-lib.h>

static gint counter = 0;

void
html_init_config (HtmlConfig *config)
{
	g_assert (config);

	config->nodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

HtmlFile *
html_file_new (HtmlConfig *config, const gchar *name, const gchar *title)
{
	HtmlFile *file;
	xmlNodePtr topnode, head, node;

	file = g_new0 (HtmlFile, 1);
	file->name = g_strdup (name);
	file->doc = xmlNewDoc (BAD_CAST "1.0");
	topnode = xmlNewDocNode (file->doc, NULL, BAD_CAST "html", NULL);
	xmlDocSetRootElement (file->doc, topnode);

	/* head */
	head = xmlNewChild (topnode, NULL, BAD_CAST "head", NULL);

	node = xmlNewChild (head, NULL, BAD_CAST "meta", NULL);
	xmlSetProp(node, BAD_CAST "content", BAD_CAST "charset=UTF-8");
	xmlSetProp(node, BAD_CAST "http-equiv", BAD_CAST "content-type");

	node = xmlNewChild (head, NULL, BAD_CAST "title", BAD_CAST title);
	node = xmlNewChild (head, NULL, BAD_CAST "style", 
BAD_CAST "body { "
"       margin: 0px; padding: 0px;  border:0px; "
"	font: 8pt/16pt georgia; "
"	color: #555753; "
"	margin: 5px; "
"	}"
""
"a:visited, a:link { "
"        text-decoration: none ; color: #4085cd; "
"}"
""
"a:hover { "
"        text-decoration: none; color: #FF0000;  "
"}"
""
"p { "
"	font: 8pt/16pt georgia; "
"	margin-top: 0px; "
"	text-align: justify;"
"	}"
"h2 { "
"	font: italic normal 14pt georgia; "
"	letter-spacing: 1px; "
"	margin-bottom: 0px; "
"	color: #7D775C;"
"	}"
""
"table {"
"	font-size: 8pt;"
"	/*border: 1pt solid #A8E775;*/"
"	padding: 10px;"
"}"
""
"tr {"
"	background:  #EFEFEF;"
"}"
""
"th {"
"	font-size: 12pt;"
"}"
""
".none {"
"        list-style : none;"
"	padding-left: 0px;"
"}"
""
".error {"
"        color: #FF0000;"
"        font: bold;"
"        /*font-size: large;*/"
"}"
""
".warning {"
"        color: #ff9900;"
"        font: bold;"
"        /*font-size: medium;*/"
"}"
""
".notice {"
"        color: #22bb00;"
"        font: bold;"
"        /*font-size: medium;*/"
"}"
""
"#inactive {"
"        background-color: #CCCCFF;"
"}"
""
".null {"
"       color: lightblue;"
"}"
);

	/* body */
	node = xmlNewChild (topnode, NULL, BAD_CAST "body", NULL);
	file->body = node;

	/* title */
	node = xmlNewChild (file->body, NULL, BAD_CAST "h1", BAD_CAST title);
	xmlSetProp(node, BAD_CAST "class", BAD_CAST "title");

#ifdef NO
	/* toc */
	file->toc = xmlNewChild (file->body, NULL, "ul", _("Table of contents"));
	xmlSetProp(file->toc, "class", (xmlChar*)"none");
#endif

	/* add to @config's list of files */
	config->all_files = g_slist_append (config->all_files, file);

	return file;
}

gboolean
html_file_write (HtmlFile *file, HtmlConfig *config)
{
	gchar *str;

	str = g_strdup_printf ("%s/%s", config->dir, file->name);
	gint i;
	i = xmlSaveFormatFile (str, file->doc, TRUE);
	if (i == -1) 
		g_warning (_("Error writing XML file %s"), str);
	g_free (str);

	return i!=-1;
}

void
html_file_free  (HtmlFile *file)
{
	xmlFreeDoc (file->doc);
	g_free (file->name);
	g_free (file);
}

void
html_declare_node (HtmlConfig *config, const gchar *path, xmlNodePtr node)
{
	html_declare_node_own (config, g_strdup (path), node);
}

void
html_declare_node_own (HtmlConfig *config, gchar *path, xmlNodePtr node)
{
	xmlNodePtr enode;

	enode = g_hash_table_lookup (config->nodes, path);
	if (enode) {
		g_warning ("Node path %s is already attributed", path);
		g_free (path);
		return;
	}
	
	g_hash_table_insert (config->nodes, path, node);
	/*g_print ("--- Added node @ %s\n", path);*/
}

/*
 * if @link_to starts with a '/' then a # is prepended (internal link)
 */
void
real_html_add_link_to_node (xmlNodePtr node, const gchar *text, const gchar *link_to)
{
	xmlNodePtr href;
	gchar *tmp;

	href = xmlNewNode (NULL, (xmlChar*)"a");
	tmp = g_strdup_printf (" [%s] ", text);
	xmlNodeSetContent (href, BAD_CAST tmp);
	g_free (tmp);
	if (node->children) {
		xmlNodePtr sibl;

		sibl = node->children;
		while (sibl && xmlNodeIsText (sibl))
			sibl = sibl->next;
		if (sibl)
			xmlAddPrevSibling (sibl, href);
		else
			xmlAddChild (node, href);	
	}
	else
		xmlAddChild (node, href);
	if (*link_to == '/') {
		tmp = g_strdup_printf ("#%s", link_to);
		xmlSetProp(href, BAD_CAST "href", BAD_CAST tmp);
		g_free (tmp);
	}
	else
		xmlSetProp(href, BAD_CAST "href", BAD_CAST link_to);
}

void
html_add_link_to_node (HtmlConfig *config, const gchar *nodepath, const gchar *text, const gchar *link_to)
{
	xmlNodePtr node;

	node = g_hash_table_lookup (config->nodes, nodepath);
	if (!node) {
		g_warning ("Can't link non existant node '%s'", nodepath);
		return;
	}

	real_html_add_link_to_node (node, text, link_to);
}

void
html_add_to_toc (G_GNUC_UNUSED HtmlConfig *config, HtmlFile *file, const gchar *text, const gchar *link_to)
{
	xmlNodePtr li;

	li = xmlNewChild (file->toc, NULL, BAD_CAST "li", NULL);
	real_html_add_link_to_node (li, text, link_to);
}

xmlNodePtr
html_add_header (HtmlConfig *config, HtmlFile *file, const gchar *text)
{
	xmlNodePtr hnode, ntmp;
	gchar *tmp;
	
	hnode = xmlNewChild (file->body, NULL, BAD_CAST "h2", BAD_CAST text);
	tmp = g_strdup_printf ("/a/%d", counter++);
	html_add_to_toc (config, file, text, tmp);
	ntmp = xmlNewChild (hnode, NULL, BAD_CAST "a", BAD_CAST "");
	xmlSetProp(ntmp, BAD_CAST "name", BAD_CAST tmp);
	g_free (tmp);
	
	return hnode;
}

void
html_mark_path_error (HtmlConfig *config, const gchar *nodepath)
{
	xmlNodePtr node;

	node = g_hash_table_lookup (config->nodes, nodepath);
	if (!node) {
		g_warning ("Can't link non existant node '%s'", nodepath);
		return;
	}
	html_mark_node_error (config, node);
}

void
html_mark_node_error (G_GNUC_UNUSED HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, BAD_CAST "class", BAD_CAST "error");
}

void
html_mark_node_warning (G_GNUC_UNUSED HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, BAD_CAST "class", BAD_CAST "warning");
}

void
html_mark_node_notice (G_GNUC_UNUSED HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, BAD_CAST "class", BAD_CAST "notice");
}

xmlNodePtr
html_render_attribute_str (xmlNodePtr parent, const gchar *node_type, 
		      const gchar *att_name, const gchar *att_val)
{
	xmlNodePtr node;
	gchar *tmp;

	tmp = g_strdup_printf ("%s = %s", att_name, att_val);
	node = xmlNewChild (parent, NULL, BAD_CAST node_type, BAD_CAST tmp);
	g_free (tmp);

	return node;
}

xmlNodePtr
html_render_attribute_bool (xmlNodePtr parent, const gchar *node_type, 
			    const gchar *att_name, gboolean att_val)
{
	xmlNodePtr node;
	gchar *tmp;

	tmp = g_strdup_printf ("%s = %s", att_name, att_val ? _("Yes") : _("No"));
	node = xmlNewChild (parent, NULL, BAD_CAST node_type, BAD_CAST tmp);
	g_free (tmp);

	return node;
}

xmlNodePtr
html_render_data_model (xmlNodePtr parent, GdaDataModel *model)
{
	xmlNodePtr node, tr, td;
        gint rows, cols, i;

        g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

        node = xmlNewChild (parent, NULL, BAD_CAST "table", BAD_CAST "");

	cols = gda_data_model_get_n_columns (model);
	rows = gda_data_model_get_n_rows (model);

        /* set the table structure */
	tr = xmlNewChild (node, NULL, BAD_CAST "tr", NULL);
        for (i = 0; i < cols; i++) {
                GdaColumn *column;

                column = gda_data_model_describe_column (model, i);
                if (!column) {
                        xmlFreeNode (node);
                        return NULL;
                }

                td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST gda_column_get_name (column));
        }
	
	/* add the model data to the XML output */
        if (rows > 0) {
                gint r, c;

                for (r = 0; r < rows; r++) {
			tr = xmlNewChild (node, NULL, BAD_CAST "tr", BAD_CAST "");
                        for (c = 0 ; c < cols; c++) {
                                GValue *value;

                                value = (GValue *) gda_data_model_get_value_at (model, c, r, NULL);
				if (!value) {
					xmlNodePtr p;
					td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
					p = xmlNewChild (td, NULL, BAD_CAST "p", BAD_CAST "ERROR");
					xmlSetProp(p, BAD_CAST "class", BAD_CAST "null");
				}
				else if (gda_value_is_null (value)) {
					xmlNodePtr p;
					td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
					p = xmlNewChild (td, NULL, BAD_CAST "p", BAD_CAST "NULL");
					xmlSetProp(p, BAD_CAST "class", BAD_CAST "null");
				}
				else {
					gchar *str;
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else
						str = gda_value_stringify (value);
					td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST str);
					g_free (str);
				}
                        }
                }
        }

        return node;
}
