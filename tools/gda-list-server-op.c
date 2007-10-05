#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <glib/gi18n-lib.h>

gchar *prov = NULL;
gchar *op = NULL;
gboolean list_ops = FALSE;
GdaServerProvider *prov_obj = NULL;

static GOptionEntry entries[] = {
        { "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", "provider"},
        { "op", 'o', 0, G_OPTION_ARG_STRING, &op, "Operation", "operation name"},
        { "list-ops", 'l', 0, G_OPTION_ARG_NONE, &list_ops, "List existing operations", NULL },
        { NULL }
};

static xmlDocPtr merge_specs (const gchar *xml_dir, GdaServerOperationType type, gboolean *op_supported, GError **error);

gint 
main (int argc, char **argv) {
	gchar *xml_dir;
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (_("Gda server operations list"));
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_warning ("Can't parse arguments: %s", error->message);
		return 1;
        }
        g_option_context_free (context);

	gda_init ("list-server-op", PACKAGE_VERSION, argc, argv);
	xml_dir = gda_gbr_get_file_path (GDA_DATA_DIR, "libgda-3.0", NULL);
	g_print (_("Using XML descriptions in %s\n"), xml_dir);
	if (prov)
		g_print ("For provider %s\n", prov);

	if (prov) {
		GdaProviderInfo *prov_info;
		prov_info = gda_config_get_provider_by_name (prov);
		if (!prov_info) {
			g_print (_("Could not find provider '%s', check it is installed and the spelling is correct.\n"),
				 prov);
			return 1;
		}

		GModule *handle;
		void (*plugin_init) (const gchar *);
		GdaServerProvider *(*plugin_create_provider) (void);
		handle = g_module_open (prov_info->location, G_MODULE_BIND_LAZY);
		if (!handle) {
			g_print (_("Could not load provider at '%s'.\n"), prov_info->location);
			return 1;
		}
		/* initialize plugin if supported */
		if (g_module_symbol (handle, "plugin_init", (gpointer) &plugin_init)) {
			gchar *dirname;
			
			dirname = g_path_get_dirname (prov_info->location);
			plugin_init (dirname);
			g_free (dirname);
		}
		
		/* create provider object */
		if (g_module_symbol (handle, "plugin_create_provider", (gpointer) &plugin_create_provider)) {
			prov_obj = plugin_create_provider ();
			if (!prov_obj) {
				g_print (_("Could not create provider object from '%s'\n"), 
					 prov_info->location);
				return 1;
			}
		}
		else {
			g_print (_("Could not create provider object from '%s' (missing plugin_create_provider symbol)\n"), 
				 prov_info->location);
			return 1;
		}
	}

	if (list_ops) {
		GdaServerOperationType type;
		if (prov)
			g_print (_("Existing operation types for provider '%s':\n"), prov);
		else
			g_print (_("Existing operation types:\n"));
		for (type = GDA_SERVER_OPERATION_CREATE_DB; type < GDA_SERVER_OPERATION_NB; type++) {
			if (! prov_obj ||
			    (prov_obj && gda_server_provider_supports_operation (prov_obj, NULL, type, NULL)))
				g_print ("%s\n", gda_server_operation_op_type_to_string (type));
		}
		return 0;
	}

	GdaServerOperationType type;
	for (type = GDA_SERVER_OPERATION_CREATE_DB; type != GDA_SERVER_OPERATION_NB; type++) {
		xmlDocPtr doc;
		GError *error = NULL;
		gboolean op_supported;

		if (op && strcmp (op, gda_server_operation_op_type_to_string (type)))
			continue;

		g_print (_("Description for type: %s\n"), gda_server_operation_op_type_to_string (type));
		doc = merge_specs (xml_dir, type, &op_supported, &error);
		if (doc) {
			xmlChar *buf;
			gint len;
			xmlKeepBlanksDefault (0);
			xmlDocDumpFormatMemory (doc, &buf, &len, 1);
			g_print ("%s\n", buf);
			xmlFree (buf);
			xmlFreeDoc (doc);
		}
		else {
			if (!op_supported)
				g_print (_("Operation not supported\n"));
			else
				g_print (_("Error: %s\n"), error && error->message ? error->message : _("No detail"));
			if (error)
				g_error_free (error);
		}
	}
	g_free (xml_dir);

	return 0;
}

static void
remove_all_extra (xmlNodePtr node)
{
	xmlNodePtr child;
	for (child = node->last; child; ) {
		xmlChar *lang = xmlGetProp (child, (xmlChar*)"lang");
		if (lang || !strcmp (child->name, "sources")) {
			/* remove this node */
			xmlNodePtr prev = child->prev;
			if (lang)
				xmlFree (lang);

			xmlUnlinkNode (child);
			xmlFreeNode (child);
			child = prev;
		}
		else {
			remove_all_extra (child);
			child = child->prev;
		}
	}
}

typedef struct {
	GdaServerOperationNodeType  node_type;
	gchar                      *path;
	gchar                      *name;
	gchar                      *type;
	gchar                      *descr;
} Path;
static void
path_free (Path *path)
{
	g_free (path->path);
	g_free (path->name);
	g_free (path->type);
	g_free (path->descr);
	g_free (path);
}

/* create a list of Path structures */
static GSList *
make_paths (xmlNodePtr node, const gchar *parent_path, GSList *exist_list)
{
	xmlChar *id;
	GSList *retlist = exist_list;
	id = xmlGetProp (node, BAD_CAST "id");
	if (id) {
		Path *path;
		gchar *pstr = g_strdup_printf ("%s/%s", parent_path, id);

		path = g_new0 (Path, 1);
		path->path = pstr;
		retlist = g_slist_append (retlist, path);

		if (!strcmp (node->name, "parameters"))
			path->node_type = GDA_SERVER_OPERATION_NODE_PARAMLIST;
		else if (!strcmp (node->name, "parameter"))
			path->node_type = GDA_SERVER_OPERATION_NODE_PARAM;
		else if (!strcmp (node->name, "sequence"))
			path->node_type = GDA_SERVER_OPERATION_NODE_SEQUENCE;
		else if (!strcmp (node->name, "gda_array"))
			path->node_type = GDA_SERVER_OPERATION_NODE_DATA_MODEL;
		else if (!strcmp (node->name, "gda_array_field")) {
			path->node_type = GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN;
			g_free (path->path);
			pstr = g_strdup_printf ("%s/@%s", parent_path, id);
			path->path = pstr;
		}
		else
			path->node_type = GDA_SERVER_OPERATION_NODE_UNKNOWN;

		xmlChar *prop;
		prop = xmlGetProp (node, BAD_CAST "name");
		if (prop) {
			path->name = g_strdup (prop);
			xmlFree (prop);
		}
		prop = xmlGetProp (node, BAD_CAST "descr");
		if (prop) {
			path->descr = g_strdup (prop);
			xmlFree (prop);
		}
		prop = xmlGetProp (node, BAD_CAST "gdatype");
		if (prop) {
			path->type = g_strdup (prop);
			xmlFree (prop);
		}

		/* children */
		xmlNodePtr child;
		for (child = node->children; child; child = child->next) 
			retlist = make_paths (child, pstr, retlist);


		xmlFree (id);
	}
	else {
		/* children */
		xmlNodePtr child;
		for (child = node->children; child; child = child->next) 
			retlist = make_paths (child, parent_path, retlist);
	}

	return retlist;
}

const gchar *
node_type_to_string (GdaServerOperationType node_type)
{
	switch (node_type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		return "PARAMLIST";
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		return "DATA_MODEL";
	case GDA_SERVER_OPERATION_NODE_PARAM:
		return "PARAMETER";
	case GDA_SERVER_OPERATION_NODE_SEQUENCE:
		return "SEQUENCE";
	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
		return "SEQUENCE_ITEM";
	default:
		break;
	}
	return "Unknown";
}

static void
merge_spec_from_root (xmlNodePtr root, const gchar *provider, xmlDocPtr pdoc)
{
	/* make list of paths */
	GSList *paths, *list;

	paths = make_paths (xmlDocGetRootElement (pdoc), "", NULL);
	for (list = paths; list; list = list->next) {
		Path *path = (Path *) list->data;

		/* find node for this path ID, and create it if not yet present */
		xmlNodePtr node = NULL;
		for (node = root->children; node; node = node->next) {
			if (!strcmp (node->name, "path")) {
				xmlChar *pid;
				pid = xmlGetProp (node, "id");
				if (pid) {
					if (!strcmp (pid, path->path)) {
						xmlFree (pid);
						break;
					}
					xmlFree (pid);
				}
			}
			    
		}
		if (!node) {
			node = xmlNewChild (root, NULL, BAD_CAST "path", NULL);
			xmlSetProp (node, BAD_CAST "id", BAD_CAST path->path);
			xmlSetProp (node, BAD_CAST "node_type", BAD_CAST node_type_to_string (path->node_type));
			if (path->type && *path->type)
				xmlSetProp (node, BAD_CAST "gdatype", BAD_CAST path->type);	
		}

		/* add this provider's specific information */
		xmlNodePtr pnode;
		if (!prov) {
			pnode = xmlNewChild (node, NULL, BAD_CAST "prov", NULL);
			xmlSetProp (pnode, BAD_CAST "prov_name", BAD_CAST provider);
		}
		else
			pnode = node;
		xmlSetProp (pnode, BAD_CAST "name", BAD_CAST path->name);
		if (path->descr && *path->descr)
			xmlSetProp (pnode, BAD_CAST "descr", BAD_CAST path->descr);

		if (!prov) {
			/* check node type is similar to "node->node_type" */
			xmlChar *node_type;
			node_type = xmlGetProp (node, BAD_CAST "node_type");
			g_assert (node_type);
			if (strcmp (node_type, node_type_to_string (path->node_type)))
				xmlSetProp (pnode, BAD_CAST "node_type", node_type_to_string (path->node_type));
			xmlFree (node_type);
			
			/* check gdatype is similar to "node->gdatype" */
			node_type = xmlGetProp (node, BAD_CAST "gdatype");
			if ((node_type && path->type && strcmp (node_type, path->type)) ||
			    (!node_type && path->type) ||
			    (node_type && *node_type && !path->type))
				xmlSetProp (pnode, BAD_CAST "gdatype", path->type);
			if (node_type) 
				xmlFree (node_type);
		}
	}
	
	/* mem free */
	g_slist_foreach (paths, (GFunc) path_free, NULL);
	g_slist_free (paths);
}

static xmlDocPtr
merge_specs (const gchar *xml_dir, GdaServerOperationType type, gboolean *op_supported, GError **error)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	GDir *dir;
	const gchar *file;
	gchar *lcprov = NULL;
	dir = g_dir_open (xml_dir, 0, error);
	if (!dir)
		return NULL;

	if (prov)
		lcprov = g_ascii_strdown (prov, -1);
	*op_supported = FALSE;

	for (file = g_dir_read_name (dir); file; file = g_dir_read_name (dir)) {
		gchar *suffix, *lc;

		lc = g_ascii_strdown (gda_server_operation_op_type_to_string (type), -1);
		suffix = g_strdup_printf ("_specs_%s.xml", lc);
		g_free (lc);
		if (g_str_has_suffix (file, suffix)) {
			gchar *provider_end, *provider;

			xmlDocPtr pdoc;
			gchar *cfile = g_build_filename (xml_dir, file, NULL);

			provider = g_strdup (file);
			provider_end = g_strrstr (provider, suffix);
			*provider_end = 0;
			
			if (lcprov && strcmp (lcprov, provider))
				continue;

			*op_supported = TRUE;
			pdoc = xmlParseFile (cfile);
			g_free (cfile);
			if (!pdoc) {
				g_warning (_("Can't parse file '%s', ignoring it"), file);
				continue;
			}

			remove_all_extra (xmlDocGetRootElement (pdoc));

			if (!doc) {
				doc = xmlNewDoc ("1.0");
				root = xmlNewNode (NULL, BAD_CAST "server_op");
				xmlDocSetRootElement (doc, root);
			}
			merge_spec_from_root (root, provider, pdoc);
						
			xmlFreeDoc (pdoc);
			g_free (provider);
		}
		g_free (suffix);
	}
	g_dir_close (dir);
	g_free (lcprov);
	return doc;
}
