/*
 * Copyright (C) 2011 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2017 Daniel Espinosa <esodan@gmail.com>
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

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <glib/gi18n-lib.h>
#include "gda-ldap.h"
#include "gda-ldap-util.h"
#include <libgda/gda-util.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-server-provider-private.h> /* for gda_server_provider_get_real_main_context () */
#include <libgda/gda-connection-internal.h> /* for gda_connection_increase/decrease_usage() */

static void
ldap_attribute_free (LdapAttribute *lat)
{
	g_free (lat->name);
	g_free (lat);
}

static void
ldap_class_free (GdaLdapClass *lcl)
{
	g_free (lcl->oid);
	g_strfreev (lcl->names);
	g_free (lcl->description);
	
	if (lcl->req_attributes)
		g_strfreev (lcl->req_attributes);

	if (lcl->opt_attributes)
		g_strfreev (lcl->opt_attributes);
	g_slist_free (lcl->parents);
	g_slist_free (lcl->children);
	g_free (lcl);
}

/*
 * Data copied from GQ's sources and transformed,
 * see ftp://ftp.rfc-editor.org/in-notes/rfc2252.txt
 */
static LdapAttrType ldap_types [] = {
	{ "1.3.6.1.4.1.1466.115.121.1.1",
	  "ACI Item",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.2",
	  "Access Point",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.3",
	  "Attribute Type Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.4",
	  "Audio",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.5",
	  "Binary",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.6",
	  "Bit String",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.7",
	  "Boolean",
	  G_TYPE_BOOLEAN,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.8",
	  "Certificate",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.9",
	  "Certificate List",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.10",
	  "Certificate Pair",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/,
	},
	{ "1.3.6.1.4.1.1466.115.121.1.11",
	  "Country String",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.12",
	  "DN",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.13",
	  "Data Quality Syntax",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.14",
	  "Delivery Method",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.15",
	  "Directory String",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.16",
	  "DIT Content Rule Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.17",
	  "DIT Structure Rule Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.18",
	  "DL Submit Permission",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.19",
	  "DSA Quality Syntax",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.20",
	  "DSE Type",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.21",
	  "Enhanced Guide",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.22",
	  "Facsimile Telephone Number",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.23",
	  "Fax",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.24",
	  "Generalized Time",
	  G_TYPE_NONE,
    -4 /*G_TYPE_DATE_TIME*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.25",
	  "Guide",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.26",
	  "IA5 String",
	  G_TYPE_STRING, /* as ASCII */
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.27",
	  "INTEGER",
	  G_TYPE_INT,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.28",
	  "JPEG",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.29",
	  "Master And Shadow Access Points",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.30",
	  "Matching Rule Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.31",
	  "Matching Rule Use Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.32",
	  "Mail Preference",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.33",
	  "MHS OR Address",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.34",
	  "Name And Optional UID",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.35",
	  "Name Form Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.36",
	  "Numeric String",
	  G_TYPE_NONE,
    -3 /*GDA_TYPE_NUMERIC*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.37",
	  "Object Class Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.38",
	  "OID",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.39",
	  "Other Mailbox",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.40",
	  "Octet String",
	  G_TYPE_NONE,
    -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.41",
	  "Postal Address",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.42",
	  "Protocol Information",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.43",
	  "Presentation Address",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.44",
	  "Printable String",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.45",
	  "Subtree Specification",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.46",
	  "Supplier Information",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.47",
	  "Supplier Or Consumer",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.48",
	  "Supplier And Consumer",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.49",
	  "Supported Algorithm",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.50",
	  "Telephone Number",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.51",
	  "Teletex Terminal Identifier",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.52",
	  "Telex Number",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.53",
	  "UTC Time",
	  G_TYPE_NONE,
    -2 /*GDA_TYPE_TIME*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.54",
	  "LDAP Syntax Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.55",
	  "Modify Rights",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.56",
	  "LDAP Schema Definition",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.57",
	  "LDAP Schema Description",
	  G_TYPE_STRING,
    0
	},
	{ "1.3.6.1.4.1.1466.115.121.1.58",
	  "Substring Assertion",
	  G_TYPE_STRING,
    0
	}
};

LdapAttrType unknown_type = {
	"", "Unknown type", G_TYPE_STRING
};

/**
 * gda_ldap_get_type_info:
 *
 * Returns: the #LdapAttrType associated to @oid, NEVER NULL
 */
LdapAttrType *
gda_ldap_get_type_info (const gchar *oid)
{
	static GHashTable *hash = NULL;
	LdapAttrType *retval = NULL;
	if (hash) {
		if (oid)
			retval = g_hash_table_lookup (hash, oid);
	}
	else {
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		gint i, nb;
		nb = sizeof (ldap_types) / sizeof (LdapAttrType);
		for (i = 0; i < nb; i++) {
			LdapAttrType *type;
			type = & (ldap_types[i]);
			if (g_type_is_a (type->gtype, G_TYPE_NONE) && type->gtype_custom == -1)
				type->gtype = GDA_TYPE_BINARY;
			else if (g_type_is_a (type->gtype, G_TYPE_NONE) && type->gtype_custom == -2)
				type->gtype = GDA_TYPE_TIME;
			else if (g_type_is_a (type->gtype, G_TYPE_NONE) && type->gtype_custom == -3)
				type->gtype = GDA_TYPE_NUMERIC;
			else if (g_type_is_a (type->gtype, G_TYPE_NONE) && type->gtype_custom == -4)
				type->gtype = G_TYPE_DATE_TIME;
			g_hash_table_insert (hash, type->oid, type);
		}
		if (oid)
			retval = g_hash_table_lookup (hash, oid);
	}
	return retval ? retval : &unknown_type;
}

typedef struct {
	GdaLdapConnection *cnc;
	LdapConnectionData *cdata;
	const gchar *attribute;
} WorkerLdapAttrInfoData;

static LdapAttribute *
worker_gda_ldap_get_attr_info (WorkerLdapAttrInfoData *data, GError **error)
{
	LdapAttribute *retval = NULL;

	if (data->cdata->attributes_hash)
		return g_hash_table_lookup (data->cdata->attributes_hash, data->attribute);

	/* initialize known types */
	data->cdata->attributes_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							NULL,
							(GDestroyNotify) ldap_attribute_free);
	

	if (data->cdata->attributes_cache_file) {
		/* try to load from cache file, which must contain one line per attribute:
		 * <syntax oid>,0|1,<attribute name>
		 */
		gchar *fdata;
		if (g_file_get_contents (data->cdata->attributes_cache_file, &fdata, NULL, NULL)) {
			gchar *start, *ptr;
			gchar **array;
			start = fdata;
			while (1) {
				gboolean done = FALSE;
				for (ptr = start; *ptr && (*ptr != '\n'); ptr++);
				if (*ptr == '\n')
					*ptr = 0;
				else
					done = TRUE;
				
				if (*start && (*start != '#')) {
					array = g_strsplit (start, ",", 3);
					if (array[0] && array[1] && array[2]) {
						LdapAttribute *lat;
						lat = g_new (LdapAttribute, 1);
						lat->name = g_strdup (array[2]);
						lat->type = gda_ldap_get_type_info (array[0]);
						lat->single_value = (*array[1] == '0' ? FALSE : TRUE);
						g_hash_table_insert (data->cdata->attributes_hash,
								     lat->name, lat);
						/*g_print ("CACHE ADDED [%s][%p][%d] for OID %s\n",
						  lat->name, lat->type, lat->single_value,
						  array[0]);*/
					}
					g_strfreev (array);
				}
				if (done)
					break;
				else
					start = ptr+1;
			}
			g_free (fdata);
			return g_hash_table_lookup (data->cdata->attributes_hash, data->attribute);
		}
	}

	GString *string = NULL;
	LDAPMessage *msg, *entry;
	int res;
	gchar *subschema = NULL;

	char *subschemasubentry[] = {"subschemaSubentry", NULL};
	char *schema_attrs[] = {"attributeTypes", NULL};
	
	/* look for subschema */
	if (! gda_ldap_ensure_bound (data->cnc, NULL))
		return NULL;

	gda_ldap_execution_slowdown (data->cnc);
	res = ldap_search_ext_s (data->cdata->handle, "", LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 subschemasubentry, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	if (res != LDAP_SUCCESS) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	if ((entry = ldap_first_entry (data->cdata->handle, msg))) {
		char *attr;
		BerElement *ber;
		if ((attr = ldap_first_attribute (data->cdata->handle, entry, &ber))) {
			BerValue **bvals;
			if ((bvals = ldap_get_values_len (data->cdata->handle, entry, attr))) {
				subschema = g_strdup (bvals[0]->bv_val);
				ldap_value_free_len (bvals);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (! subschema) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	/* look for attributeTypes */
	gda_ldap_execution_slowdown (data->cnc);
	res = ldap_search_ext_s (data->cdata->handle, subschema, LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 schema_attrs, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	g_free (subschema);
	if (res != LDAP_SUCCESS) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	if (data->cdata->attributes_cache_file)
		string = g_string_new ("# Cache file. This file can safely be removed, in this case\n"
				       "# it will be automatically recreated.\n"
				       "# DO NOT MODIFY\n");
	for (entry = ldap_first_entry (data->cdata->handle, msg);
	     entry;
	     entry = ldap_next_entry (data->cdata->handle, msg)) {
		char *attr;
		BerElement *ber;
		for (attr = ldap_first_attribute (data->cdata->handle, msg, &ber);
		     attr;
		     attr = ldap_next_attribute (data->cdata->handle, msg, ber)) {
			if (strcasecmp(attr, "attributeTypes")) {
				ldap_memfree (attr);
				continue;
			}

			BerValue **bvals;
			bvals = ldap_get_values_len (data->cdata->handle, entry, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals[i]; i++) {
					LDAPAttributeType *at;
					const char *errp;
					int retcode;
					at = ldap_str2attributetype (bvals[i]->bv_val, &retcode,
								     &errp,
								     LDAP_SCHEMA_ALLOW_ALL);
					if (at && at->at_names && at->at_syntax_oid &&
					    at->at_names[0] && *(at->at_names[0])) {
						LdapAttribute *lat;
						lat = g_new (LdapAttribute, 1);
						lat->name = g_strdup (at->at_names [0]);
						lat->type = gda_ldap_get_type_info (at->at_syntax_oid);
						lat->single_value = (at->at_single_value == 0 ? FALSE : TRUE);
						g_hash_table_insert (data->cdata->attributes_hash,
								     lat->name, lat);
						/*g_print ("ADDED [%s][%p][%d] for OID %s\n",
						  lat->name, lat->type, lat->single_value,
						  at->at_syntax_oid);*/
						if (string)
							g_string_append_printf (string, "%s,%d,%s\n",
										at->at_syntax_oid,
										lat->single_value,
										lat->name);
									  
					}
					if (at)
						ldap_memfree (at);
				}
				ldap_value_free_len (bvals);
			}
			  
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (string) {
		if (! g_file_set_contents (data->cdata->attributes_cache_file, string->str, -1, NULL)) {
			gchar *dirname;
			dirname = g_path_get_dirname (data->cdata->attributes_cache_file);
			g_mkdir_with_parents (dirname, 0700);
			g_free (dirname);
			g_file_set_contents (data->cdata->attributes_cache_file, string->str, -1, NULL);
		}
		g_string_free (string, TRUE);
	}

	gda_ldap_may_unbind (data->cnc);
	retval = g_hash_table_lookup (data->cdata->attributes_hash, data->attribute);
	return retval;
}

/**
 * gda_ldap_get_attr_info:
 * @cnc: a #GdaLdapConnection
 * @cdata:
 * @attribute:
 *
 * Returns: (transfer none): the #LdapAttribute for @attribute, or %NULL
 */
LdapAttribute *
gda_ldap_get_attr_info (GdaLdapConnection *cnc, LdapConnectionData *cdata, const gchar *attribute)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	if (! attribute || !cdata)
		return NULL;

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	WorkerLdapAttrInfoData data;
	data.cnc = cnc;
	data.cdata = cdata;
	data.attribute = attribute;

	gda_connection_increase_usage ((GdaConnection*) cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gda_ldap_get_attr_info, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage ((GdaConnection*) cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);
	return (LdapAttribute*) retval;
}

/*
 * Classes
 */
static gchar **make_array_from_strv (char **values, guint *out_size);
static void classes_h_func (GdaLdapClass *lcl, gchar **supclasses, LdapConnectionData *cdata);
static gint classes_sort (GdaLdapClass *lcl1, GdaLdapClass *lcl2);

typedef struct {
	GdaLdapConnection *cnc;
	LdapConnectionData *cdata;
	const gchar *classname;
} WorkerLdapClassInfoData;

static GdaLdapClass *
worker_gdaprov_ldap_get_class_info (WorkerLdapClassInfoData *data, GError **error)
{
	GdaLdapClass *retval = NULL;

	/* initialize known classes */
	data->cdata->classes_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							   NULL,
							   (GDestroyNotify) ldap_class_free);

	LDAPMessage *msg, *entry;
	int res;
	gchar *subschema = NULL;

	char *subschemasubentry[] = {"subschemaSubentry", NULL};
	char *schema_attrs[] = {"objectClasses", NULL};
	
	/* look for subschema */
	if (! gda_ldap_ensure_bound (data->cnc, NULL))
		return NULL;

	gda_ldap_execution_slowdown (data->cnc);
	res = ldap_search_ext_s (data->cdata->handle, "", LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 subschemasubentry, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	if (res != LDAP_SUCCESS) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	if ((entry = ldap_first_entry (data->cdata->handle, msg))) {
		char *attr;
		BerElement *ber;
		if ((attr = ldap_first_attribute (data->cdata->handle, entry, &ber))) {
			BerValue **bvals;
			if ((bvals = ldap_get_values_len (data->cdata->handle, entry, attr))) {
				subschema = g_strdup (bvals[0]->bv_val);
				ldap_value_free_len (bvals);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (! subschema) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	/* look for attributeTypes */
	gda_ldap_execution_slowdown (data->cnc);
	res = ldap_search_ext_s (data->cdata->handle, subschema, LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 schema_attrs, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	g_free (subschema);
	if (res != LDAP_SUCCESS) {
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}

	GHashTable *h_refs;
	h_refs = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_strfreev);
	for (entry = ldap_first_entry (data->cdata->handle, msg);
	     entry;
	     entry = ldap_next_entry (data->cdata->handle, msg)) {
		char *attr;
		BerElement *ber;
		for (attr = ldap_first_attribute (data->cdata->handle, msg, &ber);
		     attr;
		     attr = ldap_next_attribute (data->cdata->handle, msg, ber)) {
			if (strcasecmp(attr, "objectClasses")) {
				ldap_memfree (attr);
				continue;
			}

			BerValue **bvals;
			bvals = ldap_get_values_len (data->cdata->handle, entry, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals[i]; i++) {
					LDAPObjectClass *oc;
					const char *errp;
					int retcode;
					oc = ldap_str2objectclass (bvals[i]->bv_val, &retcode,
								   &errp,
								   LDAP_SCHEMA_ALLOW_ALL);
					if (oc && oc->oc_oid && oc->oc_names && oc->oc_names[0]) {
						GdaLdapClass *lcl;
						guint k;
						lcl = g_new0 (GdaLdapClass, 1);
						lcl->oid = g_strdup (oc->oc_oid);
//#define CLASS_DEBUG
#ifdef CLASS_DEBUG
						g_print ("FOUND CLASS\n");
#endif
						lcl->names = make_array_from_strv (oc->oc_names,
										   &(lcl->nb_names));
						for (k = 0; lcl->names[k]; k++) {
#ifdef CLASS_DEBUG
							g_print ("  oc_names[%d] = %s\n",
								 k, lcl->names[k]);
#endif
							g_hash_table_insert (data->cdata->classes_hash,
									     lcl->names[k],
									     lcl);
						}
						if (oc->oc_desc) {
#ifdef CLASS_DEBUG
							g_print ("  oc_desc = %s\n", oc->oc_desc);
#endif
							lcl->description = g_strdup (oc->oc_desc);
						}
#ifdef CLASS_DEBUG
						g_print ("  oc_kind = %d\n", oc->oc_kind);
#endif
						switch (oc->oc_kind) {
						case 0:
							lcl->kind = GDA_LDAP_CLASS_KIND_ABSTRACT;
							break;
						case 1:
							lcl->kind = GDA_LDAP_CLASS_KIND_STRUTURAL;
							break;
						case 2:
							lcl->kind = GDA_LDAP_CLASS_KIND_AUXILIARY;
							break;
						default:
							lcl->kind = GDA_LDAP_CLASS_KIND_UNKNOWN;
							break;
						}
						lcl->obsolete = oc->oc_obsolete;
#ifdef CLASS_DEBUG
						g_print ("  oc_obsolete = %d\n", oc->oc_obsolete);

#endif
						gchar **refs;
						refs = make_array_from_strv (oc->oc_sup_oids, NULL);
						if (refs)
							g_hash_table_insert (h_refs, lcl, refs);
						else
							data->cdata->top_classes = g_slist_insert_sorted (data->cdata->top_classes,
									     lcl, (GCompareFunc) classes_sort);
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_sup_oids && oc->oc_sup_oids[k]; k++)
							g_print ("  oc_sup_oids[0] = %s\n",
								 oc->oc_sup_oids[k]);
#endif

						lcl->req_attributes =
							make_array_from_strv (oc->oc_at_oids_must,
									      &(lcl->nb_req_attributes));
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_at_oids_must && oc->oc_at_oids_must[k]; k++)
							g_print ("  oc_at_oids_must[0] = %s\n",
								 oc->oc_at_oids_must[k]);
#endif
						lcl->opt_attributes =
							make_array_from_strv (oc->oc_at_oids_may,
									      &(lcl->nb_opt_attributes));
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_at_oids_may && oc->oc_at_oids_may[k]; k++)
							g_print ("  oc_at_oids_may[0] = %s\n",
								 oc->oc_at_oids_may[k]);
#endif
						  
					}
					if (oc)
						ldap_memfree (oc);
				}
				ldap_value_free_len (bvals);
			}
			  
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	/* create hierarchy */
	g_hash_table_foreach (h_refs, (GHFunc) classes_h_func, data->cdata);
	g_hash_table_destroy (h_refs);

	retval = g_hash_table_lookup (data->cdata->classes_hash, data->classname);
	gda_ldap_may_unbind (data->cnc);
	return retval;
}

/**
 * gdaprov_ldap_get_class_info:
 * @cnc: a #GdaLdapConnection (not %NULL)
 * @classname: the class name (not %NULL)
 *
 * Returns: the #GdaLdapClass for @classname, or %NULL
 */
GdaLdapClass *
gdaprov_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (classname, NULL);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	LdapConnectionData *cdata;
        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
                return NULL;
	}

	if (cdata->classes_hash) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		return g_hash_table_lookup (cdata->classes_hash, classname);
	}

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	WorkerLdapClassInfoData data;
	data.cnc = cnc;
	data.cdata = cdata;
	data.classname = classname;

	gda_connection_increase_usage ((GdaConnection*) cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gdaprov_ldap_get_class_info, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage ((GdaConnection*) cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);
	return (GdaLdapClass*) retval;
}

static gint
my_sort_func (gconstpointer a, gconstpointer b)
{
	gchar *sa, *sb;
	sa = * (gchar**) a;
	sb = * (gchar**) b;
	if (sa && sb)
		return g_utf8_collate (sa, sb);
	else if (sa)
		return -1;
	else if (sb)
		return 1;
	else
		return 0;
}

static gchar **
make_array_from_strv (char **values, guint *out_size)
{
	if (out_size)
		*out_size = 0;
	if (!values)
		return NULL;
	GArray *array;
	gint i;
	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	for (i = 0; values[i]; i++) {
		gchar *tmp;
		tmp = g_strdup (values [i]);
		g_array_append_val (array, tmp);
	}
	if (out_size)
		*out_size = array->len;

	g_array_sort (array, (GCompareFunc) my_sort_func);

	return (gchar**) g_array_free (array, FALSE);
}

static gint
classes_sort (GdaLdapClass *lcl1, GdaLdapClass *lcl2)
{
	return g_utf8_collate (lcl1->names[0], lcl2->names[0]);
}

static void
classes_h_func (GdaLdapClass *lcl, gchar **supclasses, LdapConnectionData *cdata)
{
	gint i;
	for (i = 0; supclasses [i]; i++) {
		GdaLdapClass *parent;
		gchar *clname = supclasses [i];
#ifdef CLASS_DEBUG
		g_print ("class [%s] inherits [%s]\n", lcl->names[0], clname);
#endif
		parent = g_hash_table_lookup (cdata->classes_hash, clname);
		if (!parent)
			continue;
		lcl->parents = g_slist_insert_sorted (lcl->parents, parent, (GCompareFunc) classes_sort);
		parent->children = g_slist_insert_sorted (parent->children, lcl, (GCompareFunc) classes_sort);
	}
	if ((i == 0) && !g_slist_find (cdata->top_classes, lcl))
		cdata->top_classes = g_slist_insert_sorted (cdata->top_classes, lcl, (GCompareFunc) classes_sort);
}

/*
 * _gda_ldap_get_top_classes
 */
const GSList *
gdaprov_ldap_get_top_classes (GdaLdapConnection *cnc)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	
        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
		return NULL;
	if (! cdata->classes_hash) {
		/* force classes init */
		gdaprov_ldap_get_class_info (cnc, "top");
	}
	return cdata->top_classes;
}

/*
 * gda_ldap_get_g_type
 *
 * Compute the GType either from a specified GType of from the attribute name (giving precedence
 * to the specified GType if any)
 */
GType
gda_ldap_get_g_type (GdaLdapConnection *cnc, LdapConnectionData *cdata, const gchar *attribute_name, const gchar *specified_gtype)
{
	GType coltype = GDA_TYPE_NULL;
	if (specified_gtype)
		coltype = gda_g_type_from_string (specified_gtype);
	if ((coltype == G_TYPE_INVALID) || (coltype == GDA_TYPE_NULL)) {
		LdapAttribute *lat;
		lat = gda_ldap_get_attr_info (cnc, cdata, attribute_name);
		if (lat)
			coltype = lat->type->gtype;
	}
	if ((coltype == G_TYPE_INVALID) || (coltype == GDA_TYPE_NULL))
		coltype = G_TYPE_STRING;
	return coltype;
}

/*
 * gda_ldap_attr_value_to_g_value:
 * Converts a #BerValue to a new #GValue
 *
 * Returns: a new #GValue, or %NULL on error
 */
GValue *
gda_ldap_attr_value_to_g_value (LdapConnectionData *cdata, GType type, BerValue *bv)
{
	GValue *value = NULL;
	if ((type == G_TYPE_DATE_TIME) ||
	    (type == G_TYPE_DATE)) {
		/* see ftp://ftp.rfc-editor.org/in-notes/rfc4517.txt,
		 * section 3.3.13: Generalized Time
		 */
		gboolean conv;
		GDateTime *dt;

		dt = g_date_time_new_from_iso8601 (bv->bv_val, NULL);

		if (dt) {
		    if (g_type_is_a (type, G_TYPE_DATE)) {
			GDate *date;
			date = g_date_new_dmy (g_date_time_get_day_of_month (dt),
					       g_date_time_get_month (dt),
					       g_date_time_get_year (dt));
			value = gda_value_new (type);
			g_value_take_boxed (value, date);
		    }

		    if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
			value = gda_value_new (G_TYPE_DATE_TIME);
			g_value_set_boxed (value, dt);
		    }
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		guchar *data;
		data = g_new (guchar, bv->bv_len);
		memcpy (data, bv->bv_val, sizeof (guchar) * bv->bv_len);
		value = gda_value_new_binary (data, bv->bv_len);
	}
	else
		value = gda_value_new_from_string (bv->bv_val, type);

	return value;
}

BerValue *
gda_ldap_attr_g_value_to_value (LdapConnectionData *cdata, const GValue *cvalue)
{
	BerValue *bv;

	if (!cvalue)
		return NULL;

	bv = g_new (struct berval, 1);

	if (G_VALUE_TYPE (cvalue) == G_TYPE_STRING) {
		const gchar *cstr;
		cstr = g_value_get_string (cvalue);
		bv->bv_val = g_strdup (cstr);
		bv->bv_len = strlen (cstr);
	}
	else if (G_VALUE_TYPE (cvalue) == G_TYPE_DATE_TIME) {
		GDateTime *ts;
		gchar *str;
		ts = g_value_get_boxed (cvalue);
		if (g_date_time_get_second (ts) == (gint) g_date_time_get_seconds (ts)) {
			if (g_date_time_get_utc_offset (ts) == 0)
				str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02d",
															 g_date_time_get_year (ts),
															 g_date_time_get_month (ts),
															 g_date_time_get_day_of_month (ts),
															 g_date_time_get_hour (ts),
															 g_date_time_get_minute (ts),
															 g_date_time_get_second (ts));
			else {
				str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02d",
															 g_date_time_get_year (ts),
															 g_date_time_get_month (ts),
															 g_date_time_get_day_of_month (ts),
															 g_date_time_get_hour (ts),
															 g_date_time_get_minute (ts),
															 g_date_time_get_second (ts));
				TO_IMPLEMENT;
			}
		}
		else {
			if (g_date_time_get_utc_offset (ts) == 0)
				str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02d,%lu",
															 g_date_time_get_year (ts),
															 g_date_time_get_month (ts),
															 g_date_time_get_day_of_month (ts),
															 g_date_time_get_hour (ts),
															 g_date_time_get_minute (ts),
															 g_date_time_get_second (ts),
															 (gulong) ((g_date_time_get_seconds (ts) - g_date_time_get_second (ts)) * 1000000.0));
			else {
				str = g_strdup_printf ("%04d-%02d-%02dT%02d:%02d:%02d,%lu",
															 g_date_time_get_year (ts),
															 g_date_time_get_month (ts),
															 g_date_time_get_day_of_month (ts),
															 g_date_time_get_hour (ts),
															 g_date_time_get_minute (ts),
															 g_date_time_get_second (ts),
															 (gulong) ((g_date_time_get_seconds (ts) - g_date_time_get_second (ts)) * 1000000.0));
				TO_IMPLEMENT;
			}
		}
		bv->bv_val = str;
		bv->bv_len = strlen (str);
	}
	else if (G_VALUE_TYPE (cvalue) == G_TYPE_DATE) {
		GDate *date;
		gchar *str;
		date = (GDate*) g_value_get_boxed (cvalue);
		str = g_strdup_printf ("%04d-%02d-%02d", g_date_get_year (date), g_date_get_month (date),
				       g_date_get_day (date));
		bv->bv_val = str;
		bv->bv_len = strlen (str);
	}
	else if (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL) {
		bv->bv_val = NULL;
		bv->bv_len = 0;
	}
	else if (G_VALUE_TYPE (cvalue) == GDA_TYPE_BINARY) {
		TO_IMPLEMENT;
	}
	else if (G_VALUE_TYPE (cvalue) == GDA_TYPE_BLOB) {
		TO_IMPLEMENT;
	}
	else {
		gchar *str;
		str = gda_value_stringify (cvalue);
		bv->bv_val = str;
		bv->bv_len = strlen (str);
	}
	return bv;
}

/*
 * Frees @bvalue, which MUST have been created using gda_ldap_attr_g_value_to_value()
 */
void
gda_ldap_attr_value_free (LdapConnectionData *cdata, BerValue *bvalue)
{
	g_free (bvalue->bv_val);
	g_free (bvalue);
}


/*
 * make sure we respect http://www.faqs.org/rfcs/rfc2253.html
 */
static gchar *
rewrite_dn_component (const char *str, guint len)
{
	guint i;
	gint nbrewrite = 0;
	for (i = 0; i < len; i++) {
		// "," / "=" / "+" / "<" /  ">" / "#" / ";"
		if ((str[i] == ',') || (str[i] == '=') || (str[i] == '+') ||
		    (str[i] == '<') || (str[i] == '>') || (str[i] == '#') || (str[i] == ';'))
			nbrewrite++;
	}
	if (nbrewrite == 0)
		return NULL;

	gchar *tmp, *ptr;
	tmp = g_new (gchar, len + 2*nbrewrite + 1);
	for (i = 0, ptr = tmp; i < len; i++) {
		if ((str[i] == ',') || (str[i] == '=') || (str[i] == '+') ||
		    (str[i] == '<') || (str[i] == '>') || (str[i] == '#') || (str[i] == ';')) {
			int t;
			*ptr = '\\';
			ptr++;
			t = str[i] / 16;
			if (t < 10)
				*ptr = '0' + t;
			else
				*ptr = 'A' + t - 10;
			ptr++;
			t = str[i] % 16;
			if (t < 10)
				*ptr = '0' + t;
			else
				*ptr = 'A' + t - 10;
		}
		else
			*ptr = str[i];
		ptr++;
	}
	*ptr = 0;
	return tmp;
}

/**
 * _gda_Rdn2str:
 *
 * Returns: a new string
 */
static gchar *
_gda_Rdn2str (LDAPRDN rdn)
{
	if (!rdn)
		return NULL;
	gint i;
	GString *string = NULL;
	for (i = 0; rdn[i]; i++) {
		LDAPAVA *ava = rdn [i];
		if (g_utf8_validate (ava->la_attr.bv_val, ava->la_attr.bv_len, NULL) &&
		    g_utf8_validate (ava->la_value.bv_val, ava->la_value.bv_len, NULL)) {
			gchar *tmp;
			if (string)
				g_string_append_c (string, '+');
			else
				string = g_string_new ("");

			/* attr name */
			tmp = rewrite_dn_component (ava->la_attr.bv_val, ava->la_attr.bv_len);
			if (tmp) {
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else
				g_string_append_len (string, ava->la_attr.bv_val, ava->la_attr.bv_len);
			g_string_append_c (string, '=');

			/* attr value */
			tmp = rewrite_dn_component (ava->la_value.bv_val, ava->la_value.bv_len);
			if (tmp) {
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else
				g_string_append_len (string, ava->la_value.bv_val, ava->la_value.bv_len);
		}
		else {
			if (string) {
				g_string_free (string, TRUE);
				return NULL;
			}
		}
	}
	return g_string_free (string, FALSE);
}


/**
 * _gda_dn2str:
 *
 * Returns: a new string
 */
static gchar *
_gda_dn2str (LDAPDN dn)
{
	if (!dn)
		return NULL;

	gint i;
	GString *string = NULL;
	for (i = 0; dn[i]; i++) {
		LDAPRDN rdn = dn[i];
		gchar *tmp;
		tmp = _gda_Rdn2str (rdn);
		if (tmp) {
			if (string)
				g_string_append_c (string, ',');
			else
				string = g_string_new ("");
			g_string_append (string, tmp);
			g_free (tmp);
		}
		else {
			if (string) {
				g_string_free (string, TRUE);
				return NULL;
			}
		}
	}
	return g_string_free (string, FALSE);
}

/*
 * parse_dn
 *
 * Parse and reconstruct @attr if @out_userdn is not %NULL
 *
 * Returns: %TRUE if all OK
 */
gboolean
gda_ldap_parse_dn (const char *attr, gchar **out_userdn)
{
	LDAPDN tmpDN;
	if (out_userdn)
		*out_userdn = NULL;

	if (!attr)
		return FALSE;

	/* decoding */
	if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return FALSE;
		}
	}

	if (out_userdn) {
		gchar *userdn;
		userdn = _gda_dn2str (tmpDN);
		ldap_dnfree (tmpDN);
		if (userdn)
			*out_userdn = userdn;
		else
			return FALSE;
	}
	else
		ldap_dnfree (tmpDN);

	return TRUE;
}

/*
 * Proxy functions
 */

static gint
attr_array_sort_func (gconstpointer a, gconstpointer b)
{
	GdaLdapAttribute *att1, *att2;
	att1 = *((GdaLdapAttribute**) a);
	att2 = *((GdaLdapAttribute**) b);
	return strcmp (att1->attr_name, att2->attr_name);
}

typedef struct {
	GdaLdapConnection *cnc;
	LdapConnectionData *cdata;
	const gchar *dn;
} WorkerLdapDescrEntryData;

static GdaLdapEntry *
worker_gdaprov_ldap_describe_entry (WorkerLdapDescrEntryData *data, GError **error)
{
	if (! gda_ldap_ensure_bound (data->cnc, error))
		return NULL;

	gda_ldap_execution_slowdown (data->cnc);

	int res;
	LDAPMessage *msg = NULL;
	const gchar *real_dn;
	real_dn = data->dn ? data->dn : data->cdata->base_dn;
 retry:
	res = ldap_search_ext_s (data->cdata->handle, real_dn, LDAP_SCOPE_BASE,
				 "(objectClass=*)", NULL, 0,
				 NULL, NULL, NULL, -1,
				 &msg);
	switch (res) {
	case LDAP_SUCCESS:
	case LDAP_NO_SUCH_OBJECT: {
		gint nb_entries;
		LDAPMessage *ldap_row;
		char *attr;
		BerElement* ber;
		GdaLdapEntry *lentry;
		GArray *array = NULL;

		nb_entries = ldap_count_entries (data->cdata->handle, msg);
		if (nb_entries == 0) {
			ldap_msgfree (msg);
			gda_ldap_may_unbind (data->cnc);
			return NULL;
		}
		else if (nb_entries > 1) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("LDAP server returned more than one entry with DN '%s'"),
				     real_dn);
			gda_ldap_may_unbind (data->cnc);
			return NULL;
		}

		lentry = g_new0 (GdaLdapEntry, 1);
		lentry->dn = g_strdup (real_dn);
		lentry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);
		array = g_array_new (TRUE, FALSE, sizeof (GdaLdapAttribute*));
		ldap_row = ldap_first_entry (data->cdata->handle, msg);
		for (attr = ldap_first_attribute (data->cdata->handle, ldap_row, &ber);
		     attr;
		     attr = ldap_next_attribute (data->cdata->handle, ldap_row, ber)) {
			BerValue **bvals;
			GArray *varray = NULL;
			bvals = ldap_get_values_len (data->cdata->handle, ldap_row, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals [i]; i++) {
					if (!varray)
						varray = g_array_new (TRUE, FALSE, sizeof (GValue *));
					GValue *value;
					GType type;
					type = gda_ldap_get_g_type (data->cnc, data->cdata, attr, NULL);
					/*g_print ("Type for attr %s is %s\n", attr, gda_g_type_to_string (type)); */
					value = gda_ldap_attr_value_to_g_value (data->cdata, type, bvals[i]);
					g_array_append_val (varray, value);
				}
				ldap_value_free_len (bvals);
			}
			if (varray) {
				GdaLdapAttribute *lattr = NULL;
				lattr = g_new0 (GdaLdapAttribute, 1);
				lattr->attr_name = g_strdup (attr);
				lattr->values = (GValue**) varray->data;
				lattr->nb_values = varray->len;
				g_array_free (varray, FALSE);

				g_array_append_val (array, lattr);
				g_hash_table_insert (lentry->attributes_hash, lattr->attr_name, lattr);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
		ldap_msgfree (msg);
		if (array) {
			g_array_sort (array, (GCompareFunc) attr_array_sort_func);
			lentry->attributes = (GdaLdapAttribute**) array->data;
			lentry->nb_attributes = array->len;
			g_array_free (array, FALSE);
		}
		gda_ldap_may_unbind (data->cnc);
		return lentry;
	}
	case LDAP_SERVER_DOWN:
	default: {
		if (res == LDAP_SERVER_DOWN) {
			gint i;
			for (i = 0; i < 5; i++) {
				if (gda_ldap_rebind (data->cnc, NULL))
					goto retry;
				g_usleep (G_USEC_PER_SEC * 2);
			}
		}
		/* error */
		int ldap_errno;
		ldap_get_option (data->cdata->handle, LDAP_OPT_ERROR_NUMBER, &ldap_errno);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_OTHER_ERROR,
			     "%s", ldap_err2string(ldap_errno));
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}
	}
}

GdaLdapEntry *
gdaprov_ldap_describe_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (!dn || (dn && *dn), NULL);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	LdapConnectionData *cdata;
        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
                return NULL;
	}

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	WorkerLdapDescrEntryData data;
	data.cnc = cnc;
	data.cdata = cdata;
	data.dn = dn;

	gda_connection_increase_usage ((GdaConnection*) cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gdaprov_ldap_describe_entry, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage ((GdaConnection*) cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);
	return (GdaLdapEntry*) retval;
}

static gint
entry_array_sort_func (gconstpointer a, gconstpointer b)
{
	GdaLdapEntry *e1, *e2;
	e1 = *((GdaLdapEntry**) a);
	e2 = *((GdaLdapEntry**) b);
	return strcmp (e2->dn, e1->dn);
}

typedef struct {
	GdaLdapConnection *cnc;
	LdapConnectionData *cdata;
	const gchar *dn;
	gchar **attributes;
} WorkerEntryChildrenData;

GdaLdapEntry **
worker_gdaprov_ldap_get_entry_children (WorkerEntryChildrenData *data, GError **error)
{
	int res;
	LDAPMessage *msg;
	gda_ldap_execution_slowdown (data->cnc);

	if (! gda_ldap_ensure_bound (data->cnc, error))
		return NULL;

 retry:
	msg = NULL;
	res = ldap_search_ext_s (data->cdata->handle, data->dn ? data->dn : data->cdata->base_dn, LDAP_SCOPE_ONELEVEL,
				 "(objectClass=*)", data->attributes, 0,
				 NULL, NULL, NULL, -1,
				 &msg);

	switch (res) {
	case LDAP_SUCCESS:
	case LDAP_NO_SUCH_OBJECT: {
		LDAPMessage *ldap_row;
		GArray *children;

		children = g_array_new (TRUE, FALSE, sizeof (GdaLdapEntry *));
		for (ldap_row = ldap_first_entry (data->cdata->handle, msg);
		     ldap_row;
		     ldap_row = ldap_next_entry (data->cdata->handle, ldap_row)) {
			char *attr;
			GdaLdapEntry *lentry = NULL;
			attr = ldap_get_dn (data->cdata->handle, ldap_row);
			if (attr) {
				gchar *userdn = NULL;
				if (gda_ldap_parse_dn (attr, &userdn)) {
					lentry = g_new0 (GdaLdapEntry, 1);
					lentry->dn = userdn;
				}
				ldap_memfree (attr);
			}
			
			if (!lentry) {
				gint i;
				for (i = 0; (guint) i < children->len; i++) {
					GdaLdapEntry *lentry;
					lentry = g_array_index (children, GdaLdapEntry*, i);
					gda_ldap_entry_free (lentry);
				}
				g_array_free (children, TRUE);
				children = NULL;
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
					     "%s",
					     _("Could not parse distinguished name returned by LDAP server"));
				break;
			}
			else if (data->attributes) {
				BerElement* ber;
				GArray *array; /* array of GdaLdapAttribute pointers */
				lentry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);
				array = g_array_new (TRUE, FALSE, sizeof (GdaLdapAttribute*));
				for (attr = ldap_first_attribute (data->cdata->handle, ldap_row, &ber);
				     attr;
				     attr = ldap_next_attribute (data->cdata->handle, ldap_row, ber)) {
					BerValue **bvals;
					GArray *varray = NULL;
					bvals = ldap_get_values_len (data->cdata->handle, ldap_row, attr);
					if (bvals) {
						gint i;
						for (i = 0; bvals [i]; i++) {
							if (!varray)
								varray = g_array_new (TRUE, FALSE, sizeof (GValue *));
							GValue *value;
							GType type;
							type = gda_ldap_get_g_type (data->cnc, data->cdata, attr, NULL);
							/*g_print ("%d Type for attr %s is %s\n", i, attr, gda_g_type_to_string (type));*/
							value = gda_ldap_attr_value_to_g_value (data->cdata, type, bvals[i]);
							g_array_append_val (varray, value);
						}
						ldap_value_free_len (bvals);
					}
					if (varray) {
						GdaLdapAttribute *lattr = NULL;
						lattr = g_new0 (GdaLdapAttribute, 1);
						lattr->attr_name = g_strdup (attr);
						lattr->values = (GValue**) varray->data;
						lattr->nb_values = varray->len;
						g_array_free (varray, FALSE);
						
						g_array_append_val (array, lattr);
						g_hash_table_insert (lentry->attributes_hash, lattr->attr_name, lattr);
					}
					ldap_memfree (attr);
				}
				if (ber)
					ber_free (ber, 0);
				if (array) {
					g_array_sort (array, (GCompareFunc) attr_array_sort_func);
					lentry->attributes = (GdaLdapAttribute**) array->data;
					lentry->nb_attributes = array->len;
					g_array_free (array, FALSE);
				}
			}
			g_array_append_val (children, lentry);
		}
		ldap_msgfree (msg);
		gda_ldap_may_unbind (data->cnc);
		if (children) {
			g_array_sort (children, (GCompareFunc) entry_array_sort_func);
			return (GdaLdapEntry**) g_array_free (children, FALSE);
		}
		else
			return NULL;
	}
	case LDAP_SERVER_DOWN:
	default: {
		if (res == LDAP_SERVER_DOWN) {
			gint i;
			if (msg) {
				ldap_msgfree (msg);
				msg = NULL;
			}
			for (i = 0; i < 5; i++) {
				if (gda_ldap_rebind (data->cnc, NULL))
					goto retry;
				g_usleep (G_USEC_PER_SEC * 2);
			}
		}
		/* error */
		int ldap_errno;
		ldap_get_option (data->cdata->handle, LDAP_OPT_ERROR_NUMBER, &ldap_errno);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_OTHER_ERROR,
			     "%s", ldap_err2string(ldap_errno));
		if (msg)
			ldap_msgfree (msg);
		gda_ldap_may_unbind (data->cnc);
		return NULL;
	}
	}
}

GdaLdapEntry **
gdaprov_ldap_get_entry_children (GdaLdapConnection *cnc, const gchar *dn, gchar **attributes, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (!dn || (dn && *dn), NULL);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	LdapConnectionData *cdata;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("cdata != NULL failed");
		return NULL;
	}

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	WorkerEntryChildrenData data;
	data.cnc = cnc;
	data.cdata = cdata;
	data.dn = dn;
	data.attributes = attributes;

	gda_connection_increase_usage ((GdaConnection*) cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gdaprov_ldap_get_entry_children, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage ((GdaConnection*) cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);
	return (GdaLdapEntry **) retval;
}

gchar **
gdaprov_ldap_dn_split (const gchar *dn, gboolean all)
{
	LDAPDN tmpDN;
	GArray *array;
	gint imax = G_MAXINT;

	g_return_val_if_fail (dn && *dn, NULL);

	/*g_print ("%s (%s, %d)\n", __FUNCTION__, dn, all);*/

	/* decoding */
	if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return NULL;
		}
	}

	/* encoding */
	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	if (!all)
		imax = 1;

	gint i;
	LDAPRDN *rdn = tmpDN;
	for (i = 0; rdn [i] && (i < imax); i++) {
		gchar *tmp;
		tmp = _gda_Rdn2str (rdn [i]);
		if (tmp) {
			g_array_append_val (array, tmp);
			/*g_print ("\t[%s]\n", value);*/
		}
		else
			goto onerror;
	}

	if (!all && (i == 1) && rdn [1]) {
		gchar *tmp;
		tmp = _gda_dn2str (rdn+1);
		if (tmp) {
			g_array_append_val (array, tmp);
			/*g_print ("\t[%s]\n", value);*/
		}
		else
			goto onerror;
	}
	ldap_dnfree (tmpDN);

	return (gchar**) g_array_free (array, FALSE);

 onerror:
	for (i = 0; (guint) i < array->len; i++) {
		gchar *tmp;
		tmp = g_array_index (array, gchar*, i);
		g_free (tmp);
	}
	g_array_free (array, TRUE);
	return NULL;
}

gboolean
gdaprov_ldap_is_dn (const gchar *dn)
{
	LDAPDN tmpDN;

	g_return_val_if_fail (dn && *dn, FALSE);
	if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return FALSE;
		}
	}
	ldap_dnfree (tmpDN);
	return TRUE;
}

const gchar *
gdaprov_ldap_get_base_dn (GdaLdapConnection *cnc)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;
	else
		return cdata->base_dn;
}

static gint
def_cmp_func (GdaLdapAttributeDefinition *def1, GdaLdapAttributeDefinition *def2)
{
	return strcmp (def1->name, def2->name);
}

static GSList *
handle_ldap_class (GdaLdapConnection *cnc, LdapConnectionData *cdata, GdaLdapClass *kl, GSList *retlist, GHashTable *hash)
{
	guint j;
	/*g_print ("Class [%s] (%s)\n", kl->oid, kl->names[0]);*/
	for (j = 0; j < kl->nb_req_attributes; j++) {
		/*g_print ("  attr [%s] REQ\n", kl->req_attributes [j]);*/
		GdaLdapAttributeDefinition *def;
		LdapAttribute *latt;
		latt = gda_ldap_get_attr_info (cnc, cdata, kl->req_attributes [j]);
		def = g_hash_table_lookup (hash, kl->req_attributes [j]);
		if (def)
			def->required = TRUE;
		else {
			def = g_new0 (GdaLdapAttributeDefinition, 1);
			def->name = g_strdup (kl->req_attributes [j]);
			def->required = TRUE;
			def->g_type = latt ? latt->type->gtype : G_TYPE_STRING;
			g_hash_table_insert (hash, def->name, def);
			retlist = g_slist_insert_sorted (retlist, def, (GCompareFunc) def_cmp_func);
		}
	}
	for (j = 0; j < kl->nb_opt_attributes; j++) {
		/*g_print ("  attr [%s] OPT\n", kl->opt_attributes [j]);*/
		GdaLdapAttributeDefinition *def;
		LdapAttribute *latt;
		latt = gda_ldap_get_attr_info (cnc, cdata, kl->opt_attributes [j]);
		def = g_hash_table_lookup (hash, kl->opt_attributes [j]);
		if (!def) {
			def = g_new0 (GdaLdapAttributeDefinition, 1);
			def->name = g_strdup (kl->opt_attributes [j]);
			def->required = FALSE;
			def->g_type = latt ? latt->type->gtype : G_TYPE_STRING;
			g_hash_table_insert (hash, def->name, def);
			retlist = g_slist_insert_sorted (retlist, def, (GCompareFunc) def_cmp_func);
		}
	}

	/* handle parents */
	GSList *list;
	for (list = kl->parents; list; list = list->next)
		retlist = handle_ldap_class (cnc, cdata, (GdaLdapClass*) list->data, retlist, hash);

	return retlist;
}

GSList *
gdaprov_ldap_get_attributes_list (GdaLdapConnection *cnc, GdaLdapAttribute *object_class_attr)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (object_class_attr, NULL);

	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;

	guint i;
	const gchar *tmp;
	GHashTable *hash;
	GSList *retlist = NULL;

	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < object_class_attr->nb_values; i++) {
		if (G_VALUE_TYPE (object_class_attr->values [i]) != G_TYPE_STRING) {
			g_warning (_("Unexpected data type '%s' for objectClass attribute!"),
				   gda_g_type_to_string (G_VALUE_TYPE (object_class_attr->values [i])));
			continue;
		}
		tmp = g_value_get_string (object_class_attr->values [i]);

		GdaLdapClass *kl;
		kl = gdaprov_ldap_get_class_info (cnc, tmp);
		if (!kl) {
			/*g_warning (_("Can't get information about '%s' class"), tmp);*/
			continue;
		}
		retlist = handle_ldap_class (cnc, cdata, kl, retlist, hash);
	}
	g_hash_table_destroy (hash);

	return retlist;
}

/*
 * gda_ldap_execution_slowdown:
 *
 * This function honors the GdaConnection:execution-slowdown property when using the LDAP interface
 * directly
 */
void
gda_ldap_execution_slowdown (GdaLdapConnection *cnc)
{
	guint delay;
	g_object_get (cnc, "execution-slowdown", &delay, NULL);
	if (delay > 0) {
		g_print ("Delaying LDAP query execution for %u ms\n", delay);
                g_usleep (delay);
	}
}
