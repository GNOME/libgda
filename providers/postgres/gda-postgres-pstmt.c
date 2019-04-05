/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2019 Daniel Espinsa <esodan@gmail.com>
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
#include "gda-postgres-pstmt.h"
#include "gda-postgres-util.h"

static void gda_postgres_pstmt_class_init (GdaPostgresPStmtClass *klass);
static void gda_postgres_pstmt_init       (GdaPostgresPStmt *pstmt);
static void gda_postgres_pstmt_dispose    (GObject *object);

typedef struct {
	/* PostgreSQL identifies a prepared statement by its name, which we'll keep here,
	 * along with a pointer to a GdaConnection and a PGconn because when the prepared
	 * statement is destroyed, we need to call "DEALLOCATE..."
	 */
	GWeakRef        cnc;
	PGconn         *pconn;
	gchar          *prep_name;
	gboolean        date_format_change; /* TRUE if this statement may incur a date format change */
  gboolean        deallocated;
} GdaPostgresPStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaPostgresPStmt, gda_postgres_pstmt, GDA_TYPE_PSTMT)

static void 
gda_postgres_pstmt_class_init (GdaPostgresPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	/* virtual functions */
	object_class->dispose = gda_postgres_pstmt_dispose;
}

static void
gda_postgres_pstmt_init (GdaPostgresPStmt *pstmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);
	
  g_weak_ref_init (&priv->cnc, NULL);
  priv->pconn = NULL;
	priv->prep_name = NULL;
	priv->date_format_change = FALSE;
  priv->deallocated = FALSE;
}

static void
gda_postgres_pstmt_dispose (GObject *object)
{
	GdaPostgresPStmt *pstmt = (GdaPostgresPStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);

  if (!priv->deallocated) {
    GdaConnection *cnc = NULL;

    cnc = g_weak_ref_get (&priv->cnc);
    if (cnc != NULL) {
      /* deallocate statement */
	    gchar *sql;
	    PGresult *pg_res;

	    sql = g_strdup_printf ("DEALLOCATE %s", priv->prep_name);
	    pg_res = _gda_postgres_PQexec_wrap (cnc, priv->pconn, sql);
	    g_free (sql);
	    if (pg_res)
		    PQclear (pg_res);
      g_object_unref (cnc);
    }
    priv->deallocated = TRUE;
  }
	/* free memory */
	g_clear_pointer (&priv->prep_name, g_free);

	/* chain to parent class */
	G_OBJECT_CLASS (gda_postgres_pstmt_parent_class)->dispose (object);
}

GdaPostgresPStmt *
gda_postgres_pstmt_new (GdaConnection *cnc, PGconn *pconn, const gchar *prep_name)
{
	GdaPostgresPStmt *pstmt;

	pstmt = (GdaPostgresPStmt *) g_object_new (GDA_TYPE_POSTGRES_PSTMT, NULL);
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);
	priv->prep_name = g_strdup (prep_name);
	g_weak_ref_set (&priv->cnc, cnc);
  // FIXME: PGconn should be get from the GdaConnection
	priv->pconn = pconn;

	return pstmt;
}


const gchar*
gda_postgres_pstmt_get_prep_name (GdaPostgresPStmt *pstmt)
{
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);
  return priv->prep_name;
}
gboolean
gda_postgres_pstmt_get_date_format_change (GdaPostgresPStmt *pstmt)
{
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);
  return priv->date_format_change;
}
void
gda_postgres_pstmt_set_date_format_change (GdaPostgresPStmt *pstmt, gboolean change)
{
  GdaPostgresPStmtPrivate *priv = gda_postgres_pstmt_get_instance_private (pstmt);
  priv->date_format_change = change;
}
