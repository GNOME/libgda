#include "html-doc.h"
#include <glib/gi18n-lib.h>

HtmlDoc *
html_doc_new (const gchar *title)
{
	HtmlDoc *hdoc;
	xmlNodePtr topnode, head, node, div, container;

	hdoc = g_new0 (HtmlDoc, 1);
	hdoc->doc = xmlNewDoc ("1.0");
	topnode = xmlNewDocNode (hdoc->doc, NULL, "html", NULL);
	xmlDocSetRootElement (hdoc->doc, topnode);

	/* head */
	head = xmlNewChild (topnode, NULL, "head", NULL);

	node = xmlNewChild (head, NULL, "meta", NULL);
	xmlSetProp(node, "http-equiv", (xmlChar*)"Content-Type");
	xmlSetProp(node, "content", (xmlChar*)"text/html; charset=UTF-8");

	node = xmlNewChild (head, NULL, "meta", NULL);
	xmlSetProp(node, "http-equiv", (xmlChar*)"refresh");
	xmlSetProp(node, "content", (xmlChar*)"30"); /* refresh the page every 30 seconds */

	node = xmlNewChild (head, NULL, "title", title);
	node = xmlNewChild (head, NULL, "link", NULL);
	xmlSetProp(node, "href", (xmlChar*)"/gda.css");
	xmlSetProp(node, "rel", (xmlChar*)"stylesheet");
	xmlSetProp(node, "type", (xmlChar*)"text/css");

	/* body */
	node = xmlNewChild (topnode, NULL, "body", NULL);
	hdoc->body = node;

	/* container */
	container = xmlNewChild (hdoc->body, NULL, "div", NULL);
	xmlSetProp(container, "id", (xmlChar*)"container");

	/* top */
	div = xmlNewChild (container, NULL, "div", NULL);
	xmlSetProp (div, "id", (xmlChar*)"top");

	xmlNewChild (div, NULL, "h1", title);
	

	/* leftnav */
	div = xmlNewChild (container, NULL, "div", NULL);
	xmlSetProp(div, "id", (xmlChar*)"leftnav");
	hdoc->sidebar = div;

	/* content */
	div = xmlNewChild (container, NULL, "div", NULL);
	xmlSetProp(div, "id", (xmlChar*)"content");
	hdoc->content = div;

	/* footer */
	div = xmlNewChild (container, NULL, "div", NULL);
	xmlSetProp(div, "id", (xmlChar*)"footer");
	xmlNewChild (div, NULL, "p", NULL);

	return hdoc;
}

void
html_doc_free  (HtmlDoc *hdoc)
{
	xmlFreeDoc (hdoc->doc);
	g_free (hdoc);
}

xmlChar *
html_doc_to_string (HtmlDoc *hdoc, gsize *out_size)
{
	xmlChar *retval = NULL;
	int size;
	xmlNodePtr li, a, node;

	node = xmlNewChild (hdoc->sidebar, NULL, "ul", "Misc");
	li = xmlNewChild (node, NULL, "li", NULL);
	a = xmlNewChild (li, NULL, "a", _("Help"));
	xmlSetProp (a, "href", (xmlChar*)"/___help");

	xmlDocDumpFormatMemory (hdoc->doc, &retval, &size, 1);
	if (out_size)
		*out_size = (gsize) size;

	return retval;
}
