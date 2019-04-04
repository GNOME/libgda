/*
 * Copyright (C) YEAR The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include "gda-capi.h"
#include "gda-capi-recordset.h"
#include "gda-capi-provider.h"
#include <gda-data-select-private.h>
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_capi_recordset_class_init (GdaCapiRecordsetClass *klass);
static void gda_capi_recordset_init       (GdaCapiRecordset *recset);
static void gda_capi_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_capi_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_capi_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


typedef struct {
	GdaConnection *cnc;
	/* TO_ADD: specific information */
} GdaCapiRecordsetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaCapiRecordset, gda_capi_recordset, GDA_TYPE_DATA_SELECT)
/*
 * Object init and finalize
 */
static void
gda_capi_recordset_init (GdaCapiRecordset *recset)
{
	g_return_if_fail (GDA_IS_CAPI_RECORDSET (recset));
  GdaCapiRecordsetPrivate *priv = gda_capi_recordset_get_instance_private (recset);
	priv->cnc = NULL;

	/* initialize specific information */
	TO_IMPLEMENT;
}

static void
gda_capi_recordset_class_init (GdaCapiRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	object_class->dispose = gda_capi_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_capi_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_capi_recordset_fetch_random;

	pmodel_class->fetch_next = gda_capi_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_capi_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_capi_recordset_fetch_at;
}

static void
gda_capi_recordset_dispose (GObject *object)
{
	GdaCapiRecordset *recset = (GdaCapiRecordset *) object;

	g_return_if_fail (GDA_IS_CAPI_RECORDSET (recset));
  GdaCapiRecordsetPrivate *priv = gda_capi_recordset_get_instance_private (recset);

	if (priv->cnc)
		g_object_unref (priv->cnc);

	/* free specific information */
	TO_IMPLEMENT;
	g_free (priv);
	priv = NULL;

	G_OBJECT_CLASS (gda_capi_recordset_parent_class)->dispose (object);
}


/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_capi_recordset_new (GdaConnection *cnc, GdaCapiPStmt *ps, GdaSet *exec_params,
			GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaCapiRecordset *model;
  CapiConnectionData *cdata;
  gint i;
	GdaDataModelAccessFlags rflags;

  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
  g_return_val_if_fail (ps != NULL, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API*/
        if (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) < 0)
                /*gda_pstmt_set_ncols (_GDA_PSTMT (ps),  ...);*/
		TO_IMPLEMENT;

        /* completing @ps if not yet done */
        if (!gda_pstmt_get_types (_GDA_PSTMT (ps)) && (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		for (i = 0; i < gda_pstmt_get_ncols (_GDA_PSTMT (ps)); i++)
			gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps), g_slist_prepend (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps)),
									 gda_column_new ()));
		gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps), g_slist_reverse (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps))));

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
		for (i=0, list = gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps));
		     i < gda_pstmt_get_ncols (_GDA_PSTMT (ps));
		     i++, list = list->next) {
			GdaColumn *column;
			
			column = GDA_COLUMN (list->data);

			/* use C API to set columns' information using gda_column_set_*() */
			g_warning("column not used: %p", column); /* Avoids a compiler warning. */
			TO_IMPLEMENT;
		}
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
  model = g_object_new (GDA_TYPE_CAPI_RECORDSET,
      "prepared-stmt", ps,
      "model-usage", rflags,
      "exec-params", exec_params, NULL);
  GdaCapiRecordsetPrivate *priv = gda_capi_recordset_get_instance_private (model);
  priv->cnc = cnc;
	g_object_ref (cnc);

	/* post init specific code */
	TO_IMPLEMENT;

        return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_capi_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaCapiRecordset *imodel;

	imodel = GDA_CAPI_RECORDSET (model);
	if (gda_data_select_get_advertized_nrows (model) >= 0)
		return gda_data_select_get_advertized_nrows (model);

	/* use C API to determine number of rows,if possible */
	g_warning("imodel not used: %p", imodel); /* Avoids a compiler warning. */
	TO_IMPLEMENT;

	return gda_data_select_get_advertized_nrows (model);
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is MANDATORY if the data model supports random access
 * - this method is only called when data model is used in random access mode
 */
static gboolean 
gda_capi_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum,
				 GError **error)
{
	/*GdaCapiRecordset *imodel = GDA_CAPI_RECORDSET (model);*/

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create and "give" filled #GdaRow object for all the rows in the model
 */
#if 0
static gboolean
gda_capi_recordset_store_all (GdaDataSelect *model, GError **error)
{
	GdaCapiRecordset *imodel;
	gint i;

	imodel = GDA_CAPI_RECORDSET (model);

	/* default implementation */
	for (i = 0; i < model->advertized_nrows; i++) {
		GdaRow *prow;
		if (! gda_capi_recordset_fetch_random (model, &prow, i, error))
			return FALSE;
	}
	return TRUE;
}
#endif

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is MANDATORY
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_capi_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow,
			       gint rownum, GError **error)
{
	/* GdaCapiRecordset *imodel = (GdaCapiRecordset*) model; */

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is OPTIONAL (in this case the data model is assumed not to
 *   support moving iterators backward)
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_capi_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow,
			       gint rownum, GError **error)
{
	/* GdaCapiRecordset *imodel = (GdaCapiRecordset*) model; */

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is OPTIONAL and usefull only if there is a method quicker
 *   than iterating one step at a time to the correct position.
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_capi_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow,
			     gint rownum, GError **error)
{
	/* GdaCapiRecordset *imodel = (GdaCapiRecordset*) model; */
	
	TO_IMPLEMENT;

	return TRUE;
}
