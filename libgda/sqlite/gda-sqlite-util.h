/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dj@starfire-programming.net>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2005 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_SQLITE_UTIL_H__
#define __GDA_SQLITE_UTIL_H__

#include "gda-sqlite.h"

G_BEGIN_DECLS

void  _gda_sqlite_compute_types_hash (SqliteConnectionData *scnc);
GType _gda_sqlite_compute_g_type (int sqlite_type);

#ifdef GDA_DEBUG
void                _gda_sqlite_test_keywords (void);
#endif
GdaSqlReservedKeywordsFunc _gda_sqlite_get_reserved_keyword_func (void);

gchar                     *_gda_sqlite_identifier_quote          (GdaServerProvider *provider, GdaConnection *cnc,
								  const gchar *id,
								  gboolean meta_store_convention, gboolean force_quotes);

gboolean _gda_sqlite_check_transaction_started (GdaConnection *cnc, gboolean *out_started, GError **error);

G_END_DECLS

#endif

