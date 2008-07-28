/* GDA Capi provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
#include "gda-capi.h"
#include "gda-capi-recordset.h"
#include "gda-capi-provider.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_capi_recordset_class_init (GdaCapiRecordsetClass *klass);
static void gda_capi_recordset_init       (GdaCapiRecordset *recset,
					     GdaCapiRecordsetClass *klass);
static void gda_capi_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_capi_recordset_fetch_nb_rows (GdaPModel *model);
static gboolean gda_capi_recordset_fetch_random (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_next (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_prev (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_capi_recordset_fetch_at (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);


struct _GdaCapiRecordsetPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information */
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_capi_recordset_init (GdaCapiRecordset *recset,
			   GdaCapiRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_CAPI_RECORDSET (recset));
	recset->priv = g_new0 (GdaCapiRecordsetPrivate, 1);
	recset->priv->cnc = NULL;

	/* initialize specific information */
	TO_IMPLEMENT;
}

static void
gda_capi_recordset_class_init (GdaCapiRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaPModelClass *pmodel_class = GDA_PMODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);

		/* free specific information */
		TO_IMPLEMENT;
		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_capi_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaCapiRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_capi_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaCapiRecordset),
			0,
			(GInstanceInitFunc) gda_capi_recordset_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PMODEL, "GdaCapiRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_capi_recordset_new (GdaConnection *cnc, GdaCapiPStmt *ps, GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaCapiRecordset *model;
        CapiConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (CapiConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0)
                /*_GDA_PSTMT (ps)->ncols = ...;*/
		TO_IMPLEMENT;

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
		
		/* fill GdaColumn's data */
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     i < GDA_PSTMT (ps)->ncols; 
		     i++, list = list->next) {
			GdaColumn *column;
			
			column = GDA_COLUMN (list->data);

			/* use C API to set columns' information using gda_column_set_*() */
			TO_IMPLEMENT;
		}
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_CAPI_RECORDSET, "prepared-stmt", ps, "model-usage", rflags, NULL);
        model->priv->cnc = cnc;
	g_object_ref (cnc);

	/* post init specific code */
	TO_IMPLEMENT;

        return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_capi_recordset_fetch_nb_rows (GdaPModel *model)
{
	GdaCapiRecordset *imodel;

	imodel = GDA_CAPI_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	/* use C API to determine number of rows,if possible */
	TO_IMPLEMENT;

	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_pmodel_take_row(). If new row objects are "given" to the GdaPModel implemantation
 * using that method, then this method should detect when all the data model rows have been analysed
 * (when model->nb_stored_rows == model->advertized_nrows) and then possibly discard the API handle
 * as it won't be used anymore to fetch rows.
 */
static gboolean 
gda_capi_recordset_fetch_random (GdaPModel *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaCapiRecordset *imodel;

	imodel = GDA_CAPI_RECORDSET (model);

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create and "give" filled #GdaRow object for all the rows in the model
 */
static gboolean
gda_capi_recordset_store_all (GdaPModel *model, GError **error)
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

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_capi_recordset_fetch_next (GdaPModel *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaCapiRecordset *imodel = (GdaCapiRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_capi_recordset_fetch_prev (GdaPModel *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaCapiRecordset *imodel = (GdaCapiRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_pmodel_take_row().
 */
static gboolean 
gda_capi_recordset_fetch_at (GdaPModel *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaCapiRecordset *imodel = (GdaCapiRecordset*) model;
	
	TO_IMPLEMENT;

	return TRUE;
}

