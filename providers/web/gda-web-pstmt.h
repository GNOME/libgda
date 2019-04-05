/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2019 Daniel Espinosa <malerba@gmail.com>
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

#ifndef __GDA_WEB_PSTMT_H__
#define __GDA_WEB_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-web.h"
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TYPE_WEB_PSTMT            (gda_web_pstmt_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaWebPStmt, gda_web_pstmt, GDA, WEB_PSTMT, GdaPStmt)

struct _GdaWebPStmtClass {
	GdaPStmtClass  parent_class;
};

GdaWebPStmt  *gda_web_pstmt_new              (GdaConnection *cnc, const gchar *pstmt_hash);
gchar        *gda_web_pstmt_get_pstmt_hash   (GdaWebPStmt *stmt);
void          gda_web_pstmt_set_pstmt_hash   (GdaWebPStmt *stmt, const gchar *pstmt_hash);
void          gda_web_pstmt_free_pstmt_hash  (GdaWebPStmt *stmt);

G_END_DECLS

#endif
