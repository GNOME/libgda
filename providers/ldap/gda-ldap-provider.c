/*
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 German Poo-Caaman~o <gpoo@ubiobio.cl>
 * Copyright (C) 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2004 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-ldap-connection.h"
#include <libgda/gda-server-provider-impl.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-util.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include "gda-ldap.h"
#include "gda-ldap-provider.h"
#include "gdaprov-data-model-ldap.h"
#include "gda-ldap-util.h"
#include <libgda/gda-server-provider-private.h> /* for gda_server_provider_get_real_main_context () */
#include <libgda/gda-connection-internal.h> /* for gda_connection_increase/decrease_usage() */

static void gda_ldap_provider_class_init (GdaLdapProviderClass *klass);
static void gda_ldap_provider_init       (GdaLdapProvider *provider);

static const gchar *gda_ldap_provider_get_name (GdaServerProvider *provider);
static const gchar *gda_ldap_provider_get_version (GdaServerProvider *provider);
static GdaConnection *gda_ldap_provider_create_connection (GdaServerProvider *provider);
static gboolean gda_ldap_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc, 
						      GdaQuarkList *params, GdaQuarkList *auth);
static gboolean gda_ldap_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static GObject *gda_ldap_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
						     GdaStatement *stmt, GdaSet *params,
						     GdaStatementModelUsage model_usage,
						     GType *col_types, GdaSet **last_inserted_row, GError **error);
static const gchar *gda_ldap_provider_get_server_version (GdaServerProvider *provider,
							  GdaConnection *cnc);

G_DEFINE_TYPE (GdaLdapProvider, gda_ldap_provider, GDA_TYPE_VPROVIDER_DATA_MODEL)
/* 
 * private connection data destroy 
 */
static void gda_ldap_free_cnc_data (LdapConnectionData *cdata);

/*
 * GdaLdapProvider class implementation
 */
GdaServerProviderBase ldap_base_functions = {
	gda_ldap_provider_get_name,
	gda_ldap_provider_get_version,
	gda_ldap_provider_get_server_version,
	NULL,
	NULL,
	gda_ldap_provider_create_connection,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	gda_ldap_provider_prepare_connection,
	gda_ldap_provider_close_connection,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	gda_ldap_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_ldap_provider_class_init (GdaLdapProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &ldap_base_functions);

	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) NULL);

	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						(gpointer) NULL);

}

static void
gda_ldap_provider_init (G_GNUC_UNUSED GdaLdapProvider *pg_prv)
{
	/* nothing specific there */
}


/*
 * Get provider name request
 */
static const gchar *
gda_ldap_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return LDAP_PROVIDER_NAME;
}

/* 
 * Get version request
 */
static const gchar *
gda_ldap_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

static GdaConnection *
gda_ldap_provider_create_connection (GdaServerProvider *provider)
{
	GdaConnection *cnc;
        g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (provider), NULL);

        cnc = g_object_new (GDA_TYPE_LDAP_CONNECTION, "provider", provider, NULL);

        return cnc;
}

/*
 * compute cache file's absolute name
 */
static gchar *
compute_data_file_name (GdaQuarkList *params, gboolean is_cache, const gchar *data_type)
{
	/* real cache file name */
	GString *string;
	gchar *cfile, *evalue;
	const gchar *base_dn;
	const gchar *host;
	const gchar *require_ssl;
	const gchar *port;
	gint rport;
	gboolean use_ssl;

        base_dn = gda_quark_list_find (params, "DB_NAME");
	host = gda_quark_list_find (params, "HOST");
	if (!host)
		host = "127.0.0.1";
        port = gda_quark_list_find (params, "PORT");
        require_ssl = gda_quark_list_find (params, "USE_SSL");
	use_ssl = (require_ssl && ((*require_ssl == 't') || (*require_ssl == 'T'))) ? TRUE : FALSE;
	if (port && *port)
		rport = atoi (port);
	else {
		if (use_ssl)
			rport = LDAPS_PORT;
		else
			rport = LDAP_PORT;
	}
	string = g_string_new ("");
	evalue = gda_rfc1738_encode (host);
	g_string_append_printf (string, ",=%s", evalue);
	g_free (evalue);
	g_string_append_printf (string, ";PORT=%d", rport);
	if (base_dn) {
		evalue = gda_rfc1738_encode (base_dn);
		g_string_append_printf (string, ";BASE_DN,=%s", evalue);
		g_free (evalue);
	}
	evalue = g_compute_checksum_for_string (G_CHECKSUM_SHA1, string->str, -1);
	g_string_free (string, TRUE);
	if (is_cache)
		cfile = g_strdup_printf ("%s_%s", evalue, data_type);
	else
		cfile = g_strdup_printf ("ldap-%s.%s", evalue, data_type);
	g_free (evalue);
	
	gchar *fname;
	if (is_cache)
		fname = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (),
				      "libgda", "ldap", cfile, NULL);
	else
		fname = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
				      "libgda", cfile, NULL);

	g_free (cfile);
	return fname;
}

typedef struct {
	gchar *filter_format;
	gchar *attribute;
} LdapAuthMapping;

LdapAuthMapping mappings[] = {
	{"(&(uid=%s)(objectclass=inetOrgPerson))", "uid"},
	{"(sAMAccountName=%s)", "sAMAccountName"}, /* Active Directory */
};

/*
 * Function called during initialization phase => no need to use the GdaWorker object
 *
 * Using @url and @username, performs the following tasks:
 * - bind to the LDAP server anonymously
 * - search the directory to identify the entry for the provided user name,
 *   filter: (&(uid=##uid)(objectclass=inetOrgPerson))
 * - if one and only one entry is returned, get the DN of the entry and check that the UID is correct
 *
 * If all the steps are right, it returns the DN of the identified entry as a new string.
 */
static gchar *
fetch_user_dn (const gchar *url, const gchar *base, const gchar *username, LdapAuthMapping *mapping)
{
	LDAP *ld;
	int res;
	int version = LDAP_VERSION3;
	gchar *dn = NULL;
	LDAPMessage *msg = NULL;

	if (! username)
		return NULL;

	res = ldap_initialize (&ld, url);
	if (res != LDAP_SUCCESS)
		return NULL;

	res = ldap_set_option (ld, LDAP_OPT_PROTOCOL_VERSION, &version);
        if (res != LDAP_SUCCESS) {
		if (res == LDAP_PROTOCOL_ERROR) {
			version = LDAP_VERSION2;
			res = ldap_set_option (ld, LDAP_OPT_PROTOCOL_VERSION, &version);
		}
		if (res != LDAP_SUCCESS)
			goto out;
        }

	struct berval cred;
        memset (&cred, 0, sizeof (cred));
	res = ldap_sasl_bind_s (ld, NULL, NULL, &cred, NULL, NULL, NULL);
	if (res != LDAP_SUCCESS)
		goto out;

	gchar *filter;
	gchar *attributes[] = {NULL, NULL};
	attributes[0] = mapping->attribute;
	filter = g_strdup_printf (mapping->filter_format, username);
	res = ldap_search_ext_s (ld, base, LDAP_SCOPE_SUBTREE,
				 filter, attributes, 0,
				 NULL, NULL, NULL, 2, &msg);
	g_free (filter);
	if (res != LDAP_SUCCESS)
		goto out;

	LDAPMessage *ldap_row;
	for (ldap_row = ldap_first_entry (ld, msg);
	     ldap_row;
	     ldap_row = ldap_next_entry (ld, ldap_row)) {
		char *attr, *uid;
		attr = ldap_get_dn (ld, ldap_row);
		if (attr) {
			BerElement* ber;
			for (uid = ldap_first_attribute (ld, ldap_row, &ber);
			     uid;
			     uid = ldap_next_attribute (ld, ldap_row, ber)) {
				BerValue **bvals;
				bvals = ldap_get_values_len (ld, ldap_row, uid);
				if (!bvals || !bvals[0] || bvals[1] || strcmp (bvals[0]->bv_val, username)) {
					g_free (dn);
					dn = NULL;
				}
				ldap_value_free_len (bvals);
				ldap_memfree (uid);
			}

			if (dn) {
				/* more than 1 entry => unique DN could not be identified */
				g_free (dn);
				dn = NULL;
				goto out;
			}

			dn = g_strdup (attr);
			ldap_memfree (attr);
		}
	}

 out:
	if (msg)
		ldap_msgfree (msg);
	ldap_unbind_ext (ld, NULL, NULL);
	/*g_print ("Identified DN: [%s]\n", dn);*/
	return dn;
}

/* 
 * Prepare connection request
 *
 * In this function, the following _must_ be done:
 *   - check for the presence and validify of the parameters required to actually open a connection,
 *     using @params
 *   - open the real connection to the database using the parameters previously checked, create one or
 *     more GdaDataModel objects and declare them to the virtual connection with table names
 *   - create a LdapConnectionData structure and associate it to @cnc
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_ldap_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaQuarkList *params, GdaQuarkList *auth)
{
	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* Check for connection parameters */
	const gchar *base_dn;
	const gchar *host;
	const gchar *tmp;
	const gchar *port;
	const gchar *user = NULL;
	gchar *dnuser = NULL;
        const gchar *pwd = NULL;
        const gchar *time_limit = NULL;
        const gchar *size_limit = NULL;
        const gchar *tls_method = NULL;
        const gchar *tls_cacert = NULL;
	int rtls_method = -1;
	gint rport;
	gboolean use_ssl, use_cache;

	/* calling the parent's function first */
	GdaServerProviderBase *parent_functions;
        parent_functions = gda_server_provider_get_impl_functions_for_class (gda_ldap_provider_parent_class, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);
	if (parent_functions->prepare_connection) {
		if (! parent_functions->prepare_connection (GDA_SERVER_PROVIDER (provider), cnc, params, auth))
			return FALSE;
	}

        base_dn = gda_quark_list_find (params, "DB_NAME");
        if (!base_dn) {
                gda_connection_add_event_string (cnc, "%s",
                                                 _("The connection string must contain the DB_NAME value"));
                return FALSE;
        }
	host = gda_quark_list_find (params, "HOST");
	if (!host)
		host = "127.0.0.1";
        port = gda_quark_list_find (params, "PORT");
        tmp = gda_quark_list_find (params, "USE_SSL");
	use_ssl = (tmp && ((*tmp == 't') || (*tmp == 'T'))) ? TRUE : FALSE;
	tmp = gda_quark_list_find (params, "USE_CACHE");
	use_cache = (!tmp || ((*tmp == 't') || (*tmp == 'T'))) ? TRUE : FALSE;
	if (port && *port)
		rport = atoi (port);
	else {
		if (use_ssl)
			rport = LDAPS_PORT;
		else
			rport = LDAP_PORT;
	}
	user = gda_quark_list_find (auth, "USERNAME");
        if (!user)
                user = gda_quark_list_find (params, "USERNAME");
        pwd = gda_quark_list_find (auth, "PASSWORD");
        if (!pwd)
                pwd = gda_quark_list_find (params, "PASSWORD");

	tls_cacert = gda_quark_list_find (params, "TLS_CACERT");
	tls_method = gda_quark_list_find (params, "TLS_REQCERT");
	if (tls_method && *tls_method) {
		if (! g_ascii_strcasecmp (tls_method, "never"))
			rtls_method = LDAP_OPT_X_TLS_NEVER;
		else if (! g_ascii_strcasecmp (tls_method, "hard"))
			rtls_method = LDAP_OPT_X_TLS_HARD;
		else if (! g_ascii_strcasecmp (tls_method, "demand"))
			rtls_method = LDAP_OPT_X_TLS_DEMAND;
		else if (! g_ascii_strcasecmp (tls_method, "allow"))
			rtls_method = LDAP_OPT_X_TLS_ALLOW;
		else if (! g_ascii_strcasecmp (tls_method, "try"))
			rtls_method = LDAP_OPT_X_TLS_TRY;
		else {
			gda_connection_add_event_string (cnc, "%s",
							 _("Invalid value for 'TLS_REQCERT'"));
			return FALSE;
		}
	}
	time_limit = gda_quark_list_find (params, "TIME_LIMIT");
	size_limit = gda_quark_list_find (params, "SIZE_LIMIT");

	/* open LDAP connection */
	LdapConnectionData *cdata;
	LDAP *ld;
        int res;
	gchar *url;

	if (use_ssl) {
		/* Configuring SSL/TLS options:
		 * this is for texting purpose only, and should actually be done through LDAP's conf.
		 * files, see: man 5 ldap.conf
		 *
		 * For example ~/.ldaprc can contain:
		 * TLS_REQCERT demand
		 * TLS_CACERT /usr/share/ca-certificates/mozilla/Thawte_Premium_Server_CA.crt
		 *
		 * Note: if server certificate verification fails,
		 * the error message is: "Can't contact LDAP server"
		 */
		if (rtls_method >= 0) {
			res = ldap_set_option (NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &rtls_method);
			if (res != LDAP_SUCCESS) {
				gda_connection_add_event_string (cnc, ldap_err2string (res));
				return FALSE;
			}
		}

		if (tls_cacert && *tls_cacert) {
			res = ldap_set_option (NULL, LDAP_OPT_X_TLS_CACERTFILE, tls_cacert);
			if (res != LDAP_SUCCESS) {
				gda_connection_add_event_string (cnc, ldap_err2string (res));
				return FALSE;
			}
		}

		url = g_strdup_printf ("ldaps://%s:%d", host, rport);
	}
	else
		url = g_strdup_printf ("ldap://%s:%d", host, rport);

	if (user && *user && ! gda_ldap_parse_dn (user, NULL)) {
		/* analysing the @user parameter */
		/* the user name is not a DN => we need to fetch the DN of the entry
		 * using filters defined in the "mappings" array @user */
		guint i;
		const gchar *ptr;
		GString *rname;
		rname = g_string_new ("");
		for (ptr = user; *ptr; ptr++) {
			if ((*ptr == ',') || (*ptr == '\\') || (*ptr == '#') ||
			    (*ptr == '+') || (*ptr == '<') ||
			    (*ptr == '>') || (*ptr == ';') || (*ptr == '"') ||
			    (*ptr == '=') || (*ptr == '*'))
				g_string_append_c (rname, '\\');
			g_string_append_c (rname, *ptr);
		}
		for (i = 0; i < sizeof (mappings) / sizeof (LdapAuthMapping); i++) {
			gchar *tmp;
			tmp = fetch_user_dn (url, base_dn, rname->str, &(mappings[i]));
			if (tmp) {
				dnuser = tmp;
				break;
			}
		}
		g_string_free (rname, TRUE);

		/* if no DN user has been found, then still use the provided name AS IS
		 * => dnuser can be %NULL here */
	}

	res = ldap_initialize (&ld, url);
        if (res != LDAP_SUCCESS) {
		gda_connection_add_event_string (cnc, ldap_err2string (res));
		g_free (url);
		g_free (dnuser);
                return FALSE;
        }

	cdata = g_new0 (LdapConnectionData, 1);
	cdata->keep_bound_count = 0;
	cdata->handle = ld;
	cdata->url = url;
	cdata->time_limit = 0;
	cdata->size_limit = 0;
	cdata->base_dn = g_strdup (base_dn);
	if (use_cache)
		cdata->attributes_cache_file = compute_data_file_name (params, TRUE, "attrs");

	/* set protocol version to 3 by default */
	int version = LDAP_VERSION3;
	res = ldap_set_option (cdata->handle, LDAP_OPT_PROTOCOL_VERSION, &version);
        if (res != LDAP_SUCCESS) {
		if (res == LDAP_PROTOCOL_ERROR) {
			version = LDAP_VERSION2;
			res = ldap_set_option (cdata->handle, LDAP_OPT_PROTOCOL_VERSION, &version);
		}
		if (res != LDAP_SUCCESS) {
			gda_connection_add_event_string (cnc, ldap_err2string (res));
			gda_ldap_free_cnc_data (cdata);
			g_free (dnuser);
			return FALSE;
		}
        }

	/* time limit */
	if (time_limit && *time_limit) {
		int limit = atoi (time_limit);
		res = ldap_set_option (cdata->handle, LDAP_OPT_TIMELIMIT, &limit);
		if (res != LDAP_SUCCESS) {
			gda_connection_add_event_string (cnc, ldap_err2string (res));
			gda_ldap_free_cnc_data (cdata);
			g_free (dnuser);
			return FALSE;
		}
		cdata->time_limit = limit;
	}

	/* size limit */
	if (size_limit && *size_limit) {
		int limit = atoi (size_limit);
		res = ldap_set_option (cdata->handle, LDAP_OPT_SIZELIMIT, &limit);
		if (res != LDAP_SUCCESS) {
			gda_connection_add_event_string (cnc, ldap_err2string (res));
			gda_ldap_free_cnc_data (cdata);
			g_free (dnuser);
			return FALSE;
		}
		cdata->size_limit = limit;
	}

	/* authentication */
	struct berval cred;
        memset (&cred, 0, sizeof (cred));
        cred.bv_len = pwd && *pwd ? strlen (pwd) : 0;
        cred.bv_val = pwd && *pwd ? (char *) pwd : NULL;
        res = ldap_sasl_bind_s (ld, dnuser ? dnuser : user, NULL, &cred, NULL, NULL, NULL);
	if (res != LDAP_SUCCESS) {
		gda_connection_add_event_string (cnc, ldap_err2string (res));
		gda_ldap_free_cnc_data (cdata);
		g_free (dnuser);
                return FALSE;
	}
	if (pwd) {
		gchar *tmp;
		tmp = g_strdup_printf ("PASSWORD=%s", pwd);
		cdata->auth = gda_quark_list_new_from_string (tmp);
		g_free (tmp);
	}
	if (dnuser) {
		gchar *tmp;
		tmp = g_strdup_printf ("USERNAME=%s", dnuser);
		if (cdata->auth)
			gda_quark_list_add_from_string (cdata->auth, tmp, FALSE);
		else
			cdata->auth = gda_quark_list_new_from_string (tmp);
		g_free (tmp);
		dnuser = NULL;
	}
	else if (user) {
		gchar *tmp;
		tmp = g_strdup_printf ("USERNAME=%s", user);
		if (cdata->auth)
			gda_quark_list_add_from_string (cdata->auth, tmp, FALSE);
		else
			cdata->auth = gda_quark_list_new_from_string (tmp);
		g_free (tmp);
	}

	/* set startup file name */
	gchar *fname;
	fname = compute_data_file_name (params, FALSE, "start");
	g_object_set ((GObject*) cnc, "startup-file", fname, NULL);
	g_free (fname);

	/* open virtual connection */
	gda_virtual_connection_internal_set_provider_data (GDA_VIRTUAL_CONNECTION (cnc), 
							   cdata, (GDestroyNotify) gda_ldap_free_cnc_data);
	gda_ldap_may_unbind (GDA_LDAP_CONNECTION (cnc));
	return TRUE;
}

/*
 * Close connection request
 *
 * In this function, the following _must_ be done:
 *   - Actually close the connection to the database using @cnc's associated LdapConnectionData structure
 *   - Free the LdapConnectionData structure and its contents
 *
 * Returns: TRUE if no error occurred, or FALSE otherwise (and an ERROR connection event must be added to @cnc)
 */
static gboolean
gda_ldap_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	/* Close the connection using the C API */
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
	if (!cdata)
		return FALSE;

	if (cdata->handle) {
		ldap_unbind_ext (cdata->handle, NULL, NULL);
		cdata->handle = NULL;
	}

	GdaServerProviderBase *fset;
	fset = gda_server_provider_get_impl_functions_for_class (gda_ldap_provider_parent_class, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);
	return fset->close_connection (provider, cnc);
}


gpointer
worker_gda_ldap_may_unbind (LdapConnectionData *cdata, GError **error)
{
	if (cdata->handle) {
                ldap_unbind_ext (cdata->handle, NULL, NULL);
		cdata->handle = NULL;
	}

	return NULL;
}

/*
 * Unbinds the connection if possible (i.e. if cdata->keep_bound_count is 0)
 * This allows to avoid keeping the connection to the LDAP server if unused
 */
void
gda_ldap_may_unbind (GdaLdapConnection *cnc)
{
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	LdapConnectionData *cdata;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
	if (!cdata || (cdata->keep_bound_count > 0)) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		return;
	}

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gda_ldap_may_unbind, (gpointer) cdata, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);
}

/*
 * Makes sure the connection is opened
 */
gboolean
gda_ldap_ensure_bound (GdaLdapConnection *cnc, GError **error)
{
	LdapConnectionData *cdata;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));

	if (!cdata)
		return FALSE;
	else if (cdata->handle)
		return TRUE;

	return gda_ldap_rebind (cnc, error);
}


gpointer
worker_gda_ldap_rebind (LdapConnectionData *cdata, GError **error)
{
	if (!cdata)
		return NULL;

	/*g_print ("Trying to reconnect...\n");*/
	LDAP *ld;
	int res;
	res = ldap_initialize (&ld, cdata->url);
	if (res != LDAP_SUCCESS) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
			     "%s", ldap_err2string (res));
		return NULL;
	}

	/* set protocol version to 3 by default */
	int version = LDAP_VERSION3;
	res = ldap_set_option (ld, LDAP_OPT_PROTOCOL_VERSION, &version);
        if (res != LDAP_SUCCESS) {
		if (res == LDAP_PROTOCOL_ERROR) {
			version = LDAP_VERSION2;
			res = ldap_set_option (ld, LDAP_OPT_PROTOCOL_VERSION, &version);
		}
		if (res != LDAP_SUCCESS) {
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
				     "%s", ldap_err2string (res));
			ldap_unbind_ext (ld, NULL, NULL);
			return NULL;
		}
        }

	/* authentication */
	struct berval cred;
	const gchar *pwd = "";
	const gchar *user = "";
	if (cdata->auth)
		pwd = gda_quark_list_find (cdata->auth, "PASSWORD");
        memset (&cred, 0, sizeof (cred));
        cred.bv_len = pwd && *pwd ? strlen (pwd) : 0;
        cred.bv_val = pwd && *pwd ? (char *) pwd : NULL;

	if (cdata->auth)
		user = gda_quark_list_find (cdata->auth, "USERNAME");
	res = ldap_sasl_bind_s (ld, user, NULL, &cred, NULL, NULL, NULL);
	if (cdata->auth)
		gda_quark_list_protect_values (cdata->auth);

	if (res != LDAP_SUCCESS) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
			     "%s", ldap_err2string (res));
		ldap_unbind_ext (ld, NULL, NULL);
                return NULL;
	}

	/* time limit */
	int limit = cdata->time_limit;
	res = ldap_set_option (cdata->handle, LDAP_OPT_TIMELIMIT, &limit);
	if (res != LDAP_SUCCESS) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
			     "%s", ldap_err2string (res));
		ldap_unbind_ext (ld, NULL, NULL);
                return NULL;
	}

	/* size limit */
	limit = cdata->size_limit;
	res = ldap_set_option (cdata->handle, LDAP_OPT_SIZELIMIT, &limit);
	if (res != LDAP_SUCCESS) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
			     "%s", ldap_err2string (res));
		ldap_unbind_ext (ld, NULL, NULL);
                return NULL;
	}

	/* all ok */
	if (cdata->handle) {
		/* don't call ldap_unbind_ext() as it often crashed the application */
		/*ldap_unbind_ext (cdata->handle, NULL, NULL);*/
	}
	cdata->handle = ld;

	/*g_print ("Reconnected!\n");*/
	return (gpointer) 0x01;
}

/*
 * Reopens a connection after the server has closed it (possibly because of a timeout)
 */
gboolean
gda_ldap_rebind (GdaLdapConnection *cnc, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	LdapConnectionData *cdata;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("cdata != NULL failed");
		return FALSE;
	}

	GdaServerProviderConnectionData *pcdata;
	pcdata = gda_connection_internal_get_provider_data_error ((GdaConnection*) cnc, NULL);

	GdaWorker *worker;
	worker = gda_worker_ref (gda_connection_internal_get_worker (pcdata));

	GMainContext *context;
	context = gda_server_provider_get_real_main_context ((GdaConnection *) cnc);

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_gda_ldap_rebind, (gpointer) cdata, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

/*
 * Server version request
 */
static const gchar *
gda_ldap_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	LdapConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return FALSE;

	if (! cdata->server_version) {
		/* FIXME: don't know how to get information about the LDAP server! */
	}
        return cdata->server_version;
}

/*
 * Extra SQL
 */
typedef struct {
	gchar             *table_name;
	gboolean           other_args; /* set to %TRUE if any of the arguments below have been specified */
	gchar             *base_dn;
	gchar             *filter;
	gchar             *attributes;
	GdaLdapSearchScope scope;
} ExtraSqlCommand;
static void extra_sql_command_free (ExtraSqlCommand *cmde);

#define NOT_AN_EXTRA_SQL_COMMAND (ExtraSqlCommand*) 0x01
#define SKIP_SPACES(x) for (; *(x) && (g_ascii_isspace (*(x)) || (*(x)=='\n')); (x)++)
static ExtraSqlCommand *parse_extra_sql_command (gchar *cmd, const gchar *cmde_name,
						 GError **error);
static GdaDataModel *table_parameters_describe (const gchar *base_dn, const gchar *filter,
						const gchar *attributes,
						GdaLdapSearchScope scope);
static GObject *
gda_ldap_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaStatement *stmt, GdaSet *params,
				     GdaStatementModelUsage model_usage,
				     GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	gchar *sql;
	sql = gda_statement_to_sql (stmt, params, NULL);
	if (sql) {
		/* parse SQL:
		 * CREATE LDAP TABLE <table name> WITH BASE='base_dn' FILTER='filter'
		 *              ATTRIBUTES='attributes' SCOPE='scope'
		 */
		gchar *ssql = sql;
		SKIP_SPACES (ssql);
		if (! g_ascii_strncasecmp (ssql, "CREATE", 6)) {
			ExtraSqlCommand *cmde;
			GError *lerror = NULL;
			GObject *retval = NULL;
			cmde = parse_extra_sql_command (ssql, "CREATE", &lerror);
			if (cmde != NOT_AN_EXTRA_SQL_COMMAND) {
				GdaConnectionEvent *event = NULL;
				if (cmde) {
					if (gda_ldap_connection_declare_table (GDA_LDAP_CONNECTION (cnc),
									       cmde->table_name, cmde->base_dn,
									       cmde->filter, cmde->attributes,
									       cmde->scope, &lerror))
						retval = (GObject*) gda_set_new (NULL);
					else {
						event = gda_connection_point_available_event (cnc,
											      GDA_CONNECTION_EVENT_ERROR);
						gda_connection_event_set_description (event, lerror && lerror->message ? 
										      lerror->message : _("No detail"));
						gda_connection_add_event (cnc, event);
						g_propagate_error (error, lerror);
					}
					extra_sql_command_free (cmde);
				}
				else {
					event = gda_connection_point_available_event (cnc,
										      GDA_CONNECTION_EVENT_ERROR);
					gda_connection_event_set_description (event, lerror && lerror->message ? 
									      lerror->message : _("No detail"));
					gda_connection_add_event (cnc, event);
					g_propagate_error (error, lerror);
				}

				gda_connection_internal_statement_executed (cnc, stmt, params, event);
				g_free (sql);
				return retval;
			}
		}

		/* parse SQL:
		 * DROP LDAP TABLE <table name>
		 */
		else if (! g_ascii_strncasecmp (ssql, "DROP", 4)) {
			ExtraSqlCommand *cmde;
			GError *lerror = NULL;
			GObject *retval = NULL;
			cmde = parse_extra_sql_command (ssql, "DROP", &lerror);
			if (cmde != NOT_AN_EXTRA_SQL_COMMAND) {
				GdaConnectionEvent *event = NULL;
				if (cmde) {
					if (cmde->other_args) {
						g_set_error (&lerror, GDA_SQL_PARSER_ERROR,
							     GDA_SQL_PARSER_SYNTAX_ERROR,
							     "%s",
							     _("Too many arguments"));
						event = gda_connection_point_available_event (cnc,
											      GDA_CONNECTION_EVENT_ERROR);
						gda_connection_event_set_description (event,
										      lerror->message);
						gda_connection_add_event (cnc, event);
						g_propagate_error (error, lerror);
					}
					else {
						if (gda_ldap_connection_undeclare_table (GDA_LDAP_CONNECTION (cnc),
											 cmde->table_name, &lerror))
							retval = (GObject*) gda_set_new (NULL);
						else {
							event = gda_connection_point_available_event (cnc,
												      GDA_CONNECTION_EVENT_ERROR);
							gda_connection_event_set_description (event, lerror && lerror->message ? 
											      lerror->message : _("No detail"));
							gda_connection_add_event (cnc, event);
							g_propagate_error (error, lerror);
						}
					}
					extra_sql_command_free (cmde);
				}
				else {
					event = gda_connection_point_available_event (cnc,
										      GDA_CONNECTION_EVENT_ERROR);
					gda_connection_event_set_description (event, lerror && lerror->message ? 
									      lerror->message : _("No detail"));
					gda_connection_add_event (cnc, event);
					g_propagate_error (error, lerror);
				}
				gda_connection_internal_statement_executed (cnc, stmt, params, event);
				g_free (sql);
				return retval;
			}
		}
		/* parse SQL:
		 * ALTER LDAP TABLE <table name> [...]
		 * DESCRIBE LDAP TABLE <table name>
		 */
		else if (! g_ascii_strncasecmp (ssql, "ALTER", 5) ||
			 ! g_ascii_strncasecmp (ssql, "DESCRIBE", 8)) {
			ExtraSqlCommand *cmde;
			GError *lerror = NULL;
			GObject *retval = NULL;
			gboolean alter;

			alter = g_ascii_strncasecmp (ssql, "ALTER", 5) ? FALSE : TRUE;
			    
			cmde = parse_extra_sql_command (ssql, alter ? "ALTER" : "DESCRIBE", &lerror);
			if ((cmde != NOT_AN_EXTRA_SQL_COMMAND) &&
			    (alter || (!alter && !cmde->other_args))) {
				GdaConnectionEvent *event = NULL;
				if (cmde) {
					const gchar *base_dn, *filter, *attributes;
					GdaLdapSearchScope scope;
					if (gda_ldap_connection_describe_table (GDA_LDAP_CONNECTION (cnc),
										cmde->table_name,
										&base_dn, &filter,
										&attributes, &scope, &lerror)) {
						if (cmde->other_args) {
							if (! cmde->base_dn && base_dn)
								cmde->base_dn = g_strdup (base_dn);
							if (! cmde->filter && filter)
								cmde->filter = g_strdup (filter);
							if (! cmde->attributes && attributes)
								cmde->attributes = g_strdup (attributes);
							if (! cmde->scope)
								cmde->scope = scope;
							if (gda_ldap_connection_undeclare_table (GDA_LDAP_CONNECTION (cnc),
												 cmde->table_name, &lerror) &&
							    gda_ldap_connection_declare_table (GDA_LDAP_CONNECTION (cnc),
											       cmde->table_name, cmde->base_dn,
											       cmde->filter, cmde->attributes,
											       cmde->scope, &lerror))
								retval = (GObject*) gda_set_new (NULL);
						}
						else {
							GdaDataModel *array;
							array = table_parameters_describe (base_dn, filter,
											   attributes, scope);
							retval = (GObject*) array;
						}
					}
					if (!retval) {
						event = gda_connection_point_available_event (cnc,
											      GDA_CONNECTION_EVENT_ERROR);
						gda_connection_event_set_description (event, lerror && lerror->message ? 
										      lerror->message : _("No detail"));
						gda_connection_add_event (cnc, event);
						g_propagate_error (error, lerror);
					}
					extra_sql_command_free (cmde);
				}
				gda_connection_internal_statement_executed (cnc, stmt, params, event);
				g_free (sql);
				return retval;
			}
		}
		g_free (sql);
	}

	/* check connection to LDAP is Ok */
	if (! gda_ldap_ensure_bound (GDA_LDAP_CONNECTION (cnc), error))
		return NULL;

	GdaServerProviderBase *fset;
	fset = gda_server_provider_get_impl_functions_for_class (gda_ldap_provider_parent_class, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);
	return fset->statement_execute (provider, cnc, stmt, params,
					model_usage, col_types,
					last_inserted_row, error);
}

/*
 * Free connection's specific data
 */
static void
gda_ldap_free_cnc_data (LdapConnectionData *cdata)
{
	if (cdata->handle)
                ldap_unbind_ext (cdata->handle, NULL, NULL);
	if (cdata->attributes_hash)
		g_hash_table_destroy (cdata->attributes_hash);
	g_free (cdata->attributes_cache_file);
	g_free (cdata->base_dn);
	g_free (cdata->server_version);
	g_free (cdata->url);
	if (cdata->auth)
		gda_quark_list_free (cdata->auth);
	g_free (cdata);
}

static const gchar *
scope_to_string (GdaLdapSearchScope scope)
{
	switch (scope) {
	case GDA_LDAP_SEARCH_BASE:
		return "BASE";
	case GDA_LDAP_SEARCH_ONELEVEL:
		return "ONELEVEL";
	case GDA_LDAP_SEARCH_SUBTREE:
		return "SUBTREE";
	default:
		return _("Unknown");
	}
}

static GdaDataModel *
table_parameters_describe (const gchar *base_dn, const gchar *filter, const gchar *attributes,
			   GdaLdapSearchScope scope)
{
	GdaDataModel *array;
	GValue *v1, *v2;
	GList *list;
	array = gda_data_model_array_new_with_g_types (2, G_TYPE_STRING,
						       G_TYPE_STRING);
	gda_data_model_set_column_title (array, 0, _("Parameter"));
	gda_data_model_set_column_title (array, 1, _("Value"));
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "BASE");
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), base_dn);
	list = g_list_append (NULL, v1);
	list = g_list_append (list, v2);
	gda_data_model_append_values (array, list, NULL);
	g_list_free (list);
	gda_value_free (v1);
	gda_value_free (v2);
	
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "FILTER");
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), filter);
	list = g_list_append (NULL, v1);
	list = g_list_append (list, v2);
	gda_data_model_append_values (array, list, NULL);
	g_list_free (list);
	gda_value_free (v1);
	gda_value_free (v2);
	
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "ATTRIBUTES");
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), attributes);
	list = g_list_append (NULL, v1);
	list = g_list_append (list, v2);
	gda_data_model_append_values (array, list, NULL);
	g_list_free (list);
	gda_value_free (v1);
	gda_value_free (v2);
	
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), "SCOPE");
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), scope_to_string (scope));
	list = g_list_append (NULL, v1);
	list = g_list_append (list, v2);
	gda_data_model_append_values (array, list, NULL);
	g_list_free (list);
	gda_value_free (v1);
	gda_value_free (v2);
	return array;
}

/*
 * Extra commands parsing
 */
/*
 * consumes @str for a singly quoted string
 * Returns: a pointer to the next non analysed char
 */
static gchar *
parse_string (gchar *str, gchar **out_part)
{
	gchar *ptr = str;
	*out_part = NULL;

	SKIP_SPACES (ptr);
	if (!*ptr)
		return NULL;
	if (*ptr != '\'') {
		if (!g_ascii_strncasecmp (ptr, "null", 4)) {
			ptr += 4;
			return ptr;
		}
		else
			return NULL;
	}
	ptr++;
	*out_part = ptr;
	for (; *ptr && (*ptr != '\''); ptr++);
	if (!*ptr)
		return NULL;
	*ptr = 0;
	ptr++;
	return ptr;
}

/*
 * consumes @str for an identifier
 * Returns: a pointer to the next non analysed char
 */
static gchar *
parse_ident (gchar *str, gchar **out_part)
{
	gchar *ptr = str;
	*out_part = NULL;

	SKIP_SPACES (ptr);
	*out_part = ptr;
	for (; *ptr && (g_ascii_isalnum (*ptr) || (*ptr == '_')) ; ptr++);
	if (ptr == *out_part) {
		*out_part = NULL;
		return NULL;
	}
	return ptr;
}

static void
extra_sql_command_free (ExtraSqlCommand *cmde)
{
	g_free (cmde->table_name);
	g_free (cmde->base_dn);
	g_free (cmde->filter);
	g_free (cmde->attributes);
	g_free (cmde);
}

/*
 * CREATE LDAP TABLE <table name> BASE='base_dn' FILTER='filter' ATTRIBUTES='attributes' SCOPE='scope'
 * DROP LDAP TABLE <table name>
 * ALTER LDAP TABLE <table name>
 * ALTER LDAP TABLE <table name> BASE='base_dn' FILTER='filter' ATTRIBUTES='attributes' SCOPE='scope'
 *
 * Returns: a #ExtraSqlCommand pointer, or %NULL, or NOT_AN_EXTRA_SQL_COMMAND (if not "CREATE LDAP...")
 */
static ExtraSqlCommand *
parse_extra_sql_command (gchar *cmd, const gchar *cmde_name, GError **error)
{
	ExtraSqlCommand *args;
	gchar *ptr, *errptr, *part, *tmp;
	args = g_new0 (ExtraSqlCommand, 1);
	args->other_args = FALSE;
	
	ptr = cmd + strlen (cmde_name);
	
	/* make sure about complete command */
	errptr = ptr;
	if (! (ptr = parse_ident (ptr, &part)))
		return NOT_AN_EXTRA_SQL_COMMAND;
	if (!part || g_ascii_strncasecmp (part, "ldap", 4))
		return NOT_AN_EXTRA_SQL_COMMAND;

	errptr = ptr;
	if (! (ptr = parse_ident (ptr, &part)))
		goto onerror;
	if (!part || g_ascii_strncasecmp (part, "table", 5))
		goto onerror;

	/* table name */
	SKIP_SPACES (ptr);
	errptr = ptr;
	if (! (ptr = parse_ident (ptr, &part)))
		goto onerror;
	tmp = g_strndup (part, ptr-part);
	args->table_name = g_ascii_strdown (tmp, -1);
	g_free (tmp);

	/* key=value arguments */
	while (TRUE) {
		errptr = ptr;
		SKIP_SPACES (ptr);
		if (! (ptr = parse_ident (ptr, &part))) {
			ptr = errptr;
			break;
		}
		if (part) {
			gchar **where = NULL;
			if (!g_ascii_strncasecmp (part, "base", 4))
				where = &(args->base_dn);
			else if (!g_ascii_strncasecmp (part, "filter", 6))
				where = &(args->filter);
			else if (!g_ascii_strncasecmp (part, "attributes", 10))
				where = &(args->attributes);
			else if (!g_ascii_strncasecmp (part, "scope", 5))
				where = NULL;
			else
				goto onerror;
			
			/* = */
			errptr = ptr;
			SKIP_SPACES (ptr);
			if (*ptr != '=')
				goto onerror;
			ptr++;
			
			/* value */
			errptr = ptr;
			SKIP_SPACES (ptr);
			if (! (ptr = parse_string (ptr, &part)))
				goto onerror;
			if (part) {
				if (where)
					*where = g_strdup (part);
				else {
					if (!g_ascii_strcasecmp (part, "base"))
						args->scope = GDA_LDAP_SEARCH_BASE;
					else if (!g_ascii_strcasecmp (part, "onelevel"))
						args->scope = GDA_LDAP_SEARCH_ONELEVEL;
					else if (!g_ascii_strcasecmp (part, "subtree"))
						args->scope = GDA_LDAP_SEARCH_SUBTREE;
					else
						goto onerror;
				}
				args->other_args = TRUE;
			}
			else
				goto onerror;
		}
		else
			break;
	}

	/* end */
	SKIP_SPACES (ptr);
	if (*ptr && (*ptr != ';'))
		goto onerror;
#ifdef GDA_DEBUG_NO
	g_print ("TABLE=>%s, BASE=>%s, FILTER=>%s, ATTRIBUTES=>%s, SCOPE=>%d\n", args->table_name,
		 args->base_dn, args->filter,
		 args->attributes, args->scope);
#endif

	return args;

 onerror:
	SKIP_SPACES (errptr);
	g_set_error (error, GDA_SQL_PARSER_ERROR, GDA_SQL_PARSER_SYNTAX_ERROR,
		     _("near \"%s\": syntax error"), errptr);
	extra_sql_command_free (args);
	return NULL;
}
