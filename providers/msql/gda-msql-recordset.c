/* GDA mSQL Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-msql.h"
#include "gda-msql-recordset.h"

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

/*----[ Forwards ]----------------------------------------------------------*/

static GObjectClass *parent_class;

static void gda_msql_recordset_class_init(GdaMsqlRecordsetClass *cl);
static void gda_msql_recordset_finalize(GObject *obj);
static void gda_msql_recordset_init(GdaMsqlRecordset *rs,
                                    GdaMsqlRecordsetClass *cl);

/*--------------------------------------------------------------------------*/

static GdaRow *fetch_row(GdaMsqlRecordset *rs,gulong rownum) {
	GdaRow        *row;
	gint           field_count,row_count;
	register gint  i;
	m_fdata       *msql_fields;
	m_row          msql_row;

	g_return_val_if_fail(GDA_IS_MSQL_RECORDSET(rs),NULL);
	if (!rs->res) {
		gda_connection_add_error_string(rs->cnc,_("Invalid mSQL handle"));
		return NULL;
	}
	row_count=msqlNumRows(rs->res);
	if (!row_count) return NULL;
	field_count=msqlNumFields(rs->res);
	if ((rownum<0) || (rownum>=row_count)) {
		gda_connection_add_error_string(rs->cnc,_("Row number out of bounds"));
		return NULL;
	}
	msqlDataSeek(rs->res,rownum);
	row=gda_row_new(GDA_DATA_MODEL(rs),field_count);
	msql_row=msqlFetchRow(rs->res);
	if (!msql_row) return NULL;
	for(i=0,msql_fields=rs->res->fieldData;msql_fields;
	    i++,msql_fields=msql_fields->next) {
		GdaValue *field;
		gchar    *s_val;

		field=(GdaValue*)gda_row_get_value(row,i);
		s_val=msql_row[i];
		switch (msql_fields->field.type) {
		case INT_TYPE:  gda_value_set_integer(field, (s_val) ? atol(s_val) : 0);
			break;
		case UINT_TYPE: gda_value_set_uinteger(field,
						       (s_val) ? (guint)atol(s_val) : 0);
			break;
		case REAL_TYPE:  gda_value_set_double(field,(s_val) ? atof(s_val) : 0.0);
			break;
		case MONEY_TYPE :gda_value_set_single(field,(s_val) ? atof(s_val) : 0.0f);
			break;
#ifdef HAVE_MSQL3
		case IPV4_TYPE:  gda_value_set_string(field,(s_val) ? s_val : "");
			break;
		case INT64_TYPE: gda_value_set_bigint(field,
						      (s_val) ? (gint64)atof(s_val) : 0);
			break;
		case UINT64_TYPE: gda_value_set_biguint(field,
							(s_val) ? (guint64)atof(s_val) : 0);
                        break;
		case INT16_TYPE:  gda_value_set_smallint(field,
							 (s_val) ? (gshort)atol(s_val) : 0);
                        break;
		case UINT16_TYPE: gda_value_set_smalluint(field,
							  (s_val) ? (gushort)atol(s_val) : 0);
                        break;
		case INT8_TYPE:   gda_value_set_tinyint(field,
							(s_val) ? (gchar)atol(s_val) : 0);
                        break;
		case UINT8_TYPE:  gda_value_set_tinyuint(field,
							 (s_val) ? (guchar)atol(s_val) : 0);
                        break;
#endif
		default:
			gda_value_set_string(field,(s_val) ? s_val : "");
		}  
	}
	return row;
}

/*
 * GdaMsql class implementation 
 */

static gint gda_msql_recordset_numrows(GdaDataModel *model) {
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)model;
  
	if (!GDA_IS_MSQL_RECORDSET(rs)) return -1;
	if (rs->res==NULL) return rs->n_rows;
	return msqlNumRows(rs->res);
}

static gint gda_msql_recordset_numcols(GdaDataModel *model) {
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)model;
  
	if (!GDA_IS_MSQL_RECORDSET(rs)) return -1;
	if (rs->res==NULL) return 0;
	return msqlNumFields(rs->res);
}


static GdaDataModelColumnAttributes 
*gda_msql_recordset_describe_column(GdaDataModelBase *model,gint col) {
	gint                field_count;
	GdaDataModelColumnAttributes *attrs;
	m_field            *msql_field;
	GdaMsqlRecordset   *rs=(GdaMsqlRecordset*)model;
  
	if (!GDA_IS_MSQL_RECORDSET(rs)) return NULL;
	if (!rs->res) {
		gda_connection_add_error_string(rs->cnc,_("Invalid mSQL handle"));
		return NULL;
	}
	field_count=msqlNumFields(rs->res);
	if (col>=field_count) return NULL;
	attrs=gda_data_model_column_attributes_new();
	msqlFieldSeek(rs->res,col);
	msql_field=msqlFetchField(rs->res);
	if (!msql_field) {
		gda_data_model_column_attributes_free(attrs);
		return NULL;
	}
	if (msql_field->name) gda_data_model_column_attributes_set_name(attrs,msql_field->name);
	gda_data_model_column_attributes_set_defined_size(attrs,msql_field->length);
	gda_data_model_column_attributes_set_table(attrs,msql_field->table);
	if (msql_field->type==MONEY_TYPE) {
		gda_data_model_column_attributes_set_scale(attrs,2);
	}
	gda_data_model_column_attributes_set_gdatype(attrs,
					 gda_msql_type_to_gda(msql_field->type));
	gda_data_model_column_attributes_set_allow_null(attrs,!IS_NOT_NULL(msql_field->flags));
	gda_data_model_column_attributes_set_primary_key(attrs,1!=1);
	gda_data_model_column_attributes_set_unique_key(attrs,IS_UNIQUE(msql_field->flags));
	gda_data_model_column_attributes_set_auto_increment(attrs,1!=1);
	return attrs;
}

static const GdaRow *gda_msql_recordset_get_row(GdaDataModelBase *model,gint row) {
	gint              rows, fetched_rows;
	register gint     i;
	GdaRow           *fields = NULL;
	GdaMsqlRecordset *rs=(GdaMsqlRecordset *)model;
  
	if (!GDA_IS_MSQL_RECORDSET(rs)) return NULL;
	rows=msqlNumRows(rs->res);
	fetched_rows=rs->rows->len;
	if (row>=rows) return NULL;
	if (row<fetched_rows) {
		return (const GdaRow*)g_ptr_array_index(rs->rows,row);
	}
	gda_data_model_freeze(GDA_DATA_MODEL(rs));
	for(i=fetched_rows;i<=row;i++) {
		fields=fetch_row(rs,i);
		if (!fields) return NULL;
		g_ptr_array_add(rs->rows,fields);
	}
	gda_data_model_thaw(GDA_DATA_MODEL(rs));
	return (const GdaRow*)fields;
}

static const GdaValue 
*gda_msql_recordset_get_value_at(GdaDataModelBase *model,gint col,gint row) {
	gint              cols;
	const GdaRow     *fields;
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)model;

	if (!GDA_IS_MSQL_RECORDSET(rs)) return NULL;
	cols=msqlNumFields(rs->res);
	if (col>=cols) return NULL;
	fields=gda_msql_recordset_get_row(model,row);
	return (fields) ? gda_row_get_value((GdaRow*)fields,col) : NULL;
}

static gboolean gda_msql_recordset_is_updatable(GdaDataModelBase *model) {
	GdaCommandType    cmd_type;
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)model;
  
	if (!GDA_IS_MSQL_RECORDSET(rs)) return FALSE;
	cmd_type=gda_data_model_get_command_type(model);
	return (cmd_type==GDA_COMMAND_TYPE_TABLE) ? TRUE : FALSE;
}

static const GdaRow 
*gda_msql_recordset_append_values(GdaDataModelBase *model,const GList *values) {
	GString          *sql;
	GdaRow           *row;
	gint              rc,cols;
	register gint     i;
	GList            *lst;
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)model;
  
	if ((!GDA_IS_MSQL_RECORDSET(rs)) ||
	    (!values) || (!gda_data_model_is_updatable(model)) ||
	    (!gda_data_model_has_changed(model))) {
		return NULL;
	}
	cols=msqlNumFields(rs->res);
	if (cols!=g_list_length((GList*)values)) {
		gda_connection_add_error_string(rs->cnc,
						_("Attempt to insert a row with an invalid number of columns"));
		return NULL;
	}
	sql=g_string_new("INSERT INTO ");
	sql=g_string_append(sql,gda_data_model_get_command_text(model));
	sql=g_string_append(sql,"(");
	for(i=0;i<cols;i++) {
		GdaDataModelColumnAttributes *fa;
    
		fa=gda_data_model_describe_column(model,i);
		if (!fa) {
			gda_connection_add_error_string(rs->cnc,
							_("Could not retrieve column's information"));
			g_string_free(sql,TRUE);
			return NULL;
		}
		if (i) sql=g_string_append(sql,",");
		sql=g_string_append(sql,gda_data_model_column_attributes_get_name(fa));
	}
	sql=g_string_append(sql,") VALUES (");
	for (lst=(GList*)values,i=0;i<cols;i++,lst=lst->next) {
		char           *val_str;
		const GdaValue *val=(const GdaValue*)lst->data;
   
		if (!val) {
			gda_connection_add_error_string(rs->cnc,
							_("Could not retrieve column's value"));
			g_string_free(sql,TRUE);
			return NULL;
		}
		if (i) sql=g_string_append(sql,",");
		val_str=gda_msql_value_to_sql_string((GdaValue*)val);
		sql=g_string_append(sql,val_str);
		g_free(val_str);
	}
	sql=g_string_append(sql,")");
	rc=msqlQuery(rs->sock,sql->str);
	g_string_free(sql,TRUE);
	if (rc<0) {
		gda_connection_add_error(rs->cnc,gda_msql_make_error(rc));
		return NULL;
	}
	row=gda_row_new_from_list(model,values);
	g_ptr_array_add(rs->rows,row);
	return (const GdaRow*)row;
}

static gboolean 
gda_msql_recordset_remove_row(GdaDataModelBase *model,const GdaRow *row) {
	return FALSE;
}

static gboolean
gda_msql_recordset_update_row(GdaDataModelBase *model,const GdaRow *row) {
	return FALSE;
}

static void gda_msql_recordset_class_init(GdaMsqlRecordsetClass *cl) {
	GObjectClass *obj_class=G_OBJECT_CLASS(cl);
	GdaDataModelBaseClass *mdl_class=GDA_DATA_MODEL_BASE_CLASS(cl);

	parent_class=g_type_class_peek_parent(cl);
	obj_class->finalize=gda_msql_recordset_finalize;
	mdl_class->get_n_rows=gda_msql_recordset_numrows;
	mdl_class->get_n_columns=gda_msql_recordset_numcols;
	mdl_class->describe_column=gda_msql_recordset_describe_column;
	mdl_class->get_row=gda_msql_recordset_get_row;
	mdl_class->get_value_at=gda_msql_recordset_get_value_at;
	mdl_class->is_updatable=gda_msql_recordset_is_updatable;
	mdl_class->append_values=gda_msql_recordset_append_values;
	mdl_class->remove_row=gda_msql_recordset_remove_row;
	mdl_class->update_row=gda_msql_recordset_update_row;
}

static void 
gda_msql_recordset_init(GdaMsqlRecordset *rs,GdaMsqlRecordsetClass *cl) {
	if (GDA_IS_MSQL_RECORDSET(rs)) {
		rs->cnc=NULL;
		rs->res=NULL;
		rs->rows=g_ptr_array_new();
	}
}

static void gda_msql_recordset_finalize(GObject *object) {
	GdaMsqlRecordset *rs=(GdaMsqlRecordset*)object;
  
	if (GDA_IS_MSQL_RECORDSET(rs)) {
		msqlFreeResult(rs->res);
		rs->res=NULL;
		for(;rs->rows->len;) {
			GdaRow *row=(GdaRow*)g_ptr_array_index(rs->rows,0);
			if (row) gda_row_free(row);
			g_ptr_array_remove_index(rs->rows,0);
		}
		g_ptr_array_free(rs->rows,TRUE);
		rs->rows=NULL;
		parent_class->finalize(object);
	}
}

GType gda_msql_recordset_get_type(void) {
	static GType type=0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(GdaMsqlRecordsetClass),
			(GBaseInitFunc)NULL,
			(GBaseFinalizeFunc)NULL,
			(GClassInitFunc)gda_msql_recordset_class_init,
			NULL,NULL,sizeof(GdaMsqlRecordset),0,
			(GInstanceInitFunc)gda_msql_recordset_init
		};
		type=g_type_register_static(PARENT_TYPE,"GdaMsqlRecordset",&info,0);
	}
	return type;
}

GdaMsqlRecordset 
*gda_msql_recordset_new(GdaConnection *cnc,m_result *res,int sock,int n_rows) {
	GdaMsqlRecordset *rs;
	m_fdata          *fields;
	register gint     i;

	if (!GDA_IS_CONNECTION(cnc)) return NULL;
	rs=g_object_new(GDA_TYPE_MSQL_RECORDSET,NULL);
	rs->cnc=cnc;
	rs->res=res;
	rs->sock=sock;
	rs->n_rows=n_rows;
	if (res!=NULL) {
		for (fields=res->fieldData,i=0;fields;fields=fields->next,i++) {
			gda_data_model_set_column_title(GDA_DATA_MODEL(rs),i,fields->field.name);
		}
	}
	return rs;
}
