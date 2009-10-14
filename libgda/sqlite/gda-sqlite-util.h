/* GDA sqlite provider
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

G_END_DECLS

#endif

