/*
 * Copyright (C) 2008 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "web-server.h"
#include <libsoup/soup.h>
#include "html-doc.h"
#include <libgda/binreloc/gda-binreloc.h>
#include "common/t-web-context.h"
#include "common/t-utils.h"
#include "common/t-connection.h"
#include "t-app.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#define MAX_CHALLENGES 10
#define MAX_AUTH_COOKIES 10

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
	GHashTable  *resources_hash; /* key = a path without the starting '/', value = a TmpResource pointer */
	GSList      *resources_list; /* list of the TmpResource pointers in @resources_hash, memory not managed here */
	guint        resources_timer;

	/* authentication */
	gchar       *token; /* FIXME: protect it! */
	GArray      *challenges; /* array of TimedString representing the currently valid challenges */
	GArray      *cookies; /* array of TimedString representing the currently valid authentication cookie values */
	guint        auth_timer;

	GSList      *terminals_list; /* list of TContext */
	guint        term_timer;
};


typedef struct {
	gchar    *string;
	GTimeVal  validity;
} TimedString;

static TimedString *timed_string_new (guint duration);
static void         timed_string_free (TimedString *ts);

static TimedString *challenge_add (WebServer *server);
static void         challenges_manage (WebServer *server);

static TimedString *auth_cookie_add (WebServer *server);
static void         auth_cookies_manage (WebServer *server);


/*
 * Temporary available data
 *
 * Each TmpResource structure represents a resource which will be available for some time (until it has
 * expired).
 *
 * If the expiration_date attribute is set to 0, then there is no expiration at all.
 */
typedef struct {
	gchar *path;
	gchar *data;
	gsize  size;
	int    expiration_date; /* 0 to avoid expiration */
} TmpResource;

static TmpResource  *tmp_resource_add (WebServer *server, const gchar *path, gchar *data, gsize data_length);
static gboolean       delete_tmp_resource (WebServer *server);
static void           tmp_resource_free (TmpResource *data);

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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (WebServerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) web_server_class_init,
			NULL,
			NULL,
			sizeof (WebServer),
			0,
			(GInstanceInitFunc) web_server_init,
			0
		};
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "WebServer", &info, 0);
		g_mutex_unlock (&registering);
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
	server->priv->resources_timer = 0;
	server->priv->resources_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify) tmp_resource_free);
	server->priv->resources_list = NULL;

	server->priv->token = g_strdup ("");
	server->priv->challenges = g_array_new (FALSE, FALSE, sizeof (TimedString*));
	server->priv->cookies = g_array_new (FALSE, FALSE, sizeof (TimedString*));
	server->priv->auth_timer = 0;

	server->priv->terminals_list = NULL;
	server->priv->term_timer = 0;
}

static void     get_cookies (SoupMessage *msg, ...);
static gboolean get_file (WebServer *server, SoupMessage *msg, const char *path, GError **error);
static void     get_root (WebServer *server, SoupMessage *msg);
static void     get_for_console (WebServer *server, SoupMessage *msg);
static gboolean get_for_cnc (WebServer *webserver, SoupMessage *msg, 
			     TConnection *tcnc, gchar **extra, GError **error);
static gboolean get_auth (WebServer *server, SoupMessage *msg, GHashTable *query);
static gboolean get_post_for_irb (WebServer *webserver, SoupMessage *msg, 
				  TConnection *tcnc, GHashTable *query, GError **error);
static void     get_for_cnclist (WebServer *webserver, SoupMessage *msg, gboolean is_authenticated);


/*#define DEBUG_SERVER*/
#ifdef DEBUG_SERVER
static void
debug_display_query (gchar *key, gchar *value, gpointer data)
{
	g_print ("\t%s => %s\n", key, value);
}
#endif

static void
server_callback (G_GNUC_UNUSED SoupServer *server, SoupMessage *msg,
                 const char *path, GHashTable *query,
                 G_GNUC_UNUSED SoupClientContext *context, WebServer *webserver)
{
#ifdef DEBUG_SERVER
        printf ("%s %s HTTP/1.%d\n", msg->method, path, soup_message_get_http_version (msg));
	/*
        SoupMessageHeadersIter iter;
        const char *name, *value;
        soup_message_headers_iter_init (&iter, msg->request_headers);
        while (soup_message_headers_iter_next (&iter, &name, &value))
                printf ("%s: %s\n", name, value);
	*/
        if (msg->request_body->length)
                printf ("Request body: %s\n", msg->request_body->data);
	if (query) {
		printf ("Query parts:\n");
		g_hash_table_foreach (query, (GHFunc) debug_display_query, NULL);
	}
#endif
	
	GError *error = NULL;
	gboolean ok = TRUE;
	gboolean done = FALSE;
	TmpResource *tmpdata;
	if ((*path != '/') || (*path && (path[1] == '/'))) {
		soup_message_set_status_full (msg, SOUP_STATUS_UNAUTHORIZED, "Wrong path name");
		return;
	}
	path++;

	/* check for authentication */
	gboolean auth_needed = TRUE;
	if (g_str_has_suffix (path, ".js") ||
	    g_str_has_suffix (path, ".css"))
		auth_needed = FALSE;
	if (auth_needed) {
		/* check cookie named "coa" */
		gchar *cookie;
		get_cookies (msg, "coa", &cookie, NULL);

		if (cookie) {
			gsize n;
			for (n = 0; n < webserver->priv->cookies->len; n++) {
				TimedString *ts = g_array_index (webserver->priv->cookies, TimedString *, n);
#ifdef DEBUG_SERVER
				g_print ("CMP Cookie %s with msg's cookie %s\n",
					 ts->string, cookie);
#endif
				if (!strcmp (ts->string, cookie)) {
					/* cookie exists => we are authenticated */
					auth_needed = FALSE;
					break;
				}
			}
			g_free (cookie);
		}

		if (auth_needed && !g_str_has_suffix (path, "~cnclist")) {
			if (!get_auth (webserver, msg, query))
				return;
		}
	}
	
	if (*path == 0) {
		if (msg->method == SOUP_METHOD_GET)  {
			get_root (webserver, msg);
			done = TRUE;
		}
	}
	else if ((tmpdata = g_hash_table_lookup (webserver->priv->resources_hash, path))) {
		if (msg->method == SOUP_METHOD_GET) {
			soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC,
						  tmpdata->data, tmpdata->size);
			soup_message_set_status (msg, SOUP_STATUS_OK);
			done = TRUE;
		}
	}
	else {
		gchar **array = NULL;
		array = g_strsplit (path, "/", 0);
		
		if (array) {
			TConnection *tcnc;
			tcnc = t_connection_get_by_name (array[0]);
		
			if (tcnc) {
				if (msg->method == SOUP_METHOD_GET) {
					ok = get_for_cnc (webserver, msg, tcnc, array[1] ? &(array[1]) : NULL, &error);
					done = TRUE;
				}
			}
			else if (!strcmp (path, "~console")) {
				get_for_console (webserver, msg);
				done = TRUE;
			}
			else if (!strcmp (path, "~irb")) {
				ok = get_post_for_irb (webserver, msg, tcnc, query, &error);
				done = TRUE;
			}
			else if (!strcmp (path, "~cnclist")) {
				get_for_cnclist (webserver, msg, !auth_needed);
				done = TRUE;
			}
			else {
				if (msg->method == SOUP_METHOD_GET) {
					ok = get_file (webserver, msg, path, &error);
					done = TRUE;
				}
			}

			g_strfreev (array);
		}
		else {
			ok= FALSE;
			done = TRUE;
		}
	}
	
	if (!ok) {
		if (error) {
			soup_message_set_status_full (msg, error->code, error->message);
			g_error_free (error);
		}
		else
			soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
	}
        
	if (!done)
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

#ifdef DEBUG_SERVER
        printf ("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);
#endif
}

/**
 * web_server_new:
 * @port: the port to listen on
 * @auth_token: (nullable): the authentication token, or %NULL
 *
 * Creates a new server of type @type
 *
 * Returns: a new #WebServer object
 */
WebServer *
web_server_new (gint port, const gchar *auth_token)
{
	WebServer *server;

	server = (WebServer*) g_object_new (WEB_TYPE_SERVER, NULL);
	server->priv->server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "gda-sql-httpd ",
						NULL);
	soup_server_add_handler (server->priv->server, NULL,
                                 (SoupServerCallback) server_callback, server, NULL);

	if (auth_token) {
		g_free (server->priv->token);
		server->priv->token = g_strdup (auth_token);
	}

	if (! soup_server_listen_local (server->priv->server, port, SOUP_SERVER_LISTEN_IPV4_ONLY, NULL)) {
		g_object_unref (server->priv->server);
		server->priv->server = NULL;
		g_object_unref (server);
		return NULL;
	}

	return server;
}


static void
web_server_dispose (GObject *object)
{
	WebServer *server;

	server = WEB_SERVER (object);
	if (server->priv) {
		if (server->priv->resources_hash) {
			g_hash_table_destroy (server->priv->resources_hash);
			server->priv->resources_hash = NULL;
		}
		if (server->priv->resources_list) {
			g_slist_free (server->priv->resources_list);
			server->priv->resources_list = NULL;
		}
		if (server->priv->server) {
			g_object_unref (server->priv->server);
			server->priv->server = NULL;
		}
		if (server->priv->resources_timer) {
			g_source_remove (server->priv->resources_timer);
			server->priv->resources_timer = 0;
		}
		if (server->priv->auth_timer) {
			g_source_remove (server->priv->auth_timer);
			server->priv->auth_timer = 0;
		}
		if (server->priv->term_timer) {
			g_source_remove (server->priv->term_timer);
			server->priv->term_timer = 0;
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
		gsize i;
		for (i = 0; i < server->priv->challenges->len; i++) {
			TimedString *ts = g_array_index (server->priv->challenges, TimedString *, i);
			timed_string_free (ts);
		}
		g_array_free (server->priv->challenges, TRUE);

		for (i = 0; i < server->priv->cookies->len; i++) {
			TimedString *ts = g_array_index (server->priv->cookies, TimedString *, i);
			timed_string_free (ts);
		}
		g_array_free (server->priv->cookies, TRUE);

		if (server->priv->terminals_list) {
			g_slist_foreach (server->priv->terminals_list, (GFunc) g_object_unref, NULL);
			g_slist_free (server->priv->terminals_list);
		}
		g_free (server->priv);
	}

	/* parent class */
	parent_class->finalize (object);
}

/*
 *
 * Server GET/POST methods
 *
 */
static HtmlDoc *create_new_htmldoc (WebServer *webserver, TConnection *tcnc);
static void get_variables (SoupMessage *msg, GHashTable *query, ...);

/*
 * GET for a file
 */
static gboolean
get_file (G_GNUC_UNUSED WebServer *server, SoupMessage *msg, const char *path, GError **error)
{
	GMappedFile *mfile;
	gchar *real_path;

	real_path = gda_gbr_get_file_path (GDA_DATA_DIR, "libgda-6.0", "web", path, NULL);
	if (!real_path)
		return FALSE;

	/*g_print ("get_file () => %s\n", real_path);*/
	if (!g_file_test (real_path, G_FILE_TEST_EXISTS)) {
		/* test if we are in the compilation directory */
		gchar *cwd, *tmp;
		cwd = g_get_current_dir ();
		tmp = g_build_filename (cwd, "gda-sql.c", NULL);
		if (g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_free (real_path);
			real_path = g_build_filename (cwd, path, NULL);
		}
		else {
			g_free (cwd);
			return FALSE;
		}
		g_free (cwd);
	}
        mfile = g_mapped_file_new (real_path, FALSE, error);
	g_free (real_path);
	if (!mfile)
		return FALSE;

	SoupBuffer *buffer;
	buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mfile),
					     g_mapped_file_get_length (mfile),
					     mfile, (GDestroyNotify) g_mapped_file_unref);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	soup_message_set_status (msg, SOUP_STATUS_OK);
	return TRUE;
}

/*
 * Creates a login form
 *
 * Returns: TRUE if the user is now authenticated, or FALSE if the user needs to authenticate
 */
static gboolean
get_auth (WebServer *server, SoupMessage *msg, GHashTable *query)
{
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;

	hdoc = html_doc_new (_("Authentication required"));
	xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("Authentication required"));
	xmlNewChild (xmlNewChild (hdoc->content, NULL, BAD_CAST "p", NULL),
		     NULL, BAD_CAST "small", BAD_CAST _("Enter authentification token as set "
							"from console, or leave empty if "
							"none required"));
	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");

	/* check to see if this page is called as an answer to authentication */
	gchar *token = NULL;
	get_variables (msg, query, "etoken", &token, NULL);

	if (token) {
		gsize n;
		for (n = 0; n < server->priv->challenges->len; n++) {
			TimedString *ts = g_array_index (server->priv->challenges, TimedString *, n);
			gchar *md5str;
			
			md5str = g_compute_hmac_for_string (G_CHECKSUM_MD5,
							    (const guchar*) server->priv->token, strlen (server->priv->token),
							    ts->string, -1);
			
			if (!strcmp (md5str, token)) {
				/* forge a new location change message */
				GString *cook;
				gchar *tmp;
				TimedString *new_cookie;
				
				new_cookie = auth_cookie_add (server);
				cook = g_string_new ("");
				tmp = gda_rfc1738_encode (new_cookie->string);
				g_string_append_printf (cook, "coa=%s; path=/", tmp);
				g_free (tmp);
				soup_message_headers_append (msg->response_headers, "Set-Cookie", cook->str);
				g_string_free (cook, TRUE);
				
				/*soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
				  soup_message_headers_append (msg->response_headers, "Location", "/");*/
				
				g_free (md5str);
				g_free (token);
				
				return TRUE;
			}
			g_free (md5str);
		}
		
		g_free (token);
	}
	

	/* Add javascript */
	xmlNodePtr form, node;
	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/md5.js");

	/* login form */
	TimedString *new_challenge = challenge_add (server);
	form = xmlNewChild (hdoc->content, NULL, BAD_CAST "form", NULL);
	gchar *str = g_strdup_printf ("javascript:etoken.value=hex_hmac_md5(token.value, '%s'); javascript:token.value=''", new_challenge->string);
	xmlSetProp (form, BAD_CAST "onsubmit", BAD_CAST str);
	g_free (str);
	
	node = xmlNewChild (form, NULL, BAD_CAST "input", BAD_CAST _("Token:"));
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "hidden");
	xmlSetProp (node, BAD_CAST "name", BAD_CAST "etoken");

	node = xmlNewChild (form, NULL, BAD_CAST "input", NULL);
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "password");
	xmlSetProp (node, BAD_CAST "name", BAD_CAST "token");

	node = xmlNewChild (form, NULL, BAD_CAST "input", NULL);
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "submit");
	xmlSetProp (node, BAD_CAST "value", BAD_CAST "login");
	xmlSetProp (node, BAD_CAST "colspan", BAD_CAST "2");

	xstr = html_doc_to_string (hdoc, &size);
	buffer = soup_buffer_new_with_owner (xstr, size, xstr, (GDestroyNotify)xmlFree);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	html_doc_free (hdoc);

	soup_message_set_status (msg, SOUP_STATUS_OK);
	return FALSE;
}

/*
 * GET for the / path
 */
static void
get_root (WebServer *server, SoupMessage *msg)
{
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;

	const GSList *list;
	list = t_app_get_all_connections ();
	if (list && !list->next) {
		/* only 1 connection => go to this one */
		TConnection *tcnc = (TConnection*) list->data;
		soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
		soup_message_headers_append (msg->response_headers, "Location", t_connection_get_name (tcnc));
		return;
	}
	else if (list) {
		/* more than one connection, redirect to the current one */
		TConnection *tcnc = t_context_get_connection (t_app_get_term_console ());
		soup_message_set_status (msg, SOUP_STATUS_TEMPORARY_REDIRECT);
		soup_message_headers_append (msg->response_headers, "Location", t_connection_get_name (tcnc));
		return;
	}

	hdoc = create_new_htmldoc (server, NULL);
	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");
	xstr = html_doc_to_string (hdoc, &size);
	buffer = soup_buffer_new_with_owner (xstr, size, xstr, (GDestroyNotify)xmlFree);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	html_doc_free (hdoc);

	soup_message_set_status (msg, SOUP_STATUS_OK);
}

static xmlNodePtr
cnc_ul (WebServer *server, gboolean is_authenticated)
{
	xmlNodePtr ul, li, a;
	const GSList *clist, *list;
	gchar *str;

	/* other connections in the sidebar */
	if (is_authenticated)
		list = t_app_get_all_connections ();
	else
		list = NULL;
	ul = xmlNewNode (NULL, BAD_CAST "ul");
	xmlNodeSetContent(ul, BAD_CAST _("Connections"));
	xmlSetProp (ul, BAD_CAST "id", BAD_CAST "cnclist");

	if (!list) {
		/* no connection at all or not authenticated */
		str = g_strdup_printf ("(%s)",  _("None"));
		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST str);
		g_free (str);
	}
	else {
		for (clist = list; clist; clist = clist->next) {
			gchar *tmp;
			TConnection *tcnc2 = (TConnection*) clist->data;
			
			li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
			a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST t_connection_get_name (tcnc2));
			tmp = gda_rfc1738_encode (t_connection_get_name (tcnc2));
			str = g_strdup_printf ("/%s", tmp);
			g_free (tmp);
			xmlSetProp (a, BAD_CAST "href", BAD_CAST  str);
			g_free (str);
		}
	}
	return ul;
}

static void
get_for_cnclist (WebServer *webserver, SoupMessage *msg, gboolean is_authenticated)
{
	xmlNodePtr ul;
	SoupBuffer *buffer;

	ul = cnc_ul (webserver, is_authenticated);
	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");

	xmlBufferPtr buf;
	buf = xmlBufferCreate ();
	xmlNodeDump (buf, NULL, ul, 1, 1);
	xmlFreeNode (ul);
	
	buffer = soup_buffer_new (SOUP_MEMORY_TEMPORARY, (gchar *) xmlBufferContent (buf),
				  strlen ((gchar *) xmlBufferContent (buf)));
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	xmlBufferFree (buf);

	soup_message_set_status (msg, SOUP_STATUS_OK);
}

/*
 * GET for the /~console path
 */
static void
get_for_console (WebServer *server, SoupMessage *msg)
{
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;

	xmlNodePtr div = NULL, node;

	hdoc = create_new_htmldoc (server, NULL);
	xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("SQL console:"));

	div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
	xmlSetProp (div, BAD_CAST "id", BAD_CAST "terminal");

	div = xmlNewChild (div, NULL, BAD_CAST "div", BAD_CAST "");
	xmlSetProp (div, BAD_CAST "id", BAD_CAST "irb");

	div = xmlNewChild (hdoc->footer, NULL, BAD_CAST "input", NULL);
	xmlSetProp (div, BAD_CAST "class", BAD_CAST "keyboard-selector-input");
	xmlSetProp (div, BAD_CAST "type", BAD_CAST "text");
	xmlSetProp (div, BAD_CAST "id", BAD_CAST "irb_input");
	xmlSetProp (div, BAD_CAST "autocomplete", BAD_CAST "off");
	
	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/jquery.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/jquery-ui.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/mouseapp_2.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/mouseirb_2.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/irb.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "script", BAD_CAST " ");
	xmlSetProp (node, BAD_CAST "type", BAD_CAST "text/javascript");
	xmlSetProp (node, BAD_CAST "src", BAD_CAST "/cnc.js");

	node = xmlNewChild (hdoc->head, NULL, BAD_CAST "link", BAD_CAST "");
	xmlSetProp(node, BAD_CAST "href", (xmlChar*)"/irb.css");
	xmlSetProp(node, BAD_CAST "rel", (xmlChar*)"stylesheet");
	xmlSetProp(node, BAD_CAST "type", (xmlChar*)"text/css");

	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/html");
	xstr = html_doc_to_string (hdoc, &size);
	buffer = soup_buffer_new_with_owner (xstr, size, xstr, (GDestroyNotify)xmlFree);
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	html_doc_free (hdoc);

	soup_message_set_status (msg, SOUP_STATUS_OK);
}

static gboolean compute_all_objects_content (HtmlDoc *hdoc, TConnection *tcnc, 
					     const gchar *human_obj_type,
					     const gchar *human_obj_type_in_schema,
					     const gchar *table_name,
					     const gchar *obj_prefix,
					     const gchar *filter,
					     GError **error);
static gboolean compute_all_triggers_content (HtmlDoc *hdoc, TConnection *tcnc, GError **error);
static gboolean compute_object_content (HtmlDoc *hdoc, WebServer *webserver, TConnection *tcnc,
					const gchar *schema, const gchar *name, GError **error);
/*
 * GET method for a connection
 */
static gboolean
get_for_cnc (WebServer *webserver, SoupMessage *msg, TConnection *tcnc, gchar **extra, GError **error)
{
	gboolean retval = FALSE;
	HtmlDoc *hdoc;
	xmlChar *xstr;
	SoupBuffer *buffer;
	gsize size;

	hdoc = create_new_htmldoc (webserver, tcnc);
	
	if (!extra || !strcmp (extra[0], "~tables")) {
		if (! compute_all_objects_content (hdoc, tcnc, 
						   _("Tables"), _("Tables in the '%s' schema"), 
						   "_tables", "table", "table_type LIKE \"%TABLE%\"", error))
			goto onerror;
	}
	else if (!strcmp (extra[0], "~views")) {
		if (! compute_all_objects_content (hdoc, tcnc, 
						   _("Views"), _("Views in the '%s' schema"), 
						   "_tables", "table", "table_type LIKE \"%VIEW%\"", error))
			goto onerror;
	}
	else if (!strcmp (extra[0], "~triggers")) {
		if (! compute_all_triggers_content (hdoc, tcnc, error))
			goto onerror;
	}
	else {
		/* extra [0] has to be a schema */
		if (extra [1]) {
			if (! compute_object_content (hdoc, webserver, tcnc, extra [0], extra [1], error))
				goto onerror;
		}
		else
			xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST "Not yet implemented");
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

static gchar *meta_struct_dump_as_graph (TConnection *tcnc, GdaMetaStruct *mstruct,
					 GdaMetaDbObject *central_dbo, GError **error);
static gboolean
compute_table_details (TConnection *tcnc, HtmlDoc *hdoc, WebServer *webserver,
		       GdaMetaStruct *mstruct, GdaMetaDbObject *dbo, GError **error)
{
	gchar *tmp;
	xmlNodePtr div, table, tr, td;
	GdaMetaTable *mt = GDA_META_TABLE (dbo);
	GSList *list;
	GdaMetaStore *store;

	tmp = g_strdup_printf (_("Columns for the '%s' table:"), dbo->obj_short_name);
	xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST tmp);
	g_free (tmp);

	div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
	table = xmlNewChild (div, NULL, BAD_CAST "table", NULL);
	xmlSetProp (table, BAD_CAST "class", (xmlChar*) BAD_CAST "ctable");
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST _("Column"));
	td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST _("Type"));
	td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST _("Nullable"));
	td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST _("Default"));
	td = xmlNewChild (tr, NULL, BAD_CAST "th", BAD_CAST _("Extra"));

	for (list = mt->columns; list; list = list->next) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);

		tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
		td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST tcol->column_name);
		if (tcol->pkey)
			xmlSetProp (td, BAD_CAST "class", BAD_CAST "pkey");
		td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST tcol->column_type);
		td = xmlNewChild (tr, NULL, BAD_CAST "td", tcol->nullok ? BAD_CAST _("yes") : BAD_CAST _("no"));
		td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST tcol->default_value);

		if (tcol->auto_incement) {
			td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST _("Auto increment"));
		} else {
			td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
		}
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
		xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("Primary key:"));
		div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
		for (ipk = 0; ipk < dbo_table->pk_cols_nb; ipk++) {
			GdaMetaTableColumn *tcol;
			if (!ul)
				ul = xmlNewChild (div, NULL, BAD_CAST "ul", NULL);

			tcol = g_slist_nth_data (dbo_table->columns, ipk);
			xmlNewChild (ul, NULL, BAD_CAST "li", tcol->column_name);
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
	graph = meta_struct_dump_as_graph (tcnc, mstruct, dbo, NULL);
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
					tmp_filename = g_strdup_printf ("~tmp/g%d", counter);
					tmp_resource_add (webserver, tmp_filename, file_data, file_data_len);
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
		xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("Relations:"));
		div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);

		if (map_node)
			xmlAddChild (div, map_node);

		xmlSetProp (div, BAD_CAST "class", BAD_CAST  "graph");
		obj = xmlNewChild (div, NULL, BAD_CAST "img", NULL);
		tmp = g_strdup_printf ("/%s", tmp_filename);
		xmlSetProp (obj, BAD_CAST "src", BAD_CAST tmp);
		xmlSetProp (obj, BAD_CAST "usemap", BAD_CAST  "#G");
		g_free (tmp);
		g_free (tmp_filename);
	}
	else {
		/* list foreign keys as we don't have a graph */
		if (dbo_table->fk_list) {
			xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("Foreign keys:"));
			GSList *list;
			for (list = dbo_table->fk_list; list; list = list->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				GdaMetaDbObject *fkdbo = tfk->depend_on;
				xmlNodePtr ul = NULL;
				gint ifk;
				
				div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
				for (ifk = 0; ifk < tfk->cols_nb; ifk++) {
					gchar *tmp;
					if (!ul) {
						tmp = g_strdup_printf (_("To '%s':"), fkdbo->obj_short_name);
						ul = xmlNewChild (div, NULL, BAD_CAST "ul", BAD_CAST tmp);
						g_free (tmp);
					}
					tmp = g_strdup_printf ("%s --> %s.%s", 
							       tfk->fk_names_array[ifk], 
							       fkdbo->obj_short_name, tfk->ref_pk_names_array[ifk]);
					xmlNewChild (ul, NULL, BAD_CAST "li", BAD_CAST tmp);
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
meta_struct_dump_as_graph (TConnection *tcnc, GdaMetaStruct *mstruct, GdaMetaDbObject *central_dbo, 
			   G_GNUC_UNUSED GError **error)
{
	GString *string;
	gchar *result;
	gchar *rfc_cnc_name;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	rfc_cnc_name = gda_rfc1738_encode (t_connection_get_name (tcnc));
	
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
compute_view_details (G_GNUC_UNUSED TConnection *tcnc, HtmlDoc *hdoc,
		      G_GNUC_UNUSED GdaMetaStruct *mstruct, GdaMetaDbObject *dbo,
		      G_GNUC_UNUSED GError **error)
{
	GdaMetaView *view = GDA_META_VIEW (dbo);
	if (view->view_def) {
		xmlNodePtr div, code;
		xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("View definition:"));
		div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
		code = xmlNewChild (div, NULL, BAD_CAST "code", BAD_CAST view->view_def);
		xmlSetProp (code, BAD_CAST "class", BAD_CAST "ccode");
	}
	return TRUE;
}

/*
 * compute information for an object assuming it's a trigger
 *
 * Returns: TRUE if the object was really a trigger
 */
static gboolean
compute_trigger_content (HtmlDoc *hdoc, G_GNUC_UNUSED WebServer *webserver, TConnection *tcnc,
			 const gchar *schema, const gchar *name, GError **error)
{
	GdaMetaStore *store;
	GdaDataModel *model;
	GValue *v0, *v1;
	GValue *tschema = NULL, *tname = NULL;
	gint i, nrows;
	xmlNodePtr code, div = NULL, sdiv, ul, li;

	store = gda_connection_get_meta_store (t_connection_get_cnc (tcnc));
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
				xmlNewChild (div, NULL, BAD_CAST "br", NULL);
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
			xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST tmp);
			g_free (tmp);

			div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
		}

		if (G_VALUE_TYPE (cv5) != GDA_TYPE_NULL)
			tmp = g_strdup_printf ("On %s (%s):", g_value_get_string (cv2), g_value_get_string (cv5));
		else
			tmp = g_strdup_printf ("On %s:", g_value_get_string (cv2));
		xmlNewChild (div, NULL, BAD_CAST "h2", BAD_CAST tmp);
		g_free (tmp);

		sdiv = xmlNewChild (div, NULL, BAD_CAST "div", NULL);
		ul = xmlNewChild (sdiv, NULL, BAD_CAST "ul", NULL);

		tmp = g_strdup_printf (_("Trigger fired for: %s"), g_value_get_string (cv6));
		li = xmlNewChild (ul, NULL, BAD_CAST "li", BAD_CAST tmp);
		g_free (tmp);

		tmp = g_strdup_printf (_("Time at which the trigger is fired: %s"), g_value_get_string (cv7));
		li = xmlNewChild (ul, NULL, BAD_CAST "li", BAD_CAST tmp);
		g_free (tmp);

		li = xmlNewChild (ul, NULL, BAD_CAST "li", BAD_CAST _("Action:"));
		code = xmlNewChild (li, NULL, BAD_CAST "code", BAD_CAST g_value_get_string (cv1));
		xmlSetProp (code, BAD_CAST "class", BAD_CAST "ccode");
	}
	
	
	g_object_unref (model);
	return TRUE;
}

/*
 * Give details about a database object
 */
static gboolean
compute_object_content (HtmlDoc *hdoc, WebServer *webserver, TConnection *tcnc,
			const gchar *schema, const gchar *name, GError **error)
{
	GdaMetaStruct *mstruct;
	GdaMetaDbObject *dbo;
	GValue *v0, *v1;
	gboolean retval;

	g_value_take_string ((v0 = gda_value_new (G_TYPE_STRING)),
			     gda_sql_identifier_quote (schema, t_connection_get_cnc (tcnc), NULL, FALSE, FALSE));
	g_value_take_string ((v1 = gda_value_new (G_TYPE_STRING)),
			     gda_sql_identifier_quote (name, t_connection_get_cnc (tcnc), NULL, FALSE, FALSE));

	mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", gda_connection_get_meta_store (t_connection_get_cnc (tcnc)), "features", GDA_META_STRUCT_FEATURE_ALL, NULL);
	dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, NULL, v0, v1, error);
	gda_value_free (v0);
	gda_value_free (v1);
	if (!dbo) {
		if (compute_trigger_content (hdoc, webserver, tcnc, schema, name, error))
			return TRUE;
		TO_IMPLEMENT;
		return FALSE;
	}
	
	switch (dbo->obj_type) {
	case GDA_META_DB_TABLE:
		retval = compute_table_details (tcnc, hdoc, webserver, mstruct, dbo, error);
		break;
	case GDA_META_DB_VIEW:
		retval = compute_table_details (tcnc, hdoc, webserver, mstruct, dbo, error);
		if (retval)
			retval = compute_view_details (tcnc, hdoc, mstruct, dbo, error);
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
compute_all_objects_content (HtmlDoc *hdoc, TConnection *tcnc, 
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

	rfc_cnc_name = gda_rfc1738_encode (t_connection_get_name (tcnc));

	store = gda_connection_get_meta_store (t_connection_get_cnc (tcnc));

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
		xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST human_obj_type);
		div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
		xmlSetProp (div, BAD_CAST "class", BAD_CAST "clist");
		ul = xmlNewChild (div, NULL, BAD_CAST "ul", NULL);
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
		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST g_value_get_string (cv1));
		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST tmp);
		g_free (tmp);
		tmp = g_strdup_printf ("%s.%s", g_value_get_string (cv0), g_value_get_string (cv1));
		xmlSetProp (a, BAD_CAST "title", BAD_CAST tmp);
		g_free (tmp);
	}
	if (nrows > 0)
		xmlNewChild (div, NULL, BAD_CAST "br", NULL);
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
			gchar *tmp;
			if (schema) {
				xmlNewChild (div, NULL, BAD_CAST "br", NULL);
				gda_value_free (schema);
			}
			schema = gda_value_copy (cv0);
			tmp = g_strdup_printf (human_obj_type_in_schema, g_value_get_string (schema));
			xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST tmp);
			g_free (tmp);
			content_added = TRUE;
			div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
			xmlSetProp (div, BAD_CAST "class", BAD_CAST "clist");
			ul = xmlNewChild (div, NULL, BAD_CAST "ul", NULL);
		}

		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST g_value_get_string (cv1));
		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST tmp);
		g_free (tmp);
	}
	if (nrows != 0)
		xmlNewChild (div, NULL, BAD_CAST "br", NULL);
	retval = TRUE;

	if (! content_added) 
		xmlNewChild (hdoc->content, NULL, BAD_CAST "br", NULL);

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
compute_all_triggers_content (HtmlDoc *hdoc, TConnection *tcnc, GError **error)
{
	gboolean retval = FALSE;
	gchar *rfc_cnc_name;
	GdaMetaStore *store;
	GdaDataModel *model;
	gint i, nrows;
	GValue *schema = NULL, *tschema = NULL, *tname = NULL;
	xmlNodePtr sul = NULL, li, a, div = NULL, sdiv = NULL;
	gboolean content_added = FALSE;

	rfc_cnc_name = gda_rfc1738_encode (t_connection_get_name (tcnc));

	store = gda_connection_get_meta_store (t_connection_get_cnc (tcnc));

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
		xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST _("Triggers:"));
		div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
		xmlSetProp (div, BAD_CAST "class", BAD_CAST "clist");
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
				xmlNewChild (sdiv, NULL, BAD_CAST "br", NULL);
			if (tschema) 
				gda_value_free (tschema);
			if (tname)
				gda_value_free (tname);
			tschema = gda_value_copy (cv3);
			tname = gda_value_copy (cv4);

			tmp = g_strdup_printf (_("For the '%s.%s' table:"), g_value_get_string (tschema),
					       g_value_get_string (tname));
			xmlNewChild (div, NULL, BAD_CAST "h2", BAD_CAST tmp);
			g_free (tmp);

			sdiv = xmlNewChild (div, NULL, BAD_CAST "div", NULL);
			xmlSetProp (sdiv, BAD_CAST "class", BAD_CAST "clist");
			sul = xmlNewChild (sdiv, NULL, BAD_CAST "ul", NULL);
		}

		li = xmlNewChild (sul, NULL, BAD_CAST "li", NULL);
		tmp = g_strdup_printf ("%s (%s)", g_value_get_string (cv1), g_value_get_string (cv2));
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST tmp);
		g_free (tmp);

		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST tmp);
		g_free (tmp);
	}
	if (nrows > 0)
		xmlNewChild (sdiv, NULL, BAD_CAST "br", NULL);
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
				xmlNewChild (sdiv, NULL, BAD_CAST "br", NULL);
				gda_value_free (schema);
			}
			schema = gda_value_copy (cv0);
			tmp = g_strdup_printf (_("Triggers in the '%s' schema:"), g_value_get_string (schema));
			xmlNewChild (hdoc->content, NULL, BAD_CAST "h1", BAD_CAST tmp);
			g_free (tmp);
			content_added = TRUE;
			div = xmlNewChild (hdoc->content, NULL, BAD_CAST "div", NULL);
			xmlSetProp (div, BAD_CAST "class", BAD_CAST "clist");

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
				xmlNewChild (sdiv, NULL, BAD_CAST "br", NULL);
			if (tschema) 
				gda_value_free (tschema);
			if (tname)
				gda_value_free (tname);
			tschema = gda_value_copy (cv3);
			tname = gda_value_copy (cv4);

			tmp = g_strdup_printf (_("For the '%s.%s' table:"), g_value_get_string (tschema),
					       g_value_get_string (tname));
			xmlNewChild (div, NULL, BAD_CAST "h2", BAD_CAST tmp);
			g_free (tmp);

			sdiv = xmlNewChild (div, NULL, BAD_CAST "div", NULL);
			xmlSetProp (sdiv, BAD_CAST "class", BAD_CAST "clist");
			sul = xmlNewChild (sdiv, NULL, BAD_CAST "ul", NULL);
		}

		li = xmlNewChild (sul, NULL, BAD_CAST "li", NULL);
		tmp = g_strdup_printf ("%s (%s)", g_value_get_string (cv1), g_value_get_string (cv2));
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST tmp);
		g_free (tmp);

		e0 = gda_rfc1738_encode (g_value_get_string (cv0));
		e1 = gda_rfc1738_encode (g_value_get_string (cv1));
		tmp = g_strdup_printf ("/%s/%s/%s", rfc_cnc_name, e0, e1);
		g_free (e0);
		g_free (e1);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST tmp);
		g_free (tmp);
	}
	if (nrows != 0)
		xmlNewChild (sdiv, NULL, BAD_CAST "br", NULL);
	retval = TRUE;

	if (! content_added) 
		xmlNewChild (hdoc->content, NULL, BAD_CAST "br", NULL);

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

/*
 * Get variables
 *
 * ...: a NULL terminated list of:
 *      - variable name: const gchar*  
 *      - place holder for tha variable's contents: gchar **
 */
static void
get_variables (SoupMessage *msg, GHashTable *query, ...)
{
	va_list ap;
	gchar *name;
	GHashTable *rquery;

	if (query)
		rquery = query;
	else if (msg->request_body->length)
		rquery = soup_form_decode (msg->request_body->data);
	else
		return;

	va_start (ap, query);
	for (name = va_arg (ap, gchar*); name; name = va_arg (ap, gchar*)) {
		gchar **stringadr = va_arg (ap, gchar**);
		const gchar *tmp;
		tmp = g_hash_table_lookup (rquery, name);
		if (tmp)
			*stringadr = g_strdup (tmp);
		else
			*stringadr = NULL;
	}
	va_end (ap);

	if (!query)
		g_hash_table_destroy (rquery);
}

/*
 * Get variables
 *
 * ...: a NULL terminated list of:
 *      - variable name: const gchar*  
 *      - place holder for tha variable's contents: gchar **
 */
static void
get_cookies (SoupMessage *msg, ...)
{
	const gchar *cookies;
	GdaQuarkList *ql;
	va_list ap;
	gchar *name;

	cookies = soup_message_headers_get_list (msg->request_headers, "Cookie");
	ql = gda_quark_list_new_from_string (cookies);

	va_start (ap, msg);
	for (name = va_arg (ap, gchar*); name; name = va_arg (ap, gchar*)) {
		gchar **stringadr = va_arg (ap, gchar**);
		const gchar *tmp;
		tmp = gda_quark_list_find (ql, name);
		if (tmp)
			*stringadr = g_strdup (tmp);
		else
			*stringadr = NULL;
	}
	va_end (ap);
	
	gda_quark_list_free (ql);
}

static HtmlDoc*
create_new_htmldoc (WebServer *webserver, TConnection *tcnc)
{
	HtmlDoc *hdoc;
	gchar *str;

	gchar *rfc_cnc_name;

	xmlNodePtr ul, li, a;

	if (tcnc) {
		str = g_strdup_printf (_("Database information for '%s'"), t_connection_get_name (tcnc));
		hdoc = html_doc_new (str);
		g_free (str);
	}
	else
		hdoc = html_doc_new (_("Database information"));

	/* other connections in the sidebar */
	ul = cnc_ul (webserver, TRUE);
	xmlAddChild (hdoc->sidebar, ul);

	/* list all database object's types for which information can be obtained */
	if (tcnc) {
		rfc_cnc_name = gda_rfc1738_encode (t_connection_get_name (tcnc));
		ul = xmlNewChild (hdoc->sidebar, NULL, BAD_CAST "ul", BAD_CAST _("Objects"));
		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST _("Tables"));
		str = g_strdup_printf ("/%s/~tables", rfc_cnc_name);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST str);
		g_free (str);
		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST _("Views"));
		str = g_strdup_printf ("/%s/~views", rfc_cnc_name);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST str);
		g_free (str);
		li = xmlNewChild (ul, NULL, BAD_CAST "li", NULL);
		a = xmlNewChild (li, NULL, BAD_CAST "a", BAD_CAST _("Triggers"));
		str = g_strdup_printf ("/%s/~triggers", rfc_cnc_name);
		xmlSetProp (a, BAD_CAST "href", BAD_CAST str);
		g_free (str);
		g_free (rfc_cnc_name);
	}
	return hdoc;
}


/*
 *
 * IRB
 *
 */

static gboolean
delete_consoles (WebServer *server)
{
	GSList *list;
	GDateTime *dt = g_date_time_new_now_local();

	for (list = server->priv->terminals_list; list; ) {
		TContext *console = (TContext *) list->data;
		GDateTime *lastused;
		lastused = t_context_get_last_time_used (console);
		if (g_date_time_get_second (lastused) + 600 < g_date_time_get_second (dt)) {
			GSList *n = list->next;
			server->priv->terminals_list = g_slist_delete_link (server->priv->terminals_list, list);
			list = n;
			g_object_unref (G_OBJECT (console));
		}
		else
			list = list->next;
	}

	g_date_time_unref (dt);

	if (! server->priv->terminals_list) {
		server->priv->term_timer = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static gchar *
gda_sql_console_execute (WebServer *server, TContext *console, const gchar *command, GError **error, ToolOutputFormat format)
{
	gchar *loc_cmde = NULL;
	gchar *retstr = NULL;

	g_assert (console);
	loc_cmde = g_strdup (command);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		if (t_utils_command_is_complete (console, loc_cmde)) {
			/* execute command */
			ToolCommandResult *res;
			
			res = t_context_command_execute (console, loc_cmde,
							 GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
			
			if (res) {
				retstr = base_tool_output_result_to_string (res, format,
									    t_context_get_output_stream (console, NULL),
									    t_app_get_options ());
				base_tool_command_result_free (res);
			}
		}
		else {
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("Command is incomplete"));
		}
	}
	g_free (loc_cmde);

	return retstr;
}


/*
 * GET/POST  method for IRB
 */
static gboolean
get_post_for_irb (WebServer *webserver, SoupMessage *msg, G_GNUC_UNUSED TConnection *tcnc,
		  GHashTable *query, G_GNUC_UNUSED GError **error)
{
	gboolean retval = FALSE;
	SoupBuffer *buffer;
	xmlChar *contents = NULL;
	static gint counter = 0;
	
	gchar *cmd;
	gchar *cid;

	TContext *console = NULL;

	/* fetch variables */
	get_variables (msg, query, "cmd", &cmd, "cid", &cid, NULL);
	if (!cmd)
		return FALSE;

	if (cid) {
		GSList *list;
		for (list = webserver->priv->terminals_list; list; list = list->next) {
			const gchar *id;
			id = t_context_get_id (T_CONTEXT (list->data));
			if (id && !strcmp (id, cid)) {
				console = (TContext*) list->data;
				break;
			}
		}
	}

	if (!console) {
		if (!cid || !strcmp (cid, "none")) {
			gchar *str;
			str = g_strdup_printf ("console%d", counter++);
			console = T_CONTEXT (t_web_context_new (str));
			g_free (str);
		}
		else {
			console = T_CONTEXT (t_web_context_new (cid));
		}
			
		webserver->priv->terminals_list = g_slist_prepend (webserver->priv->terminals_list,
								   console);
		GTimeVal *lastused;
		lastused = t_context_get_last_time_used (console);
		g_get_current_time (lastused);
		if (!webserver->priv->term_timer)
			webserver->priv->term_timer = g_timeout_add_seconds (5, (GSourceFunc) delete_consoles,
									     webserver);

		if (!cid || !strcmp (cid, "none")) {
			soup_message_headers_replace (msg->response_headers,
						      "Content-Type", "text/xml");
			soup_message_set_status (msg, SOUP_STATUS_OK);
			g_free (cmd);
			
			/* send console's ID */
			xmlDocPtr doc;
			xmlNodePtr topnode;
			gchar *tmp;
			
			doc = xmlNewDoc (BAD_CAST "1.0");
			topnode = xmlNewDocNode (doc, NULL, BAD_CAST "result", NULL);
			xmlDocSetRootElement (doc, topnode);
			
			xmlNewChild (topnode, NULL, BAD_CAST "cid", BAD_CAST (t_context_get_id (console)));
			tmp = t_utils_compute_prompt (console, FALSE, TRUE, BASE_TOOL_OUTPUT_FORMAT_HTML);
			xmlNewChild (topnode, NULL, BAD_CAST "prompt", BAD_CAST tmp);
			g_free (tmp);
			
			int size;
			xmlDocDumpFormatMemory (doc, &contents, &size, 1);
			xmlFreeDoc (doc);
			goto resp;
		}
	}

	/* create response */
	GTimeVal *lastused;
	lastused = t_context_get_last_time_used (console);
	g_get_current_time (lastused);
	cmd = g_strstrip (cmd);
	if (*cmd) {
		xmlDocPtr doc;
		GError *lerror = NULL;
		xmlNodePtr topnode;
		gchar *tmp;

		doc = xmlNewDoc (BAD_CAST "1.0");
		topnode = xmlNewDocNode (doc, NULL, BAD_CAST "result", NULL);
		xmlDocSetRootElement (doc, topnode);

		tmp = gda_sql_console_execute (webserver, console, cmd, &lerror, BASE_TOOL_OUTPUT_FORMAT_HTML);
		if (!tmp) 
			tmp = g_strdup_printf (_("Error: %s"), 
						    lerror && lerror->message ? lerror->message : _("No detail"));
		if (lerror)
			g_error_free (lerror);

		xmlNewChild (topnode, NULL, BAD_CAST "cmde", BAD_CAST tmp);
		g_free (tmp);

		tmp = t_utils_compute_prompt (console, FALSE, TRUE, BASE_TOOL_OUTPUT_FORMAT_HTML);
		xmlNewChild (topnode, NULL, BAD_CAST "prompt", BAD_CAST tmp);
		g_free (tmp);

		int size;
		xmlDocDumpFormatMemory (doc, &contents, &size, 1);
		xmlFreeDoc (doc);
	}
	g_free (cmd);

	/* send response */
 resp:
	soup_message_headers_replace (msg->response_headers,
				      "Content-Type", "text/xml");
	if (contents) {
		buffer = soup_buffer_new_with_owner (contents, strlen ((gchar*)contents), contents, 
						     (GDestroyNotify) xmlFree);
		soup_message_body_append_buffer (msg->response_body, buffer);
		soup_buffer_free (buffer);
	}
	soup_message_set_status (msg, SOUP_STATUS_OK);
	retval = TRUE;

	return retval;
}



/*
 *
 * Temporary data management
 *
 */

/*
 * @data is stolen!
 */
static TmpResource *
tmp_resource_add (WebServer *server, const gchar *path, gchar *data, gsize data_length)
{
	TmpResource *td;
	GTimeVal tv;

	g_get_current_time (&tv);
	td = g_new0 (TmpResource, 1);
	td->path = g_strdup (path);
	td->data = data;
	td->size = data_length;
	td->expiration_date = tv.tv_sec + 30;
	g_hash_table_insert (server->priv->resources_hash, g_strdup (path), td);
	server->priv->resources_list = g_slist_prepend (server->priv->resources_list, td);
	if (!server->priv->resources_timer)
		server->priv->resources_timer = g_timeout_add_seconds (5, (GSourceFunc) delete_tmp_resource, server);
	return td;
}

static gboolean
delete_tmp_resource (WebServer *server)
{
	GSList *list;
	GTimeVal tv;
	gint n_timed = 0;

	g_get_current_time (&tv);
	for (list = server->priv->resources_list; list; ) {
		TmpResource *td = (TmpResource *) list->data;
		if ((td->expiration_date > 0) && (td->expiration_date < tv.tv_sec)) {
			GSList *n = list->next;
			g_hash_table_remove (server->priv->resources_hash, td->path);
			server->priv->resources_list = g_slist_delete_link (server->priv->resources_list, list);
			list = n;
		}
		else {
			if (td->expiration_date > 0)
				n_timed ++;
			list = list->next;
		}
	}
	if (n_timed == 0) {
		server->priv->resources_timer = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static void
tmp_resource_free (TmpResource *data)
{
	g_free (data->data);
	g_free (data);
}

/*
 *
 * Misc functions
 *
 */
static TimedString *
timed_string_new (guint duration)
{
#define STRING_SIZE 16

	TimedString *ts;
	gint i;
	GString *string;

	ts = g_new0 (TimedString, 1);
	string = g_string_new ("");
	for (i = 0; i < STRING_SIZE; i++) {
		gushort r = (((gfloat) rand ()) / (gfloat)RAND_MAX) * 255;
		g_string_append_printf (string, "%0x", r);
	}
	ts->string = string->str;
	
	g_string_free (string, FALSE);
	g_get_current_time (& (ts->validity));
	ts->validity.tv_sec += duration;
	return ts;
}

static void
timed_string_free (TimedString *ts)
{
	g_free (ts->string);
	g_free (ts);
}

static gboolean
auth_timer_manage (WebServer *server)
{
	challenges_manage (server);
	auth_cookies_manage (server);
	
	if ((server->priv->challenges->len == 0) &&
	    (server->priv->cookies->len == 0)) {
		/* remove timer */
		server->priv->auth_timer = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static TimedString *
challenge_add (WebServer *server)
{
	TimedString *ts = timed_string_new (180);
	g_array_append_val (server->priv->challenges, ts);
	if (!server->priv->auth_timer)
		server->priv->auth_timer = g_timeout_add_seconds (5, (GSourceFunc) auth_timer_manage, server);
	else
		challenges_manage (server);
	return ts;
}

static void
challenges_manage (WebServer *server)
{
	gsize i;
	GTimeVal current_ts;
	
	if (server->priv->challenges->len > MAX_CHALLENGES) {
		/* remove the oldest challenge */
		TimedString *ts = g_array_index (server->priv->challenges, TimedString *, 0);
		timed_string_free (ts);
		g_array_remove_index (server->priv->challenges, 0);
	}

	g_get_current_time (&current_ts);
	for (i = 0; i < server->priv->challenges->len; ) {
		TimedString *ts = g_array_index (server->priv->challenges, TimedString *, 0);
		if (ts->validity.tv_sec < current_ts.tv_sec) {
			timed_string_free (ts);
			g_array_remove_index (server->priv->challenges, i);
		}
		else
			i++;
	}
}

static TimedString *
auth_cookie_add (WebServer *server)
{
	TimedString *ts = timed_string_new (1800); /* half hour availability */
	g_array_append_val (server->priv->cookies, ts);
	if (!server->priv->auth_timer)
		server->priv->auth_timer = g_timeout_add_seconds (5, (GSourceFunc) auth_timer_manage,
								  server);
	else
		auth_cookies_manage (server);
#ifdef DEBUG_SERVER
	g_print ("Added cookie %s\n", ts->string);
#endif
	return ts;
}

static void
auth_cookies_manage (WebServer *server)
{
	gsize i;
	GTimeVal current_ts;
	
	if (server->priv->cookies->len > MAX_AUTH_COOKIES) {
		/* remove the oldest auth_cookie */
		TimedString *ts = g_array_index (server->priv->cookies, TimedString *, 0);
		timed_string_free (ts);
		g_array_remove_index (server->priv->cookies, 0);
	}

	g_get_current_time (&current_ts);
	for (i = 0; i < server->priv->cookies->len; ) {
		TimedString *ts = g_array_index (server->priv->cookies, TimedString *, 0);
		if (ts->validity.tv_sec < current_ts.tv_sec) {
#ifdef DEBUG_SERVER
			g_print ("Removed cookie %s\n", ts->string);
#endif
			timed_string_free (ts);
			g_array_remove_index (server->priv->cookies, i);
		}
		else
			i++;
	}
}
