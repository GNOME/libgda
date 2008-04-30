/* GDA Mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"
#include "gda-mysql-provider.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif


#define _GDA_PSTMT(x) ((GdaPStmt*)(x))


enum
{
	PROP_0,
	PROP_CHUNK_SIZE,
	PROP_CHUNKS_READ
};


static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass  *klass);
static void
gda_mysql_recordset_init (GdaMysqlRecordset       *recset,
			  GdaMysqlRecordsetClass  *klass);
static void
gda_mysql_recordset_dispose (GObject  *object);

/* virtual methods */
static gint
gda_mysql_recordset_fetch_nb_rows (GdaPModel  *model);
static gboolean
gda_mysql_recordset_fetch_random (GdaPModel  *model,
				  GdaPRow   **prow,
				  gint        rownum,
				  GError    **error);
static gboolean
gda_mysql_recordset_fetch_next (GdaPModel  *model,
				GdaPRow   **prow,
				gint        rownum,
				GError    **error);
static gboolean
gda_mysql_recordset_fetch_prev (GdaPModel  *model,
				GdaPRow   **prow,
				gint        rownum,
				GError    **error);
static gboolean
gda_mysql_recordset_fetch_at (GdaPModel  *model,
			      GdaPRow   **prow,
			      gint        rownum,
			      GError    **error);


struct _GdaMysqlRecordsetPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information */
	
	MYSQL_STMT  *mysql_stmt;
	MYSQL_BIND  *mysql_bind;    /* Array to bind columns in the result set. */

	gint  chunk_size;    /* Number of rows to fetch at a time when iterating forward/backward. */
	gint  chunks_read;   /* Number of times that we've iterated forward/backward. */
	
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_mysql_recordset_init (GdaMysqlRecordset       *recset,
			  GdaMysqlRecordsetClass  *klass)
{
	g_print ("*** %s\n", __func__);
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));
	recset->priv = g_new0 (GdaMysqlRecordsetPrivate, 1);
	recset->priv->cnc = NULL;

	/* initialize specific information */
	// TO_IMPLEMENT;
	
	recset->priv->chunk_size = 10;
	recset->priv->chunks_read = 0;
	
}


static void
gda_mysql_recordset_set_property (GObject       *object,
				  guint          param_id,
				  const GValue  *value,
				  GParamSpec    *pspec)
{
	GdaMysqlRecordset *record_set;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET(object));
	g_return_if_fail (GDA_MYSQL_RECORDSET(object)->priv != NULL);

	record_set = GDA_MYSQL_RECORDSET(object);

	switch (param_id) {
	case PROP_CHUNK_SIZE:
		record_set->priv->chunk_size = g_value_get_int (value);
		break;
	case PROP_CHUNKS_READ:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
gda_mysql_recordset_get_property (GObject     *object,
				  guint        param_id,
				  GValue      *value,
				  GParamSpec  *pspec)
{
	GdaMysqlRecordset *record_set;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET(object));
	g_return_if_fail (GDA_MYSQL_RECORDSET(object)->priv != NULL);

	record_set = GDA_MYSQL_RECORDSET(object);

	switch (param_id) {
	case PROP_CHUNK_SIZE:
		g_value_set_int (value, record_set->priv->chunk_size);
		break;
	case PROP_CHUNKS_READ:
		g_value_set_int (value, record_set->priv->chunks_read);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}


static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass  *klass)
{
	g_print ("*** %s\n", __func__);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaPModelClass *pmodel_class = GDA_PMODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_mysql_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_mysql_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_mysql_recordset_fetch_random;

	pmodel_class->fetch_next = gda_mysql_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_mysql_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_mysql_recordset_fetch_at;
	
	/* Properties. */
	object_class->set_property = gda_mysql_recordset_set_property;
	object_class->get_property = gda_mysql_recordset_get_property;

	g_object_class_install_property
		(object_class,
		 PROP_CHUNK_SIZE,
		 g_param_spec_int ("chunk-size", _("Number of rows fetched at a time"),
				   NULL,
				   1, G_MAXINT - 1, 10,
				   (G_PARAM_CONSTRUCT | G_PARAM_WRITABLE | G_PARAM_READABLE)));

	g_object_class_install_property
		(object_class,
		 PROP_CHUNKS_READ,
		 g_param_spec_int ("chunks-read", _("Number of row chunks read since the object creation"),
				   NULL,
				   0, G_MAXINT - 1, 0,
				   (G_PARAM_CONSTRUCT | G_PARAM_WRITABLE | G_PARAM_READABLE)));
	
}

static void
gda_mysql_recordset_dispose (GObject  *object)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) object;

	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);

		/* free specific information */
		// TO_IMPLEMENT;
		
                gint i;
                for (i = 0; i < (((GdaPModel *)recset)->prep_stmt)->ncols; ++i) {
                        g_free (recset->priv->mysql_bind[i].buffer);
                        g_free (recset->priv->mysql_bind[i].is_null);
                        g_free (recset->priv->mysql_bind[i].length);
                }
                g_free (recset->priv->mysql_bind);
                recset->priv->mysql_bind = NULL;

		
		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_mysql_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaMysqlRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlRecordset),
			0,
			(GInstanceInitFunc) gda_mysql_recordset_init
		};
		type = g_type_register_static (GDA_TYPE_PMODEL, "GdaMysqlRecordset", &info, 0);
	}

	return type;
}

/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_mysql_recordset_new (GdaConnection            *cnc,
			 GdaMysqlPStmt            *ps,
			 GdaDataModelAccessFlags   flags,
			 GType                    *col_types)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *model;
        MysqlConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;
	
	MYSQL_BIND *mysql_bind = NULL;
	

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0) {
                /*_GDA_PSTMT (ps)->ncols = ...;*/
		// TO_IMPLEMENT;
		
		_GDA_PSTMT(ps)->ncols = mysql_stmt_field_count (cdata->mysql_stmt);
		
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->tmpl_columns = g_slist_prepend (_GDA_PSTMT (ps)->tmpl_columns, 
									 gda_column_new ());
		_GDA_PSTMT (ps)->tmpl_columns = g_slist_reverse (_GDA_PSTMT (ps)->tmpl_columns);

		/* create prepared statement's types */
		_GDA_PSTMT (ps)->types = g_new0 (GType, _GDA_PSTMT (ps)->ncols); /* all types are initialized to GDA_TYPE_NULL */
		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= _GDA_PSTMT (ps)->ncols)
						g_warning (_("Column %d is out of range (0-%d), ignoring its specified type"), i,
							   _GDA_PSTMT (ps)->ncols - 1);
					else
						_GDA_PSTMT (ps)->types [i] = col_types [i];
				}
			}
		}

		
		MYSQL_RES *mysql_res = mysql_stmt_result_metadata (cdata->mysql_stmt);
		MYSQL_FIELD *mysql_fields = mysql_fetch_fields (mysql_res);
		
		mysql_bind = g_new0 (MYSQL_BIND, GDA_PSTMT (ps)->ncols);
		//		
		/* fill GdaColumn's data */
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     i < GDA_PSTMT (ps)->ncols; 
		     i++, list = list->next) {
			GdaColumn *column;
			
			column = GDA_COLUMN (list->data);

			/* use C API to set columns' information using gda_column_set_*() */
			// TO_IMPLEMENT;
			
			MYSQL_FIELD *field = &mysql_fields[i];
			/* g_print ("*** %s , %d\n", */
			/* 	 field->name, field->type); */
			GType gtype = _GDA_PSTMT(ps)->types[i];
			if (gtype == 0) {
				gtype = _gda_mysql_type_to_gda (cdata, field->type);
				_GDA_PSTMT(ps)->types[i] = gtype;
			}
			gda_column_set_g_type (column, gtype);
			gda_column_set_name (column, field->name);
			gda_column_set_title (column, field->name);
			gda_column_set_scale (column, (gtype == G_TYPE_DOUBLE) ? DBL_DIG :
					      (gtype == G_TYPE_FLOAT) ? FLT_DIG : 0);
			gda_column_set_defined_size (column, field->length);
			gda_column_set_references (column, "");

			/* Use @cnc's associate GdaMetaStore to get the following information:
			gda_column_set_references (column, ...);
			gda_column_set_table (column, ...);
			gda_column_set_primary_key (column, ...);
			gda_column_set_unique_key (column, ...);
			gda_column_set_allow_null (column, ...);
			gda_column_set_auto_increment (column, ...);
			*/

			
			mysql_bind[i].buffer_type = field->type;
			switch (mysql_bind[i].buffer_type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_INT24:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_YEAR:
				mysql_bind[i].buffer = g_malloc0 (sizeof(int));
				mysql_bind[i].is_null = g_malloc0 (sizeof(my_bool));
				break;
			case MYSQL_TYPE_LONGLONG:
				mysql_bind[i].buffer = g_malloc0 (sizeof(long long));
				mysql_bind[i].is_null = g_malloc0 (sizeof(my_bool));
				break;
			case MYSQL_TYPE_NULL:
				break;
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_TIMESTAMP:
				mysql_bind[i].buffer = g_malloc0 (sizeof(MYSQL_TIME));
				mysql_bind[i].is_null = g_malloc0 (sizeof(my_bool));
				break;
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
				mysql_bind[i].buffer = g_malloc0 (sizeof(double));
				mysql_bind[i].is_null = g_malloc0 (sizeof(my_bool));
				break;
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_VAR_STRING:
			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_TINY_BLOB:
			case MYSQL_TYPE_MEDIUM_BLOB:
			case MYSQL_TYPE_LONG_BLOB:
			case MYSQL_TYPE_NEWDECIMAL:
			case MYSQL_TYPE_BIT:
				mysql_bind[i].buffer = g_malloc0 (field->max_length + 1);
				mysql_bind[i].buffer_length = field->max_length + 1;
				mysql_bind[i].length = g_malloc0 (sizeof(unsigned long));
				break;
			default:
				g_warning (_("Invalid column bind data type.\n"),
					   mysql_bind[i].buffer_type);
			}
			
		}
		
                if (mysql_stmt_bind_result (cdata->mysql_stmt, mysql_bind)) {
                        g_warning ("mysql_stmt_bind_result failed: %s\n", mysql_stmt_error (cdata->mysql_stmt));
                }
		
		mysql_free_result (mysql_res);
		
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_MYSQL_RECORDSET,
			      "prepared-stmt", ps,
			      "model-usage", rflags,
			      NULL);
        model->priv->cnc = cnc;
	g_object_ref (G_OBJECT(cnc));

	/* post init specific code */
	// TO_IMPLEMENT;
	
	if (mysql_bind != NULL)
		model->priv->mysql_bind = mysql_bind;
	
	model->priv->mysql_stmt = cdata->mysql_stmt;

	((GdaPModel *) model)->advertized_nrows = mysql_stmt_affected_rows (cdata->mysql_stmt);
	

        return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_mysql_recordset_fetch_nb_rows (GdaPModel *model)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel;

	imodel = GDA_MYSQL_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	/* use C API to determine number of rows,if possible */
	// TO_IMPLEMENT;
	
	model->advertized_nrows = mysql_stmt_affected_rows (imodel->priv->mysql_stmt);
	

	return model->advertized_nrows;
}


static GdaPRow *
new_row_from_mysql_stmt (GdaMysqlRecordset  *imodel,
			 gint                rownum)
{
	g_print ("*** %s -- %d -- %d\n", __func__,
		 ((GdaPModel *) imodel)->prep_stmt->ncols, rownum);
	
	MYSQL_BIND *mysql_bind = imodel->priv->mysql_bind;
	g_assert (mysql_bind);
	
	GdaPRow *prow = gda_prow_new (((GdaPModel *) imodel)->prep_stmt->ncols);
	gint col;
	for (col = 0; col < ((GdaPModel *) imodel)->prep_stmt->ncols; ++col) {
		
		GValue *value = gda_prow_get_value (prow, col);
		GType type = ((GdaPModel *) imodel)->prep_stmt->types[col];
		gda_value_reset_with_type (value, type);
		
		gint i = col;
		
		int intvalue = 0;
		long long longlongvalue = 0;
		double doublevalue = 0.0;
		MYSQL_TIME timevalue = { 0 };
		char *strvalue = NULL;
		my_bool is_null;
		unsigned long length;
		
		switch (mysql_bind[i].buffer_type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR:
			g_memmove (&intvalue, mysql_bind[i].buffer, sizeof(int));
			g_memmove (&is_null, mysql_bind[i].is_null, sizeof(my_bool));
			
			if (type == G_TYPE_INT)
				g_value_set_int (value, intvalue);
			else if (type == G_TYPE_LONG)
				g_value_set_long (value, (long) intvalue);
			else if (type == G_TYPE_BOOLEAN)
				g_value_set_boolean (value, (gboolean) intvalue ? TRUE : FALSE);
			else {
				g_warning (_("Type %s not mapped for value %d"),
					   g_type_name (type), intvalue);
			}
			break;
		case MYSQL_TYPE_LONGLONG:
			g_memmove (&longlongvalue, mysql_bind[i].buffer, sizeof(long long));
			g_memmove (&is_null, mysql_bind[i].is_null, sizeof(my_bool));
			break;
		case MYSQL_TYPE_NULL:
			g_value_unset (value);
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			g_memmove (&timevalue, mysql_bind[i].buffer, sizeof(MYSQL_TIME));
			g_memmove (&is_null, mysql_bind[i].is_null, sizeof(my_bool));

			if (type == GDA_TYPE_TIME) {
				GdaTime time = {
					.hour = timevalue.hour,
					.minute = timevalue.minute,
					.second = timevalue.second,
					.fraction = timevalue.second_part,
				};
				gda_value_set_time (value, &time);
			} else if (type == G_TYPE_DATE) {
				GDate *date = g_date_new_dmy
					(timevalue.day, timevalue.month, timevalue.year);
				g_value_take_boxed (value, date);
				g_date_free (date);
			} else if (type == GDA_TYPE_TIMESTAMP) {
				GdaTimestamp timestamp = {
					.year = timevalue.year,
					.month = timevalue.month,
					.day = timevalue.day,
					.hour = timevalue.hour,
					.minute = timevalue.minute,
					.second = timevalue.second,
					.fraction = timevalue.second_part,
				};
				gda_value_set_timestamp (value, &timestamp);
			} else {
				g_warning (_("Type %s not mapped for value %p"),
					   g_type_name (type), timevalue);
			}

			break;
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
			g_memmove (&doublevalue, mysql_bind[i].buffer, sizeof(double));
			g_memmove (&is_null, mysql_bind[i].is_null, sizeof(my_bool));
			
			setlocale (LC_NUMERIC, "C");
			if (type == G_TYPE_FLOAT)
				g_value_set_float (value, (float) doublevalue);
			else if (type == G_TYPE_DOUBLE)
				g_value_set_double (value, doublevalue);
			else {
				g_warning (_("Type %s not mapped for value %f"),
					   g_type_name (type), intvalue);
			}
			setlocale (LC_NUMERIC, "");
			
			break;
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_BIT:
			g_memmove (&length, mysql_bind[i].length, sizeof(unsigned long));
			strvalue = g_memdup (mysql_bind[i].buffer, length + 1);
			
			if (type == G_TYPE_STRING)
				g_value_set_string (value, strvalue);
			else if (type == GDA_TYPE_BINARY) {
				GdaBinary binary = {
					.data = strvalue,
					.binary_length = length
				};
				gda_value_set_binary (value, &binary);
			} else if (type == GDA_TYPE_BLOB) {
				GdaBlob *blob = (GdaBlob *) gda_value_new_blob
					((const guchar *) strvalue, (gulong) length);
				gda_value_take_blob (value, blob);
				gda_value_free (blob);
			} else if (type == G_TYPE_DOUBLE) {
				setlocale (LC_NUMERIC, "C");
				g_value_set_double (value, atof (strvalue));
				setlocale (LC_NUMERIC, "");
			} else {
				g_warning (_("Type %s not mapped for value %p"),
					   g_type_name (type), strvalue);
			}
			
			break;
		default:
			g_warning (_("Invalid column bind data type. %d\n"),
				   mysql_bind[i].buffer_type);
		}
		
	}
	return prow;
}


/*
 * Create a new filled #GdaPRow object for the row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaPRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaPRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaPRow object is left to the implementation, which
 * can use gda_pmodel_take_row(). If new row objects are "given" to the GdaPModel implemantation
 * using that method, then this method should detect when all the data model rows have been analysed
 * (when model->nb_stored_rows == model->advertized_nrows) and then possibly discard the API handle
 * as it won't be used anymore to fetch rows.
 */
static gboolean 
gda_mysql_recordset_fetch_random (GdaPModel  *model,
				  GdaPRow   **prow,
				  gint        rownum,
				  GError    **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel;

	imodel = GDA_MYSQL_RECORDSET (model);

	// TO_IMPLEMENT;
	
	if (*prow)
		return TRUE;

	if (imodel->priv->mysql_stmt == NULL) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Internal error"));
		return FALSE;
	}
	
	if (mysql_stmt_fetch (imodel->priv->mysql_stmt))
		return FALSE;
	
	*prow = new_row_from_mysql_stmt (imodel, rownum);
	gda_pmodel_take_row (model, *prow, rownum);
	
	/* if (model->nb_stored_rows == model->advertized_nrows) { */
	/* 	g_print ("*** All the row have been converted..."); */
	/* } */
	

	return TRUE;
}

/*
 * Create and "give" filled #GdaPRow object for all the rows in the model
 */
static gboolean
gda_mysql_recordset_store_all (GdaPModel *model, GError **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel;
	gint i;

	imodel = GDA_MYSQL_RECORDSET (model);

	/* default implementation */
	for (i = 0; i < model->advertized_nrows; i++) {
		GdaPRow *prow;
		if (! gda_mysql_recordset_fetch_random (model, &prow, i, error))
			return FALSE;
	}
	return TRUE;
}

/*
 * Create a new filled #GdaPRow object for the next cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaPRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaPRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaPRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_mysql_recordset_fetch_next (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaPRow object for the previous cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaPRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaPRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaPRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_mysql_recordset_fetch_prev (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaPRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaPRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaPRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaPRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_mysql_recordset_fetch_at (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	g_print ("*** %s\n", __func__);
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;
	
	TO_IMPLEMENT;

	return TRUE;
}

