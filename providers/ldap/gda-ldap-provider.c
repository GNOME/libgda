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

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-ldap.h"
#include "gda-ldap-recordset.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_LDAP_HANDLE "GDA_Ldap_LdapHandle"
#define OBJECT_DATA_LDAP_VERSION "Gda_Ldap_Server_Version"

static void gda_ldap_provider_class_init (GdaLdapProviderClass *klass);
static void gda_ldap_provider_init       (GdaLdapProvider *provider,
					   GdaLdapProviderClass *klass);
static void gda_ldap_provider_finalize   (GObject *object);

static gboolean gda_ldap_provider_open_connection (GdaServerProvider *provider,
						    GdaConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_ldap_provider_close_connection (GdaServerProvider *provider,
						     GdaConnection *cnc);
static const gchar *gda_ldap_provider_get_database (GdaServerProvider *provider,
						     GdaConnection *cnc);
static gboolean gda_ldap_provider_supports (GdaServerProvider *provider,
					     GdaConnection *cnc,
					     GdaConnectionFeature feature);
static GdaDataModel *gda_ldap_provider_get_schema (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionSchema schema,
			       GdaParameterList *params);
static gboolean gda_ldap_provider_begin_transaction (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaTransaction *xaction);
static gboolean gda_ldap_provider_commit_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction);
static gboolean gda_ldap_provider_rollback_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction);
static const gchar *gda_ldap_provider_get_version (GdaServerProvider *provider);
static const gchar *gda_ldap_provider_get_server_version (GdaServerProvider *provider,
							      GdaConnection *cnc);
								  
static GObjectClass *parent_class = NULL;

/*
 * GdaLdapProvider class implementation
 */

static void
gda_ldap_provider_class_init (GdaLdapProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_ldap_provider_finalize;
	provider_class->open_connection = gda_ldap_provider_open_connection;
	provider_class->close_connection = gda_ldap_provider_close_connection;
	provider_class->get_database = gda_ldap_provider_get_database;
	/*provider_class->create_database = gda_ldap_provider_create_database;
	provider_class->drop_database = gda_ldap_provider_drop_database;
	provider_class->execute_command = gda_ldap_provider_execute_command;*/
	provider_class->begin_transaction = gda_ldap_provider_begin_transaction;
	provider_class->commit_transaction = gda_ldap_provider_commit_transaction;
	provider_class->rollback_transaction = gda_ldap_provider_rollback_transaction;
	provider_class->supports = gda_ldap_provider_supports;
	provider_class->get_schema = gda_ldap_provider_get_schema;
	provider_class->get_version = gda_ldap_provider_get_version;
	provider_class->get_server_version = gda_ldap_provider_get_server_version;
}

static void
gda_ldap_provider_init (GdaLdapProvider *myprv, GdaLdapProviderClass *klass)
{
}

static void
gda_ldap_provider_finalize (GObject *object)
{
	GdaLdapProvider *myprv = (GdaLdapProvider *) object;

	g_return_if_fail (GDA_IS_LDAP_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_ldap_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaLdapProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_ldap_provider_class_init,
			NULL, NULL,
			sizeof (GdaLdapProvider),
			0,
			(GInstanceInitFunc) gda_ldap_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaLdapProvider", &info, 0);
	}

	return type;
}

GdaServerProvider *
gda_ldap_provider_new (void)
{
	GdaLdapProvider *provider;

	provider = g_object_new (gda_ldap_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/* open_connection handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_open_connection (GdaServerProvider *provider,
				    GdaConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
	const gchar *t_host = NULL;
	const gchar *t_binddn = NULL;
	const gchar *t_password = NULL;
	const gchar *t_port = NULL;
	const gchar *t_flags = NULL;
	const gchar *t_authmethod = NULL;
	LDAP *ldap;
	/*GdaError *error;*/
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	/* get all parameters received */
	t_flags = gda_quark_list_find (params, "FLAGS");
	t_host = gda_quark_list_find (params, "HOST");
	t_port = gda_quark_list_find (params, "PORT");
	t_binddn = gda_quark_list_find (params, "BINDDN");
	t_password = gda_quark_list_find (params, "PASSWORD");
	t_authmethod = gda_quark_list_find (params, "AUTHMETHOD");

	if (!t_host)
		t_host = "localhost";
	if (!t_port)
		t_port = "389";
	if (!username)
		t_binddn = username;
	if (!password)
		t_password = password;

	ldap = ldap_init (t_host, atoi (t_port));
	if (ldap == NULL) {
		 /* FIXME: try to use the struct ldap_error or 
			ldap_perror or ldap_err2string */
		/* error = gda_ldap_make_error (ldap);
		gda_connection_add_error (cnc, error);*/
		
		ldap_perror (ldap, "gda-ldap-provider: ldap_init");
		
		return FALSE;
	}
	
	if (ldap_bind_s (ldap, t_binddn, t_password, 
			t_authmethod ? atoi (t_authmethod) : LDAP_AUTH_SIMPLE)
				!= LDAP_SUCCESS) {
		
		gchar *error;
			
		error = g_strdup_printf ("ldapbind: %s:%s\n", t_host, t_port);
		ldap_perror (ldap, error);
		return FALSE;
	}

	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE, ldap);
	return TRUE;
}

/* close_connection handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	LDAP *ldap;
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap)
		return FALSE;

	ldap_unbind (ldap);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE, NULL);

	return TRUE;
}
/*
static GList *
process_sql_commands (GList *reclist, GdaConnection *cnc, const gchar *sql)
{
	LDAP *ldap;
	gchar **arr;

	return reclist;
}

*/
/* get_database handler for the GdaLdapProvider class */
static const gchar *
gda_ldap_provider_get_database (GdaServerProvider *provider,
				 GdaConnection *cnc)
{
	LDAP *ldap;
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_error_string (cnc, _("Invalid LDAP handle"));
		return NULL;
	}

	return (const gchar *) "test" /*ldap->lberoptions*/;
}


/* supports handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_supports (GdaServerProvider *provider,
			     GdaConnection *cnc,
			     GdaConnectionFeature feature)
{
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);

	switch (feature) {
	case GDA_CONNECTION_FEATURE_SQL :
		return TRUE;
	default : ;
	}

	return FALSE;
}

static void
add_string_row (GdaDataModelArray *recset, const gchar *str)
{
	GdaValue *value;
	GList list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	value = gda_value_new_string (str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_data_model_append_row (GDA_DATA_MODEL (recset), &list);

	gda_value_free (value);
}

static GdaDataModel *
get_ldap_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
	LDAP *ldap;
	gchar subschema[60];
	BerElement *ptr;
	gchar *subschemasubentry[] = { "subschemaSubentry",
		NULL
	};
	gchar *schema_attrs[] = { "objectClasses", NULL	};

	gint result, retcode;
	LDAPMessage *res, *e;
	gchar *attr, **vals = NULL;
	LDAPObjectClass *oc;
	const gchar *errp;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap)
		return FALSE;

	recset = (GdaDataModelArray *) gda_data_model_array_new (4);
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Name"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 1, _("Owner"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 2, _("Comments"));
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 3, "SQL");

	result =
	    ldap_search_s (ldap, "", LDAP_SCOPE_BASE, "(objectclass=*)",
			   subschemasubentry, 0, &res);

	if (result != LDAP_SUCCESS) {
		g_print ("%s\n", ldap_err2string (result));
	}
	g_return_val_if_fail (result == LDAP_SUCCESS, NULL);

	if (res == NULL) {
		g_print ("%s\n", "no schema information found");
	}
	g_return_val_if_fail (res != NULL, NULL);

	if ((e = ldap_first_entry (ldap, res))) {
		if ((attr = ldap_first_attribute (ldap, res, &ptr))) {
			if ((vals = ldap_get_values (ldap, res, attr))) {
				if (strlen (vals[0]) < 59) {
					strcpy (subschema, vals[0]);
					ldap_value_free (vals);
				}
			}
		}
	}
	ldap_msgfree (res);

	if (subschema[0] == 0) {
		g_print ("%s\n", "no schema information found");
	}
	g_return_val_if_fail (subschema[0] != 0, NULL);

	/* g_print ("Schema search on %s\n", subschema);*/

	result = ldap_search_s (ldap, subschema, LDAP_SCOPE_BASE,
				"(objectclass=*)", schema_attrs, 0, &res);

	if (result != LDAP_SUCCESS) {
		g_print ("%s\n", ldap_err2string (result));
	}
	g_return_val_if_fail (result == LDAP_SUCCESS, NULL);

	if (res == NULL) {
		g_print ("%s\n", "no schema information found");
	}
	g_return_val_if_fail (res != NULL, NULL);

	for (e = ldap_first_entry (ldap, res); e;
	     e = ldap_next_entry (ldap, e)) {
		for (attr = ldap_first_attribute (ldap, res, &ptr); attr;
		     attr = ldap_next_attribute (ldap, res, ptr)) {
			int i, j;

			vals = ldap_get_values (ldap, res, attr);
			for (i = 0; vals[i] != NULL; i++) {
				oc = ldap_str2objectclass(vals[i], &retcode, &errp, 0x03);
				if (oc) {
					for (j = 0; oc->oc_names[j] != NULL; j++) {
						GList *value_list = NULL;

						/* fill the recordset */
						value_list = g_list_append (value_list,
									    gda_value_new_string (oc->oc_names[j]));
						value_list = g_list_append (value_list, gda_value_new_string (""));
						value_list = g_list_append (value_list, gda_value_new_string (""));
						value_list = g_list_append (value_list, gda_value_new_string (""));

						gda_data_model_append_row (GDA_DATA_MODEL (recset), value_list);
						/*printf ("%s\n", oc->oc_names[j]);*/

						g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
						g_list_free (value_list);
					}
				}
			}
		}
		ldap_value_free (vals);
	}

	return GDA_DATA_MODEL (recset);
}

static GdaDataModel *
get_ldap_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
	recset = (GdaDataModelArray *) gda_data_model_array_new (1);
	/* gda_server_recordset_model_set_field_defined_size (recset, 0, 32); */
	gda_data_model_set_column_title (GDA_DATA_MODEL (recset), 0, _("Type"));
	/* gda_server_recordset_model_set_field_scale (recset, 0, 0); */
	/* gda_server_recordset_model_set_field_gdatype (recset, 0, GDA_TYPE_STRING); */

	/* fill the recordset */
	add_string_row (recset, "blob");
	add_string_row (recset, "date");
	add_string_row (recset, "datetime");
	add_string_row (recset, "decimal");
	add_string_row (recset, "double");
	add_string_row (recset, "enum");
	add_string_row (recset, "float");
	add_string_row (recset, "int24");
	add_string_row (recset, "long");
	add_string_row (recset, "longlong");
	add_string_row (recset, "set");
	add_string_row (recset, "short");
	add_string_row (recset, "string");
	add_string_row (recset, "time");
	add_string_row (recset, "timestamp");
	add_string_row (recset, "tiny");
	add_string_row (recset, "year");

	return GDA_DATA_MODEL (recset);
}
/* get_schema handler for the GdaLdapProvider class */
static GdaDataModel *
gda_ldap_provider_get_schema (GdaServerProvider *provider,
			       GdaConnection *cnc,
			       GdaConnectionSchema schema,
			       GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	switch (schema) {
/*	case GDA_CONNECTION_SCHEMA_AGGREGATES :
		return get_ldap_aggregates (cnc, params);
	case GDA_CONNECTION_SCHEMA_DATABASES :
		return get_ldap_databases (cnc, params);
	case GDA_CONNECTION_SCHEMA_FIELDS :
		return get_table_fields (cnc, params);*/
	case GDA_CONNECTION_SCHEMA_TABLES :
		return get_ldap_tables (cnc, params);
	case GDA_CONNECTION_SCHEMA_TYPES :
		return get_ldap_types (cnc, params);
	default : ;
	}

	return NULL;
}

/* begin_transaction handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_begin_transaction (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaTransaction *xaction)
{
	LDAP *ldap;
/*	gint rc;*/
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_error_string (cnc, _("Invalid LDAP handle"));
		return FALSE;
	}

/*	rc = ldap_real_query (ldap, "BEGIN", strlen ("BEGIN"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_ldap_make_error (ldap));
		return FALSE;
	}

	return TRUE;*/
	return FALSE;
}

/* commit_transaction handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_commit_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	LDAP *ldap;
	/*gint rc;*/
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_error_string (cnc, _("Invalid LDAP handle"));
		return FALSE;
	}

/*	rc = ldap_real_query (ldap, "COMMIT", strlen ("COMMIT"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_ldap_make_error (ldap));
		return FALSE;
	}

	return TRUE;*/
	return FALSE;
}

/* rollback_transaction handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_rollback_transaction (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GdaTransaction *xaction)
{
	LDAP *ldap;
	/*gint rc;*/
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_error_string (cnc, _("Invalid LDAP handle"));
		return FALSE;
	}

/*	rc = ldap_real_query (ldap, "ROLLBACK", strlen ("ROLLBACK"));
	if (rc != 0) {
		gda_connection_add_error (cnc, gda_ldap_make_error (ldap));
		return FALSE;
	}

	return TRUE;*/
	return FALSE;
}

/* get_version handler for the GdaLDAPProvider class */
static const gchar *
gda_ldap_provider_get_version (GdaServerProvider *provider)
{
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), NULL);
	return PACKAGE_VERSION;
}

/* get_server_version handler for the GdaLDAPProvider class */
static const gchar *
gda_ldap_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;
	LDAP *ldap;
	LDAPAPIInfo ldapinfo;
	gchar *version;
	gint result;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_error_string (cnc, _("Invalid LDAP handle"));
		return NULL;
	}

	/*ldapinfo = g_malloc0 (sizeof (struct ldapapiinfo));*/
		
	version = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_VERSION);
	if (!version) {
		ldapinfo.ldapai_info_version = LDAP_API_INFO_VERSION;
		result = ldap_get_option (ldap, LDAP_OPT_API_INFO, &ldapinfo);
		if (result == LDAP_OPT_SUCCESS) {
			version = g_strdup_printf ("%s %d",
				ldapinfo.ldapai_vendor_name,
				ldapinfo.ldapai_vendor_version);
		} else {
			version = g_strdup_printf ("error %d", result);
		}
		g_object_set_data_full (G_OBJECT (cnc), 
			OBJECT_DATA_LDAP_VERSION, 
			(gpointer) version, 
			g_free);
	}

	return (const gchar *) version;
}
