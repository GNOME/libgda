/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gda-report-server.h>
#include <gda-connection-pool.h>
#include <gda-connection.h>

static Gda_ConnectionPool* connection_pool = NULL;

/*
 * Public functions
 */
void
report_server_db_init (void)
{
  if (!connection_pool)
    {
      connection_pool = gda_connection_pool_new();
    }
}

gpointer
report_server_db_open (const gchar *dsn, const gchar *user, const gchar *pwd)
{
  g_return_val_if_fail(dsn != NULL, NULL);
}
