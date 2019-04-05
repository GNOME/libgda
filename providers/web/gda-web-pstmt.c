/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-web-pstmt.h"
#include "gda-web-util.h"

static void gda_web_pstmt_class_init (GdaWebPStmtClass *klass);
static void gda_web_pstmt_init       (GdaWebPStmt *pstmt);
static void gda_web_pstmt_dispose    (GObject *object);
static void gda_web_pstmt_finalize    (GObject *object);

typedef struct {
	GWeakRef        cnc;
	gchar          *pstmt_hash;
} GdaWebPStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaWebPStmt, gda_web_pstmt, GDA_TYPE_PSTMT)

static void 
gda_web_pstmt_class_init (GdaWebPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	/* virtual functions */
	object_class->dispose = gda_web_pstmt_dispose;
	object_class->finalize = gda_web_pstmt_finalize;
}

static void
gda_web_pstmt_init (GdaWebPStmt *pstmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (pstmt);
  g_weak_ref_init (&priv->cnc, NULL);
	priv->pstmt_hash = NULL;
}

static void
gda_web_pstmt_dispose (GObject *object)
{
	GdaWebPStmt *pstmt = (GdaWebPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (pstmt);

	if (priv->pstmt_hash) {
    GdaConnection *cnc = NULL;
		WebConnectionData *cdata;
    cnc = g_weak_ref_get (&priv->cnc);
    if (cnc != NULL) {
		  cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
		  if (!cdata)
			  goto next;

		  /* send command to deallocate prepared statement */
		  xmlDocPtr doc;
		  xmlNodePtr root, cmdnode;
		  gchar *token;
		  doc = xmlNewDoc (BAD_CAST "1.0");
		  root = xmlNewNode (NULL, BAD_CAST "request");
		  xmlDocSetRootElement (doc, root);
		  token = _gda_web_compute_token (cdata);
		  xmlNewChild (root, NULL, BAD_CAST "token", BAD_CAST token);
		  g_free (token);
		  cmdnode = xmlNewChild (root, NULL, BAD_CAST "cmd", BAD_CAST "UNPREPARE");
		  xmlNewChild (cmdnode, NULL, BAD_CAST "preparehash", BAD_CAST priv->pstmt_hash);

		  xmlChar *cmde;
		  xmlDocPtr replydoc;
		  int size;
		  gchar status;

		  xmlDocDumpMemory (doc, &cmde, &size);
		  xmlFreeDoc (doc);
		  replydoc = _gda_web_send_message_to_frontend (cnc, cdata, MESSAGE_UNPREPARE, (gchar*) cmde,
							        cdata->key, &status);
		  xmlFree (cmde);
		  if (replydoc)
			  xmlFreeDoc (replydoc);
    }
		
	next:
		/* free memory */
		g_free (priv->pstmt_hash);
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_web_pstmt_parent_class)->finalize (object);
}

static void
gda_web_pstmt_finalize (GObject *object)
{
	GdaWebPStmt *pstmt = (GdaWebPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (pstmt);
  g_weak_ref_clear (&priv->cnc);
}

GdaWebPStmt *
gda_web_pstmt_new (GdaConnection *cnc, const gchar *pstmt_hash)
{
	GdaWebPStmt *pstmt;
	g_return_val_if_fail (pstmt_hash && *pstmt_hash, NULL);

  pstmt = (GdaWebPStmt *) g_object_new (GDA_TYPE_WEB_PSTMT, NULL);
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (pstmt);
	g_weak_ref_set (&priv->cnc, cnc);
  priv->pstmt_hash = g_strdup (pstmt_hash);

  return pstmt;
}

gchar*
gda_web_pstmt_get_pstmt_hash   (GdaWebPStmt *stmt)
{
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (stmt);
  return priv->pstmt_hash;
}
void
gda_web_pstmt_set_pstmt_hash   (GdaWebPStmt *stmt, const gchar *pstmt_hash)
{
  GdaWebPStmtPrivate *priv = gda_web_pstmt_get_instance_private (stmt);
  if (priv->pstmt_hash != NULL) {
    g_free (priv->pstmt_hash);
    priv->pstmt_hash = NULL;
  }
  priv->pstmt_hash = g_strdup (pstmt_hash);
}
