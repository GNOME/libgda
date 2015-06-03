/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include "gda-jdbc.h"
#include "gda-jdbc-recordset.h"
#include "gda-jdbc-provider.h"
#include "jni-wrapper.h"
#include "jni-globals.h"
#include "gda-jdbc-util.h"
#include "gda-jdbc-blob-op.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_jdbc_recordset_class_init (GdaJdbcRecordsetClass *klass);
static void gda_jdbc_recordset_init       (GdaJdbcRecordset *recset,
					     GdaJdbcRecordsetClass *klass);
static void gda_jdbc_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_jdbc_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_jdbc_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_jdbc_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


struct _GdaJdbcRecordsetPrivate {
	GdaConnection *cnc;
	GValue        *rs_value; /* JAVA GdaJResultSet object */

	gint           next_row_num;
        GdaRow        *tmp_row; /* used in cursor mode */
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_jdbc_recordset_init (GdaJdbcRecordset *recset,
			 G_GNUC_UNUSED GdaJdbcRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_JDBC_RECORDSET (recset));
	recset->priv = g_new0 (GdaJdbcRecordsetPrivate, 1);
	recset->priv->cnc = NULL;
	recset->priv->rs_value = NULL;
}

static void
gda_jdbc_recordset_class_init (GdaJdbcRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_jdbc_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_jdbc_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_jdbc_recordset_fetch_random;
	pmodel_class->fetch_next = gda_jdbc_recordset_fetch_next;
	pmodel_class->fetch_prev = NULL;
        pmodel_class->fetch_at = NULL;
}

static void
gda_jdbc_recordset_dispose (GObject *object)
{
	GdaJdbcRecordset *recset = (GdaJdbcRecordset *) object;

	g_return_if_fail (GDA_IS_JDBC_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);

		if (recset->priv->rs_value)
			gda_value_free (recset->priv->rs_value);

		if (recset->priv->tmp_row)
                        g_object_unref (recset->priv->tmp_row);

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_jdbc_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaJdbcRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_jdbc_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaJdbcRecordset),
			0,
			(GInstanceInitFunc) gda_jdbc_recordset_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaJdbcRecordset", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

/* Same as GdaJValue::jdbc_type_to_g_type
 * See http://docs.oracle.com/javase/6/docs/api/constant-values.html#java.sql.Types.ARRAY for reference
 */
static GType
jdbc_type_to_g_type (gint jdbc_type)
{
	switch (jdbc_type) {
	case 12: /* VARCHAR */
		return G_TYPE_STRING;
	case 2003: /* ARRAY */
		return GDA_TYPE_BINARY;
 	case -5: /* BIGINT */
		return G_TYPE_INT64;
 	case -2: /* BINARY */
		return GDA_TYPE_BINARY;
 	case -7: /* BIT */
		return G_TYPE_BOOLEAN;
 	case 2004: /* BLOB */
		return GDA_TYPE_BLOB;
 	case 16: /* BOOLEAN */
		return G_TYPE_BOOLEAN;
 	case 1: /* CHAR */
		return G_TYPE_STRING;
 	case 2005: /* CLOB */
 	case 70: /* DATALINK */
		return GDA_TYPE_BINARY;
 	case 91: /* DATE */
		return G_TYPE_DATE;
 	case 3: /* DECIMAL */
		return GDA_TYPE_NUMERIC;
 	case 2001: /* DISTINCT */
		return GDA_TYPE_BINARY;
 	case 8: /* DOUBLE */
		return G_TYPE_DOUBLE;
 	case 6: /* FLOAT */
		return G_TYPE_FLOAT;
 	case 4: /* INTEGER */
		return G_TYPE_INT;
 	case 2000: /* JAVA_OBJECT */
		return GDA_TYPE_BINARY;
	case -16: /* LONGNVARCHAR */
		return G_TYPE_STRING;
 	case -4: /* LONGVARBINARY */
		return GDA_TYPE_BINARY;
 	case -1: /* LONGVARCHAR */
		return G_TYPE_STRING;
	case -15: /* NCHAR */
		return G_TYPE_STRING;
	case 2011: /* NCLOB */
		return GDA_TYPE_BINARY;
 	case 0: /* NULL */
		return GDA_TYPE_NULL;
	case 2: /* NUMERIC */
		return GDA_TYPE_NUMERIC;
	case -9: /* NVARCHAR */
		return G_TYPE_STRING;
	case 1111: /* OTHER */
		return GDA_TYPE_BINARY;
 	case 7: /* REAL */
		return G_TYPE_FLOAT;
 	case 2006: /* REF */
		return GDA_TYPE_BINARY;
	case -8: /* ROWID */
		return G_TYPE_STRING;
 	case 5: /* SMALLINT */
		return GDA_TYPE_SHORT;
	case 2009: /* SQLXML */
		return G_TYPE_STRING;
 	case 2002: /* STRUCT */
		return GDA_TYPE_BINARY;
 	case 92: /* TIME */
		return GDA_TYPE_TIME;
 	case 93: /* TIMESTAMP */
		return GDA_TYPE_TIMESTAMP;
 	case -6: /* TINYINT */
		return G_TYPE_CHAR;
 	case -3: /* VARBINARY */
		return GDA_TYPE_BINARY;
	default:
		return GDA_TYPE_BINARY;
	}
}

/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_jdbc_recordset_new (GdaConnection *cnc, GdaJdbcPStmt *ps, GdaSet *exec_params,
			JNIEnv *jenv, GValue *rs_value, GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaJdbcRecordset *model;
        JdbcConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;
	GValue *jexec_res;

	GError *error = NULL;
	gint error_code;
	gchar *sql_state;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	if (ps)
		g_return_val_if_fail (GDA_IS_JDBC_PSTMT (ps), NULL);
	else
		ps = gda_jdbc_pstmt_new (NULL);

	cdata = (JdbcConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API*/
	jexec_res = jni_wrapper_method_call (jenv, GdaJResultSet__getInfos,
					     rs_value, &error_code, &sql_state, &error);
	if (!jexec_res) {
		_gda_jdbc_make_error (cnc, error_code, sql_state, error);
		gda_value_free (rs_value);
		return NULL;
	}

        if (_GDA_PSTMT (ps)->ncols < 0) {
		GValue *jfield_v;
		jfield_v = jni_wrapper_field_get (jenv, GdaJResultSetInfos__ncols, jexec_res, &error);
		if (! jfield_v) {
			gda_value_free (jexec_res);
			gda_value_free (rs_value);
			return NULL;
		}
                _GDA_PSTMT (ps)->ncols = g_value_get_int (jfield_v);
		gda_value_free (jfield_v);
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		gboolean allok = TRUE;

		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->tmpl_columns = g_slist_prepend (_GDA_PSTMT (ps)->tmpl_columns, 
									 gda_column_new ());
		_GDA_PSTMT (ps)->tmpl_columns = g_slist_reverse (_GDA_PSTMT (ps)->tmpl_columns);

		/* create prepared statement's types, all types are initialized to GDA_TYPE_NULL */
		_GDA_PSTMT (ps)->types = g_new (GType, _GDA_PSTMT (ps)->ncols);
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->types [i] = GDA_TYPE_NULL;

		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= _GDA_PSTMT (ps)->ncols) {
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   _GDA_PSTMT (ps)->ncols - 1);
						break;
					}
					else
						_GDA_PSTMT (ps)->types [i] = col_types [i];
				}
			}
		}
		
		/* fill GdaColumn's data */
		for (i = 0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     allok && (i < GDA_PSTMT (ps)->ncols);
		     i++, list = list->next) {
			GdaColumn *column;
			GValue *jcol_v;
			GValue *jcol_a;

			jcol_v = jni_wrapper_method_call (jenv, GdaJResultSetInfos__describeColumn, jexec_res, 
							  NULL, NULL, NULL, i);
			if (!jcol_v) {
				allok = FALSE;
				break;
			}
			column = GDA_COLUMN (list->data);
			jcol_a = jni_wrapper_field_get (jenv, GdaJColumnInfos__col_name, jcol_v, NULL);
			if (jcol_a) {
				if (G_VALUE_TYPE (jcol_a) != GDA_TYPE_NULL)
					gda_column_set_name (column, g_value_get_string (jcol_a));
				gda_value_free (jcol_a);
			}
			else
				allok = FALSE;
			jcol_a = jni_wrapper_field_get (jenv, GdaJColumnInfos__col_descr, jcol_v, NULL);
			if (jcol_a) {
				if (G_VALUE_TYPE (jcol_a) != GDA_TYPE_NULL)
					gda_column_set_description (column, g_value_get_string (jcol_a));
				gda_value_free (jcol_a);
			}
			else
				allok = FALSE;
			jcol_a = jni_wrapper_field_get (jenv, GdaJColumnInfos__col_type, jcol_v, NULL);
			if (jcol_a) {
				_GDA_PSTMT (ps)->types [i] = jdbc_type_to_g_type (g_value_get_int (jcol_a));
				gda_column_set_g_type (column, _GDA_PSTMT (ps)->types [i]);
				gda_value_free (jcol_a);
			}
			else
				allok = FALSE;
			gda_value_free (jcol_v);
		}
		if (!allok) {
			g_free (_GDA_PSTMT (ps)->types);
			_GDA_PSTMT (ps)->types = NULL;
			g_slist_foreach (_GDA_PSTMT (ps)->tmpl_columns, (GFunc) g_object_unref, NULL);
			g_slist_free (_GDA_PSTMT (ps)->tmpl_columns);
			_GDA_PSTMT (ps)->tmpl_columns = NULL;

			gda_value_free (jexec_res);
			gda_value_free (rs_value);
			return NULL;
		}
        }
	gda_value_free (jexec_res);

	/* declare the requested types (and connection pointer) to the used resultset */
	jbyte *ctypes;
	jbyteArray jtypes;

	ctypes = g_new (jbyte, GDA_PSTMT (ps)->ncols);
	for (i = 0; i < GDA_PSTMT (ps)->ncols; i++)
		ctypes [i] = _gda_jdbc_gtype_to_proto_type (_GDA_PSTMT (ps)->types [i]);

	jtypes = (*jenv)->NewByteArray (jenv, GDA_PSTMT (ps)->ncols);
	if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
		g_free (ctypes);
		_gda_jdbc_make_error (cnc, error_code, sql_state, error);
		gda_value_free (rs_value);
		return NULL;
	}

	(*jenv)->SetByteArrayRegion (jenv, jtypes, 0, GDA_PSTMT (ps)->ncols, ctypes);
	if (jni_wrapper_handle_exception (jenv, &error_code, &sql_state, &error)) {
		g_free (ctypes);
		_gda_jdbc_make_error (cnc, error_code, sql_state, error);
		(*jenv)->DeleteLocalRef (jenv, jtypes);
		gda_value_free (rs_value);
		return NULL;
	}

	jexec_res = jni_wrapper_method_call (jenv, GdaJResultSet__declareColumnTypes,
					     rs_value, &error_code, &sql_state, &error, jni_cpointer_to_jlong (cnc), jtypes);
	(*jenv)->DeleteLocalRef (jenv, jtypes);
	g_free (ctypes);
	
	if (!jexec_res) {
		_gda_jdbc_make_error (cnc, error_code, sql_state, error);
		gda_value_free (rs_value);
		return NULL;
	}

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_JDBC_RECORDSET, 
			      "prepared-stmt", ps, 
			      "model-usage", rflags, 
			      "exec-params", exec_params, NULL);
        model->priv->cnc = cnc;
	model->priv->rs_value = rs_value;
	g_object_ref (cnc);

        return GDA_DATA_MODEL (model);
}

static GdaRow *
fetch_next_jdbc_row (GdaJdbcRecordset *model, JNIEnv *jenv, gboolean do_store, GError **error)
{
	GValue *jexec_res;
	gint error_code;
	gchar *sql_state;
	GError *lerror = NULL;
	gboolean row_found;

	/* ask JDBC to fetch the next row and store the values */
	GdaRow *prow = NULL;
	GdaJdbcPStmt *ps;
	ps = GDA_JDBC_PSTMT (GDA_DATA_SELECT (model)->prep_stmt);
	prow = gda_row_new (_GDA_PSTMT (ps)->ncols);

	jexec_res = jni_wrapper_method_call (jenv, GdaJResultSet__fillNextRow,
					     model->priv->rs_value, &error_code, &sql_state, &lerror,
					     jni_cpointer_to_jlong (prow));
	if (!jexec_res) {
		if (error && lerror)
			*error = g_error_copy (lerror);
		_gda_jdbc_make_error (model->priv->cnc, error_code, sql_state, lerror);
		g_object_unref ((GObject*) prow);
		return NULL;
	}

	row_found = g_value_get_boolean (jexec_res);
	gda_value_free (jexec_res);
	if (! row_found) {
		GDA_DATA_SELECT (model)->advertized_nrows = model->priv->next_row_num;
		g_object_unref ((GObject*) prow);
		return NULL;
	}

	if (do_store) {
		/* insert row */
		gda_data_select_take_row (GDA_DATA_SELECT (model), prow, model->priv->next_row_num);
	}
	model->priv->next_row_num ++;

	return prow;
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_jdbc_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaJdbcRecordset *imodel;
	GdaRow *prow = NULL;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;

	imodel = GDA_JDBC_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	jenv = _gda_jdbc_get_jenv (&jni_detach, NULL);
	if (!jenv) 
		return model->advertized_nrows;

	for (prow = fetch_next_jdbc_row (imodel, jenv, TRUE, NULL);
             prow;
             prow = fetch_next_jdbc_row (imodel, jenv, TRUE, NULL));

	_gda_jdbc_release_jenv (jni_detach);
	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum.
 *
 * Each new #GdaRow created is "given" to the #GdaDataSelect implementation
 * using gda_data_select_take_row ().
 */
static gboolean 
gda_jdbc_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaJdbcRecordset *imodel;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;

	jenv = _gda_jdbc_get_jenv (&jni_detach, NULL);
	if (!jenv)
		return TRUE;

	imodel = GDA_JDBC_RECORDSET (model);
	if (imodel->priv->next_row_num >= rownum) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			     "%s", _("Requested row could not be found"));
		return TRUE;
	}
	for (*prow = fetch_next_jdbc_row (imodel, jenv, TRUE, error);
	     *prow && (imodel->priv->next_row_num < rownum);
	     *prow = fetch_next_jdbc_row (imodel, jenv, TRUE, error));

	_gda_jdbc_release_jenv (jni_detach);
        return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it). Before a new #GdaRow gets created, the previous one,
 * if set, is discarded.
 */
static gboolean 
gda_jdbc_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaJdbcRecordset *imodel = (GdaJdbcRecordset*) model;
	JNIEnv *jenv = NULL;
	gboolean jni_detach;

	jenv = _gda_jdbc_get_jenv (&jni_detach, NULL);
	if (!jenv)
		return FALSE;

	if (imodel->priv->tmp_row) {
                g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}
	if (imodel->priv->next_row_num != rownum) {
		GError *lerror = NULL;
		*prow = NULL;
		g_set_error (&lerror, GDA_DATA_MODEL_ERROR,
			     GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			     "%s", _("Can't set iterator on requested row"));
		gda_data_select_add_exception (GDA_DATA_SELECT (model), lerror);
		if (error)
			g_propagate_error (error, g_error_copy (lerror));
		_gda_jdbc_release_jenv (jni_detach);

		return TRUE;
	}
        *prow = fetch_next_jdbc_row (imodel, jenv, FALSE, error);
        imodel->priv->tmp_row = *prow;

	_gda_jdbc_release_jenv (jni_detach);
 
	return TRUE;
}

