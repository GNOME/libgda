/* GDA Web provider
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-web-pstmt.h"
#include "gda-web-util.h"

static void gda_web_pstmt_class_init (GdaWebPStmtClass *klass);
static void gda_web_pstmt_init       (GdaWebPStmt *pstmt, GdaWebPStmtClass *klass);
static void gda_web_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_web_pstmt_get_type
 *
 * Returns: the #GType of GdaWebPStmt.
 */
GType
gda_web_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaWebPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_web_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaWebPStmt),
			0,
			(GInstanceInitFunc) gda_web_pstmt_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaWebPStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_web_pstmt_class_init (GdaWebPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_web_pstmt_finalize;
}

static void
gda_web_pstmt_init (GdaWebPStmt *pstmt, GdaWebPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	
	pstmt->pstmt_hash = NULL;
}

static void
gda_web_pstmt_finalize (GObject *object)
{
	GdaWebPStmt *pstmt = (GdaWebPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	if (pstmt->pstmt_hash) {
		WebConnectionData *cdata;
		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data (pstmt->cnc);
		if (!cdata) 
			goto next;

		/* send command to deallocate prepared statement */
		xmlDocPtr doc;
		xmlNodePtr root, cmdnode, node;
		gchar *token;
		doc = xmlNewDoc (BAD_CAST "1.0");
		root = xmlNewNode (NULL, BAD_CAST "request");
		xmlDocSetRootElement (doc, root);
		token = _gda_web_compute_token (cdata);
		xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
		g_free (token);
		cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "UNPREPARE");
		node = xmlNewChild (cmdnode, NULL, BAD_CAST "preparehash", BAD_CAST pstmt->pstmt_hash);

		xmlChar *cmde;
		xmlDocPtr replydoc;
		int size;
		gchar status;
		
		xmlDocDumpMemory (doc, &cmde, &size);
		xmlFreeDoc (doc);
		replydoc = _gda_web_send_message_to_frontend (pstmt->cnc, cdata, MESSAGE_UNPREPARE, (gchar*) cmde,
							      cdata->key, &status);
		xmlFree (cmde);		
		if (replydoc)
			xmlFreeDoc (replydoc);
		
	next:
		/* free memory */
		g_free (pstmt->pstmt_hash);
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaWebPStmt *
gda_web_pstmt_new (GdaConnection *cnc, const gchar *pstmt_hash)
{
	GdaWebPStmt *pstmt;
	g_return_val_if_fail (pstmt_hash && *pstmt_hash, NULL);

        pstmt = (GdaWebPStmt *) g_object_new (GDA_TYPE_WEB_PSTMT, NULL);
	pstmt->cnc = cnc;
        pstmt->pstmt_hash = g_strdup (pstmt_hash);

        return pstmt;
}
