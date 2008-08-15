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
#include "gda-row.h"
#include "gda-pstmt.h"
#include <libgda/gda-statement.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-statement.h>

#define CLASS(x) (GDA_PMODEL_CLASS (G_OBJECT_GET_CLASS (x)))

typedef enum
{
	FIRST_QUERY = 0,
        INS_QUERY  = 0,
        UPD_QUERY  = 1,
        DEL_QUERY  = 2,
	NB_QUERIES = 3
} ModType;

/*
 * Getting a GdaRow from a model row:
 * model row ==(model->index)==> model->rows index ==(model->rows)==> GdaRow
 */
struct _GdaPModelPrivate {
	GdaConnection          *cnc;
	GSList                 *columns; /* list of GdaColumn objects */
	GArray                 *rows; /* Array of GdaRow pointers */
	GHashTable             *index; /* key = model row number + 1, value = index in @rows array + 1*/

	/* Internal iterator's information, if GDA_DATA_MODEL_CURSOR_* based access */
        gint                    iter_row; /* G_MININT if at start, G_MAXINT if at end */
        GdaDataModelIter       *iter;

	GdaDataModelAccessFlags usage_flags;
	
	GdaSet                 *exec_set; /* owned by this object (copied) */
	GdaSet                 *modif_set; /* owned by this object */
	GdaStatement           *modif_stmts[NB_QUERIES];

	GdaStatement          **upd_stmts; /* one stmt per data model column */
};

/* properties */
enum
{
        PROP_0,
	PROP_CNC,
	PROP_PREP_STMT,
	PROP_FLAGS,
	PROP_ALL_STORED,
	PROP_PARAMS,
	PROP_INS_QUERY,
	PROP_UPD_QUERY,
	PROP_DEL_QUERY
};

/* module error */
GQuark gda_pmodel_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_pmodel_error");
        return quark;
}

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

static gboolean             gda_pmodel_set_value_at    (GdaDataModel *model, gint col, gint row, 
							const GValue *value, GError **error);
static gboolean             gda_pmodel_set_values      (GdaDataModel *model, gint row, GList *values,
							GError **error);
static gint                 gda_pmodel_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gboolean             gda_pmodel_remove_row      (GdaDataModel *model, gint row, GError **error);

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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaPModel", &info, G_TYPE_FLAG_ABSTRACT);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
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
	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", 
							      "Connection from which this data model is created", 
							      NULL, GDA_TYPE_CONNECTION,
							      G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_PREP_STMT,
                                         g_param_spec_object ("prepared-stmt", NULL, NULL, GDA_TYPE_PSTMT,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_FLAGS,
					 g_param_spec_uint ("model-usage", NULL, NULL, 
							    GDA_DATA_MODEL_ACCESS_RANDOM, G_MAXUINT,
							    GDA_DATA_MODEL_ACCESS_RANDOM,
							    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_ALL_STORED,
					 g_param_spec_boolean ("store-all-rows", "Store all the rows",
							       "Tells if model has analysed all the rows", FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_PARAMS,
					 g_param_spec_object ("exec-params", NULL, 
							      _("GdaSet used when the SELECT statement was executed"), 
							      GDA_TYPE_SET,
							      G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_INS_QUERY,
                                         g_param_spec_object ("insert_query", "INSERT query", 
							      "INSERT Query to be executed to add data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_UPD_QUERY,
                                         g_param_spec_object ("update_query", "UPDATE query", 
							      "UPDATE Query to be executed to update data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_DEL_QUERY,
                                         g_param_spec_object ("delete_query", "DELETE query", 
							      "DELETE Query to be executed to remove data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

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

	iface->i_set_value_at = gda_pmodel_set_value_at;
	iface->i_set_values = gda_pmodel_set_values;
        iface->i_append_values = gda_pmodel_append_values;
	iface->i_append_row = NULL;
	iface->i_remove_row = gda_pmodel_remove_row;
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
	model->priv->cnc = NULL;
	model->priv->rows = g_array_new (FALSE, FALSE, sizeof (GdaRow *));
	model->priv->index = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->prep_stmt = NULL;
	model->priv->columns = NULL;
	model->advertized_nrows = -1; /* unknown number of rows */

	model->priv->iter_row = G_MININT;
        model->priv->iter = NULL;

	model->priv->modif_set = NULL;
	model->priv->exec_set = NULL;
	model->priv->upd_stmts = NULL;
}

static void
gda_pmodel_dispose (GObject *object)
{
	GdaPModel *model = (GdaPModel *) object;

	g_return_if_fail (GDA_IS_PMODEL (model));

	/* free memory */
	if (model->priv) {
		gint i;

		if (model->priv->cnc) {
			g_object_unref (model->priv->cnc);
			model->priv->cnc = NULL;
		}
		if (model->prep_stmt) {
			g_object_unref (model->prep_stmt);
			model->prep_stmt = NULL;
		}
		if (model->priv->rows) {
			for (i = 0; i < model->priv->rows->len; i++) {
				GdaRow *prow;
				prow = g_array_index (model->priv->rows, GdaRow *, i);
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

		if (model->priv->modif_set) {
			g_object_unref (model->priv->modif_set);
			model->priv->modif_set = NULL;
		}

		if (model->priv->exec_set) {
			g_object_unref (model->priv->exec_set);
			model->priv->exec_set = NULL;
		}

		for (i = FIRST_QUERY; i < NB_QUERIES; i++) {
			if (model->priv->modif_stmts [i]) {
				g_object_unref (model->priv->modif_stmts [i]);
				model->priv->modif_stmts [i] = NULL;
			}
		}
		if (model->priv->upd_stmts) {
			gint j, ncols;
			ncols = gda_pmodel_get_n_columns ((GdaDataModel *) model);
			for (j = 0; j < ncols; j++)
				if (model->priv->upd_stmts [j])
					g_object_unref (model->priv->upd_stmts [j]);
			g_free (model->priv->upd_stmts);
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
		case PROP_CNC:
			model->priv->cnc = g_value_get_object (value);
			if (model->priv->cnc)
				g_object_ref (model->priv->cnc);
			break;
		case PROP_PREP_STMT:
			if (model->prep_stmt)
				g_object_unref (model->prep_stmt);
			model->prep_stmt = g_value_get_object (value);
			if (model->prep_stmt)
				g_object_ref (model->prep_stmt);
			create_columns (model);
			break;
		case PROP_FLAGS: {
			GdaDataModelAccessFlags flags = g_value_get_uint (value);
			if (!(flags & GDA_DATA_MODEL_ACCESS_RANDOM) &&
			    (flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD))
				flags = GDA_DATA_MODEL_ACCESS_CURSOR;
			model->priv->usage_flags = flags;
			break;
		}
		case PROP_ALL_STORED:
			if ((model->advertized_nrows < 0) && CLASS (model)->fetch_nb_rows)
				CLASS (model)->fetch_nb_rows (model);
				
			if (model->nb_stored_rows != model->advertized_nrows) {
				if (CLASS (model)->store_all)
					CLASS (model)->store_all (model, NULL);
			}
			break;
		case PROP_PARAMS: {
			GdaSet *set;
			set = g_value_get_object (value);
			if (set) 
				model->priv->exec_set = gda_set_copy (set);
			break;
		}
		case PROP_INS_QUERY:
			if (model->priv->modif_stmts [INS_QUERY])
				g_object_unref (model->priv->modif_stmts [INS_QUERY]);
			model->priv->modif_stmts [INS_QUERY] = g_value_get_object (value);
			if (model->priv->modif_stmts [INS_QUERY])
				g_object_ref (model->priv->modif_stmts [INS_QUERY]);
			break;
		case PROP_DEL_QUERY:
			if (model->priv->modif_stmts [DEL_QUERY])
				g_object_unref (model->priv->modif_stmts [DEL_QUERY]);
			model->priv->modif_stmts [DEL_QUERY] = g_value_get_object (value);
			if (model->priv->modif_stmts [DEL_QUERY])
				g_object_ref (model->priv->modif_stmts [DEL_QUERY]);
			break;
		case PROP_UPD_QUERY:
			if (model->priv->modif_stmts [UPD_QUERY])
				g_object_unref (model->priv->modif_stmts [UPD_QUERY]);
			model->priv->modif_stmts [UPD_QUERY] = g_value_get_object (value);
			if (model->priv->modif_stmts [UPD_QUERY])
				g_object_ref (model->priv->modif_stmts [UPD_QUERY]);
			break;
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
		case PROP_CNC:
			g_value_set_object (value, model->priv->cnc);
			break;
		case PROP_PREP_STMT:
			g_value_set_object (value, model->prep_stmt);
			break;
		case PROP_FLAGS:
			g_value_set_uint (value, model->priv->usage_flags);
			break;
		case PROP_ALL_STORED:
			if (!model->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
				g_warning ("Cannot set the 'store-all-rows' property when acces mode is cursor based");
			else {
				if ((model->advertized_nrows < 0) && CLASS (model)->fetch_nb_rows)
					CLASS (model)->fetch_nb_rows (model);
				g_value_set_boolean (value, model->nb_stored_rows == model->advertized_nrows);
			}
			break;
		case PROP_PARAMS:
			g_value_set_object (value, model->priv->exec_set);
			break;
		case PROP_INS_QUERY:
			g_value_set_object (value, model->priv->modif_stmts [INS_QUERY]);
			break;
		case PROP_DEL_QUERY:
			g_value_set_object (value, model->priv->modif_stmts [DEL_QUERY]);
			break;
		case PROP_UPD_QUERY:
			g_value_set_object (value, model->priv->modif_stmts [UPD_QUERY]);
			break;
		default:
			break;
		}
	}
}

/**
 * gda_pmodel_take_row
 * @model: a #GdaPModel data model
 * @row: a #GdaRow row
 * @rownum: "external" advertized row number
 *
 * Stores @row into @model, externally advertized at row number @rownum. The reference to
 * @row is stolen.
 */
void
gda_pmodel_take_row (GdaPModel *model, GdaRow *row, gint rownum)
{
	g_return_if_fail (GDA_IS_PMODEL (model));
	g_return_if_fail (GDA_IS_ROW (row));

	if (g_hash_table_lookup (model->priv->index, GINT_TO_POINTER (rownum + 1))) 
		g_error ("INTERNAL error: row %d already exists, aborting", rownum);

	g_hash_table_insert (model->priv->index, GINT_TO_POINTER (rownum + 1), GINT_TO_POINTER (model->priv->rows->len + 1));
	g_array_append_val (model->priv->rows, row);
	model->nb_stored_rows = model->priv->rows->len;
}

/**
 * gda_pmodel_get_stored_row
 * @model: a #GdaPModel data model
 * @rownum: "external" advertized row number
 *
 * Get the #GdaRow object stored within @model at row @rownum
 *
 * Returns: the requested #GdaRow, or %NULL if not found
 */
GdaRow *
gda_pmodel_get_stored_row (GdaPModel *model, gint rownum)
{
	gint irow;
	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (model->priv->index, GINT_TO_POINTER (rownum + 1)));
	if (irow <= 0) 
		return NULL;
	else 
		return g_array_index (model->priv->rows, GdaRow *, irow - 1);
}

/**
 * gda_pmodel_get_connection
 * @model: a #GdaPModel data model
 *
 * Get a pointer to the #GdaConnection object which was used when @model was created
 * (and which may be used internally by @model).
 *
 * Returns: a pointer to the #GdaConnection, or %NULL
 */
GdaConnection *
gda_pmodel_get_connection (GdaPModel *model)
{
	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	return model->priv->cnc;
}

/*
 * Add the +/-<col num> holders to model->priv->modif_set
 */
static gboolean
compute_modif_set (GdaPModel *model, GError **error)
{
	gint i;
	
	if (model->priv->modif_set)
		g_object_unref (model->priv->modif_set);
	if (model->priv->exec_set)
		model->priv->modif_set = gda_set_copy (model->priv->exec_set);
	else
		model->priv->modif_set = gda_set_new (NULL);

	for (i = 0; i < NB_QUERIES; i++) {
		GdaSet *set;
		if (! model->priv->modif_stmts [i])
			continue;
		if (! gda_statement_get_parameters (model->priv->modif_stmts [i], &set, error)) {
			g_object_unref (model->priv->modif_set);
			model->priv->modif_set = NULL;
			return FALSE;
		}

		gda_set_merge_with_set (model->priv->modif_set, set);
		g_object_unref (set);
	}

#ifdef GDA_DEBUG_NO
	GSList *list;
	g_print ("-------\n");
	for (list = model->priv->modif_set->holders; list; list = list->next) {
		GdaHolder *h = GDA_HOLDER (list->data);
		g_print ("=> holder '%s'\n", gda_holder_get_id (h));
	}
#endif

	return TRUE;
}

/*
 * converts "[+-]<num>" to <num> and returns FALSE if @pname is not like
 * "[+-]<num>". <num> is stored in @result, and the +/- signed is stored in @old_val
 * (@old_val is set to TRUE if there is a "-")
 */
static gboolean
param_name_to_int (const gchar *pname, gint *result, gboolean *old_val)
{
	gint sum = 0;
	const gchar *ptr;

	if (!pname || ((*pname != '-') && (*pname != '+')))
		return FALSE;
	
	ptr = pname + 1;
	while (*ptr) {
		if ((*ptr > '9') || (*ptr < '0'))
			return FALSE;
		sum = sum * 10 + *ptr - '0';
		ptr++;
	}
	
	if (result) 
		*result = sum;
	if (old_val)
		*old_val = (*pname == '-') ? TRUE : FALSE;
	
	return TRUE;
}

/**
 * gda_pmodel_set_modification_query
 * @model: a #GdaPModel data model
 * @mod_stmt: a #GdaStatement (INSERT, UPDATE or DELETE)
 * @error: a place to store errors, or %NULL
 *
 * Forces @model to allow data modification using @mod_stmt as the statement executed when the corresponding
 * modification is requested
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_pmodel_set_modification_statement (GdaPModel *model, GdaStatement *mod_stmt, GError **error)
{
	ModType mtype = NB_QUERIES;

	g_return_val_if_fail (GDA_IS_PMODEL (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (mod_stmt), FALSE);

	switch (gda_statement_get_statement_type (mod_stmt)) {
	case GDA_SQL_STATEMENT_INSERT:
		mtype = INS_QUERY;
		break;
	case GDA_SQL_STATEMENT_DELETE:
		mtype = DEL_QUERY;
		break;
	case GDA_SQL_STATEMENT_UPDATE:
		mtype = UPD_QUERY;
		break;
	default:
		break;
	}
	
	if (mtype != NB_QUERIES) {
		if (! gda_sql_statement_check_structure (mod_stmt, error))
			return FALSE;

		if (model->priv->modif_stmts[mtype]) {
			g_object_unref (model->priv->modif_stmts[mtype]);
			model->priv->modif_stmts[mtype] = NULL;
		}

		/* prepare model->priv->modif_set */
		if (!compute_modif_set (model, error))
			return FALSE;

		/* check that all the parameters required to execute @mod_stmt are in model->priv->modif_set */
		GdaSet *params;
		GSList *list;
		if (! gda_statement_get_parameters (mod_stmt, &params, error))
			return FALSE;
		for (list = params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			GdaHolder *eholder;
			eholder = gda_set_get_holder (model->priv->modif_set, gda_holder_get_id (holder));
			if (!eholder) {
				gint num;
				gboolean is_old;

				if (!param_name_to_int (gda_holder_get_id (holder), &num, &is_old)) {
					g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MODIFICATION_STATEMENT_ERROR,
						     _("Modification statement uses an unknown '%s' parameter"),
						     gda_holder_get_id (holder));
					g_object_unref (params);
					return FALSE;
				}
				if (num > gda_pmodel_get_n_columns ((GdaDataModel*) model)) {
					g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
						     _("Column %d out of range (0-%d)"), num, 
						     gda_pmodel_get_n_columns ((GdaDataModel*) model)-1);
					g_object_unref (params);
					return FALSE;
				}
				gda_set_add_holder (model->priv->modif_set, holder);
			}
			else if (gda_holder_get_g_type (holder) != gda_holder_get_g_type (eholder)) {
				g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MODIFICATION_STATEMENT_ERROR,
					     _("Modification statement's  '%s' parameter is a %s when it should be a %s"),
					     gda_holder_get_id (holder),
					     gda_g_type_to_string (gda_holder_get_g_type (holder)),
					     gda_g_type_to_string (gda_holder_get_g_type (eholder)));
				g_object_unref (params);
				return FALSE;
			}
		}
		g_object_unref (params);

		model->priv->modif_stmts[mtype] = mod_stmt;
		g_object_ref (mod_stmt);
	}
	else {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MODIFICATION_STATEMENT_ERROR,
			     _("Modification statement must be an INSERT, UPDATE or DELETE statement"));
		return FALSE;
	}

#ifdef GDA_DEBUG_NO
	GSList *hlist;
	g_print ("SET MODIF QUERY\n");
	if (model->priv->modif_set) {
		for (hlist = model->priv->modif_set->holders; hlist; hlist = hlist->next) {
			GdaHolder *h = GDA_HOLDER (hlist->data);
			g_print ("  %s type=> %s (%d)\n", gda_holder_get_id (h), g_type_name (gda_holder_get_g_type (h)),
				 gda_holder_get_g_type (h));
		}
	}
#endif

	return TRUE;
}

/**
 * gda_pmodel_compute_modification_queries
 * @model: a #GdaPModel data model
 * @require_pk: FALSE if all fields can be used in the WHERE condition when no primary key exists
 * @error: a place to store errors, or %NULL
 *
 * Makes @model try to compute INSERT, UPDATE and DELETE statements to be used when modifying @model's contents.
 * Note: any modification statement set using gda_pmodel_set_modification_statement() will first be unset
 *
 * Returns: TRUE if no error occurred. If FALSE is returned, then some modification statement may still have been
 * computed
 */
gboolean
gda_pmodel_compute_modification_statements (GdaPModel *model, gboolean require_pk, GError **error)
{
	GdaStatement *stmt;
	ModType mtype;
	g_return_val_if_fail (GDA_IS_PMODEL (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	stmt = gda_pstmt_get_gda_statement (model->prep_stmt);
	if (!stmt) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("No SELECT statement to use"));
		return FALSE;
	}
	if (!model->priv->cnc) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_CONNECTION_ERROR,
			     _("No connection to use"));
		return FALSE;
	}
	for (mtype = FIRST_QUERY; mtype < NB_QUERIES; mtype++)
		if (model->priv->modif_stmts[mtype]) {
			g_object_unref (model->priv->modif_stmts[mtype]);
			model->priv->modif_stmts[mtype] = NULL;
		}
	return gda_compute_dml_statements (model->priv->cnc, stmt, require_pk,
					   &(model->priv->modif_stmts[INS_QUERY]),
					   &(model->priv->modif_stmts[UPD_QUERY]),
					   &(model->priv->modif_stmts[DEL_QUERY]), error);
	if (!compute_modif_set (model, error)) {
		for (mtype = FIRST_QUERY; mtype < NB_QUERIES; mtype++)
			if (model->priv->modif_stmts[mtype]) {
				g_object_unref (model->priv->modif_stmts[mtype]);
				model->priv->modif_stmts[mtype] = NULL;
			}
		return FALSE;
	}
	return TRUE;
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
	GdaDataModelAccessFlags flags = 0;

	g_return_val_if_fail (GDA_IS_PMODEL (model), 0);
	imodel = GDA_PMODEL (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		flags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD) {
		if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
			flags = GDA_DATA_MODEL_ACCESS_CURSOR;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;
	}

	if (imodel->priv->modif_stmts [UPD_QUERY])
		flags |= GDA_DATA_MODEL_ACCESS_UPDATE;
	if (imodel->priv->modif_stmts [INS_QUERY])
		flags |= GDA_DATA_MODEL_ACCESS_INSERT;
	if (imodel->priv->modif_stmts [DEL_QUERY])
		flags |= GDA_DATA_MODEL_ACCESS_DELETE;

	return flags;
}

static const GValue *
gda_pmodel_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaRow *prow;
	gint irow, nrows;
	GdaPModel *imodel;

	g_return_val_if_fail (GDA_IS_PMODEL (model), NULL);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, NULL);

	/* available only if GDA_DATA_MODEL_ACCESS_RANDOM */
	if (! (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM))
		return NULL;

	/* check row number validity */
	nrows = imodel->advertized_nrows < 0 ? gda_pmodel_get_n_rows (model) : imodel->advertized_nrows;
	if ((row < 0) || ((nrows >= 0) && (row >= nrows)))
		return NULL;

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->index, GINT_TO_POINTER (row + 1)));
	if (irow <= 0) {
		prow = NULL;
		if (CLASS (model)->fetch_random) 
			CLASS (model)->fetch_random (imodel, &prow, row, NULL);
	}
	else 
		prow = g_array_index (imodel->priv->rows, GdaRow *, irow - 1);
	
	if (prow) 
		return gda_row_get_value (prow, col);

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
	
	if (! imodel->priv->modif_stmts [UPD_QUERY])
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

static void update_iter (GdaPModel *imodel, GdaRow *prow);
static gboolean
gda_pmodel_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaPModel *imodel;
	GdaRow *prow = NULL;
	gint target_iter_row;
	gint irow;

	g_return_val_if_fail (GDA_IS_PMODEL (model), FALSE);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, FALSE);
	g_return_val_if_fail (CLASS (model)->fetch_next, FALSE);

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

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->index, GINT_TO_POINTER (target_iter_row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->rows, GdaRow *, irow - 1);
	if (!CLASS (model)->fetch_next (imodel, &prow, target_iter_row, NULL))
		TO_IMPLEMENT;
	
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
	GdaRow *prow = NULL;
	gint target_iter_row;
	gint irow;

	g_return_val_if_fail (GDA_IS_PMODEL (model), FALSE);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, FALSE);
	g_return_val_if_fail (CLASS (model)->fetch_prev, FALSE);

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

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->index, GINT_TO_POINTER (target_iter_row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->rows, GdaRow *, irow - 1);
	if (!CLASS (model)->fetch_prev (imodel, &prow, target_iter_row, NULL))
		TO_IMPLEMENT;

	if (prow) {
		imodel->priv->iter_row = target_iter_row;
                update_iter (imodel, prow);
                return TRUE;
	}

 prev_error:
        g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
        imodel->priv->iter_row = G_MININT;
        return FALSE;
}

static gboolean
gda_pmodel_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	GdaPModel *imodel;
	GdaRow *prow = NULL;
	gint irow;

	g_return_val_if_fail (GDA_IS_PMODEL (model), FALSE);
	imodel = (GdaPModel *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_move_iter_at_row_default (model, iter, row);

        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->index, GINT_TO_POINTER (row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->rows, GdaRow *, irow - 1);

	if (CLASS (model)->fetch_at) {
		if (!CLASS (model)->fetch_at (imodel, &prow, row, NULL))
			TO_IMPLEMENT;
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
		if (prow) {
			imodel->priv->iter_row = row;
			update_iter (imodel, prow);
			return TRUE;
		}
		else {
			/* implementation of fetch_at() is optional */
			TO_IMPLEMENT; /* iter back or forward the right number of times */
			return FALSE;
		}
	}
}

static void
update_iter (GdaPModel *imodel, GdaRow *prow)
{
        gint i;
	GdaDataModelIter *iter = imodel->priv->iter;
	GSList *plist;
	gboolean update_model;
	
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	if (update_model)
		g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	
	for (i = 0, plist = GDA_SET (iter)->holders; 
	     plist;
	     i++, plist = plist->next) {
		const GValue *value;
		value = gda_row_get_value (prow, i);
		gda_holder_set_value ((GdaHolder*) plist->data, value);
        }

	g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row, NULL);
	if (update_model)
		g_object_set (G_OBJECT (iter), "update_model", update_model, NULL);
}

/*
 * creates a derivative of the model->priv->modif_stmts [UPD_QUERY] statement
 * where only the column @col is updated.
 *
 * Returns: a new #GdaStatement, or %NULL
 */
static GdaStatement *
compute_single_update_stmt (GdaPModel *model, gint col, GError **error)
{
	GdaSqlStatement *sqlst;
	GdaSqlStatementUpdate *upd;
	GdaStatement *updstmt = NULL;

	/* get a copy of complete UPDATE stmt */
	g_assert (model->priv->modif_stmts [UPD_QUERY]);
	g_object_get (G_OBJECT (model->priv->modif_stmts [UPD_QUERY]), "structure", &sqlst, NULL);
	g_assert (sqlst);
	g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_UPDATE);
	upd = (GdaSqlStatementUpdate*) sqlst->contents;
	
	/* remove non necessary fields to update */
	GSList *elist, *flist;
	gboolean field_found = FALSE;
	for (elist = upd->expr_list, flist = upd->fields_list; elist && flist; ) {
		GdaSqlExpr *expr = (GdaSqlExpr *) elist->data;
		gint num;
		gboolean old_val;
		if (! expr->param_spec || !param_name_to_int (expr->param_spec->name, &num, &old_val) || old_val) {
			/* ignore this field to be updated */
			elist = elist->next;
			flist = flist->next;
			continue;
		}
		if (num == col) {
			/* this field is the field to be updated */
			field_found = TRUE;
			elist = elist->next;
			flist = flist->next;
			continue;
		}
		/* remove that field */
		GSList *nelist, *nflist;
		nelist = elist->next;
		nflist = flist->next;
		gda_sql_expr_free (expr);
		gda_sql_field_free ((GdaSqlField*) flist->data);

		upd->expr_list = g_slist_delete_link (upd->expr_list, elist);
		upd->fields_list = g_slist_delete_link (upd->fields_list, flist);
		elist = nelist;
		flist = nflist;
	}

	/* create a new GdaStatement */
	if (field_found)
		updstmt = (GdaStatement *) g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
	else 
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d can't be modified"), col);
	gda_sql_statement_free (sqlst);

#ifdef GDA_DEBUG_NO
	if (updstmt) {
		gchar *sql;
		sql = gda_statement_serialize (updstmt);
		g_print ("UPDATE for col %d => %s\n", col, sql);
		g_free (sql);
	}
	else
		g_print ("UPDATE for col %d: ERROR\n", col);
#endif

	return updstmt;
}

static gboolean
gda_pmodel_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	GdaPModel *imodel;
	gint i, ncols;
	GdaHolder *holder;
	gchar *str;

	imodel = (GdaPModel *) model;

	g_return_val_if_fail (imodel->priv, FALSE);

	if (! (imodel->priv->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     _("Data model does not support random access"));
		return FALSE;
	}
	if (! imodel->priv->modif_stmts [UPD_QUERY]) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("No UPDATE statement provided"));
		return FALSE;
	}

	if (row >= gda_pmodel_get_n_rows (model)) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Row %d out of range (0-%d)"), row, gda_pmodel_get_n_rows (model)-1);
		return FALSE;
	}

	ncols = gda_pmodel_get_n_columns (model);
	if (col >= ncols) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d out of range (0-%d)"), col, ncols-1);
		return FALSE;
	}

	if (! imodel->priv->upd_stmts)
		imodel->priv->upd_stmts = g_new0 (GdaStatement *, ncols);
	if (! imodel->priv->upd_stmts [col]) {
		imodel->priv->upd_stmts [col] = compute_single_update_stmt (imodel, col, error);
		if (!imodel->priv->upd_stmts [col])
			return FALSE;
	}
	
	str = g_strdup_printf ("+%d", col);
	holder = gda_set_get_holder (imodel->priv->modif_set, str);
	g_free (str);
	if (! holder) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d can't be modified"), col);
		return FALSE;
	}
	if (! gda_holder_set_value (holder, value)) {
		g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_VALUE_ERROR,
			     _("Wrong value type"));
		return FALSE;
	}

	for (i = 0; i < ncols; i++) {
		str = g_strdup_printf ("-%d", i);
		holder = gda_set_get_holder (imodel->priv->modif_set, str);
		g_free (str);
		if (holder) {
			if (! gda_holder_set_value (holder,
						    gda_data_model_get_value_at (model, i, row))) {
				g_set_error (error, GDA_PMODEL_ERROR, GDA_PMODEL_VALUE_ERROR,
					     _("Wrong value type"));
				return FALSE;
			}
		}
	}

#ifdef GDA_DEBUG_NO
	gchar *sql;
	GError *lerror = NULL;
	sql = gda_statement_to_sql_extended (imodel->priv->upd_stmts [col],
					     imodel->priv->cnc,
					     imodel->priv->modif_set, 
					     GDA_STATEMENT_SQL_PRETTY, NULL,
					     &lerror);
	g_print ("%s(): SQL=> %s\n", __FUNCTION__, sql);
	if (!sql)
		g_print ("\tERR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
	g_free (sql);
#endif

	if (gda_connection_statement_execute_non_select (imodel->priv->cnc, 
							 imodel->priv->upd_stmts [col],
							 imodel->priv->modif_set, NULL, error) == -1)
		return FALSE;
	
	TO_IMPLEMENT;

	return TRUE;
}

static gboolean
gda_pmodel_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaPModel *imodel;
	imodel = (GdaPModel *) model;

	g_return_val_if_fail (imodel->priv, FALSE);
	TO_IMPLEMENT;

	return FALSE;
}

static gint
gda_pmodel_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaPModel *imodel;
	imodel = (GdaPModel *) model;

	g_return_val_if_fail (imodel->priv, -1);
	TO_IMPLEMENT;

	return -1;
}

static gboolean
gda_pmodel_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaPModel *imodel;
	imodel = (GdaPModel *) model;

	g_return_val_if_fail (imodel->priv, FALSE);
	TO_IMPLEMENT;

	return FALSE;
}
