/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2014 Anders Jonsson <anders.jonsson@norsjovallen.se>
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

/*#define DEBUG_WEB_PROV*/

#include <glib/gi18n-lib.h>
#include "gda-web-util.h"
#include <string.h>
#include "../reuseable/reuse-all.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

static gboolean
check_hash (const gchar *key, const gchar *data, const gchar *expected_hash)
{
	gboolean retval = TRUE;
	gchar *md5str;

	md5str = g_compute_hmac_for_string (G_CHECKSUM_MD5, (const guchar*) key, strlen (key),
					    data, -1);
	
	if (strcmp (md5str, expected_hash))
		retval = FALSE;	
	
	g_free (md5str);
	return retval;
}

/*
 * If there is a mismatch or an error, then gda_connection_add_event_string() is used
 *
 *  - Modifies @sbuffer (to separate HASH from XML part)
 *  - if all OK, extracs the <challenge> value and replace cdata->next_challenge with it (or simply
 *    reset cdata->next_challenge to NULL)
 *
 * Returns: a new #xmlDocPtr, or %NULL on error
 */
static xmlDocPtr
decode_buffer_response (GdaConnection *cnc, WebConnectionData *cdata, SoupBuffer *sbuffer,
			gchar *out_status_chr, guint *out_counter_id)
{
	xmlDocPtr doc;
	gchar *ptr, *response;

	if (out_status_chr)
		*out_status_chr = 0;
	if (out_counter_id)
		*out_counter_id = 0;

	g_assert (sbuffer);
	response = (gchar*) sbuffer->data;

	for (ptr = response; *ptr && (*ptr != '\n'); ptr++);
	if (*ptr != '\n') {
		gda_connection_add_event_string (cnc, _("Could not parse server's response"));
		return NULL;
	}
	*ptr = 0;
	ptr++;

	if ((cdata->key && !check_hash (cdata->key, ptr, response)) &&
	    (cdata->server_secret && !check_hash (cdata->server_secret, ptr, response))) {
		gda_connection_add_event_string (cnc, _("Invalid response hash"));
		return NULL;
	}
	doc = xmlParseMemory (ptr, strlen (ptr));

	if (!doc) {
		gda_connection_add_event_string (cnc, _("Could not parse server's reponse"));
		return NULL;
	}
	else {
		xmlNodePtr node, root;

		root = xmlDocGetRootElement (doc);
		for (node = root->children; node; node = node->next) {
			if (!strcmp ((gchar*) node->name, "session")) {
				xmlChar *contents;
				contents = xmlNodeGetContent (node);
				g_free (cdata->session_id);
				cdata->session_id = g_strdup ((gchar*) contents);
				xmlFree (contents);
			}
			else if (!strcmp ((gchar*) node->name, "challenge")) {
				xmlChar *contents;
				if (cdata->next_challenge) {
					g_free (cdata->next_challenge);
					cdata->next_challenge = NULL;
				}
				contents = xmlNodeGetContent (node);
				cdata->next_challenge = g_strdup ((gchar*) contents);
				xmlFree (contents);
			}
			else if (out_status_chr && !strcmp ((gchar*) node->name, "status")) {
				xmlChar *contents;
				contents = xmlNodeGetContent (node);
				*out_status_chr = *contents;
				xmlFree (contents);
			}
			else if (out_counter_id && !strcmp ((gchar*) node->name, "counter")) {
				xmlChar *contents;
				contents = xmlNodeGetContent (node);
				*out_counter_id = atoi ((gchar*) contents);
				xmlFree (contents);
			}
			else if (!cdata->server_id && !strcmp ((gchar*) node->name, "servertype")) {
				xmlChar *contents;
				contents = xmlNodeGetContent (node);
				cdata->server_id = g_strdup ((gchar*) contents);
				xmlFree (contents);

				cdata->reuseable = _gda_provider_reuseable_new (cdata->server_id);
#ifdef DEBUG_WEB_PROV
				g_print ("REUSEABLE [%s]: %p\n", cdata->server_id, cdata->reuseable);
#endif
			}
			else if (!cdata->server_version && !strcmp ((gchar*) node->name, "serverversion")) {
				xmlChar *contents;
				contents = xmlNodeGetContent (node);
				cdata->server_version = g_strdup ((gchar*) contents);
				xmlFree (contents);
#ifdef DEBUG_WEB_PROV
				g_print ("SERVER version [%s]\n", cdata->server_version);
#endif
			}
		}
	}

	return doc;
}

typedef struct {
	GdaConnection *cnc;
	WebConnectionData *cdata;
} ThreadData;

/* executed in sub thread */
static void
worker_got_chunk_cb (SoupMessage *msg, SoupBuffer *chunk, ThreadData *thdata)
{
	xmlDocPtr doc;
	gchar *data, *ptr;

	data = g_strndup (chunk->data, chunk->length);
	soup_message_body_set_accumulate (msg->response_body, FALSE);
	
#ifdef DEBUG_WEB_PROV
	if (*data)
		g_print (">>>> WORKER\n%s\n<<<< WORKER\n", data);
#endif
	if (!thdata->cdata->session_id) {
		ptr = strstr (data, "</reply>");
		if (ptr) {
			gchar status;
			guint counter_id;
			ptr += 8;
			*ptr = 0;
			doc = decode_buffer_response (thdata->cnc, thdata->cdata, chunk, &status, &counter_id);
			if (!doc || (status != 'O')) {
				/* this cannot happen at the moment */
				g_assert_not_reached ();
				if (doc)
					xmlFreeDoc (doc);
			}
			else {
				g_rec_mutex_lock (& (thdata->cdata->mutex));
				g_assert (thdata->cdata->worker_counter == counter_id);
				g_rec_mutex_unlock (& (thdata->cdata->mutex));
				xmlFreeDoc (doc);
			}
		}
	}
	g_free (data);
}

/* executed in sub thread */
static gpointer
start_worker_in_sub_thread (ThreadData *thdata)
{
	SoupMessage *msg;
	gulong sigid;
	gboolean runagain = TRUE;
	
	while (runagain) {
		GString *real_url;
		g_rec_mutex_lock (& (thdata->cdata->mutex));
		real_url = g_string_new (thdata->cdata->worker_url);
		if (thdata->cdata->session_id)
			g_string_append_printf (real_url, "?%s", thdata->cdata->session_id);
		thdata->cdata->worker_running = TRUE;
		if (thdata->cdata->worker_counter == 0)
			thdata->cdata->worker_counter = 1;
		else
			thdata->cdata->worker_counter ++;
		g_rec_mutex_unlock (& (thdata->cdata->mutex));

		msg = soup_message_new ("GET", real_url->str);
		/*g_print ("=== WORKER Request URL: [%s]\n", real_url->str);*/
		if (!msg) {
			g_warning (_("Invalid HOST/SCRIPT '%s'"), real_url->str);
			g_string_free (real_url, TRUE);
			g_rec_mutex_lock (& (thdata->cdata->mutex));
			thdata->cdata->worker_running = FALSE;
			g_rec_mutex_unlock (& (thdata->cdata->mutex));
			g_free (thdata);
			return NULL;
		}
		g_string_free (real_url, TRUE);
		
		sigid = g_signal_connect (msg, "got-chunk",
					  G_CALLBACK (worker_got_chunk_cb), thdata);
		guint res;
		res = soup_session_send_message (thdata->cdata->worker_session, msg);
		
		g_rec_mutex_lock (& (thdata->cdata->mutex));
		thdata->cdata->worker_running = FALSE;
		runagain = thdata->cdata->worker_needed;
		runagain = runagain && SOUP_STATUS_IS_SUCCESSFUL (res);
		g_rec_mutex_unlock (& (thdata->cdata->mutex));

		g_signal_handler_disconnect (msg, sigid);
		g_object_unref (msg);
	}

	g_free (thdata);
#ifdef DEBUG_WEB_PROV
	g_print ("Worker closed!\n");
#endif

	/* end of sub thread */
	return NULL;
}

static void
start_worker (GdaConnection *cnc, WebConnectionData *cdata)
{
	ThreadData *thdata;

	thdata = g_new0 (ThreadData, 1); /* freed by sub thread */
	thdata->cnc = cnc;
	thdata->cdata = cdata;

	/* set cdata->worker_running to TRUE to avoid having to add a delay */
	g_rec_mutex_lock (& (cdata->mutex));
	cdata->worker_running = TRUE;
	g_rec_mutex_unlock (& (cdata->mutex));

	if (! g_thread_new ("web-worker", (GThreadFunc) start_worker_in_sub_thread,
			    thdata)) {
		g_free (thdata);
		gda_connection_add_event_string (cnc, _("Can't start new thread"));
		return;
	}
	
	gint nb_retries;
	for (nb_retries = 0; nb_retries < 10; nb_retries++) {
		gboolean wait_over;
		g_rec_mutex_lock (& (cdata->mutex));
		wait_over = !cdata->worker_running || cdata->session_id;
		g_rec_mutex_unlock (& (cdata->mutex));
		if (wait_over)
			break;
		else
			g_usleep (200000);
	}

	g_rec_mutex_lock (& (cdata->mutex));
	if (!cdata->session_id) {
		/* there was an error */
		cdata->worker_running = FALSE;
	}
	g_rec_mutex_unlock (& (cdata->mutex));
}

/*
 * Adds a HASH to the message using @hash_key, or adds "NOHASH" if @hash_key is %NULL
 *
 * If there is an error, then gda_connection_add_event_string() is called
 *
 * @out_status_chr, if NOT NULL will contain the 1st char of the <status> node's contents
 */
xmlDocPtr
_gda_web_send_message_to_frontend (GdaConnection *cnc, WebConnectionData *cdata,
				   WebMessageType msgtype, const gchar *message,
				   const gchar *hash_key, gchar *out_status_chr)
{
	SoupMessage *msg;
	guint status;
	gchar *h_message;
	gchar *real_url;
	static gint counter = 0;

	if (out_status_chr)
		*out_status_chr = 0;

	/* handle the need to run the worker to get an initial sessionID */
	g_rec_mutex_lock (& (cdata->mutex));
	cdata->worker_needed = TRUE;
	if (!cdata->worker_running && !cdata->session_id) {
		g_rec_mutex_unlock (& (cdata->mutex));
		start_worker (cnc, cdata);

		g_rec_mutex_lock (& (cdata->mutex));
		if (! cdata->worker_running) {
			gda_connection_add_event_string (cnc, _("Could not run PHP script on the server"));
			cdata->worker_needed = FALSE;
			g_rec_mutex_unlock (& (cdata->mutex));
			return NULL;
		}
	}

	/* prepare new message */
	g_assert (cdata->session_id);
	real_url = g_strdup_printf ("%s?%s&c=%d", cdata->front_url, cdata->session_id, counter++);
	g_rec_mutex_unlock (& (cdata->mutex));
	msg = soup_message_new ("POST", real_url);
	if (!msg) {
		gda_connection_add_event_string (cnc, _("Invalid HOST/SCRIPT '%s'"), real_url);
		g_free (real_url);
		return NULL;
	}
	g_free (real_url);

	/* check context */
	g_rec_mutex_lock (& (cdata->mutex));
	if (gda_connection_get_transaction_status (cnc) &&
	    (!cdata->worker_running ||
	     ((msgtype == MESSAGE_EXEC) && (cdata->last_exec_counter != cdata->worker_counter)))) {
		/* update cdata->last_exec_counter so next statement can be run */
		cdata->last_exec_counter = cdata->worker_counter;

		gda_connection_add_event_string (cnc, _("The transaction has been automatically rolled back"));
		g_object_unref (msg);

		gda_connection_internal_reset_transaction_status (cnc);
		g_rec_mutex_unlock (& (cdata->mutex));
		return NULL;
	}
	if (! cdata->worker_running) {
		g_rec_mutex_unlock (& (cdata->mutex));
		start_worker (cnc, cdata);

		g_rec_mutex_lock (& (cdata->mutex));
		if (! cdata->worker_running) {
			gda_connection_add_event_string (cnc, _("Could not run PHP script on the server"));
			g_object_unref (msg);
			g_rec_mutex_unlock (& (cdata->mutex));
			return NULL;
		}
	}
	g_rec_mutex_unlock (& (cdata->mutex));

	/* finalize and send message */
 	if (hash_key) {
		gchar *md5str;
		md5str = g_compute_hmac_for_string (G_CHECKSUM_MD5, (const guchar*) hash_key, strlen (hash_key),
						    message, -1);

		GString *string;
		string = g_string_new (md5str);
		g_free (md5str);
		g_string_append_c (string, '\n');
		g_string_append (string, message);
		h_message = g_string_free (string, FALSE);
	}
	else
		h_message = g_strdup_printf ("NOHASH\n%s", message);
	
#ifdef DEBUG_WEB_PROV
	g_print ("=== START of request ===\n%s\n=== END of request ===\n", h_message);
#endif
	soup_message_set_request (msg, "text/plain",
				  SOUP_MEMORY_COPY, h_message, strlen (h_message));
	g_free (h_message);
	g_object_set (G_OBJECT (cdata->front_session), SOUP_SESSION_TIMEOUT, 20, NULL);
	status = soup_session_send_message (cdata->front_session, msg);

	g_rec_mutex_lock (& (cdata->mutex));
	cdata->worker_needed = FALSE;
	g_rec_mutex_unlock (& (cdata->mutex));

	if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
		gda_connection_add_event_string (cnc, msg->reason_phrase);
		g_object_unref (msg);
		return NULL;
	}

	xmlDocPtr doc;
	guint counter_id;
	doc = _gda_web_decode_response (cnc, cdata, msg->response_body, out_status_chr, &counter_id);
	g_object_unref (msg);
	
	g_rec_mutex_lock (& (cdata->mutex));
	if (msgtype == MESSAGE_EXEC)
		cdata->last_exec_counter = counter_id;
	g_rec_mutex_unlock (& (cdata->mutex));

	return doc;
}


/*
 * If there is a mismatch or an error, then gda_connection_add_event_string() is used
 *
 *  - if all OK, extracs the <challenge> value and replace cdata->next_challenge with it (or simply
 *    reset cdata->next_challenge to NULL)
 *
 * Returns: a new #xmlDocPtr, or %NULL on error
 */
xmlDocPtr
_gda_web_decode_response (GdaConnection *cnc, WebConnectionData *cdata, SoupMessageBody *body,
			  gchar *out_status_chr, guint *out_counter_id)
{
	SoupBuffer *sbuffer;
	xmlDocPtr doc;
	sbuffer = soup_message_body_flatten (body);
#ifdef DEBUG_WEB_PROV
	g_print ("=== START of response ===\n%s\n=== END of response ===\n", (gchar*) sbuffer->data);
#endif
	doc = decode_buffer_response (cnc, cdata, sbuffer, out_status_chr, out_counter_id);
	soup_buffer_free (sbuffer);
	return doc;
}

/*
 * Creates a new string
 */
gchar *
_gda_web_compute_token (WebConnectionData *cdata)
{
	g_return_val_if_fail (cdata->next_challenge && cdata->key, NULL);

	gchar *md5str;
	md5str = g_compute_hmac_for_string (G_CHECKSUM_MD5, (const guchar*) cdata->key, strlen (cdata->key),
					    cdata->next_challenge, -1);	
	return md5str;
}

/*
 * Cleans any remaining data on the web server
 */
void
_gda_web_do_server_cleanup (GdaConnection *cnc, WebConnectionData *cdata)
{
	SoupMessage *msg;
	guint status;
	gchar *real_url;
	gint nb_retries;

	/* wait for worker to finish */
	g_rec_mutex_lock (& (cdata->mutex));
	for (nb_retries = 0; (nb_retries < 10) && cdata->worker_running; nb_retries ++) {
		g_rec_mutex_unlock (& (cdata->mutex));
		g_usleep (50000);
		g_rec_mutex_lock (& (cdata->mutex));
	}
	g_rec_mutex_unlock (& (cdata->mutex));
 
	real_url = g_strdup_printf ("%s/gda-clean.php?%s", cdata->server_base_url, cdata->session_id);
	msg = soup_message_new ("GET", real_url);
	if (!msg) {
		gda_connection_add_event_string (cnc, _("Invalid HOST/SCRIPT '%s'"), real_url);
 		g_free (real_url);
		return;
 	}
	g_free (real_url);

	g_object_set (G_OBJECT (cdata->front_session), SOUP_SESSION_TIMEOUT, 5, NULL);
	status = soup_session_send_message (cdata->front_session, msg);
	g_object_unref (msg);

	if (!SOUP_STATUS_IS_SUCCESSFUL (status))
		g_warning (_("Error cleaning data on the server for session %s"), cdata->session_id);
#ifdef DEBUG_WEB_PROV
	else
		g_print ("CLEANUP DONE!\n");
#endif
}

/**
 * _gda_web_set_connection_error_from_xmldoc
 *
 * Handles errors reported by @doc, and ser @error if not %NULL
 *
 * Returns: a #GdaConnectionEvent, which must not be modified or freed
 */
GdaConnectionEvent *
_gda_web_set_connection_error_from_xmldoc (GdaConnection *cnc, xmlDocPtr doc, GError **error)
{
	xmlNodePtr node, root;
	GdaConnectionEvent *ev = NULL;

	g_return_val_if_fail (doc, NULL);

	root = xmlDocGetRootElement (doc);
	for (node = root->children; node; node = node->next) {
		if (!strcmp ((gchar*) node->name, "status")) {
			xmlChar *prop;
			prop = xmlGetProp (node, BAD_CAST "error");
			if (prop) {
				ev = gda_connection_add_event_string (cnc, (gchar*) prop);
				xmlFree (prop);
			}
			else
				ev = gda_connection_add_event_string (cnc, _("Non detailed error"));
			break;
		}
	}

	if (ev && error) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s",
			     gda_connection_event_get_description (ev));
	}

	return ev;
}

/*
 * Actually closes the connection from Libgda's point of view
 */
void
_gda_web_change_connection_to_closed (GdaConnection *cnc, WebConnectionData *cdata)
{
	cdata->forced_closing = TRUE;
	gda_connection_close (cnc, NULL);
}

/*
 * Free connection's specific data
 */
void
_gda_web_free_cnc_data (WebConnectionData *cdata)
{
	if (!cdata)
		return;

	if (cdata->reuseable) {
		g_assert (cdata->reuseable->operations);
		if (cdata->reuseable->operations->re_reset_data)
			cdata->reuseable->operations->re_reset_data (cdata->reuseable);
		g_free (cdata->reuseable);
	}
	g_free (cdata->server_id);
	g_free (cdata->server_version);
	g_free (cdata->server_base_url);
	g_free (cdata->front_url);
	g_free (cdata->worker_url);
	g_rec_mutex_clear (& (cdata->mutex));
	if (cdata->worker_session)
		g_object_unref (cdata->worker_session);
	if (cdata->front_session)
		g_object_unref (cdata->front_session);
	g_free (cdata->session_id);
	g_free (cdata->server_secret);
	g_free (cdata->key);
	g_free (cdata->next_challenge);

	g_free (cdata);
}
