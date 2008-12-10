
/* web-server.c
 *
 * Copyright (C) 2008 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <stdarg.h>
#include <string.h>
#include "web-server.h"
#include <libsoup/soup.h>
#include "html-doc.h"

/* 
 * Main static functions 
 */
static void web_server_class_init (WebServerClass * class);
static void web_server_init (WebServer *server);
static void web_server_dispose (GObject *object);
static void web_server_finalize (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


struct _WebServerPrivate
{
	SoupServer  *server;
	GHashTable  *tmpdata_hash; /* key = a path without the starting '/', value = a TmpData pointer */
	GSList      *tmpdata_list; /* list of the TmpData pointers in @tmpdata_hash, memory not managed here */
	guint        timer;
};

/*
 * Temporary available data
 *
 * Each TmpData structure represents a ressource which will be available for some time (until it has
 * expired).
 *
 * If the expiration_date attribute is set to 0, then there is no expiration at all.
 */
typedef struct {
	gchar *path;
	gchar *data;
	gsize  size;
	int    expiration_date; /* 0 to avoid expiration */
} TmpData;

static gboolean
delete_tmp_data (WebServer *server)
{
	GSList *list;
	GTimeVal tv;
	gint n_timed = 0;

	g_get_current_time (&tv);
	for (list = server->priv->tmpdata_list; list; ) {
		TmpData *td = (TmpData *) list->data;
		if ((td->expiration_date > 0) && (td->expiration_date < tv.tv_sec)) {
			GSList *n = list->next;
			g_hash_table_remove (server->priv->tmpdata_hash, td->path);
			server->priv->tmpdata_list = g_slist_delete_link (server->priv->tmpdata_list, list);
			list = n;
		}
		else {
			if (td->expiration_date > 0)
				n_timed ++;
			list = list->next;
		}
	}
	if (n_timed == 0) {
		server->priv->timer = 0;
		return FALSE;
	}
	else
		return TRUE;
}

/*
 * @data is stolen!
 */
static TmpData *
tmp_data_add (WebServer *server, const gchar *path, gchar *data, gsize data_length)
{
	TmpData *td;
	GTimeVal tv;

	g_get_current_time (&tv);
	td = g_new0 (TmpData, 1);
	td->path = g_strdup (path);
	td->data = data;
	td->size = data_length;
	td->expiration_date = tv.tv_sec + 30;
	g_hash_table_insert (server->priv->tmpdata_hash, g_strdup (path), td);
	server->priv->tmpdata_list = g_slist_prepend (server->priv->tmpdata_list, td);
	if (!server->priv->timer)
		server->priv->timer = g_timeout_add_seconds (5, (GSourceFunc) delete_tmp_data, server);
	return td;
}

/*
 * @data is static
 */
static TmpData *
tmp_static_data_add (WebServer *server, const gchar *path, gchar *data, gsize data_length)
{
	TmpData *td;

	td = g_new0 (TmpData, 1);
	td->path = g_strdup (path);
	td->data = data;
	td->size = data_length;
	td->expiration_date = 0;
	g_hash_table_insert (server->priv->tmpdata_hash, g_strdup (path), td);
	server->priv->tmpdata_list = g_slist_prepend (server->priv->tmpdata_list, td);
	return td;
}

static void
tmp_data_free (TmpData *data)
{
	g_free (data->data);
	g_free (data);
}

#define GDA_CSS \
"body {" \
"        margin: 0px;" \
"        background-color: white;" \
"        font-family: sans-serif;" \
"        color: black;" \
"}" \
"" \
"a {" \
"    color: #0000ff;" \
"    border: 0px;" \
"}" \
"" \
"a:active {" \
"        color: #ff0000;" \
"}" \
"" \
"a:visited {" \
"        color: #551a8b;" \
"}" \
"" \
"" \
"#container" \
"{" \
"    width: 97%;" \
"    margin: 1%;" \
"    background-color: #fff;" \
"    color: #333;" \
"}" \
"" \
"" \
"" \
"#top" \
"{" \
"    background: #729FCF;" \
"    float: left;" \
"    width: 100%;" \
"    font-size: 75%;" \
"}" \
"" \
"#top h1" \
"{" \
"    margin: 0;" \
"    margin-left: 85px;" \
"    padding-top: 20px;" \
"    padding-bottom: 20px;" \
"    color: #eeeeec;" \
"}" \
"" \
"#top ul {" \
"    list-style: none;" \
"    text-align: right;" \
"    padding: 0 1ex;" \
"    margin: 0;" \
"    font-size: 85%;" \
"}" \
"" \
"#top li a {" \
"    font-weight: bold;" \
"    color: #FFFFFF;" \
"    margin: 0 2ex;" \
"    text-decoration: none;" \
"    line-height: 30px;" \
"" \
"}" \
"" \
"" \
"/*" \
" * Left naivgation pane" \
" */" \
"#leftnav" \
"{" \
"    float: left;" \
"    width: 140px;" \
"    margin: 0;" \
"    padding-top: 5;" \
"" \
"    background: #2E3436;" \
"    color: #FFFFFF;" \
"}" \
"" \
"#leftnav ul {" \
"    font-weight: bold;" \
"    list-style: none;" \
"    padding: 0 10px 10px;;" \
"    margin: 0 0 0 0;" \
"    font-size: 90%;" \
"}" \
"" \
"#leftnav li a {" \
"    font-weight: normal;" \
"    color: #FFFFFF;" \
"    margin: 0 0 0 0;" \
"    padding: 0 10px;" \
"    text-decoration: none;" \
"    font-size: 80%;" \
"}" \
"" \
"#leftnav p { margin: 0 0 1em 0; }" \
"" \
"/* " \
" * Content" \
" */" \
"#content" \
"{" \
"    /*background: red;*/" \
"    margin-left: 140px;" \
"    padding: 5em 1em;" \
"}" \
"" \
"#content h1" \
"{ " \
"    /*background: green;*/" \
"    margin: 5em 0 .5em 0 0;" \
"    font-size: 100%;" \
"}" \
"" \
"#content h2" \
"{ " \
 "    /*background: green;*/"			\
"    padding-left: 5;" \
"    font-size: 80%;" \
"}" \
"" \
"#content ul {" \
"    font-weight: bold;" \
"    list-style: none;" \
"    padding: 0 10px 10px;;" \
"    margin: 0 0 0 0;" \
"    font-size: 90%;" \
"}" \
"" \
"#content li {" \
"    font-weight: normal;" \
"    margin: 0 0 0 0;" \
"    padding: 0 10px;" \
"    text-decoration: none;" \
"    font-size: 80%;" \
"}" \
"" \
"div.clist" \
"{" \
"    /*background: blue;*/" \
"    padding: 0;" \
"    overflow: hidden;" \
"}" \
"" \
".clist ul" \
"{" \
"    /*background: lightgray;*/" \
"    margin-bottom: 0;" \
"" \
"    float: left;" \
"    width: 100%;" \
"    margin: 0;" \
"    margin-left: 10px;" \
"    padding: 0;" \
"    list-style: none;" \
"}" \
"" \
".clist li" \
"{" \
"    /*background: lightblue;*/" \
"    float: left;" \
"    width: 33%;" \
"    margin: 0;" \
"    padding: 0;" \
"    font-size: 90%;" \
"    /*word-wrap: break-word;*/" \
"}" \
"" \
".clist a" \
"{" \
"    font-weight: normal;" \
"    color: #050505;" \
"    margin: 0 0 0 0;" \
"    padding: 0 0 0 0;" \
"    text-decoration: none;" \
"}" \
"" \
".clist br" \
"{" \
"    clear: both;" \
"}" \
"" \
"table.ctable" \
"{" \
"    font-weight: normal;" \
"    font-size: 90%;" \
"    width: 100%;" \
"    background-color: #fafafa;" \
"    border: 1px #6699CC solid;" \
"    border-collapse: collapse;" \
"    border-spacing: 0px;" \
"    margin-top: 0px;" \
"    margin-bottom: 5px;" \
"}" \
"" \
".ctable th" \
"{" \
"    border-bottom: 2px solid #6699CC;" \
"    background-color: #729FCF;" \
"    text-align: center;" \
"    font-weight: bold;" \
"    color: #eeeeec;" \
"}" \
"" \
".ctable td" \
"{" \
"    padding-left: 2px;" \
"    border-left: 1px dotted #729FCF;" \
"}" \
"" \
".graph" \
"{" \
"    /*background: lightblue;*/" \
"    padding: 0;" \
"}" \
"" \
".graph img" \
"{" \
"    max-width: 100%;" \
"    height: auto;" \
"    border: 0;" \
"}" \
"" \
".pkey" \
"{" \
"    /*background: lightblue;*/" \
"    color: blue;" \
"    font-weight: bold;" \
"}" \
"" \
".ccode" \
"{" \
"    /*background: lightblue;*/" \
"    padding-left: 5px;" \
"}" \
"" \
"/*" \
" * Footer" \
" */" \
"#footer" \
"{" \
"    clear: both;" \
"    margin: 0;" \
"    padding: 2;" \
"    color: #eeeeec;" \
"    background: #729FCF;" \
"}"


/* module error */
GQuark web_server_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("web_server_error");
        return quark;
}

GType
web_server_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (WebServerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) web_server_class_init,
			NULL,
			NULL,
			sizeof (WebServer),
			0,
			(GInstanceInitFunc) web_server_init
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "WebServer", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
web_server_class_init (WebServerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);


	/* virtual functions */
	object_class->dispose = web_server_dispose;
	object_class->finalize = web_server_finalize;
}

static void
web_server_init (WebServer *server)
{
	server->priv = g_new0 (WebServerPrivate, 1);
	server->priv->timer = 0;
	server->priv->tmpdata_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify) tmp_data_free);
	server->priv->tmpdata_list = NULL;

	tmp_static_data_add (server, "gda.css", GDA_CSS, strlen (GDA_CSS));
}


static gboolean get_file (SoupServer *server, SoupMessage *msg, const char *path, GError **error);
static void     get_root (SoupServer *server, SoupMessage *msg);
static gboolean get_for_cnc (WebServer *webserver, SoupMessage *msg, 
			     const ConnectionSetting *cs, gchar **extra, GError **error);


static void
server_callback (SoupServer *server, SoupMessage *msg,
                 const char *path, GHashTable *query,
                 SoupClientContext *context, WebServer *webserver)
{
	/*#define DEBUG_SERVER*/
#ifdef DEBUG_SERVER
        printf ("%s %s HTTP/1.%d\n", msg->method, path, soup_message_get_http_version (msg));
        SoupMessageHeadersIter iter;
        const char *name, *value;
        soup_message_headers_iter_init (&iter, msg->request_headers);
        while (soup_message_headers_iter_next (&iter, &name, &value))
                printf ("%s: %s\n", name, value);
        if (msg->request_body->length)
                printf ("Request body: %s\n", msg->request_body->data);
#endif
	
        if (msg->method == SOUP_METHOD_GET) {
		GError *error = NULL;
		gboolean ok = TRUE;
		TmpData *tmpdata;
		if (*path != '/') {
			soup_message_set_status_full (msg, SOUP_STATUS_UNAUTHORIZED, "Wrong path name");
			return;
		}
		path++;

		if (*path == 0)
			get_root (server, msg);
		else if ((tmpdata = g_hash_table_lookup (webserver->priv->tmpdata_hash, path))) {
			soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC,
						  tmpdata->data, tmpdata->size);
			soup_message_set_status (msg, SOUP_STATUS_OK);
		}
		else {
			gchar **array = NULL;
			array = g_strsplit (path, "/", 0);

			const ConnectionSetting *cs;
			cs = gda_sql_get_connection (array[0]);

			if (cs) 
				ok = get_for_cnc (webserver, msg, cs, array[1] ? &(array[1]) : NULL, &error);
			else {
				/*ok = get_file (webserver, msg, path, &error);*/
				ok = FALSE;
			}
			if (array)
				g_strfreev (array);
		}

		if (!ok) {
			if (error) {
				soup_message_set_status_full (msg, error->code, error->message);
				g_error_free (error);
			}
			else
				soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
		}
	}
        else
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
#ifdef DEBUG_SERVER
        printf ("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);
#endif
}

/**
 * web_server_new
 * @type: the #GType requested
 *
 * Creates a new server of type @type
 *
 * Returns: a new #WebServer object
 */
WebServer *
web_server_new (gint port)
{
	WebServer *server;

	server = (WebServer*) g_object_new (WEB_TYPE_SERVER, NULL);
	server->priv->server = soup_server_new (SOUP_SERVER_PORT, port,
						SOUP_SERVER_SERVER_HEADER, "gda-sql-httpd ",
						NULL);
	soup_server_add_handler (server->priv->server, NULL,
                                 (SoupServerCallback) server_callback, server, NULL);
	
	soup_server_run_async (server->priv->server);

	return server;
}


static void
web_server_dispose (GObject *object)
{
	WebServer *server;

	server = WEB_SERVER (object);
	if (server->priv) {
		if (server->priv->tmpdata_hash) {
			g_hash_table_destroy (server->priv->tmpdata_hash);
			server->priv->tmpdata_hash = NULL;
		}
		if (server->priv->tmpdata_list) {
			g_slist_free (server->priv->tmpdata_list);
			server->priv->tmpdata_list = NULL;
		}
		if (server->priv->server) {
			g_object_unref (server->priv->server);
			server->priv->server = NULL;
		}
		if (server->priv->timer) {
			g_source_remove (server->priv->timer);
			server->priv->timer = 0;
		}		
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
web_server_finalize (GObject   * object)
{
	WebServer *server;

	g_return_if_fail (object != NULL);
	g_return_if_fail (WEB_IS_SERVER (object));

	server = WEB_SERVER (object);
	if (server->priv) {
		g_free (server->priv);
	}

	/* parent class */
	parent_class->finalize (object);
}

/*
 * GET for a file
 */
static gboolean
get_file (SoupServer *server, SoupMessage *msg, const char *path, GError **error)
{
	GMappedFile *mfile;
        mfile = g_mapped_file_new (path, FALSE, error);
	if (!mfile)
		return FALSE;

	SoupBuffer *buffer;
	buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mfile),
					     g_mapped_file_get_length (mfile),
					     mfile, (GDestroyNotify) g_mapped_file_free);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	soup_message_set_status (msg, SOUP_STATUS_OK);
	return TRUE;
}

/*
 * GET for the / path
 */
static void
get_root (SoupServer *server, SoupMessage *msg)
{
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;

	const GSList *list;
	list = gda_sql_get_all_connections ();
	if (0 && list && !list->next) {
		/* only 1 connection => go to this one */
		ConnectionSetting *cs = (ConnectionSetting*) list->data;
		soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
		soup_message_headers_append (msg->response_headers, "Location", cs->name);
		return;
	}
	hdoc = html_doc_new (_("Database information"));
	if (!list) {
		/* no connection at all */
		xmlNodePtr node;

		node = xmlNewChild (hdoc->content, NULL, "h1", _("No connection opened."));
		node = xmlNewChild (hdoc->content, NULL, "p", _("Open a connection from the console and reload this page"));
	}
	else {
		/* more than one connection, redirect to the current one */
		const ConnectionSetting *cs = gda_sql_get_current_connection ();
		soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
		soup_message_headers_append (msg->response_headers, "Location", cs->name);
		return;
	}

	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");
	xstr = html_doc_to_string (hdoc, &size);
	buffer = soup_buffer_new_with_owner (xstr, size, xstr, (GDestroyNotify)xmlFree);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	html_doc_free (hdoc);

	soup_message_set_status (msg, SOUP_STATUS_OK);
}

static gboolean compute_all_objects_content (HtmlDoc *hdoc, const ConnectionSetting *cs, 
					     const gchar *human_obj_type,
					     const gchar *human_obj_type_in_schema,
					     const gchar *table_name,
					     const gchar *obj_prefix,
					     const gchar *filter,
					     GError **error);
static gboolean compute_all_triggers_content (HtmlDoc *hdoc, const ConnectionSetting *cs, GError **error);
static gboolean compute_object_content (HtmlDoc *hdoc, WebServer *webserver, const ConnectionSetting *cs,
					const gchar *schema, const gchar *name, GError **error);
/*
 * GET method for a connection
 */
static gboolean
get_for_cnc (WebServer *webserver, SoupMessage *msg, const ConnectionSetting *cs, gchar **extra, GError **error)
{
	gboolean retval = FALSE;
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;
	gchar *str;

	gchar *rfc_cnc_name;

	xmlNodePtr ul, li, a;

	const GSList *clist;


	str = g_strdup_printf (_("Database information for '%s'"), cs->name);
	hdoc = html_doc_new (str);
	g_free (str);

	/* other connections in the sidebar */
	ul = xmlNewChild (hdoc->sidebar, NULL, "ul", _("Connections"));
	li = xmlNewChild (ul, NULL, "li", NULL);
	str = g_strdup_printf ("(%s)",  _("From console"));
	a = xmlNewChild (li, NULL, "a", str);
	g_free (str);
	xmlSetProp (a, "href", (xmlChar*) "/");

	for (clist = gda_sql_get_all_connections (); clist; clist = clist->next) {
		gchar *tmp;
		ConnectionSetting *cs = (ConnectionSetting*) clist->data;
			
		li = xmlNewChild (ul, NULL, "li", NULL);
		a = xmlNewChild (li, NULL, "a", cs->name);
		tmp = gda_rfc1738_encode (cs->name);
		str = g_strdup_printf ("/%s", tmp);
		g_free (tmp);
		xmlSetProp (a, "href", (xmlChar*) str);
		g_free (str);
	}

	/* list all database object's types for which information can be obtained */
	rfc_cnc_name = gda_rfc1738_encode (cs->name);
	ul = xmlNewChild (hdoc->sidebar, NULL, "ul", _("Objects"));
	li = xmlNewChild (ul, NULL, "li", NULL);
	a = xmlNewChild (li, NULL, "a", _("Tables"));
	str = g_strdup_printf ("/%s/___tables", rfc_cnc_name);
	xmlSetProp (a, "href", (xmlChar*) str);
	g_free (str);
	li = xmlNewChild (ul, NULL, "li", NULL);
	a = xmlNewChild (li, NULL, "a", _("Views"));
	str = g_strdup_printf ("/%s/___views", rfc_cnc_name);
	xmlSetProp (a, "href", (xmlChar*) str);
	g_free (str);
	li = xmlNewChild (ul, NULL, "li", NULL);
	a = xmlNewChild (li, NULL, "a", _("Triggers"));
	str = g_strdup_printf ("/%s/___triggers", rfc_cnc_name);
	xmlSetProp (a, "href", (xmlChar*) str);
	g_free (str);
	g_free (rfc_cnc_name);
	
#ifdef GDA_DEBUG_NO
	if (extra) {
		gint i;
		for (i = 0; extra[i]; i++) 
			g_print ("EXTRA %d: #%s#\n", i, extra[i]);
	}
#endif
	if (!extra || !strcmp (extra[0], "___tables")) {
		if (! compute_all_objects_content (hdoc, cs, 
						   _("Tables"), _("Tables in the '%s' schema"), 
						   "_tables", "table", "table_type LIKE \"%TABLE%\"", error))
			goto onerror;
	}
	else if (!strcmp (extra[0], "___views")) {
		if (! compute_all_objects_content (hdoc, cs, 
						   _("Views"), _("Views in the '%s' schema"), 
						   "_tables", "table", "table_type LIKE \"%VIEW%\"", error))
			goto onerror;
	}
	else if (!strcmp (extra[0], "___triggers")) {
		if (! compute_all_triggers_content (hdoc, cs, error))
			goto onerror;
	}
	else {
		/* extra [0] has to be a schema */
		if (extra [1]) {
			if (! compute_object_content (hdoc, webserver, cs, extra [0], extra [1], error))
				goto onerror;
		}
		else
			xmlNewChild (hdoc->content, NULL, "h1", "Not yet implemented");
	}

	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");
	xstr = html_doc_to_string (hdoc, &size);
	buffer = soup_buffer_new_with_owner (xstr, size, xstr, (GDestroyNotify)xmlFree);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	soup_message_set_status (msg, SOUP_STATUS_OK);
	retval = TRUE;

 onerror:
	html_doc_free (hdoc);
	return retval;
}

static void
meta_table_column_foreach_attribute_func (const gchar *att_name, const GValue *value, GString **string)
{
	if (!strcmp (att_name, GDA_ATTRIBUTE_AUTO_INCREMENT) && 
	    (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN) && 
	    g_value_get_boolean (value)) {
		if (*string) {
			g_string_append (*string, ", ");
			g_string_append (*string, _("Auto increment"));
		}
		else
			*string = g_string_new (_("Auto increment"));
	}
}

static gchar *meta_struct_dump_as_graph (const ConnectionSetting *cs, GdaMetaStruct *mstruct,
					 GdaMetaDbObject *central_dbo, GError **error);
static gboolean
compute_table_details (const ConnectionSetting *cs, HtmlDoc *hdoc, WebServer *webserver,
		       GdaMetaStruct *mstruct, GdaMetaDbObject *dbo, GError **error)
{
	gchar *tmp;
	xmlNodePtr div, table, tr, td;
	GdaMetaTable *mt = GDA_META_TABLE (dbo);
	GSList *list;
	GdaMetaStore *store;

	tmp = g_strdup_printf (_("Columns for the '%s' table:"), dbo->obj_short_name);
	xmlNewChild (hdoc->content, NULL, "h1", tmp);
	g_free (tmp);

	div = xmlNewChild (hdoc->content, NULL, "div", NULL);
	table = xmlNewChild (div, NULL, "table", NULL);
	xmlSetProp (table, "class", (xmlChar*) "ctable");
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "th", _("Column"));
	td = xmlNewChild (tr, NULL, "th", _("Type"));
	td = xmlNewChild (tr, NULL, "th", _("Nullable"));
	td = xmlNewChild (tr, NULL, "th", _("Default"));
	td = xmlNewChild (tr, NULL, "th", _("Extra"));

	for (list = mt->columns; list; list = list->next) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
		GString *string = NULL;
		
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", tcol->column_name);
		if (tcol->pkey)
			xmlSetProp (td, "class", "pkey");
		td = xmlNewChild (tr, NULL, "td", tcol->column_type);
		td = xmlNewChild (tr, NULL, "td", tcol->nullok ? _("yes") : _("no"));
		td = xmlNewChild (tr, NULL, "td", tcol->default_value);

		gda_meta_table_column_foreach_attribute (tcol, 
				    (GdaAttributesManagerFunc) meta_table_column_foreach_attribute_func, &string);
		if (string) {
			td = xmlNewChild (tr, NULL, "td", string->str);
			g_string_free (string, TRUE);
		}
		else
			td = xmlNewChild (tr, NULL, "td", NULL);
	}

	/* finished if we don't have a table */
	if (dbo->obj_type != GDA_META_DB_TABLE)
		return TRUE;

	/* show primary key */
	GdaMetaTable *dbo_table = GDA_META_TABLE (dbo);
#ifdef NONO
	if (dbo_table->pk_cols_nb > 0) {
		gint ipk;
		xmlNodePtr ul = NULL;
		xmlNewChild (hdoc->content, NULL, "h1", _("Primary key:"));
		div = xmlNewChild (hdoc->content, NULL, "div", NULL);
		for (ipk = 0; ipk < dbo_table->pk_cols_nb; ipk++) {
			GdaMetaTableColumn *tcol;
			if (!ul)
				ul = xmlNewChild (div, NULL, "ul", NULL);

			tcol = g_slist_nth_data (dbo_table->columns, ipk);
			xmlNewChild (ul, NULL, "li", tcol->column_name);
		}
	}
#endif

	/* Add objects depending on this table */
	if (!gda_meta_struct_complement_depend (mstruct, dbo, error))
		return FALSE;
	
	/* Add tables from which this one depends */
	GValue *v0, *v1;
	GdaDataModel *model;
	g_object_get (G_OBJECT (mstruct), "meta-store", &store, NULL);
	g_value_set_string ((v0 = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
	model = gda_meta_store_extract (store, "SELECT table_catalog, table_schema, table_name FROM "
					"_referential_constraints WHERE "
					"ref_table_schema = ##tschema::string AND ref_table_name = ##tname::string",
					error, "tschema", v0, "tname", v1, NULL);
	gda_value_free (v0);
	gda_value_free (v1);
	g_object_unref (store);
	if (model) {
		gint i, nrows;
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			const GValue *cv0, *cv1 = NULL, *cv2 = NULL;
			cv0 = gda_data_model_get_value_at (model, 0, i, NULL);
			if (cv0) {
				cv1 = gda_data_model_get_value_at (model, 1, i, NULL);
				if (cv1)
					cv2 = gda_data_model_get_value_at (model, 2, i, NULL);
			}
			if (cv0 && cv1 && cv2)
				gda_meta_struct_complement (mstruct, GDA_META_DB_TABLE, cv0, cv1, cv2, NULL);
		}
		g_object_unref (model);
	}

	/* create a graph */
	gchar *graph, *tmp_filename = NULL;
	xmlNodePtr map_node = NULL;
	graph = meta_struct_dump_as_graph (cs, mstruct, dbo, NULL);
	if (graph) {
		gchar *dotname, *suffix;
		static gint counter = 0;

		suffix = g_strdup_printf (".gda_graph_tmp-%d.dot", ++counter);
		dotname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
		g_free (suffix);

		if (g_file_set_contents (dotname, graph, -1, error)) {
			gchar *pngname, *mapname;
			gchar *argv[] = {"dot", "-Tpng", "-o",  NULL, "-Tcmapx", "-o", NULL, NULL, NULL};
			
			suffix = g_strdup_printf (".gda_graph_tmp-%d", counter);
			pngname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
			g_free (suffix);
			
			suffix = g_strdup_printf (".gda_graph_tmp-%d.map", counter);
			mapname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
			g_free (suffix);

			argv[3] = pngname;
			argv[6] = mapname;
			argv[7] = dotname;
			if (g_spawn_sync (NULL, argv, NULL,
					  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
					  NULL, NULL,
					  NULL, NULL, NULL, NULL)) {
				xmlDocPtr map;
				map = xmlParseFile (mapname);
				if (map) {
					map_node = xmlDocGetRootElement (map);
					xmlUnlinkNode (map_node);
					xmlFreeDoc (map);
				}

				gchar *file_data;
				gsize file_data_len;
				if (g_file_get_contents (pngname, &file_data, &file_data_len, NULL)) {
					tmp_filename = g_strdup_printf ("___tmp/g%d", counter);
					tmp_data_add (webserver, tmp_filename, file_data, file_data_len);
				}
			}
			g_unlink(pngname);
			g_free (pngname);
			g_unlink(mapname);
			g_free (mapname);
		}
		g_unlink(dotname);
		g_free (dotname);
		g_free (graph);
	}
	if (tmp_filename) {
		xmlNodePtr obj;
		gchar *tmp;
		xmlNewChild (hdoc->content, NULL, "h1", _("Relations:"));
		div = xmlNewChild (hdoc->content, NULL, "div", NULL);

		if (map_node)
			xmlAddChild (div, map_node);

		xmlSetProp (div, "class", (xmlChar*) "graph");
		obj = xmlNewChild (div, NULL, "img", NULL);
		tmp = g_strdup_printf ("/%s", tmp_filename);
		xmlSetProp (obj, "src", (xmlChar*) tmp);
		xmlSetProp (obj, "usemap", (xmlChar*) "#G");
		g_free (tmp);
		g_free (tmp_filename);
	}
	else {
		/* list foreign keys as we don't have a graph */
		if (dbo_table->fk_list) {
			xmlNewChild (hdoc->content, NULL, "h1", _("Foreign keys:"));
			GSList *list;
			for (list = dbo_table->fk_list; list; list = list->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				GdaMetaDbObject *fkdbo = tfk->depend_on;
				xmlNodePtr ul = NULL;
				gint ifk;
				
				div = xmlNewChild (hdoc->content, NULL, "div", NULL);
				for (ifk = 0; ifk < tfk->cols_nb; ifk++) {
					gchar *tmp;
					if (!ul) {
						tmp = g_strdup_printf (_("To '%s':"), fkdbo->obj_short_name);
						ul = xmlNewChild (div, NULL, "ul", tmp);
						g_free (tmp);
					}
					tmp = g_strdup_printf ("%s --> %s.%s", 
							       tfk->fk_names_array[ifk], 
							       fkdbo->obj_short_name, tfk->ref_pk_names_array[ifk]);
					xmlNewChild (ul, NULL, "li", tmp);
					g_free (tmp);
				}
			}
		}
	}

	return TRUE;
}

/*
 * Creates a new graph (in the GraphViz syntax) representation of @mstruct.
 *
 * Returns: a new string, or %NULL if an error occurred.
 */
static gchar *
meta_struct_dump_as_graph (const ConnectionSetting *cs, GdaMetaStruct *mstruct, GdaMetaDbObject *central_dbo, 
			   GError **error)
{
	GString *string;
	gchar *result;
	gchar *rfc_cnc_name;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	rfc_cnc_name = gda_rfc1738_encode (cs->name);
	
	string = g_string_new ("digraph G {\nrankdir = LR;\ndpi = 70;\nfontname = Helvetica;\nnode [shape = plaintext];\n");
	GSList *dbo_list, *list;
	dbo_list = gda_meta_struct_get_all_db_objects (mstruct);
	for (list = dbo_list; list; list = list->next) {
		gchar *objname, *fullname;
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		GSList *list;

		/* obj human readable name, and full name */
		fullname = g_strdup_printf ("%s.%s.%s", dbo->obj_catalog, dbo->obj_schema, dbo->obj_name);
		if (dbo->obj_short_name) 
			objname = g_strdup (dbo->obj_short_name);
		else if (dbo->obj_schema)
			objname = g_strdup_printf ("%s.%s", dbo->obj_schema, dbo->obj_name);
		else
			objname = g_strdup (dbo->obj_name);

		/* URL */
		gchar *e0, *e1, *url;
		e0 = gda_rfc1738_encode (dbo->obj_schema);
		e1 = gda_rfc1738_encode (dbo->obj_name);
		url = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);

		/* node */
		switch (dbo->obj_type) {
		case GDA_META_DB_UNKNOWN:
			break;
		case GDA_META_DB_TABLE:
			g_string_append_printf (string, "\"%s\" [URL=\"%s\", tooltip=\"%s\", label=<<TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\">", fullname, url, objname);
			if (dbo == central_dbo) 
				g_string_append_printf (string, "<TR><TD COLSPAN=\"2\" BGCOLOR=\"#729FCF;\" BORDER=\"3\">%s</TD></TR>", objname);
			else
				g_string_append_printf (string, "<TR><TD COLSPAN=\"2\" BGCOLOR=\"lightgrey\" BORDER=\"1\">%s</TD></TR>", objname);
			break;
		case GDA_META_DB_VIEW:
			g_string_append_printf (string, "\"%s\" [URL=\"%s\", tooltip=\"%s\", label=<<TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\">", fullname, url, objname);
			g_string_append_printf (string, "<TR><TD BGCOLOR=\"yellow\" BORDER=\"1\">%s</TD></TR>", objname);
			break;
		default:
			TO_IMPLEMENT;
			g_string_append_printf (string, "\"%s\" [ shape = note label = \"%s\" ];", fullname, objname);
			break;
		}
		g_free (url);

		/* columns, only for tables */
		if (dbo->obj_type == GDA_META_DB_TABLE) {
			GdaMetaTable *mt = GDA_META_TABLE (dbo);
			GSList *depend_dbo_list = NULL;
			for (list = mt->columns; list; list = list->next) {
				GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
				if (tcol->pkey)
					g_string_append_printf (string, "<TR><TD ALIGN=\"left\"><FONT COLOR=\"blue\">%s</FONT></TD></TR>", 
								tcol->column_name);
				else
					g_string_append_printf (string, "<TR><TD ALIGN=\"left\">%s</TD></TR>", 
								tcol->column_name);
			}
			g_string_append (string, "</TABLE>>];\n");
			/* foreign keys */
			for (list = mt->fk_list; list; list = list->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				GString *st = NULL;
				gint fki;
				for (fki = 0; fki < tfk->cols_nb; fki++) {
					if (tfk->fk_names_array && tfk->ref_pk_names_array) {
						if (!st)
							st = g_string_new ("");
						else
							g_string_append (st, "\\n");
						g_string_append_printf (st, "%s = %s",
									tfk->fk_names_array [fki],
									tfk->ref_pk_names_array [fki]);
					}
					else
						break;
				}
				if (tfk->depend_on->obj_type != GDA_META_DB_UNKNOWN) {
					g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\" [fontsize=12, label=\"%s\"];\n", 
								fullname,
								tfk->depend_on->obj_catalog, tfk->depend_on->obj_schema, 
								tfk->depend_on->obj_name,
								st ? st->str : "");
					depend_dbo_list = g_slist_prepend (depend_dbo_list, tfk->depend_on);
				}
				if (st)
					g_string_free (st, TRUE);
			}

			/* dependencies other than foreign keys */
			for (list = dbo->depend_list; list; list = list->next) {
				if (!g_slist_find (depend_dbo_list, list->data)) {
					GdaMetaDbObject *dep_dbo = GDA_META_DB_OBJECT (list->data);
					if (dep_dbo->obj_type != GDA_META_DB_UNKNOWN) 
						g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\";\n", 
									fullname,
									dep_dbo->obj_catalog, dep_dbo->obj_schema, 
									dep_dbo->obj_name);
				}
			}

			g_slist_free (depend_dbo_list);
		}
		else if (dbo->obj_type == GDA_META_DB_VIEW) {
			GdaMetaTable *mt = GDA_META_TABLE (dbo);
			for (list = mt->columns; list; list = list->next) {
				GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
				g_string_append_printf (string, "<TR><TD ALIGN=\"left\">%s</TD></TR>", tcol->column_name);
			}
			g_string_append (string, "</TABLE>>];\n");
			/* dependencies */
			for (list = dbo->depend_list; list; list = list->next) {
				GdaMetaDbObject *ddbo = GDA_META_DB_OBJECT (list->data);
				if (ddbo->obj_type != GDA_META_DB_UNKNOWN) 
					g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\";\n", fullname,
								ddbo->obj_catalog, ddbo->obj_schema, 
								ddbo->obj_name);
			}
		}

		g_free (objname);
		g_free (fullname);
	}
	g_string_append_c (string, '}');
	g_slist_free (dbo_list);

	result = string->str;
	g_string_free (string, FALSE);
	return result;
}

static gboolean
compute_view_details (const ConnectionSetting *cs, HtmlDoc *hdoc, GdaMetaStruct *mstruct,
		      GdaMetaDbObject *dbo, GError **error)
{
	GdaMetaView *view = GDA_META_VIEW (dbo);
	if (view->view_def) {
		xmlNodePtr div, code;
		xmlNewChild (hdoc->content, NULL, "h1", _("View definition:"));
		div = xmlNewChild (hdoc->content, NULL, "div", NULL);
		code = xmlNewChild (div, NULL, "code", view->view_def);
		xmlSetProp (code, "class", (xmlChar*) "ccode");
	}
	return TRUE;
}

/*
 * compute information for an object assuming it's a trigger
 *
 * Returns: TRUE if the object was really a trigger
 */
static gboolean
compute_trigger_content (HtmlDoc *hdoc, WebServer *webserver, const ConnectionSetting *cs,
			 const gchar *schema, const gchar *name, GError **error)
{
	GdaMetaStore *store;
	GdaDataModel *model;
	GValue *v0, *v1;
	GValue *tschema = NULL, *tname = NULL;
	gint i, nrows;
	xmlNodePtr code, div = NULL, sdiv, ul, li;

	store = gda_connection_get_meta_store (cs->cnc);
	g_value_set_string ((v0 = gda_value_new (G_TYPE_STRING)), schema);
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), name);
	model = gda_meta_store_extract (store, "SELECT trigger_short_name, action_statement, event_manipulation, "
					"event_object_schema, event_object_table, trigger_comments, "
					"action_orientation, condition_timing FROM _triggers "
					"WHERE trigger_schema = ##tschema::string AND trigger_name = ##tname::string "
					"ORDER BY event_object_schema, event_object_table, event_manipulation",
					error, "tschema", v0, "tname", v1, NULL);
	gda_value_free (v0);
	gda_value_free (v1);
	if (!model)
		return FALSE;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows == 0) {
		g_object_unref (model);
		return FALSE;
	}

	for (i = 0; i < nrows; i++) {
		const GValue *cv0, *cv1, *cv2, *cv3, *cv4, *cv5, *cv6, *cv7;
		gchar *tmp;

		if (! (cv0 = gda_data_model_get_value_at (model, 0, i, error)))
			break;
		if (! (cv1 = gda_data_model_get_value_at (model, 1, i, error)))
			break;
		if (! (cv2 = gda_data_model_get_value_at (model, 2, i, error)))
			break;
		if (! (cv3 = gda_data_model_get_value_at (model, 3, i, error)))
			break;
		if (! (cv4 = gda_data_model_get_value_at (model, 4, i, error)))
			break;
		if (! (cv5 = gda_data_model_get_value_at (model, 5, i, error)))
			break;
		if (! (cv6 = gda_data_model_get_value_at (model, 6, i, error)))
			break;
		if (! (cv7 = gda_data_model_get_value_at (model, 7, i, error)))
			break;

		if ((!tschema || gda_value_differ (tschema, cv3)) ||
		    (!tname || gda_value_differ (tname, cv4))) {
			if (tschema) 
				xmlNewChild (div, NULL, "br", NULL);
			if (tschema) 
				gda_value_free (tschema);
			if (tname)
				gda_value_free (tname);
			tschema = gda_value_copy (cv3);
			tname = gda_value_copy (cv4);

			tmp = g_strdup_printf (_("Trigger '%s' for the '%s.%s' table:"),
					       g_value_get_string (cv0),
					       g_value_get_string (tschema),
					       g_value_get_string (tname));
			xmlNewChild (hdoc->content, NULL, "h1", tmp);
			g_free (tmp);

			div = xmlNewChild (hdoc->content, NULL, "div", NULL);
		}

		if (G_VALUE_TYPE (cv5) != GDA_TYPE_NULL)
			tmp = g_strdup_printf ("On %s (%s):", g_value_get_string (cv2), g_value_get_string (cv5));
		else
			tmp = g_strdup_printf ("On %s:", g_value_get_string (cv2));
		xmlNewChild (div, NULL, "h2", tmp);
		g_free (tmp);

		sdiv = xmlNewChild (div, NULL, "div", NULL);
		ul = xmlNewChild (sdiv, NULL, "ul", NULL);

		tmp = g_strdup_printf (_("Trigger fired for: %s"), g_value_get_string (cv6));
		li = xmlNewChild (ul, NULL, "li", tmp);
		g_free (tmp);

		tmp = g_strdup_printf (_("Time at which the trigger is fired: %s"), g_value_get_string (cv7));
		li = xmlNewChild (ul, NULL, "li", tmp);
		g_free (tmp);

		li = xmlNewChild (ul, NULL, "li", _("Action:"));
		code = xmlNewChild (li, NULL, "code", g_value_get_string (cv1));
		xmlSetProp (code, "class", (xmlChar*) "ccode");
	}
	
	
	g_object_unref (model);
	return TRUE;
}

/*
 * Give details about a database object
 */
static gboolean
compute_object_content (HtmlDoc *hdoc, WebServer *webserver, const ConnectionSetting *cs,
			const gchar *schema, const gchar *name, GError **error)
{
	GdaMetaStruct *mstruct;
	GdaMetaDbObject *dbo;
	GValue *v0, *v1;
	gboolean retval;

	if (gda_sql_identifier_needs_quotes (schema))
		g_value_take_string ((v0 = gda_value_new (G_TYPE_STRING)),
				     gda_sql_identifier_add_quotes (schema));
	else
		g_value_set_string ((v0 = gda_value_new (G_TYPE_STRING)), schema);
	if (gda_sql_identifier_needs_quotes (name))
		g_value_take_string ((v1 = gda_value_new (G_TYPE_STRING)),
				     gda_sql_identifier_add_quotes (name));
	else
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), name);

	mstruct = gda_meta_struct_new (gda_connection_get_meta_store (cs->cnc),
				       GDA_META_STRUCT_FEATURE_ALL);
	dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, NULL, v0, v1, error);
	gda_value_free (v0);
	gda_value_free (v1);
	if (!dbo) {
		if (compute_trigger_content (hdoc, webserver, cs, schema, name, error))
			return TRUE;
		TO_IMPLEMENT;
		return FALSE;
	}
	
	switch (dbo->obj_type) {
	case GDA_META_DB_TABLE:
		retval = compute_table_details (cs, hdoc, webserver, mstruct, dbo, error);
		break;
	case GDA_META_DB_VIEW:
		retval = compute_table_details (cs, hdoc, webserver, mstruct, dbo, error);
		if (retval)
			retval = compute_view_details (cs, hdoc, mstruct, dbo, error);
		break;
	default:
		TO_IMPLEMENT;
	}
	g_object_unref (mstruct);
	return TRUE;
}


/*
 * Lists all objects of a type (tables, view, ...)
 */
static gboolean
compute_all_objects_content (HtmlDoc *hdoc, const ConnectionSetting *cs, 
			     const gchar *human_obj_type,
			     const gchar *human_obj_type_in_schema,
			     const gchar *table_name,
			     const gchar *obj_prefix,
			     const gchar *filter,
			     GError **error)
{
	gboolean retval = FALSE;
	gchar *rfc_cnc_name;
	GdaMetaStore *store;
	GdaDataModel *model;
	gint i, nrows;
	gchar *sql;
	GValue *schema = NULL;
	xmlNodePtr ul, li, a, div = NULL;
	gboolean content_added = FALSE;

	rfc_cnc_name = gda_rfc1738_encode (cs->name);

	store = gda_connection_get_meta_store (cs->cnc);

	/* objects directly accessible */
	if (filter)
		sql = g_strdup_printf ("SELECT %s_schema, %s_name FROM %s WHERE %s "
				       "AND %s_short_name != %s_full_name ORDER BY %s_schema, %s_name",
				       obj_prefix, obj_prefix, table_name, filter, obj_prefix, obj_prefix,
				       obj_prefix, obj_prefix);
	else
		sql = g_strdup_printf ("SELECT %s_schema, %s_name FROM %s WHERE "
				       "%s_short_name != %s_full_name ORDER BY %s_schema, %s_name",
				       obj_prefix, obj_prefix, table_name, obj_prefix, obj_prefix,
				       obj_prefix, obj_prefix);

	model = gda_meta_store_extract (store, sql, error, NULL);
	g_free (sql);
	if (!model)
		goto out;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows > 0) {
		xmlNewChild (hdoc->content, NULL, "h1", human_obj_type);
		div = xmlNewChild (hdoc->content, NULL, "div", NULL);
		xmlSetProp (div, "class", "clist");
		ul = xmlNewChild (div, NULL, "ul", NULL);
		content_added = TRUE;
	}
	for (i = 0; i < nrows; i++) {
		const GValue *cv0, *cv1;
		gchar *tmp, *e0, *e1;
		cv0 = gda_data_model_get_value_at (model, 0, i, error);
		if (!cv0)
			goto out;
		cv1 = gda_data_model_get_value_at (model, 1, i, error);
		if (!cv1)
			goto out;
		li = xmlNewChild (ul, NULL, "li", NULL);
		a = xmlNewChild (li, NULL, "a", g_value_get_string (cv1));
		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, "href", tmp);
		g_free (tmp);
		tmp = g_strdup_printf ("%s.%s", g_value_get_string (cv0), g_value_get_string (cv1));
		xmlSetProp (a, "title", tmp);
		g_free (tmp);
	}
	if (nrows > 0)
		xmlNewChild (div, NULL, "br", NULL);
	g_object_unref (model);

	/* objects listed by schema */
	if (filter)
		sql = g_strdup_printf ("SELECT %s_schema, %s_name FROM %s WHERE %s "
				       "ORDER BY %s_schema, %s_name",
				       obj_prefix, obj_prefix, table_name, filter, obj_prefix, obj_prefix);
	else
		sql = g_strdup_printf ("SELECT %s_schema, %s_name FROM %s "
				       "ORDER BY %s_schema, %s_name",
				       obj_prefix, obj_prefix, table_name,  obj_prefix, obj_prefix);
	model = gda_meta_store_extract (store, sql, error, NULL);
	g_free (sql);
	if (!model)
		goto out;

	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv0, *cv1;
		gchar *tmp, *e0, *e1;

		cv0 = gda_data_model_get_value_at (model, 0, i, error);
		if (!cv0)
			goto out;
		cv1 = gda_data_model_get_value_at (model, 1, i, error);
		if (!cv1)
			goto out;
		if (!schema || gda_value_differ (schema, cv0)) {
			xmlNodePtr header;
			gchar *tmp;
			if (schema) {
				xmlNewChild (div, NULL, "br", NULL);
				gda_value_free (schema);
			}
			schema = gda_value_copy (cv0);
			tmp = g_strdup_printf (human_obj_type_in_schema, g_value_get_string (schema));
			header = xmlNewChild (hdoc->content, NULL, "h1", tmp);
			g_free (tmp);
			content_added = TRUE;
			div = xmlNewChild (hdoc->content, NULL, "div", NULL);
			xmlSetProp (div, "class", "clist");
			ul = xmlNewChild (div, NULL, "ul", NULL);
		}

		li = xmlNewChild (ul, NULL, "li", NULL);
		a = xmlNewChild (li, NULL, "a", g_value_get_string (cv1));
		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, "href", tmp);
		g_free (tmp);
	}
	if (nrows != 0)
		xmlNewChild (div, NULL, "br", NULL);
	retval = TRUE;

	if (! content_added) 
		xmlNewChild (hdoc->content, NULL, "br", NULL);

 out:
	if (schema)
		gda_value_free (schema);
	if (model)
		g_object_unref (model);
	g_free (rfc_cnc_name);

	return retval;
}

/*
 * Lists all objects of a type (tables, view, ...)
 */
static gboolean
compute_all_triggers_content (HtmlDoc *hdoc, const ConnectionSetting *cs, GError **error)
{
	gboolean retval = FALSE;
	gchar *rfc_cnc_name;
	GdaMetaStore *store;
	GdaDataModel *model;
	gint i, nrows;
	GValue *schema = NULL, *tschema = NULL, *tname = NULL;
	xmlNodePtr ul, sul, li, a, div = NULL, sdiv = NULL;
	gboolean content_added = FALSE;

	rfc_cnc_name = gda_rfc1738_encode (cs->name);

	store = gda_connection_get_meta_store (cs->cnc);

	model = gda_meta_store_extract (store, "SELECT trigger_schema, trigger_name, event_manipulation, "
					"event_object_schema, event_object_table FROM _triggers WHERE "
					"trigger_short_name != trigger_full_name "
					"ORDER BY trigger_schema, trigger_name, event_object_schema, "
					"event_object_table, event_manipulation",
					error, NULL);
	if (!model)
		goto out;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows > 0) {
		xmlNewChild (hdoc->content, NULL, "h1", _("Triggers:"));
		div = xmlNewChild (hdoc->content, NULL, "div", NULL);
		xmlSetProp (div, "class", "clist");
		content_added = TRUE;
	}
	for (i = 0; i < nrows; i++) {
		const GValue *cv0, *cv1, *cv2, *cv3, *cv4;
		gchar *tmp, *e0, *e1;

		if (! (cv0 = gda_data_model_get_value_at (model, 0, i, error)))
			goto out;
		if (! (cv1 = gda_data_model_get_value_at (model, 1, i, error)))
			goto out;
		if (! (cv2 = gda_data_model_get_value_at (model, 2, i, error)))
			goto out;
		if (! (cv3 = gda_data_model_get_value_at (model, 3, i, error)))
			goto out;
		if (! (cv4 = gda_data_model_get_value_at (model, 4, i, error)))
			goto out;
		
		if ((!tschema || gda_value_differ (tschema, cv3)) ||
		    (!tname || gda_value_differ (tname, cv4))) {
			gchar *tmp;
			if (tschema) 
				xmlNewChild (sdiv, NULL, "br", NULL);
			if (tschema) 
				gda_value_free (tschema);
			if (tname)
				gda_value_free (tname);
			tschema = gda_value_copy (cv3);
			tname = gda_value_copy (cv4);

			tmp = g_strdup_printf (_("For the '%s.%s' table:"), g_value_get_string (tschema),
					       g_value_get_string (tname));
			xmlNewChild (div, NULL, "h2", tmp);
			g_free (tmp);

			sdiv = xmlNewChild (div, NULL, "div", NULL);
			xmlSetProp (sdiv, "class", "clist");
			sul = xmlNewChild (sdiv, NULL, "ul", NULL);
		}

		li = xmlNewChild (sul, NULL, "li", NULL);
		tmp = g_strdup_printf ("%s (%s)", g_value_get_string (cv1), g_value_get_string (cv2));
		a = xmlNewChild (li, NULL, "a", tmp);
		g_free (tmp);

		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, "href", tmp);
		g_free (tmp);
	}
	if (nrows > 0)
		xmlNewChild (sdiv, NULL, "br", NULL);
	g_object_unref (model);
	if (tschema) {
		gda_value_free (tschema);
		tschema = NULL;
	}
	if (tname) {
		gda_value_free (tname);
		tname = NULL;
	}

	/* objects listed by schema */
	model = gda_meta_store_extract (store, "SELECT trigger_schema, trigger_name, event_manipulation, "
					"event_object_schema, event_object_table FROM _triggers "
					"ORDER BY trigger_schema, trigger_name, event_object_schema, "
					"event_object_table, event_manipulation",
					error, NULL);
	if (!model)
		goto out;

	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv0, *cv1, *cv2, *cv3, *cv4;
		gchar *tmp, *e0, *e1;

		if (! (cv0 = gda_data_model_get_value_at (model, 0, i, error)))
			goto out;
		if (! (cv1 = gda_data_model_get_value_at (model, 1, i, error)))
			goto out;
		if (! (cv2 = gda_data_model_get_value_at (model, 2, i, error)))
			goto out;
		if (! (cv3 = gda_data_model_get_value_at (model, 3, i, error)))
			goto out;
		if (! (cv4 = gda_data_model_get_value_at (model, 4, i, error)))
			goto out;
		if (!schema || gda_value_differ (schema, cv0)) {
			gchar *tmp;
			if (schema) {
				xmlNewChild (sdiv, NULL, "br", NULL);
				gda_value_free (schema);
			}
			schema = gda_value_copy (cv0);
			tmp = g_strdup_printf (_("Triggers in the '%s' schema:"), g_value_get_string (schema));
			xmlNewChild (hdoc->content, NULL, "h1", tmp);
			g_free (tmp);
			content_added = TRUE;
			div = xmlNewChild (hdoc->content, NULL, "div", NULL);
			xmlSetProp (div, "class", "clist");

			if (tschema) {
				gda_value_free (tschema);
				tschema = NULL;
			}
			if (tname) {
				gda_value_free (tname);
				tname = NULL;
			}
		}
		if ((!tschema || gda_value_differ (tschema, cv3)) ||
		    (!tname || gda_value_differ (tname, cv4))) {
			gchar *tmp;
			if (tschema) 
				xmlNewChild (sdiv, NULL, "br", NULL);
			if (tschema) 
				gda_value_free (tschema);
			if (tname)
				gda_value_free (tname);
			tschema = gda_value_copy (cv3);
			tname = gda_value_copy (cv4);

			tmp = g_strdup_printf (_("For the '%s.%s' table:"), g_value_get_string (tschema),
					       g_value_get_string (tname));
			xmlNewChild (div, NULL, "h2", tmp);
			g_free (tmp);

			sdiv = xmlNewChild (div, NULL, "div", NULL);
			xmlSetProp (sdiv, "class", "clist");
			sul = xmlNewChild (sdiv, NULL, "ul", NULL);
		}

		li = xmlNewChild (sul, NULL, "li", NULL);
		tmp = g_strdup_printf ("%s (%s)", g_value_get_string (cv1), g_value_get_string (cv2));
		a = xmlNewChild (li, NULL, "a", tmp);
		g_free (tmp);

		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, "href", tmp);
		g_free (tmp);
	}
	if (nrows != 0)
		xmlNewChild (sdiv, NULL, "br", NULL);
	retval = TRUE;

	if (! content_added) 
		xmlNewChild (hdoc->content, NULL, "br", NULL);

 out:
	if (schema)
		gda_value_free (schema);
	if (tschema) 
		gda_value_free (tschema);
	if (tname)
		gda_value_free (tname);

	if (model)
		g_object_unref (model);
	g_free (rfc_cnc_name);

	return retval;
}
