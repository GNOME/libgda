/*
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include <gda-data-model.h>
#include <gda-data-select-private.h>
#include "gda-web.h"
#include "gda-web-recordset.h"
#include "gda-web-provider.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_web_recordset_class_init (GdaWebRecordsetClass *klass);
static void gda_web_recordset_init       (GdaWebRecordset *recset);
static void gda_web_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_web_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_web_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


typedef struct {
	GdaConnection *cnc;
	
	GdaDataModel *real_model;
	GdaRow       *prow;
} GdaWebRecordsetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GdaWebRecordset, gda_web_recordset, GDA_TYPE_DATA_SELECT)

/*
 * Object init and finalize
 */
static void
gda_web_recordset_init (GdaWebRecordset *recset)
{
	g_return_if_fail (GDA_IS_WEB_RECORDSET (recset));
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (recset);
	priv->cnc = NULL;
}

static void
gda_web_recordset_class_init (GdaWebRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

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
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (recset);

	if (priv->cnc)
		g_object_unref (priv->cnc);
	if (priv->real_model)
		g_object_unref (priv->real_model);
	if (priv->prow)
		g_object_unref (priv->prow);

	G_OBJECT_CLASS (gda_web_recordset_parent_class)->dispose (object);
}

/*
 * Public functions
 */

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
	if (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) < 0) {
		xmlNodePtr child;
		gda_pstmt_set_cols (_GDA_PSTMT (ps), 0,
				                gda_pstmt_get_types (_GDA_PSTMT (ps)));
		for (child = data_node->children; child; child = child->next) {
			if (!strcmp ((gchar*) child->name, "gda_array_field"))
				gda_pstmt_set_cols (_GDA_PSTMT (ps), gda_pstmt_get_ncols (_GDA_PSTMT (ps)) + 1,
				                    gda_pstmt_get_types (_GDA_PSTMT (ps)));
		}
	}

        /* completing @ps if not yet done */
        if (!gda_pstmt_get_types (_GDA_PSTMT (ps)) && (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		for (i = 0; i < gda_pstmt_get_ncols (_GDA_PSTMT (ps)); i++)
			gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps),
			                           g_slist_prepend (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps)),
			                                            gda_column_new ()));
		gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps),
		                            g_slist_reverse (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps))));

		/* create prepared statement's types, all types are initialized to GDA_TYPE_NULL */
		gda_pstmt_set_cols (_GDA_PSTMT (ps), gda_pstmt_get_ncols (_GDA_PSTMT (ps)), g_new (GType, gda_pstmt_get_ncols (_GDA_PSTMT (ps))));
		for (i = 0; i < gda_pstmt_get_ncols (_GDA_PSTMT (ps)); i++)
			gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = GDA_TYPE_NULL;

		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= gda_pstmt_get_ncols (_GDA_PSTMT (ps))) {
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   gda_pstmt_get_ncols (_GDA_PSTMT (ps)) - 1);
						break;
					}
					else
						gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = col_types [i];
				}
			}
		}
		
		/* fill GdaColumn's data */
		xmlNodePtr child;

		for (child = data_node->children, i = 0, list = gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps));
		     child && (i < gda_pstmt_get_ncols (GDA_PSTMT (ps)));
		     child = child->next, i++, list = list->next) {
			GdaColumn *column;
			xmlChar *prop;
			gboolean typeset = FALSE;

			while (strcmp ((gchar*) child->name, "gda_array_field"))
				child = child->next;
			column = GDA_COLUMN (list->data);

			if (gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] == GDA_TYPE_NULL) {
				if (cdata && cdata->reuseable && cdata->reuseable->operations->re_get_type) {
					prop = xmlGetProp (child, BAD_CAST "dbtype");
					if (prop) {
						GType type;
						type = cdata->reuseable->operations->re_get_type (cnc, cdata->reuseable,
												  (gchar*) prop);
						if (type != GDA_TYPE_NULL) {
							gda_pstmt_get_types (GDA_PSTMT (ps)) [i] = type;
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
						gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = type;
						gda_column_set_g_type (column, type);
						xmlFree (prop);
					}
					else {
						gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = G_TYPE_STRING;
						gda_column_set_g_type (column, G_TYPE_STRING);
					}
				}
			}
			else
				gda_column_set_g_type (column, gda_pstmt_get_types (_GDA_PSTMT (ps)) [i]);
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
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (model);
  priv->cnc = cnc;
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
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (rs);

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
	priv->real_model = data;

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
	if (gda_data_select_get_advertized_nrows (model) >= 0)
		return gda_data_select_get_advertized_nrows (model);
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (imodel);

	if (priv->real_model)
		gda_data_select_set_advertized_nrows (model, gda_data_model_get_n_rows (priv->real_model));

	return gda_data_select_get_advertized_nrows (model);
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
  GdaWebRecordsetPrivate *priv = gda_web_recordset_get_instance_private (imodel);

	if (*prow) {
    return TRUE;
  }

	if (priv->real_model) {
		gint i, ncols;
		ncols = gda_data_model_get_n_columns ((GdaDataModel*) model);
		if (!priv->prow)
			priv->prow = gda_row_new (ncols);
		for (i = 0; i < ncols; i++) {
			const GValue *cvalue;
			GValue *pvalue;
			cvalue = gda_data_model_get_value_at (priv->real_model, i, rownum, error);
			if (!cvalue)
				return FALSE;
			pvalue = gda_row_get_value (priv->prow, i);
			gda_value_reset_with_type (pvalue, G_VALUE_TYPE (cvalue));
			g_value_copy (cvalue, pvalue);
		}
		*prow = priv->prow;
		return TRUE;
	}

	return FALSE;
}
