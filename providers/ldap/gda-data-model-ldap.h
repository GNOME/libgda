/*
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012,2019 Daniel Espinosa <despinosa@src.gnome.org>
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

#ifndef __GDA_DATA_MODEL_LDAP_H__
#define __GDA_DATA_MODEL_LDAP_H__

#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL_LDAP            (gda_data_model_ldap_get_type())
#define GDA_DATA_MODEL_LDAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL_LDAP, GdaDataModelLdap))
#define GDA_DATA_MODEL_LDAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_MODEL_LDAP, GdaDataModelLdapClass))
#define GDA_IS_DATA_MODEL_LDAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_MODEL_LDAP))
#define GDA_IS_DATA_MODEL_LDAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_MODEL_LDAP))

typedef struct _GdaDataModelLdap        GdaDataModelLdap;
typedef struct _GdaDataModelLdapClass   GdaDataModelLdapClass;
typedef struct _GdaDataModelLdapPrivate GdaDataModelLdapPrivate;

struct _GdaDataModelLdap {
	GObject                  object;
	GdaDataModelLdapPrivate *priv;
};

struct _GdaDataModelLdapClass {
	GObjectClass             parent_class;

	/*< private >*/
        /* Padding for future expansion */
        void (*_gda_reserved1) (void);
        void (*_gda_reserved2) (void);
        void (*_gda_reserved3) (void);
        void (*_gda_reserved4) (void);
};

/**
 * GdaLdapSearchScope:
 * @GDA_LDAP_SEARCH_BASE: search of the base object only
 * @GDA_LDAP_SEARCH_ONELEVEL: search of immediate children of the base object, but does not include the base object itself
 * @GDA_LDAP_SEARCH_SUBTREE: search of the base object and the entire subtree below the base object
 *
 * Defines the search scope of an LDAP search command, relative to the base object.
 */
typedef enum {	
	GDA_LDAP_SEARCH_BASE     = 1,
	GDA_LDAP_SEARCH_ONELEVEL = 2,
	GDA_LDAP_SEARCH_SUBTREE  = 3
} GdaLdapSearchScope;

/**
 * SECTION:gda-data-model-ldap
 * @short_description: GdaDataModel to extract LDAP information
 * @title: GdaDataModelLdap
 * @stability: Unstable
 * @see_also: #GdaDataModel
 *
 * The #GdaDataModelLdap object allows to perform LDAP searches.
 *
 * Note: this type of data model is available only if the LDAP library was found at compilation time and
 * if the LDAP provider is correctly installed.
 */

GType             gda_data_model_ldap_get_type        (void) G_GNUC_CONST;
GdaDataModelLdap *gda_data_model_ldap_new_with_config  (GdaConnection *cnc,
							const gchar *base_dn, const gchar *filter,
							const gchar *attributes, GdaLdapSearchScope scope);

GList            *gda_data_model_ldap_compute_columns  (GdaConnection *cnc, const gchar *attributes);

G_END_DECLS

#endif
