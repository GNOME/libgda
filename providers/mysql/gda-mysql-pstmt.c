/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-mysql-pstmt.h"

static void gda_mysql_pstmt_class_init (GdaMysqlPStmtClass  *klass);
static void gda_mysql_pstmt_init (GdaMysqlPStmt       *pstmt);
static void gda_mysql_pstmt_dispose (GObject  *object);

typedef struct {
	GdaConnection  *cnc;

	MYSQL          *mysql;
	MYSQL_STMT     *mysql_stmt;
	gboolean        stmt_used; /* TRUE if a recorset already uses this prepared statement,
                                    * necessary because only one recordset can use mysql_stmt at a time */
	MYSQL_BIND     *mysql_bind_result;
} GdaMysqlPStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaMysqlPStmt, gda_mysql_pstmt, GDA_TYPE_PSTMT)

static void 
gda_mysql_pstmt_class_init (GdaMysqlPStmtClass  *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	/* virtual functions */
	object_class->dispose = gda_mysql_pstmt_dispose;
}

static void
gda_mysql_pstmt_init (GdaMysqlPStmt       *pstmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (pstmt);
	priv->cnc = NULL;
  priv->mysql = NULL;
  priv->mysql_stmt = NULL;
  priv->stmt_used = FALSE;
  priv->mysql_bind_result = NULL;
	priv->mysql_bind_result = NULL;
}

static void
gda_mysql_pstmt_dispose (GObject  *object)
{
	GdaMysqlPStmt *pstmt = (GdaMysqlPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (pstmt);

  if (priv->cnc != NULL) {
    g_object_unref (priv->cnc);
    priv->cnc = NULL;
  }
	/* free memory */
	if (priv->mysql_stmt) {
		mysql_stmt_close (priv->mysql_stmt);
    priv->mysql_stmt = NULL;
  }
  if (priv->mysql_bind_result != NULL) {
    gda_mysql_pstmt_free_mysql_bind_result (pstmt);
  }

	/* chain to parent class */
	G_OBJECT_CLASS (gda_mysql_pstmt_parent_class)->dispose (object);
}

/*
 * Steals @mysql_stmt
 */
GdaMysqlPStmt *
gda_mysql_pstmt_new (GdaConnection  *cnc,
		     MYSQL          *mysql,
		     MYSQL_STMT     *mysql_stmt)
{
	GdaMysqlPStmt *ps = (GdaMysqlPStmt *) g_object_new (GDA_TYPE_MYSQL_PSTMT,
							    NULL);
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (ps);
	priv->cnc = g_object_ref (cnc);
	priv->mysql = mysql;
	priv->mysql_stmt = mysql_stmt;
	priv->stmt_used = FALSE;

	return ps;
}


gboolean
gda_mysql_pstmt_get_stmt_used  (GdaMysqlPStmt *stmt)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  return priv->stmt_used;
}
void
gda_mysql_pstmt_set_stmt_used  (GdaMysqlPStmt *stmt, gboolean used)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  priv->stmt_used = used;
}
MYSQL_STMT*
gda_mysql_pstmt_get_mysql_stmt (GdaMysqlPStmt *stmt)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  return priv->mysql_stmt;
}

MYSQL_BIND*
gda_mysql_pstmt_get_mysql_bind_result (GdaMysqlPStmt *stmt)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  return priv->mysql_bind_result;
}
void
gda_mysql_pstmt_free_mysql_bind_result (GdaMysqlPStmt *stmt)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  gint i;
  for (i = 0; i < gda_pstmt_get_ncols ((GdaPStmt *) stmt); ++i) {
	  g_free (priv->mysql_bind_result[i].buffer);
	  g_free (priv->mysql_bind_result[i].is_null);
	  g_free (priv->mysql_bind_result[i].length);
  }
  g_free (priv->mysql_bind_result);
  priv->mysql_bind_result = NULL;
}

void
gda_mysql_pstmt_set_mysql_bind_result (GdaMysqlPStmt *stmt, MYSQL_BIND *bind)
{
  GdaMysqlPStmtPrivate *priv = gda_mysql_pstmt_get_instance_private (stmt);
  priv->mysql_bind_result = bind;
}
