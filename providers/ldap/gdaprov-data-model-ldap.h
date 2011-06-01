/*
 * Copyright (C) 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_PROXY_DATA_MODEL_LDAP_H__
#define __GDA_PROXY_DATA_MODEL_LDAP_H__

#include <libgda/gda-data-model-ldap.h>

G_BEGIN_DECLS

GType         gdaprov_data_model_ldap_get_type  (void) G_GNUC_CONST;
GdaDataModel *_gdaprov_data_model_ldap_new       (GdaConnection *cnc,
						  const gchar *base_dn, const gchar *filter,
						  const gchar *attributes, GdaLdapSearchScope scope);
GList        *gdaprov_data_model_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes);

G_END_DECLS

#endif
