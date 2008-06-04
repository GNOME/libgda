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
	file->doc = xmlNewDoc ("1.0");
	topnode = xmlNewDocNode (file->doc, NULL, "html", NULL);
	xmlDocSetRootElement (file->doc, topnode);

	/* head */
	head = xmlNewChild (topnode, NULL, "head", NULL);

	node = xmlNewChild (head, NULL, "meta", NULL);
	xmlSetProp(node, "content", (xmlChar*)"charset=ISO-8859-1");
	xmlSetProp(node, "http-equiv", (xmlChar*)"content-type");

	node = xmlNewChild (head, NULL, "title", title);
	node = xmlNewChild (head, NULL, "style", 
"body { "
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
"	padding: 3px;"
"}"
""
"tr {"
"	background:  #EFEEEF;"
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
"        font-size: large;"
"}"
""
".warning {"
"        color: #ff9900;"
"        font: bold;"
"        font-size: medium;"
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
	node = xmlNewChild (topnode, NULL, "body", NULL);
	file->body = node;

	/* title */
	node = xmlNewChild (file->body, NULL, "h1", title);
	xmlSetProp(node, "class", (xmlChar*)"title");

	/* toc */
	file->toc = xmlNewChild (file->body, NULL, "ul", _("Table of contents"));
	xmlSetProp(file->toc, "class", (xmlChar*)"none");

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
	xmlNodeSetContent (href, tmp);
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
		xmlSetProp(href, (xmlChar*)"href", tmp);
		g_free (tmp);
	}
	else
		xmlSetProp(href, (xmlChar*)"href", link_to);
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
html_add_to_toc (HtmlConfig *config, HtmlFile *file, const gchar *text, const gchar *link_to)
{
	xmlNodePtr li;

	li = xmlNewChild (file->toc, NULL, "li", NULL);
	real_html_add_link_to_node (li, text, link_to);
}

xmlNodePtr
html_add_header (HtmlConfig *config, HtmlFile *file, const gchar *text)
{
	xmlNodePtr hnode, ntmp;
	gchar *tmp;
	
	hnode = xmlNewChild (file->body, NULL, "h2", text);
	tmp = g_strdup_printf ("/a/%d", counter++);
	html_add_to_toc (config, file, text, tmp);
	ntmp = xmlNewChild (hnode, NULL, "a", "");
	xmlSetProp(ntmp, (xmlChar*)"name", tmp);
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
html_mark_node_error (HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, "class", (xmlChar*)"error");
}

void
html_mark_node_warning (HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, "class", (xmlChar*)"warning");
}

void
html_mark_node_notice (HtmlConfig *config, xmlNodePtr node)
{
	xmlSetProp(node, "class", (xmlChar*)"notice");
}

xmlNodePtr
html_render_attribute_str (xmlNodePtr parent, const gchar *node_type, 
		      const gchar *att_name, const gchar *att_val)
{
	xmlNodePtr node;
	gchar *tmp;

	tmp = g_strdup_printf ("%s = %s", att_name, att_val);
	node = xmlNewChild (parent, NULL, node_type, tmp);
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
	node = xmlNewChild (parent, NULL, node_type, tmp);
	g_free (tmp);

	return node;
}

xmlNodePtr
html_render_data_model (xmlNodePtr parent, GdaDataModel *model)
{
	xmlNodePtr node, tr, td;
        gint rows, cols, i;

        g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

        node = xmlNewChild (parent, NULL, "table", "");

	cols = gda_data_model_get_n_columns (model);
	rows = gda_data_model_get_n_rows (model);

        /* set the table structure */
	tr = xmlNewChild (node, NULL, "tr", NULL);
        for (i = 0; i < cols; i++) {
                GdaColumn *column;

                column = gda_data_model_describe_column (model, i);
                if (!column) {
                        xmlFreeNode (node);
                        return NULL;
                }

                td = xmlNewChild (tr, NULL, "th", gda_column_get_name (column));
        }
	
	/* add the model data to the XML output */
        if (rows > 0) {
                gint r, c;

                for (r = 0; r < rows; r++) {
			tr = xmlNewChild (node, NULL, "tr", "");
                        for (c = 0 ; c < cols; c++) {
                                GValue *value;
                                gchar *str;

                                value = (GValue *) gda_data_model_get_value_at (model, c, r);
				if (!value || gda_value_is_null (value)) {
					xmlNodePtr p;
					td = xmlNewChild (tr, NULL, "td", NULL);
					p = xmlNewChild (td, NULL, "p", "NULL");
					xmlSetProp(p, "class", (xmlChar*)"null");
				}
				else {
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else
						str = gda_value_stringify (value);
					td = xmlNewChild (tr, NULL, "td", str);
					g_free (str);
				}
                        }
                }
        }

        return node;
}
