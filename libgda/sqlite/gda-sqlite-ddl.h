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

#ifndef __GDA_SQLITE_DDL_H__
#define __GDA_SQLITE_DDL_H__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

gchar *_gda_sqlite_render_CREATE_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_DROP_TABLE   (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_RENAME_TABLE (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_ADD_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_DROP_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_RENAME_COLUMN  (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_CREATE_INDEX (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_DROP_INDEX   (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_CREATE_VIEW  (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);
gchar *_gda_sqlite_render_DROP_VIEW    (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error);

G_END_DECLS

#endif

