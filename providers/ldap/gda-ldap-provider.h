/* GDA LDAP provider
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_ldap_provider_h__)
#  define __gda_ldap_provider_h__

#include <libgda/gda-server-provider.h>

G_BEGIN_DECLS

#define GDA_TYPE_LDAP_PROVIDER            (gda_ldap_provider_get_type())
#define GDA_LDAP_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_LDAP_PROVIDER, GdaLdapProvider))
#define GDA_LDAP_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_LDAP_PROVIDER, GdaLdapProviderClass))
#define GDA_IS_LDAP_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_LDAP_PROVIDER))
#define GDA_IS_LDAP_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_LDAP_PROVIDER))

typedef struct _GdaLdapProvider      GdaLdapProvider;
typedef struct _GdaLdapProviderClass GdaLdapProviderClass;

struct _GdaLdapProvider {
	GdaServerProvider provider;
};

struct _GdaLdapProviderClass {
	GdaServerProviderClass parent_class;
};

GType              gda_ldap_provider_get_type (void) G_GNUC_CONST;
GdaServerProvider *gda_ldap_provider_new (void);

G_END_DECLS

#endif
