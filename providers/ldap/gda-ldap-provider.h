/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_LDAP_PROVIDER_H__
#define __GDA_LDAP_PROVIDER_H__

#include <virtual/gda-vprovider-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_LDAP_PROVIDER            (gda_ldap_provider_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaLdapProvider, gda_ldap_provider, GDA, LDAP_PROVIDER, GdaVproviderDataModel)

struct _GdaLdapProviderClass {
	GdaVproviderDataModelClass parent_class;
};

G_END_DECLS

#endif
