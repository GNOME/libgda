/* GDA ORACLE Provider
 * Copyright (C) 2000-2001 The GNOME Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_oracle_connection_h__)
#  define __gda_oracle_connection_h__

#include <libgda/gda-server-connection.h>

G_BEGIN_DECLS

gboolean gda_oracle_connection_initialize (GdaServerConnection *cnc);
gboolean gda_oracle_connection_open (GdaServerConnection *cnc,
				     const gchar *database,
				     const gchar *username,
				     const gchar *password);
gboolean gda_oracle_connection_close (GdaServerConnection *cnc);
gboolesn gda_oracle_connection_begin_transaction (GdaServerConnection *cnc,
						  const gchar *trans_id);
gboolean gda_oracle_connection_commit_transaction (GdaServerConnection *cnc,
						   const gchar *trans_id);
gboolean gda_oracle_connection_rollback_transaction (GdaServerConnection *cnc,
						     const gchar *trans_id);
void     gda_oracle_connection_make_error (GdaServerConnection *cnc);

G_END_DECLS

#endif
