/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_PSTMT_H__
#define __GDA_POSTGRES_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-postgres.h"

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_PSTMT            (gda_postgres_pstmt_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaPostgresPStmt, gda_postgres_pstmt, GDA, POSTGRES_PSTMT, GdaPStmt)

struct _GdaPostgresPStmtClass {
	GdaPStmtClass  parent_class;
};

GdaPostgresPStmt *gda_postgres_pstmt_new       (GdaConnection *cnc, PGconn *pconn, const gchar *prep_name);
const gchar      *gda_postgres_pstmt_get_prep_name (GdaPostgresPStmt *pstmt);
gboolean          gda_postgres_pstmt_get_date_format_change (GdaPostgresPStmt *pstmt);
void              gda_postgres_pstmt_set_date_format_change (GdaPostgresPStmt *pstmt, gboolean change);

G_END_DECLS

#endif
