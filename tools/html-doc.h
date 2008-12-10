#include <glib.h>
#include <libxml/tree.h>

typedef struct {
	xmlDocPtr   doc;
	xmlNodePtr  body;

	xmlNodePtr  sidebar;
	xmlNodePtr  content;
} HtmlDoc;

HtmlDoc  *html_doc_new       (const gchar *title);
void      html_doc_free      (HtmlDoc *hdoc);
xmlChar  *html_doc_to_string (HtmlDoc *hdoc, gsize *out_size);
