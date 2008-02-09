/* GDA library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-private.h>
#include <string.h>
#include "gda-pmodel.h"
#include "gda-prow.h"
#include "gda-pstmt.h"
#include <libgda/gda-statement.h>
#include <libgda/gda-holder.h>

#define CLASS(x) (GDA_PMODEL_CLASS (G_OBJECT_GET_CLASS (x)))

/*
 * Getting a GdaPRow from a model row:
 * model row ==(model->index)==> model->rows index ==(model->rows)==> GdaPRow
 */
struct _GdaPModelPrivate {
	GSList                 *columns; /* list of GdaColumn objects */
	GArray                 *rows; /* Array of GdaPRow */
	GHashTable             *index; /* key = model row number + 1, value = index in @rows array + 1*/

	/* Internal iterator's information, if GDA_DATA_MODEL_CURSOR_* based access */
        gint                    iter_row; /* G_MININT if at start, G_MAXINT if at end */
        GdaDataModelIter       *iter;

	GdaDataModelAccessFlags usage_flags;
};

/* properties */
enum
{
        PROP_0,
	PROP_PREP_STMT,
	PROP_MOD_STMT,
	PROP_FLAGS
};

static void gda_pmodel_class_init (GdaPModelClass *klass);
static void gda_pmodel_init       (GdaPModel *model, GdaPModelClass *klass);
static void gda_pmodel_dispose    (GObject *object);
static void gda_pmodel_finalize   (GObject *object);

static void gda_pmodel_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gda_pmodel_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_pmodel_data_model_init (GdaDataModelClass *iface);
static gint                 gda_pmodel_get_n_rows      (GdaDataModel *model);
static gint                 gda_pmodel_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_pmodel_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_pmodel_get_access_flags(GdaDataModel *model);
static const GValue        *gda_pmodel_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_pmodel_get_attributes_at (GdaDataModel *model, gint col, gint row);

static GdaDataModelIter    *gda_pmodel_create_iter     (GdaDataModel *model);
static gboolean             gda_pmodel_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_pmodel_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_pmodel_iter_at_row     (GdaDataModel *model, GdaDataModelIter *iter, gint row);

static GObjectClass *parent_class = NULL;

/**
 * gda_pmodel_get_type
 *
 * Returns: the #GType of GdaPModel.
 */
GType
gda_pmodel_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaPModelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_pmodel_class_init,
			NULL,
			NULL,
			sizeof (GdaPModel),
			0,
			(GInstanceInitFunc) gda_pmodel_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_pmodel_data_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GdaPModel", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
	}
	return type;
}

static void 
gda_pmodel_class_init (GdaPModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_pmodel_set_property;
        object_class->get_property = gda_pmodel_get_property;
	g_object_class_install_property (object_class, PROP_PREP_STMT,
                                         g_param_spec_object ("prepared-stmt", NULL, NULL, GDA_TYPE_PSTMT,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_MOD_STMT,
                                         g_param_spec_object ("modif-stmt", NULL, NULL, GDA_TYPE_STATEMENT,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_FLAGS,
					 g_param_spec_uint ("model-usage", NULL, NULL, 
							    GDA_DATA_MODEL_ACCESS_RANDOM, G_MAXUINT,
							    GDA_DATA_MODEL_ACCESS_RANDOM,
							    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_pmodel_dispose;
	object_class->finalize = gda_pmodel_finalize;
}

static void
gda_pmodel_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_pmodel_get_n_rows;
	iface->i_get_n_columns = gda_pmodel_get_n_columns;
	iface->i_describe_column = gda_pmodel_describe_column;
        iface->i_get_access_flags = gda_pmodel_get_access_flags;
	iface->i_get_value_at = gda_pmodel_get_value_at;
	iface->i_get_attributes_at = gda_pmodel_get_attributes_at;

	iface->i_create_iter = gda_pmodel_create_iter;
        iface->i_iter_at_row = gda_pmodel_iter_at_row;
        iface->i_iter_next = gda_pmodel_iter_next;
        iface->i_iter_prev = gda_pmodel_iter_prev;

	iface->i_set_value_at = NULL;
	iface->i_set_values = NULL;
        iface->i_append_values = NULL;
	iface->i_append_row = NULL;
	iface->i_remove_row = NULL;
	iface->i_find_row = NULL;
	
	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
	iface->i_send_hint = NULL;
}

static void
gda_pmodel_init (GdaPModel *model, GdaPModelClass *klass)
{
	g_return_if_fail (GDA_IS_PMODEL (model));
	model->priv = g_new0 (GdaPModelPrivate, 1);
	model->priv->rows = g_array_new (FALSE, FALSE, sizeof (GdaPRow *));
	model->priv->index = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->prep_stmt = NULL;
	model->priv->columns = NULL;
	model->advertized_nrows = -1; /* unknown number of rows */

	model->priv->iter_row = G_MININT;
        model->priv->iter = NULL;
}

static void
gda_pmodel_dispose (GObject *object)
{
	GdaPModel *model = (GdaPModel *) object;

	g_return_if_fail (GDA_IS_PMODEL (model));

	/* free memory */
	if (model->priv) {
		if (model->prep_stmt) {
			g_object_unref (model->prep_stmt);
			model->prep_stmt = NULL;
		}
		if (model->priv->rows) {
			gint i;
			for (i = 0; i < model->priv->rows->len; i++) {
				GdaPRow *prow;
				prow = g_array_index (model->priv->rows, GdaPRow *, i);
				g_object_unref (prow);
			}
			g_array_free (model->priv->rows, TRUE);
			model->priv->rows = NULL;
		}
		if (model->priv->index) {
			g_hash_table_destroy (model->priv->index);
			model->priv->index = NULL;
		}
		if (model->priv->columns) {
			g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->columns);
			model->priv->columns = NULL;
		}
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_pmodel_finalize (GObject *object)
{
	GdaPModel *model = (GdaPModel *) object;

	g_return_if_fail (GDA_IS_PMODEL (model));

	/* free memory */
	if (model->priv) {
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
create_columns (GdaPModel *model) 
{
	gint i;
	if (model->priv->columns) {
		g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
		g_slist_free (model->priv->columns);
		model->priv->columns = NULL;
	}
	if (!model->prep_stmt)
		return;

	if (model->prep_stmt->ncols < 0)
		g_error ("INTERNAL implementation error: unknown number of columns in GdaPStmt, \n"
			 "set number of columns before using with GdaPModel");
	if (model->prep_stmt->tmpl_columns) {
		/* copy template columns */
		GSList *list;
		for (list = model->prep_stmt->tmpl_columns; list; list = list->next)
			model->priv->columns = g_slist_append (model->priv->columns, 
							       gda_column_copy (GDA_COLUMN (list->data)));
	}
	else 
		/* create columns */
		for (i = 0; i < model->prep_stmt->ncols; i++) {
			GdaColumn *gda_col;
			gda_col = gda_column_new ();
			if (model->prep_stmt->types) 
				gda_column_set_g_type (gda_col, model->prep_stmt->types [i]);
			model->priv->columns = g_slist_append (model->priv->columns, gda_col);
		}
}

static void
gda_pmodel_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaPModel *model = (GdaPModel *) object;
	if (model->priv) {
		switch (param_id) {
		case PROP_PREP_STMT:
			if (model->prep_stmt)
				g_object_unref (model->prep_stmt);
			model->prep_stmt = g_value_get_object (value);
			if (model->prep_stmt)
				g_object_ref (model->prep_stmt);
			create_columns (model);
			break;
		case PROP_MOD_STMT:
			TO_IMPLEMENT;
			break;
		case PROP_FLAGS: {
			GdaDataModelAccessFlags flags = g_value_get_uint (value);
			if (!(flags & GDA_DATA_MODEL_ACCESS_RANDOM) &&
			    (flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD))
				flags = GDA_DATA_MODEL_ACCESS_CURSOR;
			model->priv->usage_flags = flags;
			break;
		}
		default:
			break;
		}
	}
}

static void
gda_pmodel_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaPModel *model = (GdaPModel *) object;
	if (model->priv) {
		switch (param_id) {
		case PROP_PREP_STMT:
			g_value_set_object (value, model->prep_stmt);
			break;
		case PROP_MOD_STMT:
			TO_IMPLEMENT;
			break;
		case PROP_FLAGS:
			g_value_set_uint (value, model->priv->usage_flags);
			break;
		default:
			break;
		}
	}
}

/**
 * gda_pmodel_take_row
 * @model: a #GdaPModel data model
 * @row: a #GdaPRow row
 * @rownum: "external" advertized row number
 *
 * Stores @row into @model, externally advertized at row number @rownum
 */
void
gda_pmodel_take_row (GdaPModel *model, GdaPRow *row, gint rownum)
{
	g_return_if_fail (GDA_IS_PMODEL (model));
	g_return_if_fail (GDA_IS_PROW (row));

	if (g_hash_table_lookup (model->priv->index, GINT_TO_POINTER (rownum + 1))) 
		g_error ("INTERNAL error: row %d already exists, aborting", rownum);

	g_hash_table_insert (model->priv->index, GINT_TO_POINTER (rownum + 1), GINT_TO_POINTER (model->priv->rows->len + 1));
	g_array_append_val (model->priv->rows, row);
	g_object_ref (row);
}

/**
 * gda_pmodel_get_stored_row
 * @model: a #GdaPModel data model
 * @rownum: "external" advertized row number
 *
 * Get the #GdaPRow object stored within @model at row @rownum
 *
 * Returns: the requested #GdaPRow, or %NULL if not found
 */
GdaPRow *
gda_pmodel_get_stored_row (GdaPModel *model, gint rownum)
{
	gint irow;
	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (model->priv->index, GINT_TO_POINTER (rownum + 1)));
	if (irow <= 0) 
		return NULL;
	else 
		return g_array_index (model->priv->rows, GdaPRow *, irow - 1);
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_pmodel_get_n_rows (GdaDataModel *model)
{
	GdaPModel *imodel;
	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = GDA_PMODEL (model);
	g_return_val_if_fail (imodel->priv, 0);

	if ((imodel->advertized_nrows < 0) && 
	    (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) &&
	    CLASS (model)->fetch_nb_rows)
		return CLASS (model)->fetch_nb_rows (imodel);
		
	return imodel->advertized_nrows;
}

static gint
gda_pmodel_get_n_columns (GdaDataModel *model)
{
	GdaPModel *imodel;
	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = GDA_PMODEL (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->prep_stmt)
		return imodel->prep_stmt->ncols;
	else
		return 0;
}

static GdaColumn *
gda_pmodel_describe_column (GdaDataModel *model, gint col)
{
	GdaPModel *imodel;
	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	imodel = GDA_PMODEL (model);
	g_return_val_if_fail (imodel->priv, NULL);

	return g_slist_nth_data (imodel->priv->columns, col);
}

static GdaDataModelAccessFlags
gda_pmodel_get_access_flags (GdaDataModel *model)
{
	GdaPModel *imodel;
	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = GDA_PMODEL (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		return GDA_DATA_MODEL_ACCESS_RANDOM;
	else if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD) {
		if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
			return GDA_DATA_MODEL_ACCESS_CURSOR;
		return GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;
	}
	g_assert_not_reached ();
}

static const GValue *
gda_pmodel_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaPRow *prow;
	gint irow;
	GdaPModel *imodel;

	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);

	/* available only if GDA_DATA_MODEL_ACCESS_RANDOM */
	if (! (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM))
		return NULL;

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->index, GINT_TO_POINTER (row + 1)));
	if (irow <= 0) {
		if (CLASS (model)->fetch_random)
			prow = CLASS (model)->fetch_random (imodel, row, NULL);
	}
	else 
		prow = g_array_index (imodel->priv->rows, GdaPRow *, irow - 1);
	
	if (prow) 
		return gda_prow_get_value (prow, col);
	return NULL;
}

static GdaValueAttribute
gda_pmodel_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute flags = 0;
	GdaPModel *imodel;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	TO_IMPLEMENT;
	flags = GDA_VALUE_ATTR_NO_MODIF;
	
	return flags;
}

static GdaDataModelIter *
gda_pmodel_create_iter (GdaDataModel *model)
{
	GdaPModel *imodel;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER,
							  "data_model", model, NULL);
	else {
		/* Create the iter if necessary, or just return the existing iter: */
		if (! imodel->priv->iter) {
			imodel->priv->iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER,
										"data_model", model, NULL);
			imodel->priv->iter_row = -1;
		}
		g_object_ref (imodel->priv->iter);
		return imodel->priv->iter;
	}
}

static void update_iter (GdaPModel *imodel, GdaPRow *prow);
static gboolean
gda_pmodel_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaPModel *imodel;
	GdaPRow *prow = NULL;
	gint target_iter_row;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_move_iter_next_default (model, iter);

	g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	if (imodel->priv->iter_row == G_MAXINT)
		return FALSE;
	else if (imodel->priv->iter_row == G_MININT)
		target_iter_row = 0;
	else
		target_iter_row = imodel->priv->iter_row + 1;

	if (CLASS (model)->fetch_next)
		prow = CLASS (model)->fetch_next (imodel, target_iter_row, NULL);
	else
		g_error ("INTERNAL error: fetch_next() virtual method not implemented, aborting");
	
	if (prow) {
		imodel->priv->iter_row = target_iter_row;
                update_iter (imodel, prow);
                return TRUE;
	}
	else {
		g_signal_emit_by_name (iter, "end_of_data");
                g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
                imodel->priv->iter_row = G_MAXINT;
                return FALSE;
	}
}

static gboolean
gda_pmodel_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaPModel *imodel;
	GdaPRow *prow = NULL;
	gint target_iter_row;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_move_iter_prev_default (model, iter);

        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

        if (imodel->priv->iter_row <= 0)
                goto prev_error;

        else if (imodel->priv->iter_row == G_MAXINT) {
                g_assert (imodel->advertized_nrows >= 0);
                target_iter_row = imodel->advertized_nrows - 1;
        }
        else
                target_iter_row = imodel->priv->iter_row - 1;

	if (CLASS (model)->fetch_prev)
		prow = CLASS (model)->fetch_prev (imodel, target_iter_row, NULL);
	else
		g_error ("INTERNAL error: fetch_prev() virtual method not implemented, aborting");

	if (prow) {
		imodel->priv->iter_row = target_iter_row;
                update_iter (imodel, prow);
                return TRUE;
	}
	else 
		goto prev_error;

 prev_error:
        g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
        imodel->priv->iter_row = G_MININT;
        return FALSE;
}

static gboolean
gda_pmodel_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	GdaPModel *imodel;
	GdaPRow *prow = NULL;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_move_iter_at_row_default (model, iter, row);

        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	if (CLASS (model)->fetch_at) {
		prow = CLASS (model)->fetch_at (imodel, row, NULL);
		if (prow) {
			imodel->priv->iter_row = row;
			update_iter (imodel, prow);
			return TRUE;
		}
		else {
			g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
			imodel->priv->iter_row = G_MININT;
			return FALSE;
		}
	}
	else {
		/* implementation of fetch_at() is optional */
		TO_IMPLEMENT; /* iter back or forward the right number of times */
		return FALSE;
	}
}

static void
update_iter (GdaPModel *imodel, GdaPRow *prow)
{
        gint i, length;
	GdaDataModelIter *iter = imodel->priv->iter;
	GSList *plist;
	gboolean update_model;
	
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	
	length = gda_prow_get_length (prow);
	g_assert (length == g_slist_length (GDA_SET (iter)->holders));
	for (i = 0, plist = GDA_SET (iter)->holders; 
	     i < length;
	     i++, plist = plist->next) {
		const GValue *value;
		value = gda_prow_get_value (prow, i);
		gda_holder_set_value (GDA_HOLDER (plist->data), value);
        }

	g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row, NULL);
	g_object_set (G_OBJECT (iter), "update_model", update_model, NULL);
}
