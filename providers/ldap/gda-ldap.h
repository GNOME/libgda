/* GDA LDAP provider
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	    Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      German Poo-Caaman~o <gpoo@ubiobio.cl>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_ldap_h__)
#  define __gda_ldap_h__

#include <glib/gmacros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-value.h>
#include "gda-ldap-provider.h"
#include <ldap.h>
#include <ldap_schema.h>

#define GDA_LDAP_PROVIDER_ID          "GDA LDAP provider"

G_BEGIN_DECLS

/*enum enum_field_types ldap_type;*/
/*
 * Utility functions
 */

GdaConnectionEvent     *gda_ldap_make_error (LDAP *handle);
/*GType  gda_ldap_type_to_gda (enum enum_field_types ldap_type);*/
gchar        *gda_ldap_value_to_sql_string (GValue *value);

G_END_DECLS

#endif
