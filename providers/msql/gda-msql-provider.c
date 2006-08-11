/* GDA mSQL Provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net
 *         Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-msql.h"
#include "gda-msql-recordset.h"
/*#include "gda-msql-provider.h"*/

#include <libgda/sql-delimiter/gda-sql-delimiter.h>

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_MSQL_HANDLE "GDA_mSQL_mSQLHandle"
#define OBJECT_DATA_MSQL_DBNAME "GDA_mSQL_dbname"

/*----[ Forwards ]----------------------------------------------------------*/

static void gda_msql_provider_class_init(GdaMsqlProviderClass*);
static void gda_msql_provider_init(GdaMsqlProvider*,GdaMsqlProviderClass*);
static void gda_msql_provider_finalize(GObject*);
static const gchar *gda_msql_provider_get_version(GdaServerProvider*);
static gboolean gda_msql_provider_open_connection(GdaServerProvider*,
                                                  GdaConnection*,
                                                  GdaQuarkList*,
                                                  const gchar*,
                                                  const gchar*);
static gboolean gda_msql_provider_close_connection(GdaServerProvider*,
                                                   GdaConnection*);
static const gchar *gda_msql_provider_get_server_version(GdaServerProvider*,
                                                         GdaConnection*);
static const gchar *gda_msql_provider_get_database(GdaServerProvider*,
                                                   GdaConnection*);
static gboolean gda_msql_provider_change_database(GdaServerProvider*,
                                                  GdaConnection*,const gchar*);
static GList *gda_msql_provider_execute_command(GdaServerProvider*,
                                                GdaConnection*,
                                                GdaCommand*,
                                                GdaParameterList*);
static gboolean gda_msql_provider_begin_transaction(GdaServerProvider*,
                                                    GdaConnection*,
                                                    GdaTransaction*);
static gboolean gda_msql_provider_commit_transaction(GdaServerProvider*,
                                                     GdaConnection*,
                                                     GdaTransaction*);
static gboolean gda_msql_provider_rollback_transaction(GdaServerProvider*,
                                                       GdaConnection*,
                                                       GdaTransaction*);
static gboolean gda_msql_provider_supports(GdaServerProvider*,
                                           GdaConnection*,
                                           GdaConnectionFeature);
static GdaDataModel *gda_msql_provider_get_schema(GdaServerProvider*,
                                                  GdaConnection*,
                                                  GdaConnectionSchema,
                                                  GdaParameterList*);

static GObjectClass *parent_class = NULL;

/*
 * GdaMsqlProvider class implementation 
 */

static void gda_msql_provider_class_init(GdaMsqlProviderClass *cl)
{
	GObjectClass *ocl           = G_OBJECT_CLASS(cl);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS(cl);

	parent_class=g_type_class_peek_parent(cl);
	ocl->finalize=gda_msql_provider_finalize;

	provider_class->get_version = gda_msql_provider_get_version;
	provider_class->get_server_version = gda_msql_provider_get_server_version;
	provider_class->get_info = NULL;
	provider_class->supports = gda_msql_provider_supports;
	provider_class->get_schema = gda_msql_provider_get_schema;

	provider_class->get_data_handler = NULL;
	provider_class->string_to_value = NULL;
	provider_class->get_def_dbms_type = NULL;

	provider_class->open_connection = gda_msql_provider_open_connection;
	provider_class->close_connection = gda_msql_provider_close_connection;
	provider_class->get_database = gda_msql_provider_get_database;
	provider_class->change_database = gda_msql_provider_change_database;

	provider_class->supports_operation = NULL;
        provider_class->create_operation = NULL;
        provider_class->render_operation = NULL;
        provider_class->perform_operation = NULL;

	provider_class->execute_command = gda_msql_provider_execute_command;
	provider_class->get_last_insert_id = NULL;

	provider_class->begin_transaction = gda_msql_provider_begin_transaction;
	provider_class->commit_transaction = gda_msql_provider_commit_transaction;
	provider_class->rollback_transaction = gda_msql_provider_rollback_transaction;
	
	provider_class->create_blob = NULL;
	provider_class->fetch_blob = NULL;	
}

static void 
gda_msql_provider_init(GdaMsqlProvider *p,GdaMsqlProviderClass *cl)
{
	/* pathetically empty */
}

static void 
gda_msql_provider_finalize(GObject *obj)
{
	GdaMsqlProvider *p=(GdaMsqlProvider*)obj;
  
	if (!GDA_IS_MSQL_PROVIDER(p))
		return;
	parent_class->finalize(obj);
}

GType gda_msql_provider_get_type(void)
{
	static GType type=0;
  
	if (!type) {
		static GTypeInfo info = {
			sizeof(GdaMsqlProviderClass),
			(GBaseInitFunc)NULL,
			(GBaseFinalizeFunc)NULL,
			(GClassInitFunc)gda_msql_provider_class_init,
			NULL,NULL,sizeof(GdaMsqlProvider),0,
			(GInstanceInitFunc)gda_msql_provider_init
		};
		type=g_type_register_static(PARENT_TYPE,"GdaMsqlProvider",&info,0);
	}
	return type;
}

GdaServerProvider *gda_msql_provider_new(void)
{
	GdaMsqlProvider *p;

	p=g_object_new(gda_msql_provider_get_type(),NULL);
	return GDA_SERVER_PROVIDER(p);
}

static const gchar *gda_msql_provider_get_version(GdaServerProvider *p1)
{
	GdaMsqlProvider *p=(GdaMsqlProvider*)p1;
  
	if (!GDA_IS_MSQL_PROVIDER(p))
		return NULL;
	return PACKAGE_VERSION;
}

static gboolean gda_msql_provider_open_connection(GdaServerProvider *p1,
                                                  GdaConnection *cnc,
                                                  GdaQuarkList *params,
                                                  const gchar *username,
                                                  const gchar *password)
{
	const gchar *_host = NULL;
	const gchar *_db = NULL;
	GdaConnectionEvent *error;
	int rc=-1,sock=-1;
	gint *sock_ptr;
	GdaMsqlProvider *p=(GdaMsqlProvider*)p1;
  
	if (!GDA_IS_MSQL_PROVIDER(p)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	_host=gda_quark_list_find(params,"HOST");
	_db=gda_quark_list_find(params,"DATABASE");
	if (!_host) _host="localhost";
	sock=msqlConnect((char*)_host);
	if (sock<0) {
		error=gda_msql_make_error(sock);
		gda_connection_add_event(cnc,error);
		return FALSE;
	}
	rc=msqlSelectDB(sock,(char*)_db);
	if (rc<0) {
		error=gda_msql_make_error(rc);
		if (cnc) gda_connection_add_event(cnc,error);
		msqlClose(sock);
		return FALSE;
	}
	sock_ptr=g_new0(gint,1);
	*sock_ptr=(gint)sock;
	g_object_set_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE,sock_ptr);
	g_object_set_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_DBNAME,g_strdup(_db));
	return TRUE;
}

static gboolean gda_msql_provider_close_connection(GdaServerProvider *p1,
                                                   GdaConnection     *cnc)
{
	gint *sock_ptr;
	GdaMsqlProvider *p=(GdaMsqlProvider*)p1;
  
	if (!GDA_IS_MSQL_PROVIDER(p)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	sock_ptr=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if (!sock_ptr) return FALSE;
	msqlClose((int)*sock_ptr);
	if (sock_ptr) {
		free(sock_ptr);
		sock_ptr=NULL;
	}
	g_object_set_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE,NULL);
	return TRUE;
}

/*
 * usually this function works without a server handle, but for 
 * some reasons i think we should use a check anyways.
 */
static const gchar *
gda_msql_provider_get_server_version(GdaServerProvider *p1,
				     GdaConnection     *cnc)
{
	gint *sock_ptr;
	GdaMsqlProvider *p=(GdaMsqlProvider*)p1;
  
	if (!GDA_IS_MSQL_PROVIDER(p)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	sock_ptr=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if (!sock_ptr) return NULL;
	return (const gchar*)msqlGetServerInfo();
}

static GList *process_sql_commands(GList *rl,GdaConnection *cnc,
                                   const gchar *sql)
{
	gchar    **arr;
	gint      *sock;
	GdaConnectionOptions options;

	sock=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if ((!sock) || (*sock<0)) {
		gda_connection_add_event_string(cnc,_("Invalid mSQL handle"));
		return NULL;
	}
	options = gda_connection_get_options(cnc);
	arr = gda_delimiter_split_sql (sql);
	if (arr) {
		register gint n=0;

		while(arr[n]) {
			gint rc;
			m_result *res;
			GdaMsqlRecordset *rs;

			rc=msqlQuery(*sock,arr[n]);
			if (rc<0) {
				gda_connection_add_event(cnc,gda_msql_make_error(rc));
				break;
			}
			res=msqlStoreResult();
			rs=gda_msql_recordset_new(cnc,res,*sock,(int)rc);
			if (GDA_IS_MSQL_RECORDSET(rs)) {
				g_object_set (G_OBJECT (rs), 
					      "command_text", arr[n],
					      "command_type", GDA_COMMAND_TYPE_SQL, NULL);
				rl=g_list_append(rl,rs);
			}
			++n;
		}
		g_strfreev(arr);
	}
	return rl;
}

static const gchar *
gda_msql_provider_get_database(GdaServerProvider *p,GdaConnection *cnc)
{
	gint  *sock;
	gchar *dbname;
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return NULL;
	if (!GDA_IS_CONNECTION(cnc)) return NULL;
	sock=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if ((!sock) || (*sock<0)) {
		gda_connection_add_event_string(cnc,_("Invalid mSQL handle"));
		return NULL;
	}
	dbname=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_DBNAME);
	if (!dbname) return NULL;
	return (const gchar*)dbname;
}

static gboolean
gda_msql_provider_change_database(GdaServerProvider *p,GdaConnection *cnc,
                                  const gchar *name)
{
	gint  rc;
	gint *sock;
	gchar *dbname;
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	sock=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if ((!sock) || (*sock<0)) {
		gda_connection_add_event_string(cnc,_("Invalid mSQL handle"));
		return FALSE;
	}
	dbname=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_DBNAME);
	if (!dbname) return FALSE;
	if (!name) return FALSE;
	rc=msqlSelectDB(*sock,(char*)name);
	if (rc<0) {
		gda_connection_add_event(cnc,gda_msql_make_error(rc));
		return FALSE;
	}
	g_free(dbname);  
	g_object_set_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_DBNAME,g_strdup(name));
	return TRUE;
}

static GList *
gda_msql_provider_execute_command(GdaServerProvider *p,
                                  GdaConnection *cnc,
                                  GdaCommand *cmd,
                                  GdaParameterList *params)
{
	GList *rl = NULL;
	gchar *str;
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return NULL;
	if (!GDA_IS_CONNECTION(cnc)) return NULL;
	if (!cmd) return NULL;  
	switch (gda_command_get_command_type(cmd)) {
	case GDA_COMMAND_TYPE_SQL:
		rl=process_sql_commands(rl,cnc,gda_command_get_text(cmd));
		break;
	case GDA_COMMAND_TYPE_TABLE:
		str=g_strdup_printf("SELECT * FROM %s",gda_command_get_text(cmd));
		rl=process_sql_commands(rl,cnc,str);
		if (rl && GDA_IS_DATA_MODEL(rl->data))
			g_object_set (G_OBJECT (rl->data), 
				      "command_text", arr[n],
				      "command_type", GDA_COMMAND_TYPE_TABLE, NULL);
		g_free(str);
		break;
	default:;
	}
	return rl;
}

static gboolean 
gda_msql_provider_begin_transaction(GdaServerProvider *p,
                                    GdaConnection *cnc,
                                    GdaTransaction *xaction)
{
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	gda_connection_add_event_string(cnc,
					_("mSQL doesn't support transactions."));
	return FALSE;
}

static gboolean 
gda_msql_provider_commit_transaction(GdaServerProvider *p,
                                     GdaConnection *cnc,
                                     GdaTransaction *xaction)
{
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	gda_connection_add_event_string(cnc,
					_("mSQL doesn't support transactions."));
	return FALSE;
}

static gboolean 
gda_msql_provider_rollback_transaction(GdaServerProvider *p,
                                       GdaConnection *cnc,
                                       GdaTransaction *xaction)
{
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return FALSE;
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	gda_connection_add_event_string(cnc,
					_("mSQL doesn't support transactions."));
	return FALSE;
}

static gboolean 
gda_msql_provider_supports(GdaServerProvider *p,
                           GdaConnection *cnc,
                           GdaConnectionFeature feature)
{
	GdaMsqlProvider *mp=(GdaMsqlProvider*)p;

	if (!GDA_IS_MSQL_PROVIDER(mp)) return FALSE;
	switch (feature) {
	case GDA_CONNECTION_FEATURE_AGGREGATES: return FALSE;
	case GDA_CONNECTION_FEATURE_SQL: return TRUE;
	case GDA_CONNECTION_FEATURE_TRANSACTIONS: return FALSE;
	default:;
	}
	return FALSE;
}

static GdaDataModel *
get_msql_databases(GdaConnection *cnc,GdaParameterList *params)
{
	gint  *sock_ptr;
	GdaMsqlRecordset *rs;
	m_result *res;
  
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	sock_ptr=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if (!sock_ptr) return NULL;
	res=msqlListDBs(*sock_ptr);
	rs=gda_msql_recordset_new(cnc,res,*sock_ptr,(int)msqlNumRows(res));
	return (rs) ? GDA_DATA_MODEL(rs) : NULL;
}

static GdaDataModel *
get_msql_tables(GdaConnection *cnc,GdaParameterList *params)
{
	gint  *sock;
	m_row  row;
	GdaDataModelArray *model;
	m_result *res;
  
	if (!GDA_IS_CONNECTION(cnc)) return FALSE;
	sock=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	if (!sock) return NULL;
	res=msqlListTables(*sock);

	model = gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TABLES));
	gda_server_provider_init_schema_model (model, GDA_CONNECTION_SCHEMA_TABLES);

	while ((row=msqlFetchRow(res))) {
		GList *vlist = NULL;
		GValue *tmpval;

		tmpval = g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), row[0]);
		vlist = g_list_append (vlist, tmpval);
		vlist = g_list_append (vlist, gda_value_new_null ());
		vlist = g_list_append (vlist, gda_value_new_null ());
		vlist = g_list_append (vlist, gda_value_new_null ());
		gda_data_model_append_values (GDA_DATA_MODEL (model), vlist);
	}
	msqlFreeResult(res);
	return GDA_DATA_MODEL(model);    
}

static GdaDataModel *
get_msql_types(GdaConnection *cnc,GdaParameterList *params)
{
	GdaDataModelArray *rs;
	gint               i;
	struct {
		const gchar *name;
		const gchar *owner;
		const gchar *comments;
		GType type;
	} types[] = {
		{"char","","String of characters (or other 8bit data",G_TYPE_STRING},
		{"text","","variable length string of characters",G_TYPE_STRING},
		{"int","","Signed integer",G_TYPE_INT},
		{"real","","Decimal or Scientific Notation Values",G_TYPE_DOUBLE},
		{"uint","","Unsigned integer values",G_TYPE_INT},
		{"date","","Date values",G_TYPE_DATE},
		{"time","","Time values",GDA_TYPE_TIME},
		{"money","","Numerical values with 2 fixed decimal places",G_TYPE_DOUBLE}
	};
  
	if (!GDA_IS_CONNECTION(cnc)) return NULL;

	rs = (GdaDataModelArray*) gda_data_model_array_new (gda_server_provider_get_schema_nb_columns (GDA_CONNECTION_SCHEMA_TYPES));
	gda_server_provider_init_schema_model (rs, GDA_CONNECTION_SCHEMA_TYPES);

	for (i=0;i<sizeof(types)/sizeof(types[0]);i++) {
		GList *value_list=NULL;
		GValue *tmpval;

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].name);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].owner);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), types[i].comments);
		value_list = g_list_append (value_list, tmpval);

		g_value_set_ulong (tmpval = gda_value_new (G_TYPE_ULONG), types[i].type);
		value_list = g_list_append (value_list, tmpval);

		gda_data_model_append_values(GDA_DATA_MODEL(rs), value_list);
		g_list_foreach (value_list, (GFunc)gda_value_free, NULL);
		g_list_free (value_list);
	}
	return GDA_DATA_MODEL(rs);
}

static GList *
field_row_to_value_list(m_fdata *res)
{
	GList  *value_list = NULL;
	GValue *tmpval;

	g_return_val_if_fail(res!=NULL,NULL);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), res->field.name);
	value_list = g_list_append(value_list, tmpval);

	g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), msqlTypeNames[res->field.type]);
	value_list = g_list_append(value_list, tmpval);

	g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), res->field.length);
	value_list = g_list_append(value_list, tmpval);

	g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), 0);	
	value_list = g_list_append(value_list, tmpval);

	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), IS_NOT_NULL(res->field.flags));
	value_list = g_list_append(value_list, tmpval);

	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
	value_list = g_list_append(value_list, tmpval);

	g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), IS_UNIQUE(res->field.flags));
	value_list = g_list_append(value_list, tmpval);

	value_list = g_list_append(value_list, gda_value_new_null ());
	value_list = g_list_append(value_list, gda_value_new_null ());
	return value_list;
}

static GdaDataModel *
get_table_fields(GdaConnection *cnc,GdaParameterList *params)
{
	const gchar       *table_name;
	gint              *sock,r;
	GdaParameter      *par;
	GdaDataModelArray *rs;
	m_fdata           *fields;
	m_result          *res;
	struct {
		const gchar  *name;
		GType  type;
	} fields_desc[] = {
		{ N_("Field Name"),    G_TYPE_STRING  },
		{ N_("Data type"),     G_TYPE_STRING  },
		{ N_("Size"),          G_TYPE_INT },
		{ N_("Scale"),         G_TYPE_INT },
		{ N_("Not null"),      G_TYPE_BOOLEAN },
		{ N_("Primary key?"),  G_TYPE_BOOLEAN },
		{ N_("Unique index?"), G_TYPE_BOOLEAN },
		{ N_("References"),    G_TYPE_STRING  },
		{ N_("Default value"), G_TYPE_STRING  }
	};
  
	sock=g_object_get_data(G_OBJECT(cnc),OBJECT_DATA_MSQL_HANDLE);
	g_return_val_if_fail(GDA_IS_CONNECTION(cnc),NULL);
	g_return_val_if_fail(params!=NULL,NULL);
	if ((!sock) || (*sock<0)) {
		gda_connection_add_event_string(cnc,_("Invalid mSQL handle"));
		return NULL;
	}
	par=gda_parameter_list_find(params,"name");
	if (!par) {
		gda_connection_add_event_string(cnc,
						_("Table name is needed but none specified in parameter list"));
		return NULL;
	}
	table_name=g_value_get_string((GValue*)gda_parameter_get_value(par));
	if (!table_name) {
		gda_connection_add_event_string(cnc,
						_("Table name is needed but none specified in parameter list"));
		return NULL;
	}
	res=msqlListFields(*sock,(char*)table_name);
	if (!res) {
		gda_connection_add_event(cnc,gda_msql_make_error(*sock));
		return NULL;
	}
	rs=(GdaDataModelArray*)gda_data_model_array_new(9);
	for(r=0;r<sizeof(fields_desc)/sizeof(fields_desc[0]);r++) {
		gda_data_model_set_column_title(GDA_DATA_MODEL(rs),r,
						_(fields_desc[r].name));
		/*gda_server_recordset_model_set_field_scale(rs,r,0);
		  gda_server_recordset_model_set_field_gdatype(rs,r,_fields_desc[r].type);
		*/
	}
	for(r=0,fields=res->fieldData;fields;fields=fields->next) {
		GList *value_list;
    
		value_list = field_row_to_value_list(fields);
		if (!value_list) {
			g_object_unref(G_OBJECT(rs));
			return NULL;
		}
		gda_data_model_append_values(GDA_DATA_MODEL(rs),(const GList*)value_list);
		g_list_foreach(value_list,(GFunc)gda_value_free,NULL);
		g_list_free(value_list);
	}
	msqlFreeResult(res);
	return GDA_DATA_MODEL(rs);
} 

static GdaDataModel *
gda_msql_provider_get_schema(GdaServerProvider *p,
			     GdaConnection *cnc,
			     GdaConnectionSchema schema,
			     GdaParameterList *params)
{
  
	g_return_val_if_fail(GDA_IS_SERVER_PROVIDER(p),NULL);
	g_return_val_if_fail(GDA_IS_CONNECTION(cnc),NULL);
	switch(schema) {
	case GDA_CONNECTION_SCHEMA_DATABASES:
		return get_msql_databases(cnc,params);
	case GDA_CONNECTION_SCHEMA_FIELDS:
		return get_table_fields(cnc,params);
	case GDA_CONNECTION_SCHEMA_TABLES:
		return get_msql_tables(cnc,params);
	case GDA_CONNECTION_SCHEMA_TYPES:
		return get_msql_types(cnc,params);
	default:;
	}
	return NULL;
}
