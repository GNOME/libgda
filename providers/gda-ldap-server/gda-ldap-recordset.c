/* GNOME DB LDAP Provider
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-ldap.h"

gboolean
gda_ldap_recordset_new (GdaServerRecordset *recset)
{
  return TRUE;
}

gint
gda_ldap_recordset_move_next (GdaServerRecordset *recset)
{
  return -1;
}

gint
gda_ldap_recordset_move_prev (GdaServerRecordset *recset)
{
  return -1;
}

gint
gda_ldap_recordset_close (GdaServerRecordset *recset)
{
  return -1;
}

void
gda_ldap_recordset_free (GdaServerRecordset *recset)
{
}


