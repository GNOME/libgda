/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#if !defined(__gda_postgres_h__)
#  define __gda_postgres_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <bonobo/bonobo-i18n.h>
#include <libgda/gda-server.h>
#include <gda-postgres-provider.h>
#include <libpq-fe.h>

#define GDA_POSTGRES_COMPONENT_FACTORY_ID "OAFIID:GNOME_Database_Postgres_ComponentFactory"
#define GDA_POSTGRES_PROVIDER_ID          "OAFIID:GNOME_Database_Postgres_Provider"

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaError *gda_postgres_make_error (PGconn *cnc);
//TODO: see utils.c
GdaType gda_postgres_type_to_gda (enum enum_field_types postgres_type);

G_END_DECLS

#endif

