#include <glib.h>
#include <libxml/tree.h>
#include <libgda/libgda.h>

typedef struct {
	gchar      *name;
	xmlDocPtr   doc;
	xmlNodePtr  body;
	xmlNodePtr  toc;
} HtmlFile;

typedef struct {
	HtmlFile          *index;
	
	GHashTable        *nodes; /* key=node name, value=xmlNodePtr */
	gchar             *dir;
	GSList            *all_files;
} HtmlConfig;
#define HTML_CONFIG(x) ((HtmlConfig*)(x))


/*
 * HTML output files manipulation
 * Files are built as XML files and then printed at the end
 */
void       html_init_config           (HtmlConfig *config);
HtmlFile  *html_file_new              (HtmlConfig *config, 
				       const gchar *name, const gchar *title);
gboolean   html_file_write            (HtmlFile *file, HtmlConfig *config);
void       html_file_free             (HtmlFile *file);
void       html_declare_node          (HtmlConfig *config, const gchar *path, xmlNodePtr node);
void       html_declare_node_own      (HtmlConfig *config, gchar *path, xmlNodePtr node);
void       html_add_link_to_node      (HtmlConfig *config, const gchar *nodepath,
				       const gchar *text, const gchar *link_to);
void       html_add_to_toc            (HtmlConfig *config, HtmlFile *file, const gchar *text, const gchar *link_to);
xmlNodePtr html_add_header            (HtmlConfig *config, HtmlFile *file, const gchar *text);
void       html_mark_path_error       (HtmlConfig *config, const gchar *nodepath);
void       html_mark_node_error       (HtmlConfig *config, xmlNodePtr node);
void       html_mark_node_warning     (HtmlConfig *config, xmlNodePtr node);
void       html_mark_node_notice      (HtmlConfig *config, xmlNodePtr node);

xmlNodePtr html_render_attribute_str  (xmlNodePtr parent, const gchar *node_type, 
				       const gchar *att_name, const gchar *att_val);
xmlNodePtr html_render_attribute_bool (xmlNodePtr parent, const gchar *node_type, 
				       const gchar *att_name, gboolean att_val);
xmlNodePtr html_render_data_model     (xmlNodePtr parent, GdaDataModel *model);
