/* GDA Mysql Provider
 * Copyright (C) 2006 The GNOME Foundation
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

#ifndef __GDA_MYSQL_DDL_H__
#define __GDA_MYSQL_DDL_H__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

gchar *gda_mysql_render_CREATE_DB    (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *gda_mysql_render_DROP_DB      (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *gda_mysql_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *gda_mysql_render_DROP_TABLE   (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *gda_mysql_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *gda_mysql_render_DROP_INDEX   (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);

G_END_DECLS

#endif

