/* GDA library
 * Copyright (C) 2008 - 2009 The GNOME Foundation.
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

#undef GDA_DISABLE_DEPRECATED
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-iter-extra.h>
#include <libgda/gda-data-model-private.h>
#include <string.h>
#include "gda-data-select.h"
#include "providers-support/gda-data-select-priv.h"
#include "gda-data-select-extra.h"
#include "gda-row.h"
#include "providers-support/gda-pstmt.h"
#include <libgda/gda-statement.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-parser.h>

#define CLASS(x) (GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (x)))

typedef struct {
	GdaStatement *select;
	GdaSet       *params;
	GdaRow       *row; /* non NULL once @select has been executed */
} DelayedSelectStmt;

static void delayed_select_stmt_free (DelayedSelectStmt *dstmt);

/*
 * To go from an "external" row number to an "internal" row number and then to a GdaRow, 
 * the following steps are required:
 *  - use @del_rows to determine if the row has been removed (if it's in the array), and otherwise
 *    get an "internal" row number
 *  - use the @upd_rows index to see if the row has been modified, and if it has, then use
 *    the associated DelayedSelectStmt to retrieve the GdaRow
 *  - use the virtual methods to actually retrieve the requested GdaRow
 */
static gint external_to_internal_row (GdaDataSelect *model, gint ext_row, GError **error);

typedef struct {
	GSList                 *columns; /* list of GdaColumn objects */
	GArray                 *rows; /* Array of GdaRow pointers */
	GHashTable             *index; /* key = model row number + 1, value = index in @rows array + 1*/

	/* Internal iterator's information, if GDA_DATA_MODEL_CURSOR_* based access */
        gint                    iter_row; /* G_MININT if at start, G_MAXINT if at end, "external" row number */

	GdaStatement           *sel_stmt;
	GdaSet                 *ext_params;
	gboolean                reset_with_ext_params_change;
	GdaDataModelAccessFlags usage_flags;
	
	/* attributes specific to data model modifications */
	GdaDataSelectInternals  *modif_internals;

	/* Global overriding data when the data model has been modified */
	GArray                 *del_rows; /* array[index] = number of the index'th deleted row,
					   * sorted by row number (row numbers are internal row numbers )*/
	GHashTable             *upd_rows; /* key = internal row number + 1, value = a DelayedSelectStmt pointer */

	gboolean                ref_count; /* when drop to 0 => free can be done */
} PrivateShareable;

/*
 * Getting a GdaRow from a model row:
 * [model row] ==(model->index)==> [index in model->rows] ==(model->rows)==> [GdaRow pointer]
 */
struct _GdaDataSelectPrivate {
	GdaConnection          *cnc;
        GdaDataModelIter       *iter;
	PrivateShareable       *sh;
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
	PROP_DEL_QUERY,
	PROP_SEL_STMT,
	PROP_RESET_WITH_EXT_PARAM
};

/* module error */
GQuark gda_data_select_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_data_select_error");
        return quark;
}

static void gda_data_select_class_init (GdaDataSelectClass *klass);
static void gda_data_select_init       (GdaDataSelect *model, GdaDataSelectClass *klass);
static void gda_data_select_dispose    (GObject *object);
static void gda_data_select_finalize   (GObject *object);

static void gda_data_select_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gda_data_select_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* utility functions */
typedef struct {
	gint    size; /* number of elements in the @data array */
	guchar *data; /* data[0] to data[@size-1] are valid */
} BVector;
static GdaStatement *check_acceptable_statement (GdaDataSelect *model, GError **error);
static GdaStatement *compute_single_update_stmt (GdaDataSelect *model, BVector *bv, GError **error);
static GdaStatement *compute_single_insert_stmt (GdaDataSelect *model, BVector *bv, GError **error);
static GdaStatement *compute_single_select_stmt (GdaDataSelect *model, GError **error);
static gint *compute_insert_select_params_mapping (GdaSet *sel_params, GdaSet *ins_values, GdaSqlExpr *row_cond);

static void ext_params_holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataSelect *model);


/* GdaDataModel interface */
static void                 gda_data_select_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_select_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_select_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_select_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_select_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_select_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_select_get_attributes_at (GdaDataModel *model, gint col, gint row);

static GdaDataModelIter    *gda_data_select_create_iter     (GdaDataModel *model);
static gboolean             gda_data_select_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_data_select_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_data_select_iter_at_row     (GdaDataModel *model, GdaDataModelIter *iter, gint row);

static gboolean             gda_data_select_set_value_at    (GdaDataModel *model, gint col, gint row, 
							     const GValue *value, GError **error);
static gboolean             gda_data_select_iter_set_value  (GdaDataModel *model, GdaDataModelIter *iter, gint col,
							     const GValue *value, GError **error);
static gboolean             gda_data_select_set_values      (GdaDataModel *model, gint row, GList *values,
							     GError **error);
static gint                 gda_data_select_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gboolean             gda_data_select_remove_row      (GdaDataModel *model, gint row, GError **error);

static GObjectClass *parent_class = NULL;

/**
 * gda_data_select_get_type
 *
 * Returns: the #GType of GdaDataSelect.
 */
GType
gda_data_select_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaDataSelectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_select_class_init,
			NULL,
			NULL,
			sizeof (GdaDataSelect),
			0,
			(GInstanceInitFunc) gda_data_select_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_select_data_model_init,
			NULL,
			NULL
		};

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataSelect", &info, G_TYPE_FLAG_ABSTRACT);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_data_select_class_init (GdaDataSelectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_data_select_set_property;
        object_class->get_property = gda_data_select_get_property;
	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL,
							      "Connection from which this data model is created", 
							      GDA_TYPE_CONNECTION,
							      G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_PREP_STMT,
                                         g_param_spec_object ("prepared-stmt", NULL, 
							      "Associated prepared statement (for internal usage)", 
							      GDA_TYPE_PSTMT,
							      G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_FLAGS,
					 g_param_spec_uint ("model-usage", NULL, 
							    "Determines how the data model may be used", 
							    GDA_DATA_MODEL_ACCESS_RANDOM, G_MAXUINT,
							    GDA_DATA_MODEL_ACCESS_RANDOM,
							    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_ALL_STORED,
					 g_param_spec_boolean ("store-all-rows", "Store all the rows",
							       "Tells if model has analyzed all the rows", FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_PARAMS,
					 g_param_spec_object ("exec-params", NULL, 
							      "GdaSet used when the SELECT statement was executed", 
							      GDA_TYPE_SET,
							      G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_INS_QUERY,
                                         g_param_spec_object ("insert-stmt", "INSERT statement", 
							      "INSERT Statement to be executed to add data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_UPD_QUERY,
                                         g_param_spec_object ("update-stmt", "UPDATE statement", 
							      "UPDATE Statement to be executed to update data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_DEL_QUERY,
                                         g_param_spec_object ("delete-stmt", "DELETE statement", 
							      "DELETE Statement to be executed to remove data",
							      GDA_TYPE_STATEMENT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_SEL_STMT,
					 g_param_spec_object ("select-stmt", "SELECT statement",
							      "SELECT statement which was executed to yield to the data model",
							      GDA_TYPE_STATEMENT, G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_RESET_WITH_EXT_PARAM,
					 g_param_spec_boolean ("auto-reset", "Automatically reset itself",
							       "Automatically re-run the SELECT statement if any parameter "
							       "has chanegd since it was first executed", FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
	object_class->dispose = gda_data_select_dispose;
	object_class->finalize = gda_data_select_finalize;
}

static void
gda_data_select_data_model_init (GdaDataModelIface *iface)
{
	iface->i_get_n_rows = gda_data_select_get_n_rows;
	iface->i_get_n_columns = gda_data_select_get_n_columns;
	iface->i_describe_column = gda_data_select_describe_column;
        iface->i_get_access_flags = gda_data_select_get_access_flags;
	iface->i_get_value_at = gda_data_select_get_value_at;
	iface->i_get_attributes_at = gda_data_select_get_attributes_at;

	iface->i_create_iter = gda_data_select_create_iter;
        iface->i_iter_at_row = gda_data_select_iter_at_row;
        iface->i_iter_next = gda_data_select_iter_next;
        iface->i_iter_prev = gda_data_select_iter_prev;

	iface->i_set_value_at = gda_data_select_set_value_at;
	iface->i_iter_set_value = gda_data_select_iter_set_value;
	iface->i_set_values = gda_data_select_set_values;
        iface->i_append_values = gda_data_select_append_values;
	iface->i_append_row = NULL;
	iface->i_remove_row = gda_data_select_remove_row;
	iface->i_find_row = NULL;
	
	iface->i_set_notify = NULL;
	iface->i_get_notify = NULL;
	iface->i_send_hint = NULL;
}

static void
gda_data_select_init (GdaDataSelect *model, GdaDataSelectClass *klass)
{
	ModType i;
	g_return_if_fail (GDA_IS_DATA_SELECT (model));
	model->priv = g_new0 (GdaDataSelectPrivate, 1);
	model->priv->cnc = NULL;
	model->priv->sh = g_new0 (PrivateShareable, 1);
	model->priv->sh->rows = g_array_new (FALSE, FALSE, sizeof (GdaRow *));
	model->priv->sh->index = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->prep_stmt = NULL;
	model->priv->sh->columns = NULL;
	model->nb_stored_rows = 0;
	model->advertized_nrows = -1; /* unknown number of rows */

	model->priv->sh->sel_stmt = NULL;
	model->priv->sh->ext_params = NULL;
	model->priv->sh->reset_with_ext_params_change = FALSE;

	model->priv->sh->iter_row = G_MININT;
        model->priv->iter = NULL;

	model->priv->sh->modif_internals = g_new0 (GdaDataSelectInternals, 1);
	model->priv->sh->modif_internals->safely_locked = FALSE;
	model->priv->sh->modif_internals->unique_row_condition = NULL;
	model->priv->sh->modif_internals->insert_to_select_mapping = NULL;
	model->priv->sh->modif_internals->modif_set = NULL;
	model->priv->sh->modif_internals->exec_set = NULL;
	for (i = FIRST_QUERY; i < NB_QUERIES; i++)
		model->priv->sh->modif_internals->modif_stmts[i] = NULL;
	model->priv->sh->modif_internals->upd_stmts = NULL;
	model->priv->sh->modif_internals->ins_stmts = NULL;
	model->priv->sh->modif_internals->one_row_select_stmt = NULL;

	model->priv->sh->upd_rows = NULL;
	model->priv->sh->del_rows = NULL;

	model->priv->sh->ref_count = 1;
}

static void
ext_params_holder_changed_cb (GdaSet *paramlist, GdaHolder *param, GdaDataSelect *model)
{
	if (model->priv->sh->reset_with_ext_params_change) {
		GError *error = NULL;
		if (! gda_data_select_rerun (model, &error)) {
			g_warning (_("Could not re-run SELECT statement: %s"),
				   error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
	}
}

static void
free_private_shared_data (GdaDataSelect *model)
{
	if (!model->priv->sh)
		return;
	model->priv->sh->ref_count --;
	if (model->priv->sh->ref_count == 0) {
		gint i;

		if (model->priv->sh->sel_stmt) {
			g_object_unref (model->priv->sh->sel_stmt);
			model->priv->sh->sel_stmt = NULL;
		}

		if (model->priv->sh->ext_params) {
			g_signal_handlers_disconnect_by_func (model->priv->sh->ext_params,
							      G_CALLBACK (ext_params_holder_changed_cb), model);
			g_object_unref (model->priv->sh->ext_params);
			model->priv->sh->ext_params = NULL;
		}

		if (model->priv->sh->modif_internals) {
			_gda_data_select_internals_free (model->priv->sh->modif_internals);
			model->priv->sh->modif_internals = NULL;
		}

		if (model->priv->sh->upd_rows) {
			g_hash_table_destroy (model->priv->sh->upd_rows);
			model->priv->sh->upd_rows = NULL;
		}

		if (model->priv->sh->del_rows) {
			g_array_free (model->priv->sh->del_rows, TRUE);
			model->priv->sh->del_rows = NULL;
		}
		if (model->priv->sh->rows) {
			for (i = 0; i < model->priv->sh->rows->len; i++) {
				GdaRow *prow;
				prow = g_array_index (model->priv->sh->rows, GdaRow *, i);
				g_object_unref (prow);
			}
			g_array_free (model->priv->sh->rows, TRUE);
			model->priv->sh->rows = NULL;
		}
		if (model->priv->sh->index) {
			g_hash_table_destroy (model->priv->sh->index);
			model->priv->sh->index = NULL;
		}
		if (model->priv->sh->columns) {
			g_slist_foreach (model->priv->sh->columns, (GFunc) g_object_unref, NULL);
			g_slist_free (model->priv->sh->columns);
			model->priv->sh->columns = NULL;
		}

		g_free (model->priv->sh);
	}
	model->priv->sh = NULL;
}

static void
gda_data_select_dispose (GObject *object)
{
	GdaDataSelect *model = (GdaDataSelect *) object;

	g_return_if_fail (GDA_IS_DATA_SELECT (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->cnc) {
			g_object_unref (model->priv->cnc);
			model->priv->cnc = NULL;
		}
		if (model->priv->iter) {
			g_object_unref (model->priv->iter);
			model->priv->iter = NULL;
		}
		if (model->prep_stmt) {
			g_object_unref (model->prep_stmt);
			model->prep_stmt = NULL;
		}
		free_private_shared_data (model);
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/*
 * Allows 2 GdaDataSelect objects to safely share the same private data (PrivateShareable pointer).
 * NOTE: nothing is done to prevent the master and the slave to modify the provate data at the same
 *       time: this must be done by the user implementing the GdaDataSelect objects.
 *
 * This API is used by the GdaThreadRecordset object
 *
 * On the master side (the one from which the private data is shared), nothing special happens, except
 * that master->priv->sh->ref_count is increased by 1.
 *
 * On the slave side, what happens is:
 *    - "free" slave->priv->sh
 *    - slave->priv->sh = master->priv->sh
 */
void
_gda_data_select_share_private_data (GdaDataSelect *master, GdaDataSelect *slave)
{
	master->priv->sh->ref_count ++;
	free_private_shared_data (slave);
	slave->priv->sh = master->priv->sh;
}

void
_gda_data_select_internals_free (GdaDataSelectInternals *inter)
{
	ModType i;

	if (inter->unique_row_condition) {
		gda_sql_expr_free (inter->unique_row_condition);
		inter->unique_row_condition = NULL;
	}
	
	if (inter->insert_to_select_mapping) {
		g_free (inter->insert_to_select_mapping);
		inter->insert_to_select_mapping = NULL;
	}

	if (inter->modif_set) {
		g_object_unref (inter->modif_set);
		inter->modif_set = NULL;
	}
	
	if (inter->exec_set) {
		g_object_unref (inter->exec_set);
		inter->exec_set = NULL;
	}
	
	for (i = FIRST_QUERY; i < NB_QUERIES; i++) {
		if (inter->modif_stmts [i]) {
			g_object_unref (inter->modif_stmts [i]);
			inter->modif_stmts [i] = NULL;
		}
	}
	if (inter->upd_stmts) {
		g_hash_table_destroy (inter->upd_stmts);
		inter->upd_stmts = NULL;
	}
	if (inter->ins_stmts) {
		g_hash_table_destroy (inter->ins_stmts);
		inter->ins_stmts = NULL;
	}

	if (inter->one_row_select_stmt) {
		g_object_unref (inter->one_row_select_stmt);
		inter->one_row_select_stmt = NULL;
	}

	g_free (inter);
}

static void
gda_data_select_finalize (GObject *object)
{
	GdaDataSelect *model = (GdaDataSelect *) object;

	g_return_if_fail (GDA_IS_DATA_SELECT (model));

	/* free memory */
	if (model->priv) {
		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaDataSelectInternals *
_gda_data_select_internals_steal (GdaDataSelect *model)
{
	GdaDataSelectInternals *inter;
	inter = model->priv->sh->modif_internals;
	model->priv->sh->modif_internals = NULL;

	return inter;
}

void
_gda_data_select_internals_paste (GdaDataSelect *model, GdaDataSelectInternals *inter)
{
	if (model->priv->sh->modif_internals)
		_gda_data_select_internals_free (model->priv->sh->modif_internals);
	model->priv->sh->modif_internals = inter;
}

static void
create_columns (GdaDataSelect *model) 
{
	gint i;
	if (model->priv->sh->columns) {
		g_slist_foreach (model->priv->sh->columns, (GFunc) g_object_unref, NULL);
		g_slist_free (model->priv->sh->columns);
		model->priv->sh->columns = NULL;
	}
	if (!model->prep_stmt)
		return;

	if (model->prep_stmt->ncols < 0)
		g_error ("INTERNAL implementation error: unknown number of columns in GdaPStmt, \n"
			 "set number of columns before using with GdaDataSelect");
	if (model->prep_stmt->tmpl_columns) {
		/* copy template columns */
		GSList *list;
		for (list = model->prep_stmt->tmpl_columns; list; list = list->next)
			model->priv->sh->columns = g_slist_append (model->priv->sh->columns, g_object_ref (list->data));
	}
	else {
		/* create columns */
		for (i = 0; i < model->prep_stmt->ncols; i++) {
			GdaColumn *gda_col;
			gda_col = gda_column_new ();
			if (model->prep_stmt->types) 
				gda_column_set_g_type (gda_col, model->prep_stmt->types [i]);
			model->priv->sh->columns = g_slist_append (model->priv->sh->columns, gda_col);
		}
	}
}

static void
gda_data_select_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdaDataSelect *model = (GdaDataSelect *) object;
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
			if (model->prep_stmt) {
				GdaStatement *sel_stmt;
				g_object_ref (model->prep_stmt);
				sel_stmt = gda_pstmt_get_gda_statement (model->prep_stmt);
				if (sel_stmt &&
				    gda_statement_get_statement_type (sel_stmt) == GDA_SQL_STATEMENT_SELECT) 
					model->priv->sh->sel_stmt = gda_statement_copy (sel_stmt);
			}
			create_columns (model);
			break;
		case PROP_FLAGS: {
			GdaDataModelAccessFlags flags = g_value_get_uint (value);
			if (!(flags & GDA_DATA_MODEL_ACCESS_RANDOM) &&
			    (flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD))
				flags = GDA_DATA_MODEL_ACCESS_CURSOR;
			model->priv->sh->usage_flags = flags;
			break;
		}
		case PROP_ALL_STORED:
			if (g_value_get_boolean (value)) {
				if ((model->advertized_nrows < 0) && CLASS (model)->fetch_nb_rows)
					CLASS (model)->fetch_nb_rows (model);
				
				if (model->nb_stored_rows != model->advertized_nrows) {
					if (CLASS (model)->store_all)
						CLASS (model)->store_all (model, NULL);
				}
			}
			break;
		case PROP_PARAMS: {
			GdaSet *set;
			set = g_value_get_object (value);
			if (set) {
				model->priv->sh->ext_params = g_object_ref (set);
				g_signal_connect (model->priv->sh->ext_params, "holder-changed",
						  G_CALLBACK (ext_params_holder_changed_cb), model);
				model->priv->sh->modif_internals->exec_set = gda_set_copy (set);
			}
			break;
		}
		case PROP_INS_QUERY:
			if (model->priv->sh->modif_internals->modif_stmts [INS_QUERY])
				g_object_unref (model->priv->sh->modif_internals->modif_stmts [INS_QUERY]);
			model->priv->sh->modif_internals->modif_stmts [INS_QUERY] = g_value_get_object (value);
			if (model->priv->sh->modif_internals->modif_stmts [INS_QUERY])
				g_object_ref (model->priv->sh->modif_internals->modif_stmts [INS_QUERY]);
			break;
		case PROP_DEL_QUERY:
			if (model->priv->sh->modif_internals->modif_stmts [DEL_QUERY])
				g_object_unref (model->priv->sh->modif_internals->modif_stmts [DEL_QUERY]);
			model->priv->sh->modif_internals->modif_stmts [DEL_QUERY] = g_value_get_object (value);
			if (model->priv->sh->modif_internals->modif_stmts [DEL_QUERY])
				g_object_ref (model->priv->sh->modif_internals->modif_stmts [DEL_QUERY]);
			break;
		case PROP_UPD_QUERY:
			if (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY])
				g_object_unref (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]);
			model->priv->sh->modif_internals->modif_stmts [UPD_QUERY] = g_value_get_object (value);
			if (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY])
				g_object_ref (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]);
			break;
		case PROP_RESET_WITH_EXT_PARAM:
			model->priv->sh->reset_with_ext_params_change = g_value_get_boolean (value);
			break;
		default:
			break;
		}
	}
}

static void
gda_data_select_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdaDataSelect *model = (GdaDataSelect *) object;
	if (model->priv) {
		switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, model->priv->cnc);
			break;
		case PROP_PREP_STMT:
			g_value_set_object (value, model->prep_stmt);
			break;
		case PROP_FLAGS:
			g_value_set_uint (value, model->priv->sh->usage_flags);
			break;
		case PROP_ALL_STORED:
			if (!model->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
				g_warning ("Cannot set the 'store-all-rows' property when access mode is cursor based");
			else {
				if ((model->advertized_nrows < 0) && CLASS (model)->fetch_nb_rows)
					CLASS (model)->fetch_nb_rows (model);
				g_value_set_boolean (value, model->nb_stored_rows == model->advertized_nrows);
			}
			break;
		case PROP_PARAMS:
			g_value_set_object (value, model->priv->sh->modif_internals->exec_set);
			break;
		case PROP_INS_QUERY:
			g_value_set_object (value, model->priv->sh->modif_internals->modif_stmts [INS_QUERY]);
			break;
		case PROP_DEL_QUERY:
			g_value_set_object (value, model->priv->sh->modif_internals->modif_stmts [DEL_QUERY]);
			break;
		case PROP_UPD_QUERY:
			g_value_set_object (value, model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]);
			break;
		case PROP_SEL_STMT: 
			g_value_set_object (value, check_acceptable_statement (model, NULL));
			break;
		case PROP_RESET_WITH_EXT_PARAM:
			g_value_set_boolean (value, model->priv->sh->reset_with_ext_params_change);
			break;
		default:
			break;
		}
	}
}

/**
 * gda_data_select_take_row
 * @model: a #GdaDataSelect data model
 * @row: a #GdaRow row
 * @rownum: "external" advertized row number
 *
 * Stores @row into @model, externally advertized at row number @rownum (if no row has been removed). 
 * The reference to @row is stolen.
 */
void
gda_data_select_take_row (GdaDataSelect *model, GdaRow *row, gint rownum)
{
	g_return_if_fail (GDA_IS_DATA_SELECT (model));
	g_return_if_fail (GDA_IS_ROW (row));

	if (g_hash_table_lookup (model->priv->sh->index, GINT_TO_POINTER (rownum + 1))) 
		g_error ("INTERNAL error: row %d already exists, aborting", rownum);

	g_hash_table_insert (model->priv->sh->index, GINT_TO_POINTER (rownum + 1),
			     GINT_TO_POINTER (model->priv->sh->rows->len + 1));
	g_array_append_val (model->priv->sh->rows, row);
	model->nb_stored_rows = model->priv->sh->rows->len;
}

/**
 * gda_data_select_get_stored_row
 * @model: a #GdaDataSelect data model
 * @rownum: "external" advertized row number
 *
 * Get the #GdaRow object stored within @model at row @rownum (without taking care of removed rows)
 *
 * Returns: the requested #GdaRow, or %NULL if not found
 */
GdaRow *
gda_data_select_get_stored_row (GdaDataSelect *model, gint rownum)
{
	gint irow;
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (model->priv->sh->index, GINT_TO_POINTER (rownum + 1)));
	if (irow <= 0) 
		return NULL;
	else 
		return g_array_index (model->priv->sh->rows, GdaRow *, irow - 1);
}

/**
 * gda_data_select_get_connection
 * @model: a #GdaDataSelect data model
 *
 * Get a pointer to the #GdaConnection object which was used when @model was created
 * (and which may be used internally by @model).
 *
 * Returns: a pointer to the #GdaConnection, or %NULL
 */
GdaConnection *
gda_data_select_get_connection (GdaDataSelect *model)
{
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), NULL);
	g_return_val_if_fail (model->priv, NULL);

	return model->priv->cnc;
}

/*
 * Add the +/-<col num> holders to model->priv->sh->modif_internals->modif_set
 */
static gboolean
compute_modif_set (GdaDataSelect *model, GError **error)
{
	gint i;
	
	if (model->priv->sh->modif_internals->modif_set)
		g_object_unref (model->priv->sh->modif_internals->modif_set);
	if (model->priv->sh->modif_internals->exec_set)
		model->priv->sh->modif_internals->modif_set = gda_set_copy (model->priv->sh->modif_internals->exec_set);
	else
		model->priv->sh->modif_internals->modif_set = gda_set_new (NULL);

	for (i = 0; i < NB_QUERIES; i++) {
		GdaSet *set;
		if (! model->priv->sh->modif_internals->modif_stmts [i])
			continue;
		if (! gda_statement_get_parameters (model->priv->sh->modif_internals->modif_stmts [i], &set, error)) {
			g_object_unref (model->priv->sh->modif_internals->modif_set);
			model->priv->sh->modif_internals->modif_set = NULL;
			return FALSE;
		}

		gda_set_merge_with_set (model->priv->sh->modif_internals->modif_set, set);
		g_object_unref (set);
	}

#ifdef GDA_DEBUG_NO
	GSList *list;
	g_print ("-------\n");
	for (list = model->priv->sh->modif_internals->modif_set->holders; list; list = list->next) {
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
 * gda_data_select_set_modification_statement_sql
 * @model: a #GdaDataSelect data model
 * @sql: an SQL text
 * @error: a place to store errors, or %NULL
 *
 * Offers the same feature as gda_data_select_set_modification_statement() but using an SQL statement
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_select_set_modification_statement_sql (GdaDataSelect *model, const gchar *sql, GError **error)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	const gchar *remain = NULL;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	/* check the original SELECT statement which was executed is not a compound statement */
	if (! check_acceptable_statement (model, error))
		return FALSE;

	parser = gda_connection_create_parser (model->priv->cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, &remain, error);
	g_object_unref (parser);
	if (!stmt) 
		return FALSE;
	if (remain) {
		g_object_unref (stmt);
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SQL_ERROR,
			      "%s", _("Incorrect SQL expression"));
		return FALSE;
	}
	
	retval = gda_data_select_set_modification_statement (model, stmt, error);
	g_object_unref (stmt);

	return retval;
}

/*
 * Checks that the SELECT statement which created @model exists and is correct.
 *
 * Returns: the SELECT #GdaStatement, or %NULL if an error occurred.
 */
static GdaStatement *
check_acceptable_statement (GdaDataSelect *model, GError **error)
{
	GdaStatement *sel_stmt;

	if (model->priv->sh->sel_stmt)
		return model->priv->sh->sel_stmt;

	if (! model->prep_stmt) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("Internal error: the \"prepared-stmt\" property has not been set"));
		return NULL;
	}

	sel_stmt = gda_pstmt_get_gda_statement (model->prep_stmt);
	if (! sel_stmt) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Can't get the prepared statement's actual statement"));
		return NULL;
	}

	if (gda_statement_get_statement_type (sel_stmt) != GDA_SQL_STATEMENT_SELECT) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Unsupported type of SELECT statement"));
		return NULL;
	}

	model->priv->sh->sel_stmt = gda_statement_copy (sel_stmt);
	return model->priv->sh->sel_stmt;
}

/**
 * gda_data_select_set_modification_statement
 * @model: a #GdaDataSelect data model
 * @mod_stmt: a #GdaStatement (INSERT, UPDATE or DELETE)
 * @error: a place to store errors, or %NULL
 *
 * Informs @model that it should allow modifications to the data in some columns and some rows
 * using @mod_stmt to propagate those modifications into the database.
 *
 * If @mod_stmt is:
 * <itemizedlist>
 *  <listitem><para>an UPDATE statement, then all the rows in @model will be modifyable</para></listitem>
 *  <listitem><para>a DELETE statement, then it will be possible to delete rows in @model</para></listitem>
 *  <listitem><para>in INSERT statement, then it will be possible to add some rows to @model</para></listitem>
 *  <listitem><para>any other statement, then this method will return an error</para></listitem>
 * </itemizedlist>
 *
 * This method can be called several times to specify different types of modification.
 *
 * If @mod_stmt is an UPDATE or DELETE statement then it should have a WHERE part which identifies
 * a unique row in @model (please note that this property can't be checked but may result
 * in @model behaving in an unpredictable way).
 *
 * NOTE1: However, if the gda_data_select_set_row_selection_condition()
 * or gda_data_select_set_row_selection_condition_sql() have been successfully be called before, the WHERE
 * part of @mod_stmt <emphasis>WILL</emphasis> be modified to use the row selection condition specified through one of
 * these methods (please not that it is then possible to avoid specifying a WHERE part in @mod_stmt then).
 *
 * NOTE2: if gda_data_select_set_row_selection_condition()
 * or gda_data_select_set_row_selection_condition_sql() have not yet been successfully be called before, then
 * the WHERE part of @mod_stmt will be used as if one of these functions had been called.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_select_set_modification_statement (GdaDataSelect *model, GdaStatement *mod_stmt, GError **error)
{
	ModType mtype = NB_QUERIES;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (mod_stmt), FALSE);

	/* check the original SELECT statement which was executed is not a compound statement */
	if (! check_acceptable_statement (model, error))
		return FALSE;

	/* checks on the actual modification statement */
	switch (gda_statement_get_statement_type (mod_stmt)) {
	case GDA_SQL_STATEMENT_INSERT: {
		GdaSqlStatement *sqlst;
		GdaSqlStatementInsert *ins;

		mtype = INS_QUERY;

		/* check that we have only one list of values (<=> only one row will be inserted) */
		g_object_get (G_OBJECT (mod_stmt), "structure", &sqlst, NULL);
		g_assert (sqlst);
		ins = (GdaSqlStatementInsert*) sqlst->contents;
		if (!ins->values_list || ! ins->values_list->data) {
			g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
				      "%s", _("INSERT statement must contain values to insert"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		if (ins->values_list->next) {
			g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
				      "%s", _("INSERT statement must insert only one row"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		
		gda_sql_statement_free (sqlst);
		break;
	}
	case GDA_SQL_STATEMENT_DELETE:  {
		GdaSqlStatement *sqlst;
		GdaSqlStatementDelete *del;

		mtype = DEL_QUERY;

		/* if there is no WHERE part, then use model->priv->sh->modif_internals->unique_row_condition if set */
		g_object_get (G_OBJECT (mod_stmt), "structure", &sqlst, NULL);
		g_assert (sqlst);
		del = (GdaSqlStatementDelete*) sqlst->contents;
		if (!del->cond) {
			if (model->priv->sh->modif_internals->unique_row_condition) {
				/* copy model->priv->sh->modif_internals->unique_row_condition */
				del->cond = gda_sql_expr_copy (model->priv->sh->modif_internals->unique_row_condition);
				GDA_SQL_ANY_PART (del->cond)->parent = GDA_SQL_ANY_PART (del);
				g_object_set (G_OBJECT (mod_stmt), "structure", sqlst, NULL);
			}
			else  {
				g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
				      "%s", _("DELETE statement must have a WHERE part"));
				gda_sql_statement_free (sqlst);
				return FALSE;
			}
		}
		else {
			if (model->priv->sh->modif_internals->unique_row_condition) {
				/* replace WHERE with model->priv->sh->modif_internals->unique_row_condition */
				gda_sql_expr_free (del->cond);
				del->cond = gda_sql_expr_copy (model->priv->sh->modif_internals->unique_row_condition);
				GDA_SQL_ANY_PART (del->cond)->parent = GDA_SQL_ANY_PART (del);
				g_object_set (G_OBJECT (mod_stmt), "structure", sqlst, NULL);
			}
			else if (! gda_data_select_set_row_selection_condition (model, del->cond, error)) {
				gda_sql_statement_free (sqlst);
				return FALSE;
			}
		}
		gda_sql_statement_free (sqlst);
		break;
	}
	case GDA_SQL_STATEMENT_UPDATE: {
		GdaSqlStatement *sqlst;
		GdaSqlStatementUpdate *upd;

		mtype = UPD_QUERY;

		/* if there is no WHERE part, then use model->priv->sh->modif_internals->unique_row_condition if set */
		g_object_get (G_OBJECT (mod_stmt), "structure", &sqlst, NULL);
		g_assert (sqlst);
		upd = (GdaSqlStatementUpdate*) sqlst->contents;
		if (!upd->cond) {
			if (model->priv->sh->modif_internals->unique_row_condition) {
				/* copy model->priv->sh->modif_internals->unique_row_condition */
				upd->cond = gda_sql_expr_copy (model->priv->sh->modif_internals->unique_row_condition);
				GDA_SQL_ANY_PART (upd->cond)->parent = GDA_SQL_ANY_PART (upd);
				g_object_set (G_OBJECT (mod_stmt), "structure", sqlst, NULL);
			}
			else  {
				g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
				      "%s", _("UPDATE statement must have a WHERE part"));
				gda_sql_statement_free (sqlst);
				return FALSE;
			}
		}	
		else {
			if (model->priv->sh->modif_internals->unique_row_condition) {
				/* replace WHERE with model->priv->sh->modif_internals->unique_row_condition */
				gda_sql_expr_free (upd->cond);
				upd->cond = gda_sql_expr_copy (model->priv->sh->modif_internals->unique_row_condition);
				GDA_SQL_ANY_PART (upd->cond)->parent = GDA_SQL_ANY_PART (upd);
				g_object_set (G_OBJECT (mod_stmt), "structure", sqlst, NULL);
			}
			else if (! gda_data_select_set_row_selection_condition (model, upd->cond, error)) {
				gda_sql_statement_free (sqlst);
				return FALSE;
			}
		}	
		gda_sql_statement_free (sqlst);
		break;
	}
	default:
		break;
	}
	
	if (mtype != NB_QUERIES) {
		if (! gda_statement_check_structure (mod_stmt, error))
			return FALSE;

		if (model->priv->sh->modif_internals->modif_stmts[mtype]) {
			g_object_unref (model->priv->sh->modif_internals->modif_stmts[mtype]);
			model->priv->sh->modif_internals->modif_stmts[mtype] = NULL;
		}

		/* prepare model->priv->sh->modif_internals->modif_set */
		if (!compute_modif_set (model, error))
			return FALSE;

		/* check that all the parameters required to execute @mod_stmt are in model->priv->sh->modif_internals->modif_set */
		GdaSet *params;
		GSList *list;
		if (! gda_statement_get_parameters (mod_stmt, &params, error))
			return FALSE;
		for (list = params->holders; list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			GdaHolder *eholder;
			eholder = gda_set_get_holder (model->priv->sh->modif_internals->modif_set, gda_holder_get_id (holder));
			if (!eholder) {
				gint num;
				gboolean is_old;

				if (!param_name_to_int (gda_holder_get_id (holder), &num, &is_old)) {
					g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
						     _("Modification statement uses an unknown '%s' parameter"),
						     gda_holder_get_id (holder));
					g_object_unref (params);
					return FALSE;
				}
				if (num > gda_data_select_get_n_columns ((GdaDataModel*) model)) {
					g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
						     _("Column %d out of range (0-%d)"), num, 
						     gda_data_select_get_n_columns ((GdaDataModel*) model)-1);
					g_object_unref (params);
					return FALSE;
				}
				gda_set_add_holder (model->priv->sh->modif_internals->modif_set, holder);
			}
			else if (gda_holder_get_g_type (holder) != gda_holder_get_g_type (eholder)) {
				g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
					     _("Modification statement's  '%s' parameter is a %s when it should be a %s"),
					     gda_holder_get_id (holder),
					     gda_g_type_to_string (gda_holder_get_g_type (holder)),
					     gda_g_type_to_string (gda_holder_get_g_type (eholder)));
				g_object_unref (params);
				return FALSE;
			}
		}
		g_object_unref (params);

		model->priv->sh->modif_internals->modif_stmts[mtype] = mod_stmt;
		g_object_ref (mod_stmt);
	}
	else {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Modification statement must be an INSERT, UPDATE or DELETE statement"));
		return FALSE;
	}

#ifdef GDA_DEBUG_NO
	GSList *hlist;
	g_print ("SET MODIF QUERY\n");
	if (model->priv->sh->modif_internals->modif_set) {
		for (hlist = model->priv->sh->modif_internals->modif_set->holders; hlist; hlist = hlist->next) {
			GdaHolder *h = GDA_HOLDER (hlist->data);
			g_print ("  %s type=> %s (%d)\n", gda_holder_get_id (h), g_type_name (gda_holder_get_g_type (h)),
				 gda_holder_get_g_type (h));
		}
	}
#endif

	return TRUE;
}

/**
 * gda_data_select_compute_modification_statements
 * @model: a #GdaDataSelect data model
 * @error: a place to store errors, or %NULL
 *
 * Makes @model try to compute INSERT, UPDATE and DELETE statements to be used when modifying @model's contents.
 * Note: any modification statement set using gda_data_select_set_modification_statement() will first be unset
 *
 * Returns: TRUE if no error occurred. If FALSE is returned, then some modification statement may still have been
 * computed
 */
gboolean
gda_data_select_compute_modification_statements (GdaDataSelect *model, GError **error)
{
	GdaStatement *stmt;
	ModType mtype;
	gboolean retval = TRUE;
	GdaStatement *modif_stmts[NB_QUERIES];
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	stmt = check_acceptable_statement (model, error);
	if (!stmt)
		return FALSE;

	if (!model->priv->cnc) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_CONNECTION_ERROR,
			      "%s", _("No connection to use"));
		return FALSE;
	}
	for (mtype = FIRST_QUERY; mtype < NB_QUERIES; mtype++)
		if (model->priv->sh->modif_internals->modif_stmts[mtype]) {
			g_object_unref (model->priv->sh->modif_internals->modif_stmts[mtype]);
			model->priv->sh->modif_internals->modif_stmts[mtype] = NULL;
		}

	retval = gda_compute_dml_statements (model->priv->cnc, stmt, TRUE,
					     &(modif_stmts[INS_QUERY]),
					     &(modif_stmts[UPD_QUERY]),
					     &(modif_stmts[DEL_QUERY]), error);
	if (retval) {
		for (mtype = FIRST_QUERY; mtype < NB_QUERIES; mtype++) {
#ifdef GDA_DEBUG_NO
			gchar *sql;
			sql = gda_statement_to_sql (modif_stmts[mtype], NULL, NULL);
			g_print ("type %d => %s\n", mtype, sql);
			g_free (sql);
#endif
			if (modif_stmts[mtype] &&
			    ! gda_data_select_set_modification_statement (model, modif_stmts[mtype], error)) {
				retval = FALSE;
				break;
			}
		}
	}
	if (!retval) {
		for (mtype = FIRST_QUERY; mtype < NB_QUERIES; mtype++) {
			if (modif_stmts[mtype]) 
				g_object_unref (modif_stmts[mtype]);
		}
	}
	return retval;
}

static gboolean
row_selection_condition_foreach_func (GdaSqlAnyPart *part, gpointer data, GError **error)
{
	if (part->type != GDA_SQL_ANY_SQL_OPERATION) 
		return TRUE;

	GdaSqlOperation *op = (GdaSqlOperation*) part;
	if ((op->operator_type != GDA_SQL_OPERATOR_TYPE_EQ) &&
	    (op->operator_type != GDA_SQL_OPERATOR_TYPE_AND)) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Invalid unique row condition (ony equal operators are allowed)"));
		return FALSE;
	}

	return TRUE;
}

/**
 * gda_data_select_set_row_selection_condition
 * @model: a #GdaDataSelect data model
 * @expr: a #GdaSqlExpr expression
 * @error: a place to store errors, or %NULL
 *
 * Offers the same features as gda_data_select_set_row_selection_condition_sql() but using a #GdaSqlExpr
 * structure instead of an SQL syntax.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_select_set_row_selection_condition  (GdaDataSelect *model, GdaSqlExpr *expr, GError **error)
{
	gboolean valid;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);
	g_return_val_if_fail (expr, FALSE);

	if (!check_acceptable_statement (model, error))
		return FALSE;

	if (model->priv->sh->modif_internals->unique_row_condition) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Unique row condition has already been specified"));
		return FALSE;
	}

	valid = gda_sql_any_part_foreach (GDA_SQL_ANY_PART (expr),
					  (GdaSqlForeachFunc) row_selection_condition_foreach_func,
					  NULL, error);
	if (!valid)
		return FALSE;
	
	model->priv->sh->modif_internals->unique_row_condition = gda_sql_expr_copy (expr);
	return TRUE;
}

/**
 * gda_data_select_set_row_selection_condition_sql
 * @model: a #GdaDataSelect data model
 * @sql_where: an SQL condition (without the WHERE keyword)
 * @error: a place to store errors, or %NULL
 *
 * Specifies the SQL condition corresponding to the WHERE part of a SELECT statement which would
 * return only 1 row (the expression of the primary key).
 *
 * For example for a table created as <![CDATA["CREATE TABLE mytable (part1 int NOT NULL, part2 string NOT NULL, 
 * name string, PRIMARY KEY (part1, part2))"]]>, and if @pmodel corresponds to the execution of the 
 * <![CDATA["SELECT name, part1, part2 FROM mytable"]]>, then the sensible value for @sql_where would be
 * <![CDATA["part1 = ##-1::int AND part2 = ##-2::string"]]> because the values of the 'part1' field are located
 * in @pmodel's column number 1 and the values of the 'part2' field are located
 * in @pmodel's column number 2 and the primary key is composed of (part1, part2).
 *
 * For more information about the syntax of the parameters (named <![CDATA["##-1::int"]]> for example), see the
 * <link linkend="GdaSqlParser.description">GdaSqlParser</link> documentation, and 
 * gda_data_select_set_modification_statement().
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_select_set_row_selection_condition_sql (GdaDataSelect *model, const gchar *sql_where, GError **error)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	gchar *sql;
	const gchar *remain = NULL;
	gboolean retval;
	GdaSqlStatement *sqlst;
	GdaSqlStatementSelect *selstmt;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	if (!check_acceptable_statement (model, error))
		return FALSE;

	if (model->priv->sh->modif_internals->unique_row_condition) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Unique row condition has already been specified"));
		return FALSE;
	}

	parser = gda_connection_create_parser (model->priv->cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

	sql= g_strdup_printf ("SELECT * FROM table WHERE %s", sql_where);
	stmt = gda_sql_parser_parse_string (parser, sql, &remain, error);
	g_object_unref (parser);
	if (!stmt) {
		g_free (sql);
		return FALSE;
	}
	if (remain) {
		g_object_unref (stmt);
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SQL_ERROR,
			      "%s", _("Incorrect filter expression"));
		g_free (sql);
		return FALSE;
	}
	g_free (sql);

	
	g_object_get (stmt, "structure", &sqlst, NULL);
	selstmt = (GdaSqlStatementSelect *) sqlst->contents;
	
	retval = gda_data_select_set_row_selection_condition (model, selstmt->where_cond, error);
	gda_sql_statement_free (sqlst);

	g_object_unref (stmt);
	return retval;
}

/**
 * gda_data_select_compute_row_selection_condition
 * @model: a #GdaDataSelect object
 * @error: a place to store errors, or %NULL
 *
 * Offers the same features as gda_data_select_set_row_selection_condition() but the expression
 * is computed from the meta data associated to the connection being used when @model was created.
 *
 * NOTE1: make sure the meta data associated to the connection is up to date before using this
 * method, see gda_connection_update_meta_store().
 * 
 * NOTE2: if the SELECT statement from which @model has been created uses more than one table, or
 * if the table used does not have any primary key, then this method will fail
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_select_compute_row_selection_condition (GdaDataSelect *model, GError **error)
{
	GdaSqlExpr *expr;
	gboolean retval = FALSE;
	GdaStatement *stmt;
	GdaSqlStatement *sqlst = NULL;
	GdaSqlStatementSelect *select;
	GdaSqlSelectTarget *target;
	GdaMetaStruct *mstruct = NULL;
	GdaMetaDbObject *dbo;
	GValue *nvalue = NULL;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	stmt = check_acceptable_statement (model, error);
	if (!stmt)
		return FALSE;

	if (!model->priv->cnc) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_CONNECTION_ERROR,
			      "%s", _("No connection to use"));
		return FALSE;
	}

	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT);
	select = (GdaSqlStatementSelect*) sqlst->contents;
	if (!select->from || ! select->from->targets || ! select->from->targets->data) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SQL_ERROR,
			      "%s", _("No table to select from in SELECT statement"));
		goto out;
	}
	if (select->from->targets->next) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SQL_ERROR,
			      "%s", _("SELECT statement uses more than one table to select from"));
		goto out;
	}
	target = (GdaSqlSelectTarget *) select->from->targets->data;
	if (!target->table_name) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SQL_ERROR,
			      "%s", _("No table to select from in SELECT statement"));
		goto out;
	}
	g_value_set_string ((nvalue = gda_value_new (G_TYPE_STRING)), target->table_name);
	mstruct = gda_meta_struct_new (gda_connection_get_meta_store (model->priv->cnc), GDA_META_STRUCT_FEATURE_NONE);
	dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_TABLE, NULL, NULL, nvalue, error);
	if (!dbo) 
		goto out;
       
	expr = gda_compute_unique_table_row_condition (select, GDA_META_TABLE (dbo), TRUE, error);
	retval = gda_data_select_set_row_selection_condition (model, expr, error);

 out:
	if (sqlst)
		gda_sql_statement_free (sqlst);
	if (mstruct)
		g_object_unref (mstruct);
	if (nvalue)
		gda_value_free (nvalue);

	return retval;
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_select_get_n_rows (GdaDataModel *model)
{
	GdaDataSelect *imodel;
	gint retval;
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);
	imodel = GDA_DATA_SELECT (model);
	g_return_val_if_fail (imodel->priv, 0);

	retval = imodel->advertized_nrows;
	if ((imodel->advertized_nrows < 0) && 
	    (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) &&
	    CLASS (model)->fetch_nb_rows)
		retval = CLASS (model)->fetch_nb_rows (imodel);
		
	if ((retval > 0) && (imodel->priv->sh->del_rows))
		retval -= imodel->priv->sh->del_rows->len;
	return retval;
}

static gint
gda_data_select_get_n_columns (GdaDataModel *model)
{
	GdaDataSelect *imodel;
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);
	imodel = GDA_DATA_SELECT (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->prep_stmt)
		return imodel->prep_stmt->ncols;
	else
		return 0;
}

static GdaColumn *
gda_data_select_describe_column (GdaDataModel *model, gint col)
{
	GdaDataSelect *imodel;
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), NULL);
	imodel = GDA_DATA_SELECT (model);
	g_return_val_if_fail (imodel->priv, NULL);

	return g_slist_nth_data (imodel->priv->sh->columns, col);
}

static GdaDataModelAccessFlags
gda_data_select_get_access_flags (GdaDataModel *model)
{
	GdaDataSelect *imodel;
	GdaDataModelAccessFlags flags = 0;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);
	imodel = GDA_DATA_SELECT (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		flags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD) {
		if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
			flags = GDA_DATA_MODEL_ACCESS_CURSOR;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;
	}

	if (! imodel->priv->sh->modif_internals->safely_locked) {
		if (imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY])
			flags |= GDA_DATA_MODEL_ACCESS_UPDATE;
		if (imodel->priv->sh->modif_internals->modif_stmts [INS_QUERY])
			flags |= GDA_DATA_MODEL_ACCESS_INSERT;
		if (imodel->priv->sh->modif_internals->modif_stmts [DEL_QUERY])
			flags |= GDA_DATA_MODEL_ACCESS_DELETE;
	}

	return flags;
}

/*
 * Converts an external row number to an internal row number
 * Returns: a row number, or -1 if row number does not exist
 */
static gint
external_to_internal_row (GdaDataSelect *model, gint ext_row, GError **error)
{
	gint nrows;
	gint int_row = ext_row;

	/* row number alteration: deleted rows */
	if (model->priv->sh->del_rows) {
		gint i;
		for (i = 0; i < model->priv->sh->del_rows->len; i++) {
			gint indexed = g_array_index (model->priv->sh->del_rows, gint, i);
			if (indexed <= ext_row + i)
				int_row += 1;
			else
				break;
		}
	}
	
	/* check row number validity */
	nrows = model->advertized_nrows < 0 ? gda_data_select_get_n_rows ((GdaDataModel*) model) : 
		model->advertized_nrows;
	if ((ext_row < 0) || ((nrows >= 0) && (int_row >= nrows))) {
		gint n;
		n = gda_data_select_get_n_rows ((GdaDataModel*) model);
		if (n > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d out of range (0-%d)"), ext_row, n - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), ext_row);
		return -1;
	}

	return int_row;
}

#ifdef GDA_DEBUG_NO
static void foreach_func_dump (gpointer key, gpointer value, gpointer dummy)
{
	g_print (" row %d => %p\n", *(const gint*) key, value);
}
static void dump_d (GdaDataSelect *model)
{
	if (model->priv->sh->upd_rows) {
		g_print ("Delayed SELECT for %p:\n", model);
		g_hash_table_foreach (model->priv->sh->upd_rows, foreach_func_dump, NULL);
	}
}
#endif

static const GValue *
gda_data_select_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaRow *prow;
	gint int_row, irow;
	GdaDataSelect *imodel;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), NULL);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, NULL);

	/* available only if GDA_DATA_MODEL_ACCESS_RANDOM */
	if (! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			      "%s", _("Data model does only support random access"));
		return NULL;
	}

	if ((col >= gda_data_select_get_n_columns (model)) ||
	    (col < 0)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
                             _("Column %d out of range (0-%d)"), col, gda_data_select_get_n_columns (model) - 1);
                return NULL;
	}

	int_row = external_to_internal_row (imodel, row, NULL);
	if (int_row < 0) {
		gint n;
		n = gda_data_select_get_n_rows ( model);
		if (n > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d out of range (0-%d)"), row, n - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), row);
		return NULL;
	}

	DelayedSelectStmt *dstmt = NULL;
#ifdef GDA_DEBUG_NO
	dump_d (imodel);
#endif
	if (imodel->priv->sh->upd_rows)
		dstmt = g_hash_table_lookup (imodel->priv->sh->upd_rows, &int_row);
	if (dstmt) {
		if (! dstmt->row) {
			GdaDataModel *tmpmodel;
			if (!dstmt->select || !dstmt->params) {
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					      "%s", _("Unable to retrieve data after modifications"));
				return NULL;
			}
			GType *types = NULL;
			if (imodel->prep_stmt && imodel->prep_stmt->types) {
				types = g_new (GType, imodel->prep_stmt->ncols + 1);
				memcpy (types, imodel->prep_stmt->types, sizeof (GType) * imodel->prep_stmt->ncols);
				types [imodel->prep_stmt->ncols] = G_TYPE_NONE;
			}
			tmpmodel = gda_connection_statement_execute_select_full (imodel->priv->cnc,
										 dstmt->select, 
										 dstmt->params, 
										 GDA_STATEMENT_MODEL_RANDOM_ACCESS,
										 types, 
										 NULL);
			g_free (types);

			if (!tmpmodel) {
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					      "%s", _("Unable to retrieve data after modifications, no further modification will be allowed"));
				imodel->priv->sh->modif_internals->safely_locked = TRUE;
				return NULL;
			}

			if (gda_data_model_get_n_rows (tmpmodel) != 1) {
				g_object_unref (tmpmodel);
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
					     "%s", _("Unable to retrieve data after modifications, no further modification will be allowed"));
				imodel->priv->sh->modif_internals->safely_locked = TRUE;
				return NULL;
			}

			gint i, ncols;
			ncols = gda_data_model_get_n_columns (tmpmodel);
			prow = gda_row_new (ncols);
			for (i = 0; i < ncols; i++) {
				GValue *value;
				const GValue *cvalue;
				value = gda_row_get_value (prow, i);
				cvalue = gda_data_model_get_value_at (tmpmodel, i, 0, error);
				if (!cvalue)
					return NULL;
				
				if (!gda_value_is_null (cvalue)) {
					gda_value_reset_with_type (value, G_VALUE_TYPE (cvalue));
					if (! gda_value_set_from_value (value, cvalue)) {
						g_object_unref (tmpmodel);
						g_object_unref (prow);
						g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
							      "%s", _("Unable to retrieve data after modifications, no further modification will be allowed"));
						imodel->priv->sh->modif_internals->safely_locked = TRUE;
						return NULL;
					}
				}
				else
					gda_value_set_null (value);
			}
			dstmt->row = prow;
			//gda_data_model_dump (tmpmodel, stdout);
			g_object_unref (tmpmodel);
		}
		else
			prow = dstmt->row;
	}
	else {
		irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->sh->index, 
							     GINT_TO_POINTER (int_row + 1)));
		if (irow <= 0) {
			prow = NULL;
			if (CLASS (model)->fetch_random && 
			    !CLASS (model)->fetch_random (imodel, &prow, int_row, error))
				return NULL;
		}
		else
			prow = g_array_index (imodel->priv->sh->rows, GdaRow *, irow - 1);
	}
	
	g_assert (prow);

	GValue *retval = gda_row_get_value (prow, col);
	if (gda_row_value_is_valid (prow, retval))
		return retval;
	else {
		gchar *str;
		str = g_strdup_printf (_("Unable to get value for row %d and column %d"), row, col);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", str);
		g_free (str);
		return NULL;
	}
}

static GdaValueAttribute
gda_data_select_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute flags = GDA_VALUE_ATTR_IS_UNCHANGED;
	GdaDataSelect *imodel;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	if (imodel->priv->sh->modif_internals->safely_locked ||
	    !imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY])
		flags = GDA_VALUE_ATTR_NO_MODIF;
	GdaColumn *gdacol = g_slist_nth_data (imodel->priv->sh->columns, col);
	if (gdacol) {
		if (gda_column_get_allow_null (gdacol))
			flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
	}

	return flags;
}

static GdaDataModelIter *
gda_data_select_create_iter (GdaDataModel *model)
{
	GdaDataSelect *imodel;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), 0);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER,
							  "data-model", model, NULL);
	else {
		/* Create the iter if necessary, or just return the existing iter: */
		if (! imodel->priv->iter) {
			imodel->priv->iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER,
										"data-model", model, NULL);
			imodel->priv->sh->iter_row = -1;
		}
		g_object_ref (imodel->priv->iter);
		return imodel->priv->iter;
	}
}

static gboolean update_iter (GdaDataSelect *imodel, GdaRow *prow);
static gboolean
gda_data_select_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataSelect *imodel;
	GdaRow *prow = NULL;
	gint target_iter_row;
	gint irow, int_row;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_iter_move_next_default (model, iter);

	g_return_val_if_fail (CLASS (model)->fetch_next, FALSE);

	g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	if (imodel->priv->sh->iter_row == G_MAXINT)
		return FALSE;
	else if (imodel->priv->sh->iter_row == G_MININT)
		target_iter_row = 0;
	else
		target_iter_row = imodel->priv->sh->iter_row + 1;
	int_row = external_to_internal_row (imodel, target_iter_row, NULL);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->sh->index, GINT_TO_POINTER (int_row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->sh->rows, GdaRow *, irow - 1);
	else if (!CLASS (model)->fetch_next (imodel, &prow, int_row, NULL)) {
		/* an error occurred */
		g_object_set (G_OBJECT (iter), "current-row", target_iter_row, NULL);
		gda_data_model_iter_invalidate_contents (iter);
		return FALSE;
	}
	
	if (prow) {
		imodel->priv->sh->iter_row = target_iter_row;
                return update_iter (imodel, prow);
	}
	else {
		g_signal_emit_by_name (iter, "end-of-data");
                g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
                imodel->priv->sh->iter_row = G_MAXINT;
                return FALSE;
	}
}

static gboolean
gda_data_select_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataSelect *imodel;
	GdaRow *prow = NULL;
	gint target_iter_row;
	gint irow, int_row;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_iter_move_prev_default (model, iter);

	g_return_val_if_fail (CLASS (model)->fetch_prev, FALSE);

        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

        if (imodel->priv->sh->iter_row <= 0)
                goto prev_error;

        else if (imodel->priv->sh->iter_row == G_MAXINT) {
                g_assert (imodel->advertized_nrows >= 0);
                target_iter_row = imodel->advertized_nrows - 1;
        }
        else
                target_iter_row = imodel->priv->sh->iter_row - 1;

	int_row = external_to_internal_row (imodel, target_iter_row, NULL);
	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->sh->index, GINT_TO_POINTER (int_row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->sh->rows, GdaRow *, irow - 1);
	else if (!CLASS (model)->fetch_prev (imodel, &prow, int_row, NULL)) {
		/* an error occurred */
		g_object_set (G_OBJECT (iter), "current-row", target_iter_row, NULL);
		gda_data_model_iter_invalidate_contents (iter);
		return FALSE;
	}

	if (prow) {
		imodel->priv->sh->iter_row = target_iter_row;
                return update_iter (imodel, prow);
	}

 prev_error:
        g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
        imodel->priv->sh->iter_row = G_MININT;
        return FALSE;
}

static gboolean
gda_data_select_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	GdaDataSelect *imodel;
	GdaRow *prow = NULL;
	gint irow, int_row;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	int_row = external_to_internal_row (imodel, row, NULL);
	if (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM) 
		return gda_data_model_iter_move_to_row_default (model, iter, row);

        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	irow = GPOINTER_TO_INT (g_hash_table_lookup (imodel->priv->sh->index, 
						     GINT_TO_POINTER (int_row + 1)));
	if (irow > 0)
		prow = g_array_index (imodel->priv->sh->rows, GdaRow *, irow - 1);

	if (CLASS (model)->fetch_at) {
		if (!CLASS (model)->fetch_at (imodel, &prow, int_row, NULL)) {
			/* an error occurred */
			g_object_set (G_OBJECT (iter), "current-row", row, NULL);
			gda_data_model_iter_invalidate_contents (iter);
			return FALSE;
		}

		if (prow) {
			imodel->priv->sh->iter_row = row;
			return update_iter (imodel, prow);
		}
		else {
			g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
			imodel->priv->sh->iter_row = G_MININT;
			return FALSE;
		}
	}
	else {
		if (prow) {
			imodel->priv->sh->iter_row = row;
			return update_iter (imodel, prow);
		}
		else {
			/* implementation of fetch_at() is optional */
			TO_IMPLEMENT; /* iter back or forward the right number of times */
			g_object_set (G_OBJECT (iter), "current-row", row, NULL);
			gda_data_model_iter_invalidate_contents (iter);
			return FALSE;
		}
	}
}

static gboolean
update_iter (GdaDataSelect *imodel, GdaRow *prow)
{
        gint i;
	GdaDataModelIter *iter = imodel->priv->iter;
	GSList *plist;
	gboolean update_model;
	gboolean retval = TRUE;
	
	g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
	if (update_model)
		g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
	
	for (i = 0, plist = GDA_SET (iter)->holders; 
	     plist;
	     i++, plist = plist->next) {
		GValue *value;
		GError *error = NULL;
		value = gda_row_get_value (prow, i);

		if (!gda_row_value_is_valid (prow, value)) {
			retval = FALSE;
			g_warning (_("Could not change iter's value for column %d: %s"), i,
				   error && error->message ? error->message : _("No detail"));
		}
		else if (! gda_holder_set_value ((GdaHolder*) plist->data, value, &error)) {
			if (gda_holder_get_not_null ((GdaHolder*) plist->data) &&
			    gda_value_is_null (value)) {
				gda_holder_set_not_null ((GdaHolder*) plist->data, FALSE);
				if (! gda_holder_set_value ((GdaHolder*) plist->data, value, NULL)) {
					retval = FALSE;
					g_warning (_("Could not change iter's value for column %d: %s"), i,
						   error && error->message ? error->message : _("No detail"));
					gda_holder_set_not_null ((GdaHolder*) plist->data, TRUE);
				}
				else 
					g_warning (_("Allowed GdaHolder's value to be NULL for the iterator "
						     "to be updated"));
			}
			else {
				retval = FALSE;
				g_warning (_("Could not change iter's value for column %d: %s"), i,
					   error && error->message ? error->message : _("No detail"));
			}
			if (error)
				g_error_free (error);
		}
        }

	g_object_set (G_OBJECT (iter), "current-row", imodel->priv->sh->iter_row, NULL);
	if (update_model)
		g_object_set (G_OBJECT (iter), "update-model", update_model, NULL);

	return retval;
}

/*
 * creates a derivative of the model->priv->sh->modif_internals->modif_stmts [UPD_QUERY] statement
 * where only the columns where @bv->data[colnum] is not 0 are updated.
 *
 * Returns: a new #GdaStatement, or %NULL
 */
static GdaStatement *
compute_single_update_stmt (GdaDataSelect *model, BVector *bv, GError **error)
{
	GdaSqlStatement *sqlst;
	GdaSqlStatementUpdate *upd;
	GdaStatement *updstmt = NULL;

	/* get a copy of complete UPDATE stmt */
	g_assert (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]);
	g_object_get (G_OBJECT (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]), "structure", &sqlst, NULL);
	g_assert (sqlst);
	g_free (sqlst->sql);
	sqlst->sql = NULL;
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
		if ((num < bv->size) && bv->data[num]) {
			/* this field is a field to be updated */
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
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("Some columns can't be modified"));
	gda_sql_statement_free (sqlst);

#ifdef GDA_DEBUG_NO
	GString *bvstr;
	gint i;
	gboolean first = TRUE;
	bvstr = g_string_new ("(");
	for (i = 0; i < bv->size; i++) {
		if (bv->data[i]) {
			if (first)
				first = FALSE;
			else
				g_string_append (bvstr, ", ");
			g_string_append_printf (bvstr, "%d", i);
		}
	}
	g_string_append_c (bvstr, ')');
		
	if (updstmt) {
		gchar *sql;
		sql = gda_statement_serialize (updstmt);
		g_print ("UPDATE for columns %s => %s\n", bvstr->str, sql);
		g_free (sql);
	}
	else
		g_print ("UPDATE for columns %s: ERROR\n", bvstr->str);
	g_string_free (bvstr, TRUE);
#endif

	return updstmt;
}

/*
 * creates a derivative of the model->priv->sh->modif_internals->modif_stmts [INS_QUERY] statement
 * where only the columns where @bv->data[colnum] is not 0 are not mentioned.
 *
 * Returns: a new #GdaStatement, or %NULL
 */
static GdaStatement *
compute_single_insert_stmt (GdaDataSelect *model, BVector *bv, GError **error)
{
	GdaSqlStatement *sqlst;
	GdaSqlStatementInsert *ins;
	GdaStatement *insstmt = NULL;

	/* get a copy of complete INSERT stmt */
	g_assert (model->priv->sh->modif_internals->modif_stmts [INS_QUERY]);
	g_object_get (G_OBJECT (model->priv->sh->modif_internals->modif_stmts [INS_QUERY]), "structure", &sqlst, NULL);
	g_assert (sqlst);
	g_free (sqlst->sql);
	sqlst->sql = NULL;
	g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_INSERT);
	ins = (GdaSqlStatementInsert*) sqlst->contents;
	
	/* remove fields to insert for which we don't have any value (the default value will be used) */
	GSList *elist, *flist;
	gboolean field_found = FALSE;
	for (elist = (GSList*) ins->values_list->data, flist = ins->fields_list; elist && flist; ) {
		GdaSqlExpr *expr = (GdaSqlExpr *) elist->data;
		gint num;
		gboolean old_val;
		if (! expr->param_spec || !param_name_to_int (expr->param_spec->name, &num, &old_val) || old_val) {
			/* ignore this field to be inserted */
			elist = elist->next;
			flist = flist->next;
			continue;
		}
		if ((num < bv->size) && bv->data[num]) {
			/* this field is a field to be inserted */
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

		ins->values_list->data = g_slist_delete_link ((GSList*) ins->values_list->data, elist);
		ins->fields_list = g_slist_delete_link (ins->fields_list, flist);
		elist = nelist;
		flist = nflist;
	}

	/* create a new GdaStatement */
	if (field_found)
		insstmt = (GdaStatement *) g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
	else 
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Some columns can't be modified"));
	gda_sql_statement_free (sqlst);

#ifdef GDA_DEBUG_NO
	GString *bvstr;
	gint i;
	gboolean first = TRUE;
	bvstr = g_string_new ("(");
	for (i = 0; i < bv->size; i++) {
		if (bv->data[i]) {
			if (first)
				first = FALSE;
			else
				g_string_append (bvstr, ", ");
			g_string_append_printf (bvstr, "%d", i);
		}
	}
	g_string_append_c (bvstr, ')');
		
	if (insstmt) {
		gchar *sql;
		sql = gda_statement_serialize (insstmt);
		g_print ("INSERT for columns %s => %s\n", bvstr->str, sql);
		g_free (sql);
	}
	else
		g_print ("INSERT for columns %s: ERROR\n", bvstr->str);
	g_string_free (bvstr, TRUE);
#endif

	return insstmt;
}

/*
 * creates a derivative of the SELECT statement from which @model has been created,
 * but to select only one single row (this statement is executed when a GdaRow is requested
 * after an UPDATE or INSERT statement has been run)
 *
 * The new created statement is merely the copy of the SELECT statement where the WHERE part
 * has been altered as "<update condition>"
 *
 * Returns: a new #GdaStatement, or %NULL
 */
static GdaStatement *
compute_single_select_stmt (GdaDataSelect *model, GError **error)
{
	GdaStatement *sel_stmt;
	GdaStatement *ret_stmt = NULL;
	GdaSqlStatement *sel_sqlst;
	GdaSqlExpr *row_cond = NULL;

	sel_stmt = model->priv->sh->sel_stmt;
	if (! sel_stmt) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Internal error: can't get the prepared statement's actual statement"));
		return NULL;
	}
	
	if (model->priv->sh->modif_internals->unique_row_condition)
		row_cond = gda_sql_expr_copy (model->priv->sh->modif_internals->unique_row_condition);
	else if (model->priv->sh->modif_internals->modif_stmts [DEL_QUERY]) {
		GdaStatement *del_stmt;
		GdaSqlStatement *del_sqlst;
		GdaSqlStatementDelete *del;
		del_stmt = model->priv->sh->modif_internals->modif_stmts [DEL_QUERY];
		
		g_object_get (G_OBJECT (del_stmt), "structure", &del_sqlst, NULL);
		del = (GdaSqlStatementDelete*) del_sqlst->contents;
		row_cond = del->cond;
		del->cond = NULL;
		gda_sql_statement_free (del_sqlst);
		if (!gda_data_select_set_row_selection_condition (model, row_cond, NULL)) {
			gda_sql_expr_free (row_cond);
			row_cond = NULL;
		}
	}
	else if (model->priv->sh->modif_internals->modif_stmts [UPD_QUERY]) {
		GdaStatement *upd_stmt;
		GdaSqlStatement *upd_sqlst;
		GdaSqlStatementUpdate *upd;
		upd_stmt = model->priv->sh->modif_internals->modif_stmts [UPD_QUERY];
		
		g_object_get (G_OBJECT (upd_stmt), "structure", &upd_sqlst, NULL);
		upd = (GdaSqlStatementUpdate*) upd_sqlst->contents;
		row_cond = upd->cond;
		upd->cond = NULL;
		gda_sql_statement_free (upd_sqlst);
		if (!gda_data_select_set_row_selection_condition (model, row_cond, NULL)) {
			gda_sql_expr_free (row_cond);
			row_cond = NULL;
		}
	}
	if (!row_cond) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Unable to identify a way to fetch a single row"));
		return NULL;
	}
		
	g_object_get (G_OBJECT (sel_stmt), "structure", &sel_sqlst, NULL);
	if (sel_sqlst->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			      "%s", _("Can only operate on non compound SELECT statements"));
		gda_sql_statement_free (sel_sqlst);
		gda_sql_expr_free (row_cond);
		return NULL;
	}
	
	GdaSqlStatementSelect *sel;

	sel = (GdaSqlStatementSelect*) sel_sqlst->contents;
	g_free (sel_sqlst->sql);
	sel_sqlst->sql = NULL;
	if (sel->where_cond) 
		gda_sql_expr_free (sel->where_cond);
	sel->where_cond = row_cond;
	GDA_SQL_ANY_PART (row_cond)->parent = GDA_SQL_ANY_PART (sel);

	ret_stmt = (GdaStatement *) g_object_new (GDA_TYPE_STATEMENT, "structure", sel_sqlst, NULL);

	gda_sql_statement_free (sel_sqlst);
	
#ifdef GDA_DEBUG_NO
	gchar *sql;
	sql = gda_statement_serialize (ret_stmt);
	g_print ("SINGLE ROW SELECT => %s\n", sql);
	g_free (sql);
	sql = gda_statement_to_sql (ret_stmt, NULL, NULL);
	g_print ("SINGLE ROW SELECT => %s\n", sql);
	g_free (sql);
#endif

	return ret_stmt;
}

/*
 * Hash and Equal functions for usage in a GHashTable
 * The comparison is made of BVector vectors
 */
static guint
bvector_hash (BVector *key)
{
	gint i;
	guint ret = 0;
	for (i = 0; i < key->size; i++) {
		ret += key->data [i];
		ret <<= 1;
	}
	return ret;
}

static gboolean
bvector_equal (BVector *key1, BVector *key2)
{
	if (key1->size != key2->size)
		return FALSE;
	return memcmp (key1->data, key2->data, sizeof (guchar) * key1->size) == 0 ? TRUE : FALSE;
}

static void
bvector_free (BVector *key)
{
	g_free (key->data);
	g_free (key);
}

/*
 * REM: @bv is stolen here
 */
static gboolean
vector_set_value_at (GdaDataSelect *imodel, BVector *bv, GdaDataModelIter *iter, gint row, GError **error)
{
	gint int_row, i, ncols;
	GdaHolder *holder;
	gchar *str;
	GdaStatement *stmt;
	gboolean free_bv = TRUE;

	/* arguments check */
	g_assert (bv);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			      "%s", _("Modifications are not allowed anymore"));
		return FALSE;
	}
	if (!iter && ! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Data model does only support random access"));
		return FALSE;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No UPDATE statement provided"));
		return FALSE;
	}

	if (iter)
		row = gda_data_model_iter_get_row (iter);

	int_row = external_to_internal_row (imodel, row, error);
	if (int_row < 0)
		return FALSE;

	/* compute UPDATE statement */
	if (! imodel->priv->sh->modif_internals->upd_stmts)
		imodel->priv->sh->modif_internals->upd_stmts = g_hash_table_new_full ((GHashFunc) bvector_hash, (GEqualFunc) bvector_equal, 
								 (GDestroyNotify) bvector_free, g_object_unref);
	stmt = g_hash_table_lookup (imodel->priv->sh->modif_internals->upd_stmts, bv);
	if (! stmt) {
		stmt = compute_single_update_stmt (imodel, bv, error);
		if (stmt) {
			free_bv = FALSE;
			g_hash_table_insert (imodel->priv->sh->modif_internals->upd_stmts, bv, stmt);
		}
		else {
			bvector_free (bv);
			return FALSE;
		}
	}

	/* give values to params for old values */
	ncols = gda_data_select_get_n_columns ((GdaDataModel*) imodel);
	for (i = 0; i < ncols; i++) {
		str = g_strdup_printf ("-%d", i);
		holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
		g_free (str);
		if (holder) {
			const GValue *cvalue;
			if (iter) {
				cvalue = gda_data_model_iter_get_value_at (iter, i);
				if (!cvalue) {
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
						     "%s", _("Could not get iterator's value"));
					return FALSE;
				}
			}
			else {
				cvalue = gda_data_model_get_value_at ((GdaDataModel*) imodel, i, row, error);
				if (!cvalue)
					return FALSE;
			}

			if (! gda_holder_set_value (holder, cvalue, error))
				return FALSE;
		}
	}

	if (free_bv)
		bvector_free (bv);
	
	/* actually execute UPDATE statement */
#ifdef GDA_DEBUG_NO
	gchar *sql;
	GError *lerror = NULL;
	sql = gda_statement_to_sql_extended (stmt,
					     imodel->priv->cnc,
					     imodel->priv->sh->modif_internals->modif_set, 
					     GDA_STATEMENT_SQL_PRETTY, NULL,
					     &lerror);
	g_print ("%s(): SQL=> %s\n", __FUNCTION__, sql);
	if (!sql)
		g_print ("\tERR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
	g_free (sql);
#endif

	if (gda_connection_statement_execute_non_select (imodel->priv->cnc, stmt,
							 imodel->priv->sh->modif_internals->modif_set, NULL, error) == -1)
		return FALSE;
	
	/* mark that this row has been modified */
	DelayedSelectStmt *dstmt;
	dstmt = g_new0 (DelayedSelectStmt, 1);
	if (! imodel->priv->sh->modif_internals->one_row_select_stmt)
		imodel->priv->sh->modif_internals->one_row_select_stmt = compute_single_select_stmt (imodel, error);
	if (imodel->priv->sh->modif_internals->one_row_select_stmt) {
		dstmt->select = g_object_ref (imodel->priv->sh->modif_internals->one_row_select_stmt);
		gda_statement_get_parameters (dstmt->select, &(dstmt->params), NULL);
		if (dstmt->params) {
			GSList *list;
			gboolean allok = TRUE;

			/* overwrite old values with new values if some have been provided */
			for (list = imodel->priv->sh->modif_internals->modif_set->holders; list; list = list->next) {
				GdaHolder *h = (GdaHolder*) list->data;
				gint res;
				gboolean old;
				if (gda_holder_is_valid ((GdaHolder*) list->data) && 
				    param_name_to_int (gda_holder_get_id (h), &res, &old) && 
				    !old) {
					str = g_strdup_printf ("-%d", res);
					holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
					g_free (str);
					if (holder &&
					    ! gda_holder_set_value (holder, gda_holder_get_value (h), error)) {
						allok = FALSE;
						break;
					}
				}
			}

			for (list = dstmt->params->holders; list && allok; list = list->next) {
				GdaHolder *holder = GDA_HOLDER (list->data);
				GdaHolder *eholder;
				eholder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, 
							      gda_holder_get_id (holder));
				if (!eholder || 
				    ! gda_holder_set_value (holder, gda_holder_get_value (eholder), NULL)) {
					allok = FALSE;
					break;
				}
			}

			if (!allok) {
				g_object_unref (dstmt->params);
				dstmt->params = NULL;
			}
		}
	}
	dstmt->row = NULL;
	if (! imodel->priv->sh->upd_rows)
		imodel->priv->sh->upd_rows = g_hash_table_new_full (g_int_hash, g_int_equal,
								g_free, 
								(GDestroyNotify) delayed_select_stmt_free);
	gint *tmp = g_new (gint, 1);
	*tmp = int_row;
	g_hash_table_insert (imodel->priv->sh->upd_rows, tmp, dstmt);
#ifdef GDA_DEBUG_NO
	dump_d (imodel);
#endif

	gda_data_model_row_updated ((GdaDataModel *) imodel, row);

	return TRUE;

}

static gboolean
gda_data_select_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	GdaDataSelect *imodel;
	gint ncols;
	GdaHolder *holder;
	gchar *str;

	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			     "%s", _("Modifications are not allowed anymore"));
		return FALSE;
	}
	if (! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Data model does only support random access"));
		return FALSE;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No UPDATE statement provided"));
		return FALSE;
	}

	/* arguments check */
	ncols = gda_data_select_get_n_columns (model);
	if (col >= ncols) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d out of range (0-%d)"), col, ncols-1);
		return FALSE;
	}
	
	/* invalidate all the imodel->priv->sh->modif_internals->modif_set's value holders */
	GSList *list;
	for (list = imodel->priv->sh->modif_internals->modif_set->holders; list; list = list->next) {
		GdaHolder *h = (GdaHolder*) list->data;
		if (param_name_to_int (gda_holder_get_id (h), NULL, NULL))
			gda_holder_force_invalid ((GdaHolder*) list->data);
	}

	/* give values to params for new value */
	str = g_strdup_printf ("+%d", col);
	holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
	g_free (str);
	if (! holder) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d can't be modified"), col);
		return FALSE;
	}
	if (! gda_holder_set_value (holder, value, error)) 
		return FALSE;

	/* BVector */
	BVector *bv;
	bv = g_new (BVector, 1);
	bv->size = col + 1;
	bv->data = g_new0 (guchar, bv->size);
	bv->data[col] = 1;

	return vector_set_value_at (imodel, bv, NULL, row, error);
}

static gboolean
gda_data_select_iter_set_value  (GdaDataModel *model, GdaDataModelIter *iter, gint col,
				 const GValue *value, GError **error)
{
	GdaDataSelect *imodel;
	gint ncols;
	GdaHolder *holder;
	gchar *str;

	imodel = (GdaDataSelect *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			     "%s", _("Modifications are not allowed anymore"));
		return FALSE;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No UPDATE statement provided"));
		return FALSE;
	}

	/* arguments check */
	ncols = gda_data_select_get_n_columns (model);
	if (col >= ncols) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d out of range (0-%d)"), col, ncols-1);
		return FALSE;
	}
	
	/* invalidate all the imodel->priv->sh->modif_internals->modif_set's value holders */
	GSList *list;
	for (list = imodel->priv->sh->modif_internals->modif_set->holders; list; list = list->next) {
		GdaHolder *h = (GdaHolder*) list->data;
		if (param_name_to_int (gda_holder_get_id (h), NULL, NULL))
			gda_holder_force_invalid ((GdaHolder*) list->data);
	}

	/* give values to params for new value */
	str = g_strdup_printf ("+%d", col);
	holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
	g_free (str);
	if (! holder) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Column %d can't be modified"), col);
		return FALSE;
	}
	if (! gda_holder_set_value (holder, value, error)) 
		return FALSE;

	/* BVector */
	BVector *bv;
	bv = g_new (BVector, 1);
	bv->size = col + 1;
	bv->data = g_new0 (guchar, bv->size);
	bv->data[col] = 1;

	return vector_set_value_at (imodel, bv, iter, G_MININT, error);
}

static void
delayed_select_stmt_free (DelayedSelectStmt *dstmt)
{
	if (dstmt->select)
		g_object_unref (dstmt->select);
	if (dstmt->params)
		g_object_unref (dstmt->params);
	if (dstmt->row)
		g_object_unref (dstmt->row);
	g_free (dstmt);
}

static gboolean
gda_data_select_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataSelect *imodel;
	gint i, ncols, nvalues;
	GdaHolder *holder;
	gchar *str;
	GList *list;

	imodel = (GdaDataSelect *) model;

	/* arguments check */
	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			     "%s", _("Modifications are not allowed anymore"));
		return FALSE;
	}
	if (! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Data model does only support random access"));
		return FALSE;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [UPD_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No UPDATE statement provided"));
		return FALSE;
	}
 
	ncols = gda_data_select_get_n_columns (model);
	nvalues = g_list_length (values);
	if (nvalues > ncols) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     _("Too many values (%d as maximum)"), ncols);
		return FALSE;
	}

	/* BVector */
	BVector *bv;
	gboolean has_mods = FALSE;

	bv = g_new (BVector, 1);
	bv->size = nvalues;
	bv->data = g_new0 (guchar, bv->size);
	for (i = 0, list = values; list; i++, list = list->next) {
		if (list->data) {
			bv->data[i] = 1;
			has_mods = TRUE;
		}
	}
	if (!has_mods) {
		/* no actual modification to do */
		bvector_free (bv);
		return TRUE;
	}

	/* invalidate all the imodel->priv->sh->modif_internals->modif_set's value holders */
	GSList *slist;
	for (slist = imodel->priv->sh->modif_internals->modif_set->holders; slist; slist = slist->next) {
		GdaHolder *h = (GdaHolder*) slist->data;
		if (param_name_to_int (gda_holder_get_id (h), NULL, NULL))
			gda_holder_force_invalid ((GdaHolder*) slist->data);
	}

	/* give values to params for new values */
	for (i = 0, list = values; list; i++, list = list->next) {
		if (!bv->data[i])
			continue;

		str = g_strdup_printf ("+%d", i);
		holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
		g_free (str);
		if (holder && ! gda_holder_set_value (holder, (GValue *) list->data, error)) {
			bvector_free (bv);
			return FALSE;
		}
	}

	return vector_set_value_at (imodel, bv, NULL, row, error);
}

static gint
gda_data_select_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataSelect *imodel;
	gint row, int_row, i;
	GdaHolder *holder;
	gchar *str;
	const GList *list;

	imodel = (GdaDataSelect *) model;

	g_return_val_if_fail (imodel->priv, -1);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			     "%s", _("Modifications are not allowed anymore"));
		return -1;
	}
	if (! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Data model does only support random access"));
		return -1;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [INS_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No INSERT statement provided"));
		return -1;
	}
	if (gda_data_select_get_n_rows (model) < 0) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_ACCESS_ERROR,
			     "%s", _("Cannot add a row because the number of rows in unknown"));
		return -1;
	}

	/* compute added row's number */
	row = imodel->advertized_nrows;
	if (imodel->priv->sh->del_rows)
		row -= imodel->priv->sh->del_rows->len;
	imodel->advertized_nrows++;
	int_row = external_to_internal_row (imodel, row, error);
	imodel->advertized_nrows--;

	/* BVector */
	BVector *bv;
	gboolean has_mods = FALSE, free_bv = TRUE;

	bv = g_new (BVector, 1);
	bv->size = g_list_length ((GList*) values);
	bv->data = g_new0 (guchar, bv->size);
	for (i = 0, list = values; list; i++, list = list->next) {
		if (list->data) {
			bv->data[i] = 1;
			has_mods = TRUE;
		}
	}
	if (!has_mods) {
		/* no actual modification to do */
		bvector_free (bv);
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("Missing values to insert in INSERT statement"));
		
		return -1;
	}

	/* compute INSERT statement */
	GdaStatement *stmt;
	if (! imodel->priv->sh->modif_internals->ins_stmts)
		imodel->priv->sh->modif_internals->ins_stmts = g_hash_table_new_full ((GHashFunc) bvector_hash, 
										  (GEqualFunc) bvector_equal, 
										  (GDestroyNotify) bvector_free, 
										  g_object_unref);
	stmt = g_hash_table_lookup (imodel->priv->sh->modif_internals->ins_stmts, bv);
	if (! stmt) {
		stmt = compute_single_insert_stmt (imodel, bv, error);
		if (stmt) {
			free_bv = FALSE;
			g_hash_table_insert (imodel->priv->sh->modif_internals->ins_stmts, bv, stmt);
		}
		else {
			bvector_free (bv);
			return -1;
		}
	}

	/* give values to params for new values */
	for (i = 0, list = values; list; i++, list = list->next) {
		if (!bv->data[i])
			continue;

		str = g_strdup_printf ("+%d", i);
		holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
		g_free (str);
		if (! holder) {
			g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
				     _("Column %d can't be modified"), i);
			if (free_bv)
				bvector_free (bv);
			return -1;
		}
		if (! gda_holder_set_value (holder, (GValue *) list->data, error)) {
			if (free_bv)
				bvector_free (bv);
			return -1;
		}
	}

	if (free_bv)
		bvector_free (bv);

	/* actually execute INSERT statement */
#ifdef GDA_DEBUG_NO
	gchar *sql;
	GError *lerror = NULL;
	sql = gda_statement_to_sql_extended (stmt,
					     imodel->priv->cnc,
					     imodel->priv->sh->modif_internals->modif_set, 
					     GDA_STATEMENT_SQL_PRETTY, NULL,
					     &lerror);
	g_print ("%s(): SQL=> %s\n", __FUNCTION__, sql);
	if (!sql) {
		gchar *tmp;
		g_print ("\tERR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
		tmp = gda_statement_serialize (stmt);
		g_print ("\tSER: %s\n", tmp);
		g_free (tmp);
	}
	g_free (sql);
#endif

	if (! imodel->priv->sh->modif_internals->one_row_select_stmt) {
		imodel->priv->sh->modif_internals->one_row_select_stmt = compute_single_select_stmt (imodel, error);
		if (!imodel->priv->sh->modif_internals->one_row_select_stmt)
			return -1;
	}

	GdaSet *last_insert;
	if (gda_connection_statement_execute_non_select (imodel->priv->cnc, stmt,
							 imodel->priv->sh->modif_internals->modif_set,
							 &last_insert, error) == -1)
		return -1;

	/* mark that this row has been modified */
	DelayedSelectStmt *dstmt;
	dstmt = g_new0 (DelayedSelectStmt, 1);
	if (last_insert && imodel->priv->sh->modif_internals->one_row_select_stmt) {
		dstmt->select = g_object_ref (imodel->priv->sh->modif_internals->one_row_select_stmt);
		gda_statement_get_parameters (dstmt->select, &(dstmt->params), NULL);
		if (dstmt->params) {
			GSList *list;
			if (! imodel->priv->sh->modif_internals->insert_to_select_mapping)
				imodel->priv->sh->modif_internals->insert_to_select_mapping = 
					compute_insert_select_params_mapping (dstmt->params, last_insert,
									      imodel->priv->sh->modif_internals->unique_row_condition);
			if (imodel->priv->sh->modif_internals->insert_to_select_mapping) {
				for (list = dstmt->params->holders; list; list = list->next) {
					GdaHolder *holder = GDA_HOLDER (list->data);
					GdaHolder *eholder;
					gint pos;
					
					g_assert (param_name_to_int (gda_holder_get_id (holder), &pos, NULL));
					
					eholder = g_slist_nth_data (last_insert->holders, 
								    imodel->priv->sh->modif_internals->insert_to_select_mapping[pos]);
					if (!eholder || 
					    ! gda_holder_set_value (holder, gda_holder_get_value (eholder), error)) {
						g_object_unref (dstmt->params);
						dstmt->params = NULL;
						break;
					}
				}
			}
		}
	}
	if (last_insert)
		g_object_unref (last_insert);

	dstmt->row = NULL;
	if (! imodel->priv->sh->upd_rows)
		imodel->priv->sh->upd_rows = g_hash_table_new_full (g_int_hash, g_int_equal,
								g_free, 
								(GDestroyNotify) delayed_select_stmt_free);
	gint *tmp = g_new (gint, 1);
	*tmp = int_row;
	g_hash_table_insert (imodel->priv->sh->upd_rows, tmp, dstmt);
#ifdef GDA_DEBUG_NO
	dump_d (imodel);
#endif
	imodel->advertized_nrows++;
	gda_data_model_row_inserted ((GdaDataModel *) imodel, row);

	return row;
}


static gboolean
gda_data_select_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataSelect *imodel;
	gint int_row, i, ncols, index;
	GdaHolder *holder;
	gchar *str;

	imodel = (GdaDataSelect *) model;

	g_return_val_if_fail (imodel->priv, FALSE);

	if (imodel->priv->sh->modif_internals->safely_locked) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_SAFETY_LOCKED_ERROR,
			     "%s", _("Modifications are not allowed anymore"));
		return FALSE;
	}
	if (! (imodel->priv->sh->usage_flags & GDA_DATA_MODEL_ACCESS_RANDOM)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", _("Data model does only support random access"));
		return FALSE;
	}
	if (! imodel->priv->sh->modif_internals->modif_stmts [DEL_QUERY]) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
			     "%s", _("No DELETE statement provided"));
		return FALSE;
	}

	int_row = external_to_internal_row (imodel, row, error);
	if (int_row < 0)
		return FALSE;

	ncols = gda_data_select_get_n_columns (model);
	for (i = 0; i < ncols; i++) {
		str = g_strdup_printf ("-%d", i);
		holder = gda_set_get_holder (imodel->priv->sh->modif_internals->modif_set, str);
		g_free (str);
		if (holder) {
			const GValue *cvalue;
			cvalue = gda_data_model_get_value_at (model, i, int_row, error);
			if (!cvalue)
				return FALSE;
			if (! gda_holder_set_value (holder, cvalue, error))
				return FALSE;
		}
	}

#ifdef GDA_DEBUG_NO
	gchar *sql;
	GError *lerror = NULL;
	sql = gda_statement_to_sql_extended (imodel->priv->sh->modif_internals->modif_stmts [DEL_QUERY],
					     imodel->priv->cnc,
					     imodel->priv->sh->modif_internals->modif_set, 
					     GDA_STATEMENT_SQL_PRETTY, NULL,
					     &lerror);
	g_print ("%s(): SQL=> %s\n", __FUNCTION__, sql);
	if (!sql)
		g_print ("\tERR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
	g_free (sql);
#endif
	if (gda_connection_statement_execute_non_select (imodel->priv->cnc, 
							 imodel->priv->sh->modif_internals->modif_stmts [DEL_QUERY],
							 imodel->priv->sh->modif_internals->modif_set, NULL, error) == -1)
		return FALSE;

	/* mark that this row has been removed */
	if (!imodel->priv->sh->del_rows)
		imodel->priv->sh->del_rows = g_array_new (FALSE, FALSE, sizeof (gint));
	for (index = 0, i = 0; i < imodel->priv->sh->del_rows->len; i++, index++) {
		if (g_array_index (imodel->priv->sh->del_rows, gint, i) >= int_row)
			break;
	}

	g_array_insert_val (imodel->priv->sh->del_rows, index, int_row);
	gda_data_model_row_removed (model, row);

	return TRUE;
}

/*
 * The following function creates a correspondance between the parameters required to
 * execute the model->one_row_select_stmt statement (GdaHolders named "-<num>", in ), and the GdaHolder
 * returned after having executed the model->modif_stmts[INS_QUERY] INSERT statement.
 *
 * The way of preceeding is: 
 *   - for each parameter required by model->one_row_select_stmt statement (the @sel_params argument), 
 *     use the model->priv->sh->modif_internals->unique_row_condition to get the name of the corresponding column (the GdaHolder's ID
 *     is "-<num1>" )
 *   - from the column name get the GdaHolder in the GdaSet retruned after the INSERT statement (the
 *     @ins_values argument) using the "name" property of each GdaHolder in the GdaSet (the GdaHolder's ID
 *     is "+<num2>" )
 *   - add an entry in the returned array: array[num1] = num2
 */
typedef struct {
	const gchar *hid;
	const gchar *colid;
} CorrespData;
static gboolean compute_insert_select_params_mapping_foreach_func (GdaSqlAnyPart *part, CorrespData *data, GError **error);
static gint *
compute_insert_select_params_mapping (GdaSet *sel_params, GdaSet *ins_values, GdaSqlExpr *row_cond)
{
	GArray *array;
	gint *retval;
	GSList *sel_list;

	array = g_array_new (FALSE, TRUE, sizeof (gint));
	for (sel_list = sel_params->holders; sel_list; sel_list = sel_list->next) {
		CorrespData cdata;
		const gchar *pid = gda_holder_get_id (GDA_HOLDER (sel_list->data));
		cdata.hid = pid;
		cdata.colid = NULL;

		gint spnum, ipnum;
		gboolean spold, ipold;
		if (! param_name_to_int (pid, &spnum, &spold) || !spold)
			continue;

		if (gda_sql_any_part_foreach (GDA_SQL_ANY_PART (row_cond),
					      (GdaSqlForeachFunc) compute_insert_select_params_mapping_foreach_func,
					      &cdata, NULL)) {
			g_warning ("Could not find column for parameter '%s'", cdata.hid);
			goto onerror;
		}
		g_assert (cdata.colid);
		if ((*(cdata.colid) == '"') || (*(cdata.colid) == '`'))
			gda_sql_identifier_remove_quotes ((gchar*) cdata.colid);
		/*g_print ("SEL param '%s' <=> column named '%s'\n", cdata.hid, cdata.colid);*/
		
		GSList *ins_list;
		cdata.hid = NULL;
		for (ins_list = ins_values->holders; ins_list; ins_list = ins_list->next) {
			gchar *name;
			g_object_get (G_OBJECT (ins_list->data), "name", &name, NULL);
			if (!name) {
				g_warning ("Provider does not report column names");
				goto onerror;
			}
			if (!strcmp (name, cdata.colid)) {
				cdata.hid = gda_holder_get_id (GDA_HOLDER (ins_list->data));
				g_free (name);
				break;
			}
			g_free (name);
		}

		if (!cdata.hid) {
			g_warning ("Could not find column named '%s'", cdata.colid);
			goto onerror;
		}

		/*g_print ("column named '%s' <=> INS param '%s'\n", cdata.colid, cdata.hid);*/

		if (! param_name_to_int (cdata.hid, &ipnum, &ipold) || ipold) {
			g_warning ("Provider reported a malformed parameter named '%s'", cdata.hid);
			goto onerror;
		}

		g_array_insert_val (array, spnum, ipnum);
	}

	retval = (gint *) array->data;
	g_array_free (array, FALSE);
	return retval;

 onerror:
	g_array_free (array, TRUE);
	return NULL;
}

static gboolean
compute_insert_select_params_mapping_foreach_func (GdaSqlAnyPart *part, CorrespData *data, GError **error)
{
	if (part->type != GDA_SQL_ANY_SQL_OPERATION) 
		return TRUE;

	GdaSqlOperation *op = (GdaSqlOperation*) part;
	if (op->operator_type != GDA_SQL_OPERATOR_TYPE_EQ)
		return TRUE;
	
	if (!op->operands || !op->operands->data || !op->operands->next || !op->operands->next->data ||
	    op->operands->next->next)
		return TRUE;

	GdaSqlExpr *e1, *e2;
	e1 = (GdaSqlExpr *) op->operands->data;
	e2 = (GdaSqlExpr *) op->operands->next->data;
	if (e2->value && e1->param_spec) {
		/* swap e1 and e2 */
		GdaSqlExpr *ex = e1;
		e1 = e2;
		e2 = ex;
	}
	if (e1->value && e2->param_spec) {
		/* e1->value should be a column name */
		/* e2->param_spec should be a parameter */
		if (e2->param_spec->name && !strcmp (e2->param_spec->name, data->hid)) {
			if (G_VALUE_TYPE (e1->value) != G_TYPE_STRING)
				return TRUE;
			data->colid = g_value_get_string (e1->value);
			if (* data->colid)
				return FALSE;
			data->colid = NULL;
			return TRUE;
		}
	}
	return TRUE;
}

static void
set_column_properties_from_select_stmt (GdaDataSelect *model, GdaConnection *cnc, GdaStatement *sel_stmt)
{
	GdaSqlStatement *sqlst = NULL;
	GdaSqlStatementSelect *select;
	GdaSqlSelectTarget *target;
	GSList *fields, *columns;

	g_object_get (G_OBJECT (sel_stmt), "structure", &sqlst, NULL);
	g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT);
	select = (GdaSqlStatementSelect*) sqlst->contents;

	/* we only want a single target */
	if (!select->from || !select->from->targets || select->from->targets->next)
		goto out;
	
	target = (GdaSqlSelectTarget *) select->from->targets->data;
	if (!target || !target->table_name)
		goto out;

	if (! gda_sql_statement_check_validity (sqlst, cnc, NULL))
		goto out;

	if (!target->validity_meta_object) {
		g_warning ("Internal gda_sql_statement_check_validity() error: target->validity_meta_object is not set");
		goto out;
	}

	/* FIXME: also set some column attributes using gda_column_set_attribute() */

	for (fields = select->expr_list, columns = model->priv->sh->columns; 
	     fields && columns; 
	     fields = fields->next) {
		GdaSqlSelectField *selfield = (GdaSqlSelectField*) fields->data;
		if (selfield->validity_meta_table_column) {
			GdaMetaTableColumn *tcol = selfield->validity_meta_table_column;

			/*g_print ("==> %s\n", tcol->column_name);*/
			gda_column_set_allow_null (GDA_COLUMN (columns->data), tcol->nullok);
			if (tcol->default_value) {
				GValue *dvalue;
				g_value_set_string ((dvalue = gda_value_new (G_TYPE_STRING)), tcol->default_value);
				gda_column_set_default_value (GDA_COLUMN (columns->data), dvalue);
				gda_value_free (dvalue);
			}
			columns = columns->next;
		}
		else if (selfield->validity_meta_object && 
			 (selfield->validity_meta_object->obj_type == GDA_META_DB_TABLE) &&
			 selfield->expr && selfield->expr->value && !selfield->expr->param_spec && 
			 (G_VALUE_TYPE (selfield->expr->value) == G_TYPE_STRING) &&
			 !strcmp (g_value_get_string (selfield->expr->value), "*")) {
			/* expand all the fields */
			GdaMetaTable *mtable = GDA_META_TABLE (selfield->validity_meta_object);
			GSList *tmplist;
			for (tmplist = mtable->columns; tmplist; tmplist = tmplist->next) {
				GdaMetaTableColumn *tcol = (GdaMetaTableColumn*) tmplist->data;
				/*g_print ("*==> %s\n", tcol->column_name);*/
				gda_column_set_allow_null (GDA_COLUMN (columns->data), tcol->nullok);
				if (tcol->default_value) {
					GValue *dvalue;
					g_value_set_string ((dvalue = gda_value_new (G_TYPE_STRING)), tcol->default_value);
					gda_column_set_default_value (GDA_COLUMN (columns->data), dvalue);
					gda_value_free (dvalue);
				}
				if (tmplist)
					columns = columns->next;
			}
		}
		else
			columns = columns->next;
	}
	if (fields || columns)
		g_warning ("Internal error: GdaDataSelect has %d GdaColumns, and SELECT statement has %d expressions",
			   g_slist_length (model->priv->sh->columns), g_slist_length (select->expr_list));

 out:
	gda_sql_statement_free (sqlst);
}


/**
 * gda_data_select_compute_columns_attributes
 * @model: a #GdaDataSelect data model
 * @error: a place to store errors, or %NULL
 *
 * Computes correct attributes for each of @model's columns, which includes the "NOT NULL" attribute, the
 * default value, the precision and scale for numeric values.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_select_compute_columns_attributes (GdaDataSelect *model, GError **error)
{
	GdaStatement *sel_stmt;

	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);
	g_return_val_if_fail (model->priv, FALSE);

	sel_stmt = check_acceptable_statement (model, error);
	if (! sel_stmt)
		return FALSE;

	if (!model->priv->cnc) {
		g_set_error (error, GDA_DATA_SELECT_ERROR, GDA_DATA_SELECT_CONNECTION_ERROR,
			     "%s", _("No connection to use"));
		return FALSE;
	}

	set_column_properties_from_select_stmt (model, model->priv->cnc, sel_stmt);

	return TRUE;
}

/**
 * gda_data_select_rerun
 * @model: a #GdaDataSelect data model
 * @error: a place to store errors, or %NULL
 *
 * Requests that @model be re-run to have an updated result. If an error occurs,
 * then @model will not be changed.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2
 */
gboolean
gda_data_select_rerun (GdaDataSelect *model, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), FALSE);

	GdaDataSelect *new_model;
	GdaStatement *select;

	select = check_acceptable_statement (model, error);
	if (!select)
		return FALSE;
	g_assert (model->prep_stmt);
	GType *types = NULL;
	if (model->prep_stmt->types) {
		types = g_new (GType, model->prep_stmt->ncols + 1);
		memcpy (types, model->prep_stmt->types, sizeof (GType) * model->prep_stmt->ncols);
		types [model->prep_stmt->ncols] = G_TYPE_NONE;
	}
	new_model = (GdaDataSelect*) gda_connection_statement_execute_select_full (model->priv->cnc, select, 
										   model->priv->sh->ext_params, 
										   model->priv->sh->usage_flags | GDA_STATEMENT_MODEL_ALLOW_NOPARAM,
										   types, 
										   error);
	g_free (types);
	if (!new_model) {
		/* FIXME: clear all the rows in @model, and emit the "reset" signal */
		return FALSE;
	}

	g_assert (G_OBJECT_TYPE (model) == G_OBJECT_TYPE (new_model));

	/* Raw model and new_model contents swap (except for the GObject part) */
	GTypeQuery tq;
	gpointer copy;
	gint offset = sizeof (GObject);
	gint size;
	g_type_query (G_OBJECT_TYPE (model), &tq);
	size = tq.instance_size - offset;
	copy = g_malloc (size);
	memcpy (copy, (gint8*) new_model + offset, size);
	memcpy ((gint8*) new_model + offset, (gint8*) model + offset, size);
	memcpy ((gint8*) model + offset, copy, size);
		
	/* we need to keep some data from the old model */
	GdaDataSelect *old_model = new_model; /* renamed for code's readability */
	GdaDataSelectInternals *mi;
	      
	model->priv->sh->reset_with_ext_params_change = old_model->priv->sh->reset_with_ext_params_change;
	mi = old_model->priv->sh->modif_internals;
	old_model->priv->sh->modif_internals = model->priv->sh->modif_internals;
	model->priv->sh->modif_internals = mi;

	copy = old_model->priv->sh->sel_stmt;
	old_model->priv->sh->sel_stmt = model->priv->sh->sel_stmt;
	model->priv->sh->sel_stmt = (GdaStatement*) copy;

	/* keep the same GdaColumn pointers */
	GSList *l1, *l2;
	l1 = old_model->priv->sh->columns;
	old_model->priv->sh->columns = model->priv->sh->columns;
	model->priv->sh->columns = l1;
	for (l1 = model->priv->sh->columns, l2 = old_model->priv->sh->columns;
	     l1 && l2;
	     l1 = l1->next, l2 = l2->next) {
		if ((gda_column_get_g_type ((GdaColumn*) l1->data) == GDA_TYPE_NULL) &&
		    (gda_column_get_g_type ((GdaColumn*) l2->data) != GDA_TYPE_NULL))
			gda_column_set_g_type ((GdaColumn*) l1->data, 
					       gda_column_get_g_type ((GdaColumn*) l2->data));
	}

	g_object_unref (old_model);

	/* copy all the param's holders' values from model->priv->sh->ext_params to 
	   to model->priv->sh->modif_internals->exec_set */
	GSList *list;
	if (model->priv->sh->ext_params) {
		for (list = model->priv->sh->ext_params->holders; list; list = list->next) {
			GdaHolder *h;
			h = gda_set_get_holder (model->priv->sh->modif_internals->exec_set,
						gda_holder_get_id (list->data));
			if (h) {
				GError *lerror = NULL;
				if (!gda_holder_is_valid (GDA_HOLDER (list->data))) 
					gda_holder_set_value (h, gda_holder_get_value (GDA_HOLDER (list->data)), NULL);
				else if (! gda_holder_set_value (h, gda_holder_get_value (GDA_HOLDER (list->data)),
								 &lerror)) {
					g_warning (_("An error has occurred, the value returned by the \"exec-params\" "
						     "property will be wrong: %s"),
						   lerror && lerror->message ? lerror->message : _("No detail"));
					g_clear_error (&lerror);
				}
			}
		}
	}

	/* signal a reset */
	gda_data_model_reset ((GdaDataModel*) model);

	return TRUE;
}
