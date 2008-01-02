/* GDA LDAP provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      German Poo-Caaman~o <gpoo@ubiobio.cl>
 *      Exell Enrique Franklin Jiménex <arawaco@ieee.org> 
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

#include <string.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-server-provider-extra.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
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
static const gchar *gda_ldap_provider_get_version (GdaServerProvider *provider);
static const gchar *gda_ldap_provider_get_server_version (GdaServerProvider *provider,
							  GdaConnection *cnc);
								  
static GObjectClass *parent_class = NULL;


/*private  :-D*/
char*  get_root_dse_param(LDAP*,char*);

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

	provider_class->get_version = gda_ldap_provider_get_version;
	provider_class->get_server_version = gda_ldap_provider_get_server_version;
	provider_class->get_info = NULL;
	provider_class->supports_feature = gda_ldap_provider_supports;
	provider_class->get_schema = gda_ldap_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->create_connection = NULL;
	provider_class->open_connection = gda_ldap_provider_open_connection;
	provider_class->close_connection = gda_ldap_provider_close_connection;
	provider_class->get_database = gda_ldap_provider_get_database;
	provider_class->change_database = NULL;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = NULL;
	provider_class->execute_query = NULL;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = NULL;
	provider_class->commit_transaction = NULL;
	provider_class->rollback_transaction = NULL;
	provider_class->add_savepoint = NULL;
	provider_class->rollback_savepoint = NULL;
	provider_class->delete_savepoint = NULL;
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

	if (G_UNLIKELY (type == 0)) {
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
	/*GdaConnectionEvent *error;*/
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
        /*describes best the ldap*/
        myprv->t_binddn=username; 
	if (!t_host)
		t_host = "localhost";
	if (!t_port)
		t_port = "389";
	if (!username)
		{	
			t_binddn = username;
			myprv->t_binddn=t_binddn; 
		}	
	if (!password)
		t_password = password;

	ldap = ldap_init (t_host, atoi (t_port));
	
	if (ldap == NULL) {
		/*
		 * LDAP_OPT_RESULT_CODE it does not compile
		 * LDAP_OPT_RESULT_CODE return error, i change it to 0x31
		 * but we have to use LDAP_OPT_RESULT_CODE
		 */
		ldap_get_option(ldap,0x31,&myprv->rc );
		fprintf( stderr, "%s: %s", "gda-ldap-provider: ldap_init",ldap_err2string(myprv->rc));
		return FALSE;
	}

	/*    Synchronously authenticates 
	      the client to the LDAP server using a DN and a password.
	      t_binddn =Distinguished name (DN) of the user who wants to authenticate
	      anonymous this could be NULL
	*/  
	myprv->rc=ldap_simple_bind_s(ldap,t_binddn,t_password);
	if(myprv->rc != LDAP_SUCCESS ) 
		{
			fprintf(stderr,"ldap_simple_bind_s: %s\n",ldap_err2string(myprv->rc));
			return FALSE;
		}
 
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE,ldap);
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
  	char    *name=NULL;
	LDAP    *ldap=NULL;
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

        ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	
	if (ldap) {
		gda_connection_add_event_string (cnc, _("Invalid LDAP handle"));
		return NULL;
	}

	/*namingContexts
	 *The naming contexts supported by this server (for example, dc=gnomedb,dc=libgda). 
	 * only ldap v3
	 * return the namingcontext, or NULL
	 */
        name=get_root_dse_param(ldap,"namingContexts");
        if(name)
		return name;
        /*name == NULL, return default*/
      	return 	myprv->t_binddn;  
}


/* supports handler for the GdaLdapProvider class */
static gboolean
gda_ldap_provider_supports (GdaServerProvider *provider,
			    GdaConnection *cnc,
			    GdaConnectionFeature feature)
{
	GdaLdapProvider *myprv = (GdaLdapProvider *) provider;

	g_return_val_if_fail (GDA_IS_LDAP_PROVIDER (myprv), FALSE);

	//	switch (feature) {
	//	case GDA_CONNECTION_FEATURE_SQL :
	//		return TRUE;
	//	default : ;
	//	}

	return FALSE;
}

static void
add_string_row (GdaDataModelArray *recset, const gchar *str)
{
	GValue *value;
	GList list;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (recset));

	g_value_set_string (value = gda_value_new (G_TYPE_STRING), str);
	list.data = value;
	list.next = NULL;
	list.prev = NULL;

	gda_data_model_append_values (GDA_DATA_MODEL (recset), &list, NULL);

	gda_value_free (value);
}

static GdaDataModel *
get_ldap_tables (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModel *recset;
	LDAP *ldap;
	BerElement *ptr=NULL;
        LDAPControl c;
     	LDAPObjectClass *oc;
     	LDAPMessage *res=NULL, *e;
	const gchar *errp;
	char *a;
	char  *info,*dn;
  	char    **vals=NULL;
	GList* list;
	int rc,i,j;
        char *subschema = NULL;
        const char *subschemasubentry[] = { "subschemaSubentry",
					    NULL };
        const char *schema_attrs[] = {"objectClasses",
				      NULL };
	list=NULL;	
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap)
		return FALSE;
	recset = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES));
        gda_server_provider_init_schema_model (recset, GDA_CONNECTION_SCHEMA_TABLES);

	rc = ldap_search_ext_s(ldap,
			       "",					/* search base */
			       LDAP_SCOPE_BASE,			/* scope */
			       "(objectClass=*)",			/* filter */
			       (char**) subschemasubentry,		 /* attrs */
			       0,					/* attrsonly */
			       NULL,					/* serverctrls */
			       NULL,					/* clientctrls */
			       LDAP_NO_LIMIT,				/* timeout */
			       LDAP_NO_LIMIT,				/* sizelimit */
			       &res);

	if(rc == LDAP_NOT_SUPPORTED) {
		rc = ldap_search_s(ldap,"",LDAP_SCOPE_BASE,"(objectClass=*)",
				   (char **)subschemasubentry,0,&res);
	}
	if (rc != LDAP_SUCCESS) {
		fprintf(stderr,"ldap_search_ext_s: %s\n", ldap_err2string(rc));
	}
	if (!res)  // this is: res==null
                ldap_unbind(ldap);
	g_return_val_if_fail (rc == LDAP_SUCCESS, NULL);

	if( (e = ldap_first_entry(ldap, res)) ) {
		if( (a = ldap_first_attribute(ldap, res, &ptr)) ) {
			if( (vals = ldap_get_values(ldap, res, a)) ) {
				subschema =malloc(sizeof(char)*(1+strlen(vals[0])));
				strcpy(subschema,vals[0]);		 
				ldap_value_free(vals);
			}
			ldap_memfree(a);
		}
	        if(ptr)
			ber_free(ptr, 0);
	}
	ldap_msgfree(res);
	if(subschema == NULL) {
		printf("No schema information found on server");
		return(NULL);
	}
	rc = ldap_search_ext_s(ldap,
			       subschema,		   	        /* search base */
			       LDAP_SCOPE_BASE,			/* scope */
			       "(objectClass=*)",			/* filter */
			       (char**)schema_attrs,		 /* attrs */
			       0,					/* attrsonly */
			       NULL,					/* serverctrls */
			       NULL,					/* clientctrls */
			       LDAP_NO_LIMIT,				/* timeout */
			       LDAP_NO_LIMIT,				/* sizelimit */
			       &res);
	if(rc == LDAP_NOT_SUPPORTED) {
	        rc = ldap_search_s(ldap, subschema, LDAP_SCOPE_BASE,
			           "(objectClass=*)", (char**) schema_attrs, 0,
			           &res);
	}
	free(subschema);

	if (rc != LDAP_SUCCESS) {
		fprintf(stderr,"ldap_search_ext_s subschema: %s\n", ldap_err2string(rc));
	}
	if (!res)  // this is: res==null
                ldap_unbind(ldap);
	g_return_val_if_fail (rc == LDAP_SUCCESS, NULL);
           

	for(e = ldap_first_entry(ldap,res); e; e = ldap_next_entry(ldap,e)) {
	        for(a= ldap_first_attribute(ldap,res,&ptr); a;
		    a=ldap_next_attribute(ldap,res,ptr)) {
			vals = ldap_get_values(ldap,res,a);
			if(vals) {
				for(i = 0; vals[i]!=NULL; i++) {
					char *owner,*comments,*name,*sql;
					GValue *val;
					sql=name=owner=comments=NULL;	  	
					if(!strcmp(a, "objectClasses")) {
						oc=ldap_str2objectclass(vals[i],&rc, &errp, 0x03);	
						if(oc) {
							/*only retrieves the first name */
							/*if it does not name*/
							if(!oc->oc_names[0]) {/*do not have name */
								name=malloc(sizeof(char));
								name[0]='\0';
							} 
							else {
								name=malloc(sizeof(char)*(1+strlen(oc->oc_names[0])));
								strcpy(name,oc->oc_names[0]);
							}
							/*retrieve the atriibutes and find the owner*/
							/*find the owner attribute, if exist ;)*/
							if(oc->oc_at_oids_must)
								for (j=0; oc->oc_at_oids_must[j]!=NULL; j++) {
									if(!strcmp(oc->oc_at_oids_must[j],"owner")) {
										owner="yes";
										break;
									} 
								}
							if(!owner && oc->oc_at_oids_may)
								for(j=0;oc->oc_at_oids_may[j]!=NULL;j++){
									if(!strcmp(oc->oc_at_oids_may[j],"owner")) {
										owner="yes";
										break;
									} 
								}
							if(!owner) { 
								owner=malloc(sizeof(char));
								owner[0]='\0'; 
							}
							/*retrieve the description*/
							if(!oc->oc_desc) {
								comments=malloc(sizeof(char));
								comments[0]='\0';
							}
							else {
								comments=malloc(sizeof(char)*(1+strlen(oc->oc_desc)));
								strcpy(comments,oc->oc_desc);
							}
							sql=malloc(sizeof(char));
							sql[0]='\0';
							/*the first column is added to glist*/
							val=gda_value_new (G_TYPE_STRING);
							gda_value_set_from_string(val,name,G_TYPE_STRING);			  
							list = g_list_append(list,val);
							//printf("name %s\n",name);
							/*the second column is added to glist*/
							val=gda_value_new (G_TYPE_STRING);
							gda_value_set_from_string(val,owner,G_TYPE_STRING);			  
							list = g_list_append(list,val);
							//printf("owner %s\n",owner);
							/*the third column is added to glist*/
							val=gda_value_new (G_TYPE_STRING);
							gda_value_set_from_string(val,comments,G_TYPE_STRING);			  
							list = g_list_append(list,val);
							//printf("comments %s\n",comments);
							/*the fourth column is added to glist*/
							val=gda_value_new (G_TYPE_STRING);
							gda_value_set_from_string(val,sql,G_TYPE_STRING);			  
							list = g_list_append(list,val);
							//printf("sql %s\n",sql);
							/*append a row*/
							gda_data_model_append_values (recset,list,NULL);
							g_list_foreach (list,(GFunc)gda_value_free, NULL);
							g_list_free(list);
							list=NULL;
						}//if(oc)
					}//if(strcmp("ob...
				}//for(...,vals..
				ldap_value_free(vals);  
			}//if vals
			if(a)
				ldap_memfree (a);
		} //for(....,&ptr
		if(ptr)
	                ber_free(ptr, 0);
	} //for( first...
	/* free the search results */
	ldap_msgfree( res );
	/*voila :-D*/
	return recset;
}

static GdaDataModel *
get_ldap_types (GdaConnection *cnc, GdaParameterList *params)
{
	GdaDataModelArray *recset;
        gint i;
        struct {
                const gchar *name;
                const gchar *owner;
                const gchar *comments;
                GType type; 
                const gchar *synonyms;
        } types[] = { 
                { "bool", "", "Boolean", G_TYPE_BOOLEAN, NULL },
                { "blob", "", "Binary blob", GDA_TYPE_BINARY, NULL },
                { "integer", "", "Integer", G_TYPE_INT64, NULL  },
		{ "varchar", "", "Variable Length Char", G_TYPE_STRING, "string"},
        };

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* create the recordset */
        recset = (GdaDataModelArray *) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES));
        gda_server_provider_init_schema_model (GDA_DATA_MODEL (recset), GDA_CONNECTION_SCHEMA_TYPES);

        /* fill the recordset */
        for (i = 0; i < sizeof (types) / sizeof (types[0]); i++) {
                GList *value_list = NULL;
                GValue *tmpval;

                g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].name);
                value_list = g_list_append (value_list, tmpval);

                g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].owner);
                value_list = g_list_append (value_list, tmpval);

                g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].comments);
                value_list = g_list_append (value_list, tmpval);

                g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), types[i].type);
                value_list = g_list_append (value_list, tmpval);

                g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].synonyms);
                value_list = g_list_append (value_list, tmpval);

                gda_data_model_append_values (GDA_DATA_MODEL (recset), value_list, NULL);

                g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
                g_list_free (value_list);
        }

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
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

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
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	else
		return NULL;

	ldap = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_LDAP_HANDLE);
	if (!ldap) {
		gda_connection_add_event_string (cnc, _("Invalid LDAP handle"));
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




/*root DSE
 * this function return any info about
 * the server.
 * root DSE is specified as part of the LDAPv3
 * LDAPv2 does not use it.
 * return a info if find it, else return a NULL
 * ONLY  RETURN ONE CHAR,"atrr1,atrr2,atrr3,....atrrn"
 * 
 */
char*  get_root_dse_param(LDAP *ldap, char* param)
{
	BerElement *ptr=NULL;
	int rc,i,len=0;
	LDAPControl c;
	LDAPMessage *res=NULL, *e;
	const gchar *errp;
	char *a;
	char  *info;
	char    **vals=NULL;
	char* info_dse=NULL;
	char *srvrattrs[]=
		{
			param,
			NULL
		};	
	if (!ldap)
		return NULL;
	/*before  search anything 
	 * is going to take some info  from ldapv3
	 * some root DSE data
	 */ 
	/*
	 * Set automatic referral processing off. 
	 */
	if (ldap_set_option(ldap,LDAP_OPT_REFERRALS,LDAP_OPT_OFF ) != 0 ) {
                rc = ldap_get_lderrno( ldap, NULL, NULL );
                fprintf( stderr, "ldap_set_option: %s\n", ldap_err2string(rc));
                return NULL;
	} 
	rc =ldap_search_ext_s(ldap, "", LDAP_SCOPE_BASE, "(objectclass=*)",srvrattrs,
                              0, NULL, NULL, NULL, 0, &res); 
	/* Check the search results. */
	switch(rc) {
		/* If successful, the root DSE was found ;) */
	case LDAP_SUCCESS:
		break;
		/* If the root DSE was not found, the server does not comply
		   with the LDAPv3 protocol. */
	case LDAP_PARTIAL_RESULTS:
	case LDAP_NO_SUCH_OBJECT:
	case LDAP_OPERATIONS_ERROR:
	case LDAP_PROTOCOL_ERROR:{
		printf( "LDAP server returned result code %d (%s).\n"
			"This server does not support the LDAPv3 protocol.\n",
			rc,ldap_err2string(rc));
		return NULL;
	}break;	
		/* If any other value is returned, an error must have occurred. */
	default:
		fprintf( stderr, "ldap_search_ext_s: %s\n", ldap_err2string( rc ) );
		return NULL;
	} 

	/* Since only one entry should have matched, get that entry. */
	e = ldap_first_entry(ldap,res);
	if ( e == NULL ) {
		fprintf( stderr, "ldap_search_ext_s: Unable to get root DSE. unable get %s\n",param);
		ldap_memfree(res);
		res=NULL;
		return NULL;
	}	
	/* Iterate through one attribute in the entry. */
	if(res)
		{
			a = ldap_first_attribute( ldap, e, &ptr );
			if ((vals = ldap_get_values( ldap, e, a)) != NULL ) {
				for ( i = 0; vals[i] != NULL; i++ ) {
					info_dse=realloc(info_dse,(sizeof(char)*(2+len+strlen(vals[i]))));
					if(i==0)
						info_dse[0]='\0';	
					strcat(info_dse,vals[i]);			
					strcat(info_dse,",");				 
					len=strlen(info_dse);	
				}
				info_dse[len-1]='\0';	
				/* Free memory allocated by ldap_get_values(). */
				ldap_value_free( vals );
			}
			/* Free memory allocated by ldap_first_attribute(). */
			ldap_memfree( a );
			/* Free memory allocated by ldap_first_attribute(). */
			if ( ptr != NULL ) {
				ber_free( ptr, 0 );
			}
			ldap_msgfree(res);
		}
	return info_dse;
}
