/*
 * Copyright (C) 2002 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2003 Steve Fosdick <fozzy@src.gnome.org>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc-desktop>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
#include "gda-oracle.h"
#include "gda-oracle-recordset.h"
#include "gda-oracle-provider.h"
#include "gda-oracle-util.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))
#define BUFFER_SIZE 4095 /* size of the buffer to fecth data from the server using the callback API (OCIDefineDynamic) */

static void gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_init       (GdaOracleRecordset *recset,
					     GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_oracle_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_oracle_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_oracle_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);

static GdaRow  *new_row (GdaDataSelect *imodel, GdaConnection *cnc, GdaOraclePStmt *ps);

struct _GdaOracleRecordsetPrivate {
	gint     next_row_num;
};
static GObjectClass *parent_class = NULL;

#ifdef GDA_DEBUG
#define TS_DIFF(a,b) (((b).tv_sec * 1000000 + (b).tv_usec) - ((a).tv_sec * 1000000 + (a).tv_usec))
#include <sys/time.h>
#endif

/*
 * Object init and finalize
 */
static void
gda_oracle_recordset_init (GdaOracleRecordset *recset,
			   GdaOracleRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));
	recset->priv = g_new0 (GdaOracleRecordsetPrivate, 1);
	recset->priv->next_row_num = 0;
}

static void
gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_oracle_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_oracle_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_oracle_recordset_fetch_random;

	pmodel_class->fetch_next = gda_oracle_recordset_fetch_next;
	pmodel_class->fetch_prev = NULL;
	pmodel_class->fetch_at = NULL;
}

static void
gda_oracle_recordset_dispose (GObject *object)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) object;

	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));

	if (recset->priv) {
		/* free specific information */
		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_oracle_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaOracleRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleRecordset),
			0,
			(GInstanceInitFunc) gda_oracle_recordset_init,
			NULL
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaOracleRecordset", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

static sb4
ora_def_callback (GdaOracleValue *ora_value,
		  OCIDefine *def,
		  ub4 iter,
		  dvoid **bufpp,
		  ub4 **alenpp,
		  ub1 *piecep,
		  dvoid **indpp,
		  ub2 **rcodep)
{
	*piecep = OCI_ONE_PIECE;

	if (!ora_value->value) {
		/* 1st chunk */
		ora_value->defined_size = 0;
		ora_value->value = g_new0 (guchar, BUFFER_SIZE + 1);
		*bufpp = ora_value->value;
	}
	else {
		/* other chunks */
		ora_value->defined_size += ora_value->xlen; /* previous's chunk's real size */
#ifdef GDA_DEBUG_NO
		gint i;
		g_print ("XLEN= %d\n", ora_value->xlen);
		for (i = 0 ; i < ora_value->defined_size; i++)
			g_print ("SO FAR[%d]=[%d]%c\n", i, ((gchar*) ora_value->value)[i],
				 g_unichar_isgraph (((gchar*) ora_value->value)[i]) ? ((gchar*) ora_value->value)[i] : '-');
#endif
		ora_value->value = g_renew (guchar, ora_value->value, ora_value->defined_size + BUFFER_SIZE);
		*bufpp = ora_value->value + ora_value->defined_size;
	}

	ora_value->xlen = BUFFER_SIZE;
	*alenpp = &(ora_value->xlen);
	*indpp = (dvoid *) &(ora_value->indicator);
	*rcodep = (ub2 *) &(ora_value->rcode);

	return OCI_CONTINUE;
}

/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_oracle_recordset_new (GdaConnection *cnc, GdaOraclePStmt *ps, GdaSet *exec_params,
			  GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaOracleRecordset *model;
        OracleConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;

	gint nb_rows = -1;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API */
        if (_GDA_PSTMT (ps)->ncols < 0) {
		ub4 ncolumns;
		int result;
		
                /* get the number of columns in the result set */
                result = OCIAttrGet ((dvoid *) ps->hstmt,
                                     (ub4) OCI_HTYPE_STMT,
                                     (dvoid *) &ncolumns,
                                     (ub4 *) 0,
                                     (ub4) OCI_ATTR_PARAM_COUNT,
                                     cdata->herr);
                if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                             _("Could not get the number of columns in the result set")))
                        return NULL;
		
                _GDA_PSTMT (ps)->ncols = ncolumns;
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		GList *ora_values = NULL;

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
		
		/* fill GdaColumn's data and define the GdaOracleValue structures */
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     i < GDA_PSTMT (ps)->ncols; 
		     i++, list = list->next) {
			GdaColumn *column;
			int result;
			GdaOracleValue *ora_value;
			gboolean use_callback = FALSE;

			ora_value = g_new0 (GdaOracleValue, 1);
			ora_values = g_list_prepend (ora_values, ora_value);

			/* parameter to get attributes */
			result = OCIParamGet (ps->hstmt,
					      OCI_HTYPE_STMT,
					      cdata->herr,
					      (dvoid **) &(ora_value->pard),
					      (ub4) i+1);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not get the Oracle parameter descripter in the result set"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}
			
			/* data size */
			result = OCIAttrGet ((dvoid *) (ora_value->pard),
					     OCI_DTYPE_PARAM,
					     &(ora_value->defined_size),
					     0,
					     (ub4) OCI_ATTR_DATA_SIZE,
					     cdata->herr);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not get the parameter defined size"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}
			ora_value->defined_size++;
			
			/* data type */
			result = OCIAttrGet ((dvoid *) (ora_value->pard),
					     OCI_DTYPE_PARAM,
					     &(ora_value->sql_type),
					     0,
					     OCI_ATTR_DATA_TYPE,
					     cdata->herr);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not get the parameter data type"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}

			result = OCIAttrGet ((dvoid *) (ora_value->pard),
					     OCI_DTYPE_PARAM,
					     &(ora_value->precision),
					     0,
					     OCI_ATTR_PRECISION,
					     cdata->herr);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not get the type's precision"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}

			result = OCIAttrGet ((dvoid *) (ora_value->pard),
					     OCI_DTYPE_PARAM,
					     &(ora_value->scale),
					     0,
					     OCI_ATTR_SCALE,
					     cdata->herr);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not get the type's scale"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}

			/* column's name */
			text *name;
			ub4 name_len;
			
			column = GDA_COLUMN (list->data);
			result = OCIAttrGet ((dvoid *) (ora_value->pard),
                                             (ub4) OCI_DTYPE_PARAM,
                                             (dvoid **) &name,
                                             (ub4 *) &name_len,
                                             (ub4) OCI_ATTR_NAME,
                                             (OCIError *) cdata->herr);
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
                                                     _("Could not get column name in the result set"))) {
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
                                return NULL;
			}

			gchar *name_buffer;
			name_buffer = g_new (gchar, name_len + 1);
			memcpy (name_buffer, name, name_len);
                        name_buffer [name_len] = '\0';
			gda_column_set_name (column, name_buffer);
			gda_column_set_description (column, name_buffer);
			g_free (name_buffer);

			/* for data fetching  */
			if (_GDA_PSTMT (ps)->types [i] != GDA_TYPE_NULL)
				ora_value->sql_type = _g_type_to_oracle_sqltype (_GDA_PSTMT (ps)->types [i]);

			switch (ora_value->sql_type) {
			case SQLT_CHR: /* for G_TYPE_STRING => request SQLT_CHR */
			case SQLT_STR:
			case SQLT_VCS:
			case SQLT_RID:
			case SQLT_AVC:
			case SQLT_AFC:
				ora_value->sql_type = SQLT_CHR;
				if (ora_value->defined_size == 1)
					use_callback = TRUE;
				break;

			case SQLT_INT:
				ora_value->defined_size = sizeof (gint);
				break;

			case SQLT_FLT:
			case SQLT_BFLOAT:
				ora_value->defined_size = sizeof (gfloat);
				break;

			case SQLT_BDOUBLE:
				ora_value->defined_size = sizeof (gdouble);
				break;
								
			case SQLT_DAT: /* request OCIDate */
				ora_value->sql_type = SQLT_ODT;
				break;

			case SQLT_NUM: /* for GDA_TYPE_NUMERIC => request SQLT_CHR */
			case SQLT_VNU:
				ora_value->sql_type = SQLT_CHR;
				break;
				
			case SQLT_LBI:
			case SQLT_LVB:
			case SQLT_LVC:
			case SQLT_LNG:
			case SQLT_VBI:
			case SQLT_BIN:
				use_callback = TRUE;
				break;
			default:
				use_callback = TRUE;
				break;
			}
			
			if (_GDA_PSTMT (ps)->types [i] != GDA_TYPE_NULL)
				ora_value->g_type = _GDA_PSTMT (ps)->types [i];
			else
				ora_value->g_type = _oracle_sqltype_to_g_type (ora_value->sql_type, ora_value->precision, 
									       ora_value->scale);

			if (ora_value->g_type == GDA_TYPE_BLOB) {
				/* allocate a Lob locator */
				OCILobLocator *lob;
				
				result = OCIDescriptorAlloc ((dvoid *) cdata->henv, (dvoid **) &lob,
							     (ub4) gda_oracle_blob_type (ora_value->sql_type), 
							     (size_t) 0, (dvoid **) 0);
				if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							     _("Could not allocate Lob locator"))) {
					g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
					return NULL;
				}
				ora_value->value = lob;
				ora_value->defined_size = 0;
			}
			else if (use_callback) {
				ora_value->value = NULL;
				ora_value->defined_size = 33554432; /* 32M */
			}
			else
				ora_value->value = g_malloc0 (ora_value->defined_size);
			
			ora_value->s_type = gda_g_type_to_static_type (ora_value->g_type);
			if (_GDA_PSTMT (ps)->types [i] == GDA_TYPE_NULL)
				_GDA_PSTMT (ps)->types [i] = ora_value->g_type;
			gda_column_set_g_type (column, ora_value->g_type);

#ifdef GDA_DEBUG_NO
			g_print ("**COL type is %d, GType is %s, ORA defined size is %d%s\n",
				 ora_value->sql_type,
				 g_type_name (ora_value->g_type),
				 ora_value->defined_size - 1,
				 use_callback ? " using callback": "");
#endif

			ora_value->hdef = (OCIDefine *) 0;
			ora_value->indicator = 0;
			result = OCIDefineByPos ((OCIStmt *) ps->hstmt,
						 (OCIDefine **) &(ora_value->hdef),
						 (OCIError *) cdata->herr,
						 (ub4) i + 1,
						 ora_value->value,
						 ora_value->defined_size,
						 ora_value->sql_type,
						 (dvoid *) &(ora_value->indicator),
						 &(ora_value->rlen),
						 &(ora_value->rcode),
						 (ub4) (use_callback ? OCI_DYNAMIC_FETCH : OCI_DEFAULT));
			if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						     _("Could not define by position"))) {
				OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
				g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
				return NULL;
			}

			if (use_callback) {
				result = OCIDefineDynamic ((OCIDefine *) (ora_value->hdef),
							   (OCIError *) (cdata->herr),
							   (dvoid*) (ora_value),
							   (OCICallbackDefine) ora_def_callback);
				if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
							     _("Could not define by position"))) {
					OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
					g_list_foreach (ora_values, (GFunc) _gda_oracle_value_free, NULL);
					return NULL;
				}
			}

			ora_value->use_callback = use_callback;
		}

		ps->ora_values = g_list_reverse (ora_values);
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported; if CURSOR BACKWARD
         * is requested, then we need RANDOM mode */
        if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
                rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
        else if (flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
                rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
        else
                rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	ub4 prefetch;
	prefetch = (ub4) (100);
	OCIAttrSet (ps->hstmt, OCI_HTYPE_STMT,
		    &prefetch, 0,
		    OCI_ATTR_PREFETCH_ROWS, cdata->herr);

	/* create data model */
        model = g_object_new (GDA_TYPE_ORACLE_RECORDSET, 
			      "connection", cnc,
			      "prepared-stmt", ps, 
			      "model-usage", rflags, 
			      "exec-params", exec_params, NULL);
	GDA_DATA_SELECT (model)->advertized_nrows = nb_rows;
	
        return GDA_DATA_MODEL (model);
}

static GdaRow *
fetch_next_oracle_row (GdaOracleRecordset *model, G_GNUC_UNUSED gboolean do_store, GError **error)
{
	int result;
	GdaRow *prow = NULL;
	GdaOraclePStmt *ps = GDA_ORACLE_PSTMT (((GdaDataSelect*)model)->prep_stmt);
	OracleConnectionData *cdata;
	GdaConnection *cnc;

	cnc = gda_data_select_get_connection ((GdaDataSelect*) model);
	cdata = (OracleConnectionData*)	gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return NULL;

	/* fetch row */
	if (cdata->major_version > 9)
		result = OCIStmtFetch2 (ps->hstmt,
					cdata->herr,
					(ub4) 1,
					(ub2) OCI_FETCH_NEXT,
					(sb4) 1,
					(ub4) OCI_DEFAULT);
	else
		result = OCIStmtFetch (ps->hstmt,
				       cdata->herr,
				       (ub4) 1,
				       (ub2) OCI_FETCH_NEXT,
				       (ub4) OCI_DEFAULT);
	if (result == OCI_NO_DATA) {
		GDA_DATA_SELECT (model)->advertized_nrows = model->priv->next_row_num;
		return NULL;
	}
	else {
		GdaConnectionEvent *event;
		if ((event = gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
						      _("Could not fetch next row")))) {
			/* set @error */
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_DATA_ERROR,
				     "%s", gda_connection_event_get_description (event));

			return NULL;
		}
	}

	prow = new_row ((GdaDataSelect*) model, cnc, ps);
	gda_data_select_take_row ((GdaDataSelect*) model, prow, model->priv->next_row_num);
	model->priv->next_row_num ++;

	return prow;
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_oracle_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaOracleRecordset *imodel;
        GdaRow *prow = NULL;

        imodel = GDA_ORACLE_RECORDSET (model);
        if (model->advertized_nrows >= 0)
                return model->advertized_nrows;

	struct timeval stm1, stm2;

	gettimeofday (&stm1, NULL);

        for (prow = fetch_next_oracle_row (imodel, TRUE, NULL);
             prow;
             prow = fetch_next_oracle_row (imodel, TRUE, NULL));

	gettimeofday (&stm2, NULL);
	/*g_print ("TOTAL time to get nb rows: %0.2f ms\n", (float) (TS_DIFF(stm1,stm2) / 1000.));*/

        return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * Each new #GdaRow created needs to be "given" to the #GdaDataSelect implementation using
 * gda_data_select_take_row() because backward iterating is not supported.
 */
static gboolean 
gda_oracle_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaOracleRecordset *imodel;
	OracleConnectionData *cdata;
	GdaConnection *cnc;

	imodel = GDA_ORACLE_RECORDSET (model);
	cnc = gda_data_select_get_connection (model);
	cdata = (OracleConnectionData*)	gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return TRUE;

	if (imodel->priv->next_row_num >= rownum) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			     "%s", _("Requested row could not be found"));
		return TRUE;
	}
	for (*prow = fetch_next_oracle_row (imodel, TRUE, error);
	     *prow && (imodel->priv->next_row_num < rownum);
	     *prow = fetch_next_oracle_row (imodel, TRUE, error));

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * Each new #GdaRow created needs to be "given" to the #GdaDataSelect implementation using
 * gda_data_select_take_row() because backward iterating is not supported.
 */
static gboolean 
gda_oracle_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaOracleRecordset *imodel = (GdaOracleRecordset*) model;

	if (imodel->priv->next_row_num != rownum) {
		GError *lerror = NULL;
		*prow = NULL;
		g_set_error (&lerror, GDA_DATA_MODEL_ERROR,
			     GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			     "%s", _("Can't set iterator on requested row"));
		gda_data_select_add_exception (GDA_DATA_SELECT (model), lerror);
		if (error)
			g_propagate_error (error, g_error_copy (lerror));
		return TRUE;
	}
	*prow = fetch_next_oracle_row ((GdaOracleRecordset*) model, TRUE, error);
	return TRUE;
}


/* create a new GdaRow from the last read row */
static GdaRow *
new_row (GdaDataSelect *imodel, GdaConnection *cnc, GdaOraclePStmt *ps)
{
	GdaRow *prow;
	GList *nodes;
	gint i;

	prow = gda_row_new (((GdaDataSelect*) imodel)->prep_stmt->ncols);
	for (i = 0, nodes = ps->ora_values;
	     nodes; 
	     i++, nodes = nodes->next) {
		GdaOracleValue *ora_value = (GdaOracleValue *) nodes->data;
                GValue *value = gda_row_get_value (prow, i); 
                _gda_oracle_set_value (value, ora_value, cnc);
	}

	return prow;
}
