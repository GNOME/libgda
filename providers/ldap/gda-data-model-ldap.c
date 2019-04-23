/*
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012, 2019 Daniel Espinosa <despinosa@src.gnome.org>
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

#include <string.h>
#include "gda-data-model-ldap.h"
#include <libgda/gda-connection.h>
#include <libgda/gda-config.h>
#include "gda-ldap-connection.h"
#include <gmodule.h>
#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_CNC,
	PROP_BASE,
	PROP_FILTER,
	PROP_ATTRIBUTES,
	PROP_SCOPE
};

static void
gda_data_model_ldap_set_property (G_GNUC_UNUSED GObject *object,
				  G_GNUC_UNUSED guint param_id,
				  G_GNUC_UNUSED const GValue *value,
				  G_GNUC_UNUSED GParamSpec *pspec)
{
}

static void
gda_data_model_ldap_get_property (G_GNUC_UNUSED GObject *object,
				  G_GNUC_UNUSED guint param_id,
				  G_GNUC_UNUSED GValue *value,
				  G_GNUC_UNUSED GParamSpec *pspec)
{
}


static void
dummy_gda_data_model_ldap_class_init (GdaDataModelLdapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* properties */
        object_class->set_property = gda_data_model_ldap_set_property;
        object_class->get_property = gda_data_model_ldap_get_property;

        g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("cnc", NULL, "LDAP connection",
							      GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class, PROP_BASE,
                                         g_param_spec_string ("base", NULL, "Base DN", NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_FILTER,
                                         g_param_spec_string ("filter", NULL, "LDAP filter", NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class, PROP_ATTRIBUTES,
                                         g_param_spec_string ("attributes", NULL, "LDAP attributes", NULL,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_SCOPE,
                                         g_param_spec_int ("scope", NULL, "LDAP search scope",
							   GDA_LDAP_SEARCH_BASE,
							   GDA_LDAP_SEARCH_SUBTREE,
							   GDA_LDAP_SEARCH_BASE,
							   G_PARAM_WRITABLE | G_PARAM_READABLE |
							   G_PARAM_CONSTRUCT_ONLY));
}

static void
dummy_gda_data_model_ldap_data_model_init (GdaDataModelInterface *iface)
{
  iface->get_n_rows = NULL;
  iface->get_n_columns = NULL;
  iface->describe_column = NULL;
  iface->get_access_flags = NULL;
  iface->get_value_at = NULL;
  iface->get_attributes_at = NULL;

  iface->create_iter = NULL;

  iface->set_value_at = NULL;
  iface->set_values = NULL;
  iface->append_values = NULL;
  iface->append_row = NULL;
  iface->remove_row = NULL;
  iface->find_row = NULL;

  iface->freeze = NULL;
  iface->thaw = NULL;
  iface->get_notify = NULL;
  iface->send_hint = NULL;
}

static GModule *ldap_prov_module = NULL;

static void
load_ldap_module (void)
{
	if (ldap_prov_module)
		return;

	GdaProviderInfo *pinfo;
	pinfo = gda_config_get_provider_info ("Ldap");
	if (!pinfo)
		return;
	ldap_prov_module = g_module_open (pinfo->location, 0);
}

/**
 * gda_data_model_ldap_get_type:
 *
 * Since: 4.2.8
 */
GType
gda_data_model_ldap_get_type (void)
{
        static GType type = 0;
	if (!type) {
		typedef GType (*Func) (void);
		Func func;

		load_ldap_module ();
		if (!ldap_prov_module)
			goto dummy;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_data_model_ldap_get_type", (void **) &func))
			goto dummy;

		type = func ();
		return type;

	dummy:
		if (!type) {
			/* dummy setup to enable GIR compilation */
			g_warning (_("Dummy GdaDataModelLdap object: if you see this message in your application "
				     "then it's likely that there is an installation problem with the "
				     "LDAP provider. In any case the GdaDataModelLdap object won't be useable."));
			static const GTypeInfo info = {
				sizeof (GdaDataModelLdapClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) dummy_gda_data_model_ldap_class_init,
				NULL,
				NULL,
				sizeof (GdaDataModelLdap),
				0,
				(GInstanceInitFunc) NULL,
				0
			};
			static const GInterfaceInfo data_model_info = {
				(GInterfaceInitFunc) dummy_gda_data_model_ldap_data_model_init,
				NULL,
				NULL
			};
			
			if (type == 0) {
				type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelLdap", &info, 0);
				g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
			}
		}

	}
	return type;
}

/**
 * gda_data_model_ldap_new_with_config:
 * @cnc: an LDAP opened connection (must be a balid #GdaLdapConnection)
 * @base_dn: (nullable): the base DN to search on, or %NULL
 * @filter: (nullable): an LDAP filter, for example "(objectClass=*)"
 * @attributes: (nullable): the list (CSV format) of attributes to fetch, each in the format &lt;attname&gt;[::&lt;GType&gt;]
 * @scope: the search scope
 *
 * Creates a new #GdaDataModel object to extract some LDAP contents. The returned data model will
 * contain one row for each LDAP entry returned by the search, and will
 * always return the DN (Distinguished Name) of the LDAP entry as first column. Other atttibutes
 * may be mapped to other columns, see the @attributes argument.
 *
 * Note that the actual LDAP search command is not executed until necessary (when using the returned
 * data model).
 *
 * The @base_dn is the point in the LDAP's DIT (Directory Information Tree) from where the search will
 * occur, for example "dc=gda,dc=org". A %NULL value indicates that the starting point for the
 * search will be the one specified when opening the LDAP connection.
 *
 * The @filter argument is a valid LDAP filter string, for example "(uidNumber=1001)". If %NULL, then
 * a default search filter of "(objectClass=*)" will be used.
 *
 * @attributes specifies which LDAP attributes the search must return. It is a comma separated list
 * of attribute names, for example "uidNumber, mail, uid, jpegPhoto" (spaces between attribute names
 * are ignored). If %NULL, then no attribute will be fetched. See gda_ldap_connection_declare_table()
 * for more information about this argument.
 *
 * @scope is the scope of search specified when the LDAP search is actually executed.
 *
 * In case of multi valued attributes, an error will be returned when trying to read the attribute:
 * gda_data_model_iter_get_value_at() will return %NULL when using an iterator.
 *
 * This is a convenience function intended to be used by bindings.
 *
 * Returns: (transfer full): a new #GdaDataModelLdap object
 *
 * Since: 5.2
 */
GdaDataModelLdap*
gda_data_model_ldap_new_with_config (GdaConnection *cnc,
			 const gchar *base_dn, const gchar *filter,
			 const gchar *attributes, GdaLdapSearchScope scope)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	return (GdaDataModelLdap*) g_object_new (GDA_TYPE_DATA_MODEL_LDAP,
					     "cnc", cnc, "base", base_dn,
					     "filter", filter, "attributes", attributes,
					     "scope", scope, NULL);
}

/**
 * gda_data_model_ldap_compute_columns:
 * @cnc: a #GdaConnection
 * @attributes: (nullable): a string describing which LDAP attributes to retreive, or %NULL
 *
 * Computes the #GdaColumn of the data model which would be created using @attributes when calling
 * gda_data_model_ldap_new_with_config().
 *
 * Returns: (transfer full) (element-type GdaColumn): a list of #GdaColumn objects
 *
 * Since: 4.2.8
 */
GList *
gda_data_model_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef GList *(*Func) (GdaConnection*, const gchar *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_data_model_ldap_compute_columns", (void **) &func))
			return NULL;
	}
	
	return func (cnc, attributes);
}

/*
 * _gda_ldap_describe_entry:
 * proxy for gda_ldap_describe_entry().
 */
GdaLdapEntry *
_gda_ldap_describe_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef GdaLdapEntry *(*Func) (GdaLdapConnection*, const gchar *, GError **);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_describe_entry", (void **) &func))
			return NULL;
	}
	
	return func (cnc, dn, error);
}

/*
 * _gda_ldap_get_entry_children:
 * proxy for gda_ldap_get_entry_children().
 */
GdaLdapEntry **
_gda_ldap_get_entry_children (GdaLdapConnection *cnc, const gchar *dn,
			      gchar **attributes, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef GdaLdapEntry **(*Func) (GdaLdapConnection*, const gchar *, gchar **, GError **);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_get_entry_children", (void **) &func))
			return NULL;
	}
	
	return func (cnc, dn, attributes, error);
}

/*
 * _gda_ldap_dn_split:
 * proxy for gda_ldap_dn_split().
 */
gchar **
_gda_ldap_dn_split (const gchar *dn, gboolean all)
{
	typedef gchar **(*Func) (const gchar *, gboolean);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_dn_split", (void **) &func))
			return NULL;
	}
	
	return func (dn, all);
}

/*
 * _gda_ldap_is_dn:
 * proxy for gda_ldap_dn_split().
 */
gboolean
_gda_ldap_is_dn (const gchar *dn)
{
	typedef gboolean (*Func) (const gchar *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return FALSE;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_is_dn", (void **) &func))
			return FALSE;
	}
	
	return func (dn);
}

/*
 * _gda_ldap_get_base_dn:
 * proxy for gda_ldap_get_base_dn().
 */
const gchar *
_gda_ldap_get_base_dn (GdaLdapConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef const gchar *(*Func) (GdaLdapConnection *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_get_base_dn", (void **) &func))
			return NULL;
	}
	
	return func (cnc);
}

/*
 * _gda_ldap_get_class_info:
 * proxy for gda_ldap_get_class_info()
 */
GdaLdapClass *
_gda_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef GdaLdapClass *(*Func) (GdaLdapConnection *, const gchar *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_get_class_info", (void **) &func))
			return NULL;
	}
	
	return func (cnc, classname);
}

/*
 * _gda_ldap_get_top_classes:
 * proxy for gda_ldap_get_top_classes()
 */
const GSList *
_gda_ldap_get_top_classes (GdaLdapConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	typedef const GSList *(*Func) (GdaLdapConnection *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;
		
		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_get_top_classes", (void **) &func))
			return NULL;
	}
	
	return func (cnc);
}

/*
 * _gda_ldap_entry_get_attributes_list:
 * proxy for gda_ldap_entry_get_attributes_list()
 */
GSList *
_gda_ldap_entry_get_attributes_list (GdaLdapConnection *cnc, GdaLdapEntry *entry, GdaLdapAttribute *object_class_attr)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (entry || object_class_attr, NULL);
	if (!object_class_attr) {
		g_return_val_if_fail (entry->attributes_hash, NULL);
		object_class_attr = g_hash_table_lookup (entry->attributes_hash, "objectClass");
		g_return_val_if_fail (object_class_attr, NULL);
	}

	typedef GSList *(*Func) (GdaLdapConnection *, GdaLdapAttribute *);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return NULL;

		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_get_attributes_list", (void **) &func))
			return NULL;
	}

	return func (cnc, object_class_attr);
}

/*
 * _gda_ldap_modify:
 * proxy to gda_ldap_add_entry() and others
 */
gboolean
_gda_ldap_modify (GdaLdapConnection *cnc, GdaLdapModificationType modtype,
		  GdaLdapEntry *entry, GdaLdapEntry *ref_entry, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);

	typedef gboolean (*Func) (GdaLdapConnection *,  GdaLdapModificationType, GdaLdapEntry*, GdaLdapEntry*,
				  GError **);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return FALSE;

		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_modify", (void **) &func))
			return FALSE;
	}

	return func (cnc, modtype, entry, ref_entry, error);
}

/*
 * _gda_ldap_rename_entry:
 * proxy to gda_ldap_rename_entry()
 */
gboolean
_gda_ldap_rename_entry (GdaLdapConnection *cnc, const gchar *current_dn, const gchar *new_dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);

	typedef gboolean (*Func) (GdaLdapConnection *,  const gchar*, const gchar*, GError **);
	static Func func = NULL;

	if (!func) {
		load_ldap_module ();
		if (!ldap_prov_module)
			return FALSE;

		if (!g_module_symbol (ldap_prov_module, "gdaprov_ldap_rename_entry", (void **) &func))
			return FALSE;
	}

	return func (cnc, current_dn, new_dn, error);
}
