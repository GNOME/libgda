/*
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/gda-connection-private.h>
#include "gda-web.h"
#include "gda-web-recordset.h"
#include "gda-web-provider.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_web_recordset_class_init (GdaWebRecordsetClass *klass);
static void gda_web_recordset_init       (GdaWebRecordset *recset,
					     GdaWebRecordsetClass *klass);
static void gda_web_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_web_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_web_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


struct _GdaWebRecordsetPrivate {
	GdaConnection *cnc;
	
	GdaDataModel *real_model;
	GdaRow       *prow;
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_web_recordset_init (GdaWebRecordset *recset,
			   G_GNUC_UNUSED GdaWebRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_WEB_RECORDSET (recset));
	recset->priv = g_new0 (GdaWebRecordsetPrivate, 1);
	recset->priv->cnc = NULL;
}

static void
gda_web_recordset_class_init (GdaWebRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_web_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_web_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_web_recordset_fetch_random;

	pmodel_class->fetch_next = NULL;
	pmodel_class->fetch_prev = NULL;
	pmodel_class->fetch_at = NULL;
}

static void
gda_web_recordset_dispose (GObject *object)
{
	GdaWebRecordset *recset = (GdaWebRecordset *) object;

	g_return_if_fail (GDA_IS_WEB_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);
		if (recset->priv->real_model)
			g_object_unref (recset->priv->real_model);
		if (recset->priv->prow)
			g_object_unref (recset->priv->prow);

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_web_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaWebRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_web_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaWebRecordset),
			0,
			(GInstanceInitFunc) gda_web_recordset_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaWebRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_web_recordset_new (GdaConnection *cnc, GdaWebPStmt *ps, GdaSet *exec_params,
		       GdaDataModelAccessFlags flags, GType *col_types,
		       const gchar *session_id, xmlNodePtr data_node, GError **error)
{
	GdaWebRecordset *model;
        gint i;
	WebConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (WebConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return FALSE;

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0) {
		xmlNodePtr child;
		_GDA_PSTMT (ps)->ncols = 0;
		for (child = data_node->children; child; child = child->next) {
			if (!strcmp ((gchar*) child->name, "gda_array_field"))
				_GDA_PSTMT (ps)->ncols ++;
		}
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
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
		xmlNodePtr child;

		for (child = data_node->children, i = 0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     child && (i < GDA_PSTMT (ps)->ncols);
		     child = child->next, i++, list = list->next) {
			GdaColumn *column;
			xmlChar *prop;
			gboolean typeset = FALSE;

			while (strcmp ((gchar*) child->name, "gda_array_field"))
				child = child->next;
			column = GDA_COLUMN (list->data);

			if (_GDA_PSTMT (ps)->types [i] == GDA_TYPE_NULL) {
				if (cdata && cdata->reuseable && cdata->reuseable->operations->re_get_type) {
					prop = xmlGetProp (child, BAD_CAST "dbtype");
					if (prop) {
						GType type;
						type = cdata->reuseable->operations->re_get_type (cnc, cdata->reuseable,
												  (gchar*) prop);
						if (type != GDA_TYPE_NULL) {
							_GDA_PSTMT (ps)->types [i] = type;
							gda_column_set_g_type (column, type);
							typeset = TRUE;
						}
						xmlFree (prop);
					}
				}
				if (!typeset) {
					prop = xmlGetProp (child, BAD_CAST "gdatype");
					if (prop) {
						GType type;
						
						type = gda_g_type_from_string ((gchar*) prop);
						if (type == G_TYPE_INVALID)
							type = GDA_TYPE_NULL;
						_GDA_PSTMT (ps)->types [i] = type;
						gda_column_set_g_type (column, type);
						xmlFree (prop);
					}
					else {
						_GDA_PSTMT (ps)->types [i] = G_TYPE_STRING;
						gda_column_set_g_type (column, G_TYPE_STRING);
					}
				}
			}
			else
				gda_column_set_g_type (column, _GDA_PSTMT (ps)->types [i]);
			prop = xmlGetProp (child, BAD_CAST "name");
			if (prop && *prop) {
				gda_column_set_name (column, (gchar*) prop);
				gda_column_set_description (column, (gchar*) prop);
			}
			else {
				gchar *tmp;
				tmp = g_strdup_printf ("col%d", i);
				gda_column_set_name (column, tmp);
				gda_column_set_description (column, tmp);
				g_free (tmp);
			}
			if (prop)
				xmlFree (prop);
		}
        }

	/* create data model */
        model = g_object_new (GDA_TYPE_WEB_RECORDSET, 
			      "prepared-stmt", ps, 
			      "model-usage", GDA_DATA_MODEL_ACCESS_RANDOM, 
			      "exec-params", exec_params, NULL);
        model->priv->cnc = cnc;
	g_object_ref (cnc);

        return GDA_DATA_MODEL (model);
}

/**
 * gda_web_recordset_store
 *
 * Store some more rows coming from the @data_node array
 */
gboolean
gda_web_recordset_store (GdaWebRecordset *rs, xmlNodePtr data_node, GError **error)
{
	GdaDataModel *data;
	gint i, ncols;
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_WEB_RECORDSET (rs), FALSE);
	g_return_val_if_fail (data_node, FALSE);
	g_return_val_if_fail (!strcmp ((gchar*) data_node->name, "gda_array"), FALSE);

	/* modify the @data_node tree to set the correct data types */
	ncols = gda_data_model_get_n_columns ((GdaDataModel*) rs);
	for (node = data_node->children, i = 0;
	     (i < ncols) && node;
	     node = node->next) {
		if (strcmp ((gchar*) node->name, "gda_array_field"))
			continue;
		GdaColumn *column;

		column = gda_data_model_describe_column ((GdaDataModel*) rs, i);
		i++;
		xmlSetProp (node, BAD_CAST "gdatype",
			    BAD_CAST gda_g_type_to_string (gda_column_get_g_type (column)));
	}

	data = gda_data_model_import_new_xml_node (data_node);
	/*data = 	(GdaDataModel*) g_object_new (GDA_TYPE_DATA_MODEL_IMPORT,
					      "options", options,
					      "xml-node", node, NULL);*/
	if (!data) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't import data from web server"));
		return FALSE;
	}
	rs->priv->real_model = data;

	return TRUE;
}

/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_web_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	if (imodel->priv->real_model)
		model->advertized_nrows = gda_data_model_get_n_rows (imodel->priv->real_model);

	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, corresponding to the @rownum row 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_data_select_take_row(). If new row objects are "given" to the GdaDataSelect implementation
 * using that method, then this method should detect when all the data model rows have been analyzed
 * (when model->nb_stored_rows == model->advertized_nrows) and then possibly discard the API handle
 * as it won't be used anymore to fetch rows.
 */
static gboolean 
gda_web_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);

	if (*prow)
                return TRUE;

	if (imodel->priv->real_model) {
		gint i, ncols;
		ncols = gda_data_model_get_n_columns ((GdaDataModel*) model);
		if (!imodel->priv->prow)
			imodel->priv->prow = gda_row_new (ncols);
		for (i = 0; i < ncols; i++) {
			const GValue *cvalue;
			GValue *pvalue;
			cvalue = gda_data_model_get_value_at (imodel->priv->real_model, i, rownum, error);
			if (!cvalue)
				return FALSE;
			pvalue = gda_row_get_value (imodel->priv->prow, i);
			gda_value_reset_with_type (pvalue, G_VALUE_TYPE (cvalue));
			g_value_copy (cvalue, pvalue);
		}
		*prow = imodel->priv->prow;
		return TRUE;
	}

	return FALSE;
}
