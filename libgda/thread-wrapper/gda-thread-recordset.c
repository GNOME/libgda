/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-thread-wrapper.h"
#include "gda-thread-recordset.h"
#include "gda-thread-provider.h"
#include <libgda/providers-support/gda-data-select-priv.h>
#include "gda-thread-blob-op.h"

static void gda_thread_recordset_class_init (GdaThreadRecordsetClass *klass);
static void gda_thread_recordset_init       (GdaThreadRecordset *recset,
					     GdaThreadRecordsetClass *klass);
static void gda_thread_recordset_dispose   (GObject *object);

/* virtual methods */
static gint    gda_thread_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_thread_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);

static gboolean gda_thread_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_thread_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_thread_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);

struct _GdaThreadRecordsetPrivate {
	GdaDataModel *sub_model;
	GdaThreadWrapper *wrapper;
	gint nblobs;
	gint *blobs_conv;
};
static GObjectClass *parent_class = NULL;

#define DATA_SELECT_CLASS(model) (GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (model)))
#define COPY_PUBLIC_DATA(from,to) \
	(((GdaDataSelect *) (to))->nb_stored_rows = ((GdaDataSelect *) (from))->nb_stored_rows, \
	 ((GdaDataSelect *) (to))->prep_stmt = ((GdaDataSelect *) (from))->prep_stmt, \
	 ((GdaDataSelect *) (to))->advertized_nrows = ((GdaDataSelect *) (from))->advertized_nrows)

/*
 * Object init and finalize
 */
static void
gda_thread_recordset_init (GdaThreadRecordset *recset,
			   G_GNUC_UNUSED GdaThreadRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_THREAD_RECORDSET (recset));
	recset->priv = g_new0 (GdaThreadRecordsetPrivate, 1);
	recset->priv->sub_model = NULL;
	recset->priv->wrapper = NULL;
	recset->priv->blobs_conv = NULL;
}

static void
gda_thread_recordset_class_init (GdaThreadRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_thread_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_thread_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_thread_recordset_fetch_random;
	pmodel_class->store_all = NULL;

	pmodel_class->fetch_next = gda_thread_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_thread_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_thread_recordset_fetch_at;
}

static void
gda_thread_recordset_dispose (GObject *object)
{
	GdaThreadRecordset *recset = (GdaThreadRecordset *) object;

	g_return_if_fail (GDA_IS_THREAD_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->sub_model) {
			/* unref recset->priv->sub_model in sub thread */
			gda_thread_wrapper_execute_void (recset->priv->wrapper, 
							 (GdaThreadWrapperVoidFunc) g_object_unref,
							 recset->priv->sub_model, NULL, NULL);
			recset->priv->sub_model = NULL;
			g_object_unref (recset->priv->wrapper);
			recset->priv->wrapper = NULL;

			if (recset->priv->blobs_conv) {
				g_free (recset->priv->blobs_conv);
				recset->priv->blobs_conv = NULL;
			}
		}
		g_free (recset->priv);
		recset->priv = NULL;

		((GdaDataSelect*) recset)->prep_stmt = NULL; /* don't unref since we don't hold a reference to it */
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
_gda_thread_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaThreadRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_thread_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaThreadRecordset),
			0,
			(GInstanceInitFunc) gda_thread_recordset_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaThreadRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}


/*
 * executed in the sub thread (the one which can manipulate @sub_model)
 */
GdaDataModel *
_gda_thread_recordset_new (GdaConnection *cnc, GdaThreadWrapper *wrapper, GdaDataModel *sub_model)
{
	GdaThreadRecordset *model;

	model = GDA_THREAD_RECORDSET (g_object_new (GDA_TYPE_THREAD_RECORDSET,
						    "connection", cnc, NULL));

	_gda_data_select_share_private_data (GDA_DATA_SELECT (sub_model), 
					     GDA_DATA_SELECT (model));
	model->priv->wrapper = g_object_ref (wrapper);
	model->priv->sub_model = g_object_ref (sub_model);

	gint ncols, i, nblobs;
	gint *blobs_conv = NULL;
	ncols = gda_data_model_get_n_columns (sub_model);
	nblobs = 0;
	for (i = 0; i < ncols; i++) {
		GdaColumn *col;
		col = gda_data_model_describe_column (sub_model, i);
		if (gda_column_get_g_type (col) == GDA_TYPE_BLOB) {
			if (!blobs_conv)
				blobs_conv = g_new0 (gint, ncols);
			blobs_conv [nblobs] = i;
			nblobs++;
		}
	}
	model->priv->blobs_conv = blobs_conv;
	model->priv->nblobs = nblobs;

	COPY_PUBLIC_DATA (sub_model, model);

        return GDA_DATA_MODEL (model);
}

/*
 * GdaDataSelect virtual methods
 */


/*
 * fetch nb rows
 */
static gpointer
sub_thread_fetch_nb_rows (GdaDataSelect *model, G_GNUC_UNUSED GError **error)
{
	/* WARNING: function executed in sub thread! */
	gint retval;
	gint *res;
	retval = DATA_SELECT_CLASS (model)->fetch_nb_rows (model);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %d\n", __FUNCTION__, retval);
#endif
	res = g_new (gint, 1);
	*res = retval;
	return res;
}

static gint
gda_thread_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaThreadRecordset *rs = (GdaThreadRecordset*) model;
	gint nb;
	gint *res;
	guint jid;
	jid = gda_thread_wrapper_execute (rs->priv->wrapper, 
					  (GdaThreadWrapperFunc) sub_thread_fetch_nb_rows, 
					  rs->priv->sub_model, NULL, NULL);
	
	res = (gint*) gda_thread_wrapper_fetch_result (rs->priv->wrapper, TRUE, jid, NULL);
	nb = *res;
	g_free (res);
	COPY_PUBLIC_DATA (rs->priv->sub_model, rs);
	return nb;
}

/*
 * fetch random
 */
typedef struct {
	GdaDataSelect *select;
	gint rownum;
	GdaRow **row;
} ThreadData;

static gpointer
sub_thread_fetch_random (ThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = DATA_SELECT_CLASS (data->select)->fetch_random (data->select, data->row, data->rownum, error);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
#endif
	return GINT_TO_POINTER (retval);
}

static void
alter_blob_values (GdaThreadRecordset *rs, GdaRow **prow)
{
	gint i;
	for (i = 0; i < rs->priv->nblobs; i++) {
		GValue *value = gda_row_get_value (*prow, rs->priv->blobs_conv[i]);
		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			blob = (GdaBlob*) gda_value_get_blob (value);
			if (blob->op) {
				GdaBlobOp *nop;
				nop = _gda_thread_blob_op_new (rs->priv->wrapper, blob->op);
				g_object_unref (blob->op);
				blob->op = nop;
			}
		}
	}
}

static gboolean
gda_thread_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaThreadRecordset *rs = (GdaThreadRecordset*) model;
	ThreadData wdata;
	gboolean retval;
	guint jid;

	wdata.select = (GdaDataSelect *) rs->priv->sub_model;
	wdata.rownum = rownum;
	wdata.row = prow;
	jid = gda_thread_wrapper_execute (rs->priv->wrapper, 
					  (GdaThreadWrapperFunc) sub_thread_fetch_random, 
					  &wdata, NULL, NULL);

	retval = GPOINTER_TO_INT (gda_thread_wrapper_fetch_result (rs->priv->wrapper, TRUE, jid, error)) ? 
		TRUE : FALSE;
	COPY_PUBLIC_DATA (rs->priv->sub_model, rs);

	if (*prow && rs->priv->blobs_conv)
		alter_blob_values (rs, prow);

	return retval;
}

/*
 * fetch next
 */
static gpointer
sub_thread_fetch_next (ThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = DATA_SELECT_CLASS (data->select)->fetch_next (data->select, data->row, data->rownum, error);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
#endif
	return GINT_TO_POINTER (retval);
}

static gboolean
gda_thread_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaThreadRecordset *rs = (GdaThreadRecordset*) model;
	ThreadData wdata;
	gboolean retval;
	guint jid;

	wdata.select = (GdaDataSelect *) rs->priv->sub_model;
	wdata.rownum = rownum;
	wdata.row = prow;
	jid = gda_thread_wrapper_execute (rs->priv->wrapper, 
					  (GdaThreadWrapperFunc) sub_thread_fetch_next, 
					  &wdata, NULL, NULL);
	retval = GPOINTER_TO_INT (gda_thread_wrapper_fetch_result (rs->priv->wrapper, TRUE, jid, error)) ? 
		TRUE : FALSE;
	COPY_PUBLIC_DATA (rs->priv->sub_model, rs);

	if (*prow && rs->priv->blobs_conv)
		alter_blob_values (rs, prow);

	return retval;
}

/*
 * fetch prev
 */
static gpointer
sub_thread_fetch_prev (ThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = DATA_SELECT_CLASS (data->select)->fetch_prev (data->select, data->row, data->rownum, error);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
#endif
	return GINT_TO_POINTER (retval);
}

static gboolean
gda_thread_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaThreadRecordset *rs = (GdaThreadRecordset*) model;
	ThreadData wdata;
	gboolean retval;
	guint jid;

	wdata.select = (GdaDataSelect *) rs->priv->sub_model;
	wdata.rownum = rownum;
	wdata.row = prow;
	jid = gda_thread_wrapper_execute (rs->priv->wrapper, 
					  (GdaThreadWrapperFunc) sub_thread_fetch_prev, 
					  &wdata, NULL, NULL);
	retval = GPOINTER_TO_INT (gda_thread_wrapper_fetch_result (rs->priv->wrapper, TRUE, jid, error)) ? 
		TRUE : FALSE;
	COPY_PUBLIC_DATA (rs->priv->sub_model, rs);

	if (*prow && rs->priv->blobs_conv)
		alter_blob_values (rs, prow);

	return retval;
}

/*
 * fetch at
 */
static gpointer
sub_thread_fetch_at (ThreadData *data, GError **error)
{
	/* WARNING: function executed in sub thread! */
	gboolean retval;
	retval = DATA_SELECT_CLASS (data->select)->fetch_at (data->select, data->row, data->rownum, error);
#ifdef GDA_DEBUG_NO
	g_print ("/%s() => %s\n", __FUNCTION__, retval ? "TRUE" : "FALSE");
#endif
	return GINT_TO_POINTER (retval);
}

static gboolean
gda_thread_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaThreadRecordset *rs = (GdaThreadRecordset*) model;
	ThreadData wdata;
	gboolean retval;
	guint jid;

	wdata.select = (GdaDataSelect *) rs->priv->sub_model;
	wdata.rownum = rownum;
	wdata.row = prow;
	jid = gda_thread_wrapper_execute (rs->priv->wrapper, 
					  (GdaThreadWrapperFunc) sub_thread_fetch_at, 
					  &wdata, NULL, NULL);
	retval = GPOINTER_TO_INT (gda_thread_wrapper_fetch_result (rs->priv->wrapper, TRUE, jid, error)) ? 
		TRUE : FALSE;
	COPY_PUBLIC_DATA (rs->priv->sub_model, rs);

	if (*prow && rs->priv->blobs_conv)
		alter_blob_values (rs, prow);

	return retval;
}
