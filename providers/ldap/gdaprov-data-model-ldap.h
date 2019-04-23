/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_PROXY_DATA_MODEL_LDAP_H__
#define __GDA_PROXY_DATA_MODEL_LDAP_H__

#include "gda-data-model-ldap.h"

G_BEGIN_DECLS

GType         gdaprov_data_model_ldap_get_type  (void) G_GNUC_CONST;
GdaDataModel *_gdaprov_data_model_ldap_new       (GdaConnection *cnc,
						  const gchar *base_dn, const gchar *filter,
						  const gchar *attributes, GdaLdapSearchScope scope);
GList        *gdaprov_data_model_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes);


#define GDAPROV_TYPE_DATA_MODEL_LDAP_ITER gdaprov_data_model_ldap_iter_get_type()

G_DECLARE_DERIVABLE_TYPE(GdaprovDataModelLdapIter, gdaprov_data_model_ldap_iter, GDAPROV, DATA_MODEL_LDAP_ITER, GdaDataModelIter)

struct _GdaprovDataModelLdapIterClass {
	GdaDataModelIterClass parent_class;
};

G_END_DECLS

#endif
