/* gda-data-proxy.c
 *
 * Copyright (C) 2005 - 2007 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-data-proxy.h"
#include "gda-data-model.h"
#include "gda-data-model-extra.h"
#include "gda-data-model-iter.h"
#include "gda-parameter.h"
#include "gda-parameter-list.h"
#include "gda-marshal.h"
#include "gda-enums.h"
#include "gda-util.h"
#include "gda-marshal.h"
#include "gda-data-access-wrapper.h"
#include "gda-enum-types.h"
#include <virtual/libgda-virtual.h>
#include "gda-query.h"
#include "gda-renderer.h"

/* 
 * Main static functions 
 */
static void gda_data_proxy_class_init (GdaDataProxyClass * class);
static void gda_data_proxy_init (GdaDataProxy *srv);
static void gda_data_proxy_dispose (GObject *object);
static void gda_data_proxy_finalize (GObject *object);

static void gda_data_proxy_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_data_proxy_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);
/* GdaDataModel interface */
static void                 gda_data_proxy_data_model_init (GdaDataModelClass *iface);

static gint                 gda_data_proxy_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_proxy_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_proxy_describe_column (GdaDataModel *model, gint col);
static const GValue        *gda_data_proxy_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_data_proxy_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_proxy_create_iter     (GdaDataModel *model);


static GdaDataModelAccessFlags gda_data_proxy_get_access_flags(GdaDataModel *model);

static gboolean             gda_data_proxy_set_value_at    (GdaDataModel *model, gint col, gint row, 
							    const GValue *value, GError **error);
static gboolean             gda_data_proxy_set_values      (GdaDataModel *model, gint row, 
							    GList *values, GError **error);
static gint                 gda_data_proxy_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_proxy_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_proxy_remove_row      (GdaDataModel *model, gint row, GError **error);

static void                 gda_data_proxy_set_notify      (GdaDataModel *model, gboolean do_notify_changes);
static gboolean             gda_data_proxy_get_notify      (GdaDataModel *model);
static void                 gda_data_proxy_send_hint       (GdaDataModel *model, GdaDataModelHint hint, 
							    const GValue *hint_value);


static void destroyed_object_cb (GdaObject *obj, GdaDataProxy *proxy);
#ifdef GDA_DEBUG
static void gda_data_proxy_dump (GdaDataProxy *proxy, guint offset);
#endif
#define DEBUG_SYNC
#undef DEBUG_SYNC

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
        ROW_DELETE_CHANGED,
	SAMPLE_SIZE_CHANGED,
	SAMPLE_CHANGED,
	PRE_CHANGES_APPLIED,
	POST_CHANGES_APPLIED,
	FILTER_CHANGED,
        LAST_SIGNAL
};

static gint gda_data_proxy_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0 };


/* properties */
enum
{
	PROP_0,
	PROP_MODEL,
	PROP_ADD_NULL_ENTRY,
	PROP_DEFER_SYNC,
	PROP_SAMPLE_SIZE
};

/*
 * Structure to hold the status and all the modifications made to a single
 * row. It is not possible to have ((model_row < 0) && !modify_values)
 */
typedef struct
{
	gint           model_row;    /* row index in the GdaDataModel, -1 if new row */
	gboolean       to_be_deleted;/* TRUE if row is to be deleted */
	GSList        *modify_values; /* list of RowValue structures */
	GValue       **orig_values;  /* array of the original GValues, indexed on the column numbers */
	gint           orig_values_size;
} RowModif;
#define ROW_MODIF(x) ((RowModif *)(x))

/*
 * Structure to hold the modifications made to a single value
 */
typedef struct
{
	RowModif      *row_modif;    /* RowModif in which this structure instance appears */
	gint           model_column; /* column index in the GdaDataModel */
        GValue        *value;        /* can also be GDA_TYPE_LIST for multiple values; values are owned here */
        GValue        *attributes;   /* holds flags of GdaValueAttribute */
} RowValue;
#define ROW_VALUE(x) ((RowValue *)(x))

/*
 * Structure to hold which part of the whole data is displayed
 */
typedef struct {
	GArray *mapping; /* for proxy's row nb i (or i+1 if
			  * there is a NULL rows first), mapping[i] points to
			  * the absolute row.
			  *
			  * If mapping is empty, then either there are no
			  * rows to display, or the number of rows is unknown
			  * (then it's not filled until row existance can be testes
			  */
} DisplayChunck;
static DisplayChunck *display_chunk_new (gint reserved_size);
static void           display_chunk_free (DisplayChunck *chunk);

/*
 * NOTE about the row numbers:
 * 
 * There may be more rows in the GdaDataProxy than in the GdaDataModel if:
 *  - some rows have been added in the proxy
 *    and are not yet in the GdaDataModel (RowModif->model_row = -1 in this case); or
 *  - there is a NULL row at the beginning
 * 
 * There are 3 kinds of row numbers: 
 *  - the absolute row numbers which unify under a single numbering scheme all the possible proxy's rows
 *    which can either be new rows, or data model rows (which may or may not have been modified),
 *    absolute row numbers are strictly private to the proxy object
 *  - the row numbers as identified in the proxy (named "proxy_row"), which are the row numbers used
 *    by the GdaDataModel interface
 *  - the row numbers in the GdaDataModel being proxied (named "model_row")
 *
 * Absolute row numbers are assigned as:
 *  - rows 0 to N-1 included corresponding to the N rows of the proxied data model
 *  - rows N to N+M-1 included corresponding to the M added rows
 *
 * The rule to go from one row numbering to the other is to use the row conversion functions.
 *
 */
struct _GdaDataProxyPrivate
{
	GdaDataModel      *model; /* Gda model which is proxied */

	gchar             *filter_expr;   /* NULL if no filter applied */
	GdaDataModel      *filtered_rows; /* NULL if no filter applied. Lists rows (by their number) which must be displayed */

	GValue           **columns_attrs; /* Each GValue holds a flag of GdaValueAttribute to proxy. cols. attributes */

	gint               model_nb_cols; /* = gda_data_model_get_n_columns (model) */
	gint               model_nb_rows; /* = gda_data_model_get_n_rows (model) */
	gboolean           notify_changes;

	GSList            *all_modifs; /* list of RowModif structures, for memory management */
	GSList            *new_rows;   /* list of RowModif, no data allocated in this list */
	GHashTable        *modify_rows;  /* key = model_row number, value = RowModif, NOT for new rows */

	gboolean           ignore_proxied_changes;
	gboolean           proxy_has_changed;

	gboolean           add_null_entry; /* artificially add a NULL entry at the beginning of the tree model */

	gboolean           defer_sync;

	/* chunking */
	gboolean           force_direct_mapping;
	gint               sample_first_row;
	gint               sample_last_row;
	gint               sample_size;
	guint              chunk_sync_idle_id;
	DisplayChunck     *chunk; /* number of proxy_rows depends directly on chunk->mapping->len */
	DisplayChunck     *chunk_to; /* NULL if nothing to do */
	gint               chunk_sep;
	gint               chunk_proxy_nb_rows;
	
	/* for ALL the columns of proxy */
	GdaColumn        **columns;
};


/*
 * Row conversion functions
 */

/*
 * May return -1 if:
 *  - @model_row < 0
 *  - @model_row is out of bounds (only checked if @proxy's number of rows is known)
 */
static gint
model_row_to_absolute_row (GdaDataProxy *proxy, gint model_row)
{
	if (model_row < 0)
		return -1;
	if ((model_row >= proxy->priv->model_nb_rows) && (proxy->priv->model_nb_rows >= 0))
		return -1;
	else
		return model_row;
}

/*
 * May return -1 if:
 *  - @rm is NULL
 *  - @rm is a new row and @proxy's number of rows is NOT know
 *  - @rm is not a new row and @rm->model_row == -1
 */
static gint
row_modif_to_absolute_row (GdaDataProxy *proxy, RowModif *rm)
{
	if (rm == NULL)
		return -1;
	if (rm->model_row == -1) {
		gint index;
		if (proxy->priv->model_nb_rows == -1)
			return -1;
		index = g_slist_index (proxy->priv->new_rows, rm);
		if (index < 0)
			return -1;
		return proxy->priv->model_nb_rows + index;
	}
	else
		return rm->model_row;
}

/*
 * if @rm is not %NULL, then it will contain a pointer to the RowModif structure associated to @row
 *
 * May return -1 if:
 *  - absolute row is not a model row
 */
static gint
absolute_row_to_model_row (GdaDataProxy *proxy, gint abs_row, RowModif **rm)
{
	if (abs_row < 0)
		return -1;
	if ((abs_row < proxy->priv->model_nb_rows) || (proxy->priv->model_nb_rows < 0)) {
		if (rm)
			*rm = g_hash_table_lookup (proxy->priv->modify_rows, GINT_TO_POINTER (abs_row));
		return abs_row;
	}
	else {
		if (rm)
			*rm = g_slist_nth_data (proxy->priv->new_rows, abs_row - proxy->priv->model_nb_rows);
		return -1;
	}
}

/*
 * May return -1 if:
 *  - @row is not an absolute row
 */
static gint
proxy_row_to_absolute_row (GdaDataProxy *proxy, gint proxy_row)
{
	if (proxy_row < 0)
		return -1;
	if (proxy->priv->add_null_entry && !proxy->priv->force_direct_mapping) {
		if (proxy_row == 0)
			return -1;
		else
			proxy_row--;
	}
	if (proxy->priv->chunk && !proxy->priv->force_direct_mapping) {
		if (proxy_row < proxy->priv->chunk->mapping->len)
			return g_array_index (proxy->priv->chunk->mapping, gint, proxy_row);
		else
			return -1;
	}
	else
		return proxy_row;
}

#define proxy_row_to_model_row(proxy, proxy_row) \
	absolute_row_to_model_row ((proxy), proxy_row_to_absolute_row ((proxy), (proxy_row)), NULL);
#define row_modif_to_proxy_row(proxy, rm) \
	absolute_row_to_proxy_row ((proxy), row_modif_to_absolute_row ((proxy), (rm)))


static RowModif *
proxy_row_to_row_modif (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm = NULL;
	absolute_row_to_model_row (proxy, proxy_row_to_absolute_row (proxy, proxy_row), &rm);

	return rm;
}

/*
 * May return -1 if:
 *  - @abs_row is not a proxy row 
 *  - @abs_row is out of bounds (only checked if @proxy's number of rows is known)
 */
static gint
absolute_row_to_proxy_row (GdaDataProxy *proxy, gint abs_row)
{
	gint proxy_row = -1;
	if (abs_row < 0)
		return -1;

	if (proxy->priv->chunk && !proxy->priv->force_direct_mapping) {
		gint i;
		for (i = 0; i < proxy->priv->chunk->mapping->len; i++) {
			if (g_array_index (proxy->priv->chunk->mapping, gint, i) == abs_row) {
				proxy_row = i;
				break;
			}
		}
		if ((proxy_row >= 0) && proxy->priv->add_null_entry)
			proxy_row ++;
	}
	else {
		gint proxy_n_rows;
		proxy_row = abs_row;
		if (proxy->priv->add_null_entry && !proxy->priv->force_direct_mapping)
			proxy_row ++;
		proxy_n_rows = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy);
		if ((proxy_row >= proxy_n_rows) && (proxy_n_rows >= 0))
			proxy_row = -1;
	}

	return proxy_row;
}


/*
 * Free a RowModif structure
 *
 * Warning: only the allocated RowModif is freed, it's not removed from any list or hash table.
 */
static void
row_modifs_free (RowModif *rm)
{
	GSList *list;
	gint i;
		
	list = rm->modify_values;
	while (list) {
		if (ROW_VALUE (list->data)->value)
			gda_value_free (ROW_VALUE (list->data)->value);
		g_free (list->data);
		list = g_slist_next (list);
	}
	g_slist_free (rm->modify_values);
	
	if (rm->orig_values) {
		for (i=0; i < rm->orig_values_size; i++) {
			if (rm->orig_values [i])
				gda_value_free (rm->orig_values [i]);
		}
		g_free (rm->orig_values);
	}
	
	g_free (rm);
}

/*
 * Allocates a new RowModif
 *
 * WARNING: the new RowModif is not inserted in any list or hash table.
 */
static RowModif *
row_modifs_new (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm;
	
#ifdef GDA_DEBUG
	rm = proxy_row_to_row_modif (proxy, proxy_row);
	if (rm) 
		g_warning ("%s(): RowModif already exists for that proxy_row", __FUNCTION__);
#endif
	
	rm = g_new0 (RowModif, 1);
	if (proxy_row >= 0) {
		gint i, model_row;
		
		rm->orig_values = g_new0 (GValue *, proxy->priv->model_nb_cols);
		rm->orig_values_size = proxy->priv->model_nb_cols;
		model_row = proxy_row_to_model_row (proxy, proxy_row);
		if (model_row >= 0) {
			for (i=0; i<proxy->priv->model_nb_cols; i++) {
				const GValue *oval;
				
				oval = gda_data_model_get_value_at (proxy->priv->model, i, model_row);
				if (oval)
					rm->orig_values [i] = gda_value_copy ((GValue *) oval);
			}
		}
	}
	
	return rm;
}

/* module error */
GQuark gda_data_proxy_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_data_proxy_error");
	return quark;
}

GType
gda_data_proxy_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDataProxyClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_proxy_class_init,
			NULL,
			NULL,
			sizeof (GdaDataProxy),
			0,
			(GInstanceInitFunc) gda_data_proxy_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_data_proxy_data_model_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDataProxy", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		
	}
	return type;
}

static gboolean
pre_changes_accumulator (GSignalInvocationHint *ihint,
			 GValue *return_accu,
			 const GValue *handler_return,
			 gpointer data)
{
        gboolean thisvalue;

        thisvalue = g_value_get_boolean (handler_return);
        g_value_set_boolean (return_accu, thisvalue);

        return thisvalue; /* stop signal if 'thisvalue' is FALSE */
}

static gboolean
m_pre_changes_applied (GdaDataProxy *proxy, gint row, gint proxied_row)
{
        return TRUE; /* defaults allows changes */
}

static void
gda_data_proxy_class_init (GdaDataProxyClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* signals */
	gda_data_proxy_signals [ROW_DELETE_CHANGED] =
		g_signal_new ("row_delete_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, row_delete_changed),
                              NULL, NULL,
			      gda_marshal_VOID__INT_BOOLEAN, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_BOOLEAN);
	gda_data_proxy_signals [SAMPLE_SIZE_CHANGED] =
		g_signal_new ("sample_size_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, sample_size_changed),
                              NULL, NULL,
			      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_proxy_signals [SAMPLE_CHANGED] =
		g_signal_new ("sample_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, sample_changed),
                              NULL, NULL,
			      gda_marshal_VOID__INT_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	gda_data_proxy_signals [PRE_CHANGES_APPLIED] =
		g_signal_new ("pre_changes_applied",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, pre_changes_applied),
                              pre_changes_accumulator, NULL,
                              gda_marshal_BOOLEAN__INT_INT, G_TYPE_BOOLEAN, 2, G_TYPE_INT, G_TYPE_INT);
	gda_data_proxy_signals [POST_CHANGES_APPLIED] =
		g_signal_new ("post_changes_applied",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, post_changes_applied),
                              NULL, NULL,
			      gda_marshal_VOID__INT_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	gda_data_proxy_signals [FILTER_CHANGED] = 
		g_signal_new ("filter_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
			      0, /* no associated a class method */
                              NULL, NULL,
			      gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	class->row_delete_changed = NULL;
	class->sample_size_changed = NULL;
	class->sample_changed = NULL;
	class->pre_changes_applied = m_pre_changes_applied;
	class->post_changes_applied = NULL;

	/* virtual functions */
#ifdef GDA_DEBUG
	GDA_OBJECT_CLASS (class)->dump = (void (*)(GdaObject *, guint)) gda_data_proxy_dump;
#endif

	object_class->dispose = gda_data_proxy_dispose;
	object_class->finalize = gda_data_proxy_finalize;

	/* Properties */
	object_class->set_property = gda_data_proxy_set_property;
	object_class->get_property = gda_data_proxy_get_property;

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", _("Data model"), NULL,
                                                               GDA_TYPE_DATA_MODEL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
	g_object_class_install_property (object_class, PROP_ADD_NULL_ENTRY,
					 g_param_spec_boolean ("prepend_null_entry", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DEFER_SYNC,
					 g_param_spec_boolean ("defer_sync", NULL, NULL, TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SAMPLE_SIZE,
					 g_param_spec_int ("sample_size", NULL, NULL, 0, G_MAXINT - 1, 300,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
}

static void
gda_data_proxy_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_data_proxy_get_n_rows;
	iface->i_get_n_columns = gda_data_proxy_get_n_columns;
	iface->i_describe_column = gda_data_proxy_describe_column;
	iface->i_get_access_flags = gda_data_proxy_get_access_flags;
	iface->i_get_value_at = gda_data_proxy_get_value_at;
	iface->i_get_attributes_at = gda_data_proxy_get_attributes_at;

	iface->i_create_iter = gda_data_proxy_create_iter;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

	iface->i_set_value_at = gda_data_proxy_set_value_at;
	iface->i_set_values = gda_data_proxy_set_values;
        iface->i_append_values = gda_data_proxy_append_values;
	iface->i_append_row = gda_data_proxy_append_row;
	iface->i_remove_row = gda_data_proxy_remove_row;
	iface->i_find_row = NULL;

	iface->i_set_notify = gda_data_proxy_set_notify;
	iface->i_get_notify = gda_data_proxy_get_notify;
	iface->i_send_hint = gda_data_proxy_send_hint;

	iface->row_inserted = NULL;
	iface->row_updated = NULL;
	iface->row_removed = NULL;
}

static void
gda_data_proxy_init (GdaDataProxy *proxy)
{
	proxy->priv = g_new0 (GdaDataProxyPrivate, 1);
	proxy->priv->modify_rows = g_hash_table_new (NULL, NULL);
	proxy->priv->notify_changes = TRUE;
	proxy->priv->ignore_proxied_changes = FALSE;
	proxy->priv->proxy_has_changed = FALSE;

	proxy->priv->add_null_entry = FALSE;
	proxy->priv->defer_sync = TRUE;

	proxy->priv->force_direct_mapping = FALSE;
	proxy->priv->sample_size = 0;
	proxy->priv->chunk = NULL;
	proxy->priv->chunk_to = NULL;
	proxy->priv->chunk_sync_idle_id = 0;
	proxy->priv->columns = NULL;
}

static DisplayChunck *compute_display_chunk (GdaDataProxy *proxy);
static void adjust_displayed_chunk (GdaDataProxy *proxy);
static gboolean chunk_sync_idle (GdaDataProxy *proxy);
static void ensure_chunk_sync (GdaDataProxy *proxy);


static void proxied_model_data_changed_cb (GdaDataModel *model, GdaDataProxy *proxy);
static void proxied_model_reset_cb (GdaDataModel *model, GdaDataProxy *proxy);


/**
 * gda_data_proxy_new
 * @model: Data model to be proxied
 *
 * Creates a new proxy for @model
 *
 * Returns: a new #GdaDataProxy object
 */
GObject *
gda_data_proxy_new (GdaDataModel *model)
{
	GObject *obj;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), NULL);

	obj = g_object_new (GDA_TYPE_DATA_PROXY, "dict", gda_object_get_dict (GDA_OBJECT (model)), 
			    "model", model, NULL);

	return obj;
}

static void
destroyed_object_cb (GdaObject *obj, GdaDataProxy *proxy)
{
	gda_object_destroy (GDA_OBJECT (proxy));
}

static void
clean_proxy (GdaDataProxy *proxy)
{
	if (proxy->priv->all_modifs) {
		gda_data_proxy_cancel_all_changes (proxy);
		g_hash_table_destroy (proxy->priv->modify_rows);
		proxy->priv->modify_rows = NULL;
	}

	if (proxy->priv->filter_expr) {
		g_free (proxy->priv->filter_expr);
		proxy->priv->filter_expr = NULL;
	}

	if (proxy->priv->filtered_rows) {
		g_object_unref (proxy->priv->filtered_rows);
		proxy->priv->filtered_rows = NULL;
	}

	proxy->priv->force_direct_mapping = FALSE;
	if (proxy->priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		proxy->priv->chunk_sync_idle_id = 0;
	}

	if (proxy->priv->chunk) {
		display_chunk_free (proxy->priv->chunk);
		proxy->priv->chunk = NULL;
	}
	if (proxy->priv->chunk_to) {
		display_chunk_free (proxy->priv->chunk_to);
		proxy->priv->chunk_to = NULL;
	}
	
	if (proxy->priv->columns) {
		gint i;
		for (i = 0; i < 2 * proxy->priv->model_nb_cols; i++)
			g_object_unref (G_OBJECT (proxy->priv->columns[i]));
		g_free (proxy->priv->columns);
		proxy->priv->columns = NULL;
	}
	
	if (proxy->priv->model) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (proxy->priv->model),
						      G_CALLBACK (proxied_model_data_changed_cb), proxy);
		g_signal_handlers_disconnect_by_func (G_OBJECT (proxy->priv->model),
						      G_CALLBACK (proxied_model_reset_cb), proxy);

		g_signal_handlers_disconnect_by_func (G_OBJECT (proxy->priv->model),
						      G_CALLBACK (destroyed_object_cb), proxy);
		g_object_unref (proxy->priv->model);
		proxy->priv->model = NULL;
	}
	
	if (proxy->priv->columns_attrs) {
		gint i;
		for (i = 0; i < proxy->priv->model_nb_cols; i++)
			gda_value_free ((GValue *)(proxy->priv->columns_attrs[i]));
		g_free (proxy->priv->columns_attrs);
		proxy->priv->columns_attrs = NULL;
	}
}

static void
gda_data_proxy_dispose (GObject *object)
{
	GdaDataProxy *proxy;

	g_return_if_fail (GDA_IS_DATA_PROXY (object));

	proxy = GDA_DATA_PROXY (object);
	if (proxy->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));
		clean_proxy (proxy);
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_data_proxy_finalize (GObject *object)
{
	GdaDataProxy *proxy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_PROXY (object));

	proxy = GDA_DATA_PROXY (object);
	if (proxy->priv) {
		g_free (proxy->priv);
		proxy->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void 
gda_data_proxy_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaDataProxy *proxy;

	proxy = GDA_DATA_PROXY (object);
	if (proxy->priv) {
		GdaDataModel *model;
		gint col;

		switch (param_id) {
		case PROP_MODEL:
			g_assert (!proxy->priv->model);
			model = (GdaDataModel*) g_value_get_object (value);
			g_return_if_fail (GDA_IS_DATA_MODEL (model));

			if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
				g_warning (_("GdaDataProxy can't handle non random access data models"));
				return;
			}
			proxy->priv->model = model;
			g_object_ref (model);
			gda_object_connect_destroy (GDA_OBJECT (model), G_CALLBACK (destroyed_object_cb), object);
			
			proxy->priv->model_nb_cols = gda_data_model_get_n_columns (model);
			proxy->priv->model_nb_rows = gda_data_model_get_n_rows (model);
			
			/* column attributes */
			proxy->priv->columns_attrs = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (col = 0; col < proxy->priv->model_nb_cols; col++) {
				GdaColumn *column;
				GdaValueAttribute flags = GDA_VALUE_ATTR_IS_UNCHANGED;
				
				column = gda_data_model_describe_column (model, col);
				if (gda_column_get_allow_null (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
				if (gda_column_get_default_value (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;
				proxy->priv->columns_attrs[col] = g_value_init (g_new0 (GValue, 1), GDA_TYPE_VALUE_ATTRIBUTE);
				g_value_set_flags (proxy->priv->columns_attrs[col], flags);
			}
			
			g_signal_connect (G_OBJECT (model), "changed",
					  G_CALLBACK (proxied_model_data_changed_cb), proxy);
			g_signal_connect (G_OBJECT (model), "reset",
					  G_CALLBACK (proxied_model_reset_cb), proxy);
			
			/* initial chunk settings, no need to emit any signal as it's an initial state */
			proxy->priv->chunk = compute_display_chunk (proxy);
			if (!proxy->priv->chunk->mapping) {
				display_chunk_free (proxy->priv->chunk);
				proxy->priv->chunk = NULL;
			}
			break;
		case PROP_ADD_NULL_ENTRY:
			if (proxy->priv->add_null_entry != g_value_get_boolean (value)) {
				proxy->priv->add_null_entry = g_value_get_boolean (value);

				if (proxy->priv->add_null_entry)
					gda_data_model_row_inserted ((GdaDataModel *) proxy, 0);
				else
					gda_data_model_row_removed ((GdaDataModel *) proxy, 0);
			}
			break;
		case PROP_DEFER_SYNC:
			proxy->priv->defer_sync = g_value_get_boolean (value);
			if (!proxy->priv->defer_sync && proxy->priv->chunk_sync_idle_id) {
				g_idle_remove_by_data (proxy);
				proxy->priv->chunk_sync_idle_id = 0;
				chunk_sync_idle (proxy);
			}
			break;
		case PROP_SAMPLE_SIZE:
			proxy->priv->sample_size = g_value_get_int (value);
			if (proxy->priv->sample_size < 0)
				proxy->priv->sample_size = 0;

			/* initial chunk settings, no need to emit any signal as it's an initial state */
			proxy->priv->chunk = compute_display_chunk (proxy);
			if (!proxy->priv->chunk->mapping) {
				display_chunk_free (proxy->priv->chunk);
				proxy->priv->chunk = NULL;
			}
			break;
		}
	}
}

static void
gda_data_proxy_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaDataProxy *proxy;

	proxy = GDA_DATA_PROXY (object);
	if (proxy->priv) {
		switch (param_id) {
		case PROP_ADD_NULL_ENTRY:
			g_value_set_boolean (value, proxy->priv->add_null_entry);
			break;
		case PROP_DEFER_SYNC:
			g_value_set_boolean (value, proxy->priv->defer_sync);
			break;
		case PROP_SAMPLE_SIZE:
			g_value_set_int (value, proxy->priv->sample_size);
			break;
		}
	}
}

/*
 * Callback called when the contents of the proxied data model has changed.
 * All the changes made in the proxy are discarded.
 *
 * REM: 
 * We should try to re-map all the modifications to the new data, and the modifications which can't be
 * re-mapped could be discarded, but this means we would need a way to identify columns (the equivalent
 * of a primary key for the proxied data model).
 */
static void
proxied_model_data_changed_cb (GdaDataModel *model, GdaDataProxy *proxy)
{
	gint nb_cols;

	if (proxy->priv->ignore_proxied_changes) {
		proxy->priv->proxy_has_changed = TRUE;
		return;
	}
	proxy->priv->proxy_has_changed = FALSE;

	/* stop idle adding of rows */
	if (proxy->priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		proxy->priv->chunk_sync_idle_id = 0;
	}

	proxy->priv->force_direct_mapping = FALSE;
	if (proxy->priv->chunk) {
		display_chunk_free (proxy->priv->chunk);
		proxy->priv->chunk = NULL;
	}

	if (proxy->priv->chunk_to) {
		display_chunk_free (proxy->priv->chunk_to);
		proxy->priv->chunk_to = NULL;
	}

	/* Free memory for the modifications */
	while (proxy->priv->all_modifs) {
		gint model_row = ROW_MODIF (proxy->priv->all_modifs->data)->model_row;
		row_modifs_free (ROW_MODIF (proxy->priv->all_modifs->data));
		if (model_row >= 0)
			g_hash_table_remove (proxy->priv->modify_rows, GINT_TO_POINTER (model_row));
		proxy->priv->all_modifs = g_slist_delete_link (proxy->priv->all_modifs, proxy->priv->all_modifs);
	}
	if (proxy->priv->new_rows) {/* new rows removing (no memory de-allocation here, though) */
		g_slist_free (proxy->priv->new_rows);
		proxy->priv->new_rows = NULL;
	}

	nb_cols = gda_data_model_get_n_columns (model);
	if (proxy->priv->model_nb_cols != nb_cols) {
		/* take into account the new dimensions of the proxied data model => perform a re-init of the object */
		proxied_model_reset_cb (model, proxy);
	}
	else {
		proxy->priv->model_nb_rows = gda_data_model_get_n_rows (model);

		/* initial chunk settings, no need to emit any signal as it's an initial state */
		proxy->priv->chunk = compute_display_chunk (proxy);
		if (!proxy->priv->chunk->mapping) {
			display_chunk_free (proxy->priv->chunk);
			proxy->priv->chunk = NULL;
		}
		gda_data_model_reset (GDA_DATA_MODEL (proxy));
	}
}

/* 
 * called when the proxied model emits a "reset" signal
 */
static void
proxied_model_reset_cb (GdaDataModel *model, GdaDataProxy *proxy)
{
	gboolean add_null_entry = proxy->priv->add_null_entry;
	
	g_object_ref (G_OBJECT (model));
	clean_proxy (proxy);
	gda_data_proxy_init (proxy);
	g_object_set (G_OBJECT (proxy), "model", model, "prepend_null_entry", add_null_entry, NULL);
	g_object_unref (G_OBJECT (model));
	gda_data_model_reset (GDA_DATA_MODEL (proxy));
}

/**
 * gda_data_proxy_get_proxied_model
 * @proxy: a #GdaDataProxy object
 *
 * Fetch the #GdaDataModel which @proxy does proxy
 *
 * Returns: the proxied data model
 */
GdaDataModel *
gda_data_proxy_get_proxied_model (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy->priv, NULL);

	return proxy->priv->model;
}

/**
 * gda_data_proxy_get_proxied_model_n_cols
 * @proxy: a #GdaDataProxy object
 * 
 * Get the number of columns in the proxied data model
 *
 * Returns: the number of columns, or -1 if an error occurred
 */
gint
gda_data_proxy_get_proxied_model_n_cols (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	g_return_val_if_fail (proxy->priv, -1);

	return proxy->priv->model_nb_cols;
}

/**
 * gda_data_proxy_get_proxied_model_n_rows
 * @proxy: a #GdaDataProxy object
 * 
 * Get the number of rows in the proxied data model
 *
 * Returns: the number of rows, or -1 if the number of rows is not known
 */
gint
gda_data_proxy_get_proxied_model_n_rows (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	g_return_val_if_fail (proxy->priv, -1);

	return gda_data_model_get_n_rows (proxy->priv->model);
}

/**
 * gda_data_proxy_is_read_only
 * @proxy: a #GdaDataProxy object
 *
 * Returns: TRUE if the proxied data model is itself read-only
 */
gboolean
gda_data_proxy_is_read_only (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), TRUE);
	g_return_val_if_fail (proxy->priv, TRUE);

	return ! gda_data_model_is_updatable (proxy->priv->model);
}


RowModif *find_or_create_row_modif (GdaDataProxy *proxy, gint proxy_row, gint col, RowValue **ret_rv);


/*
 * Stores the new RowValue in @rv
 */
RowModif *
find_or_create_row_modif (GdaDataProxy *proxy, gint proxy_row, gint col, RowValue **ret_rv)
{
	RowModif *rm = NULL;
	RowValue *rv = NULL;
	gint model_row;

	model_row = absolute_row_to_model_row (proxy, 
					       proxy_row_to_absolute_row (proxy, proxy_row), &rm);
	if (!rm) {
		/* create a new RowModif */
		g_assert (model_row >= 0);
		rm = row_modifs_new (proxy, proxy_row);
		rm->model_row = model_row;
		g_hash_table_insert (proxy->priv->modify_rows, GINT_TO_POINTER (model_row), rm);
		proxy->priv->all_modifs = g_slist_prepend (proxy->priv->all_modifs, rm);
	}
	else {
		/* there are already some modifications to the row, try to catch the RowValue if available */
		GSList *list;
		
		list = rm->modify_values;
		while (list && !rv) {
			if (ROW_VALUE (list->data)->model_column == col)
				rv = ROW_VALUE (list->data);
			list = g_slist_next (list);
		}
	}

	if (ret_rv)
		*ret_rv = rv;
	return rm;
}


/**
 * gda_data_proxy_get_values
 * @proxy: a #GdaDataProxy object
 * @proxy_row: a proxy row
 * @cols_index:
 * @n_cols:
 *
 * Retreive a whole list of values from the @proxy store. This function calls gda_data_proxy_get_value()
 * for each column index specified in @cols_index, and generates a #GSlist on the way.
 *
 * Returns: a new list of values (the list must be freed, not the values), or %NULL if an error occurred
 */
GSList *
gda_data_proxy_get_values (GdaDataProxy *proxy, gint proxy_row, gint *cols_index, gint n_cols)
{
	GSList *retval = NULL;
	gint i;
	const GValue *value;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy->priv, NULL);
	g_return_val_if_fail (proxy_row >= 0, NULL);

	for (i = 0; i < n_cols; i++) {
		value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy,
						     cols_index[i], proxy_row);
		if (value)
			retval = g_slist_prepend (retval, (GValue *) value);
		else {
			g_slist_free (retval);
			return NULL;
		}
	}

	return g_slist_reverse (retval);
}

/**
 * gda_data_proxy_get_value_attributes
 * @proxy: a #GdaDataProxy object
 * @proxy_row: a proxy row
 * @col: a valid proxy column
 *
 * Get the attributes of the value stored at (proxy_row, col) in @proxy, which
 * is an ORed value of #GdaValueAttribute flags
 *
 * Returns: the attribute
 */
GdaValueAttribute
gda_data_proxy_get_value_attributes (GdaDataProxy *proxy, gint proxy_row, gint col)
{
	gint model_row;
	RowModif *rm;
	gboolean value_has_modifs = FALSE;
	GdaValueAttribute flags = 0;
	gint model_column;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);
	g_return_val_if_fail (proxy_row >= 0, 0);

	model_column = col % proxy->priv->model_nb_cols;
	model_row = proxy_row_to_model_row (proxy, proxy_row);
	flags = gda_data_model_get_attributes_at (proxy->priv->model, model_column, model_row);
	if (model_row < 0) {
		flags |= GDA_VALUE_ATTR_IS_NULL;
		if (flags & GDA_VALUE_ATTR_NO_MODIF) /* A new row with unmodifiable data means unused data */
			flags |= GDA_VALUE_ATTR_UNUSED;
	}

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	if (rm) {
		if (rm->modify_values) {
			/* there are some modifications to the row */
			GSList *list;
			RowValue *rv = NULL;
			
			list = rm->modify_values;
			while (list && !rv) {
				if (ROW_VALUE (list->data)->model_column == model_column)
					rv = ROW_VALUE (list->data);
				list = g_slist_next (list);
			}
			if (rv) {
				value_has_modifs = TRUE;
				flags |= g_value_get_flags (rv->attributes);
				if (rv->value && !gda_value_is_null (rv->value))
					flags &= ~GDA_VALUE_ATTR_IS_NULL;
				else
					flags |= GDA_VALUE_ATTR_IS_NULL;
			}
		}
	}
	
	if (! value_has_modifs)
		flags |= GDA_VALUE_ATTR_IS_UNCHANGED;
		
	/* compute the GDA_VALUE_ATTR_DATA_NON_VALID attribute */
	if (! (flags & GDA_VALUE_ATTR_CAN_BE_NULL)) {
		if ((flags & GDA_VALUE_ATTR_IS_NULL) && !(flags & GDA_VALUE_ATTR_IS_DEFAULT))
			flags |= GDA_VALUE_ATTR_DATA_NON_VALID;
	}

	return flags;
}

/**
 * gda_data_proxy_alter_value_attributes
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 * @col: a valid column number
 * @alter_flags: flags to alter the attributes
 *
 * Alters the attributes of the value stored at (proxy_row, col) in @proxy. the @alter_flags
 * can only contain the GDA_VALUE_ATTR_IS_NULL, GDA_VALUE_ATTR_IS_DEFAULT and GDA_VALUE_ATTR_IS_UNCHANGED
 * flags (other flags are ignored).
 */
void
gda_data_proxy_alter_value_attributes (GdaDataProxy *proxy, gint proxy_row, gint col, GdaValueAttribute alter_flags)
{
	gint model_col;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	model_col = col % proxy->priv->model_nb_cols;
	if (alter_flags & GDA_VALUE_ATTR_IS_NULL) 
		gda_data_proxy_set_value_at ((GdaDataModel*) proxy, 
					     model_col, proxy_row, NULL, NULL);
	else {
		RowModif *rm;
		RowValue *rv = NULL;
		
		rm = find_or_create_row_modif (proxy, proxy_row, model_col, &rv);
		g_assert (rm);
		
		if (alter_flags & GDA_VALUE_ATTR_IS_DEFAULT) {
			GdaValueAttribute flags = 0;
			if (!rv) {
				/* create a new RowValue */
				rv = g_new0 (RowValue, 1);
				rv->row_modif = rm;
				rv->model_column = model_col;
				rv->attributes = gda_value_copy (proxy->priv->columns_attrs [col]);
				flags = g_value_get_flags (rv->attributes);
							
				rv->value = NULL;
				flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
				if (rm->model_row >= 0)
					flags |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;
				else
					flags &= ~GDA_VALUE_ATTR_HAS_VALUE_ORIG;
				
				rm->modify_values = g_slist_prepend (rm->modify_values, rv);	
			}
			else {
				flags = g_value_get_flags (rv->attributes);
				if (rv->value) {
					gda_value_free (rv->value);
					rv->value = NULL;
				}
			}
			flags |= GDA_VALUE_ATTR_IS_DEFAULT;
			g_value_set_flags (rv->attributes, flags);

			if (proxy->priv->notify_changes)
				gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		}
		if (alter_flags & GDA_VALUE_ATTR_IS_UNCHANGED) {
			if (!rm->orig_values)
				g_warning ("Alter_Flags = GDA_VALUE_ATTR_IS_UNCHANGED, no RowValue!");
			else
				gda_data_proxy_set_value_at ((GdaDataModel*) proxy, 
							     model_col, proxy_row,
							     rm->orig_values [model_col],
							     NULL);
		}
	}
}

/**
 * gda_data_proxy_get_proxied_model_row
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Get the @proxy's proxied model row corresponding to @proxy_row

 * Returns: the proxied model's row, or -1 if @proxy row which only exists @proxy
 */
gint
gda_data_proxy_get_proxied_model_row (GdaDataProxy *proxy, gint proxy_row)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);
	g_return_val_if_fail (proxy_row >= 0, 0);

	return proxy_row_to_model_row (proxy, proxy_row);
}

/**
 * gda_data_proxy_delete
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Marks the row @proxy_row to be deleted
 */
void
gda_data_proxy_delete (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm = NULL;
	gboolean do_signal = FALSE;
	gint model_row, abs_row;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (proxy->priv->add_null_entry && proxy_row == 0) {
		g_warning (_("The first row is an empty row artificially prepended and cannot be removed"));
		return;
	}

	if (! (gda_data_model_get_access_flags ((GdaDataModel*) proxy) & GDA_DATA_MODEL_ACCESS_DELETE))
		return;

	abs_row = proxy_row_to_absolute_row (proxy, proxy_row);
	model_row = absolute_row_to_model_row (proxy, abs_row, &rm);
	if (rm) {
		if (! rm->to_be_deleted) {
			if (rm->model_row < 0) {
				/* remove the row completely because it does not exist in the data model */
				proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
				proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
				row_modifs_free (rm);
				
				if (proxy->priv->chunk) {
					/* Update chunk */
					gint i, *v;
					gint row_cmp = proxy_row - (proxy->priv->add_null_entry ? 1 : 0);
					for (i = 0; i < proxy->priv->chunk->mapping->len; i++) {
						v = &g_array_index (proxy->priv->chunk->mapping, gint, i);
						if (*v > abs_row) 
							*v -= 1;
					}
					g_array_remove_index (proxy->priv->chunk->mapping, row_cmp);
				}

				if (proxy->priv->notify_changes) 
					gda_data_model_row_removed ((GdaDataModel *) proxy, proxy_row);
			}
			else {
				rm->to_be_deleted = TRUE;
				do_signal = TRUE;
			}
		}
	}
	else {
		/* the row is an existing row in the data model, create a new RowModif */
		rm = row_modifs_new (proxy, proxy_row);
		rm->model_row = model_row;
		g_hash_table_insert (proxy->priv->modify_rows, GINT_TO_POINTER (model_row), rm);
		proxy->priv->all_modifs = g_slist_prepend (proxy->priv->all_modifs, rm);
		rm->to_be_deleted = TRUE;
		do_signal = TRUE;
	}

	if (do_signal && proxy->priv->notify_changes) {
		gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[ROW_DELETE_CHANGED],
                               0, proxy_row, TRUE);
	}
}

/**
 * gda_data_proxy_undelete
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Remove the "to be deleted" mark at the row @proxy_row, if it existed.
 */
void
gda_data_proxy_undelete (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm = NULL;
	gboolean do_signal = FALSE;
	gint model_row;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	model_row = absolute_row_to_model_row (proxy, 
					       proxy_row_to_absolute_row (proxy, proxy_row), &rm);
	if (rm) {
		rm->to_be_deleted = FALSE;
		if (!rm->modify_values) {
			/* get rid of that RowModif */
			do_signal= TRUE;
			
			g_hash_table_remove (proxy->priv->modify_rows, GINT_TO_POINTER (model_row));
			proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
			row_modifs_free (rm);
		}
		else
			do_signal= TRUE;
	}

	if (do_signal && proxy->priv->notify_changes) {
		gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[ROW_DELETE_CHANGED],
                               0, proxy_row, FALSE);
	}
}

/**
 * gda_data_proxy_row_is_deleted
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Tells if the row number @proxy_row is marked to be deleted.
 *
 * Returns: TRUE if the row is marked to be deleted
 */
gboolean
gda_data_proxy_row_is_deleted (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	return rm && rm->to_be_deleted ? TRUE : FALSE;
}

/**
 * gda_data_proxy_row_is_inserted
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Tells if the row number @proxy_row is a row which has been inserted in @proxy
 * (and is thus not in the proxied data model).
 *
 * Returns: TRUE if the row is an inserted row
 */
gboolean
gda_data_proxy_row_is_inserted (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	if (rm && (rm->model_row < 0))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_data_proxy_find_row_from_values
 * @proxy: a #GdaDataProxy object
 * @values: a list of #GValue values
 * @cols_index: an array of #gint containing the column number to match each value of @values
 *
 * Find the first row where all the values in @values at the columns identified at
 * @cols_index match.
 *
 * NOTE: the @cols_index array MUST contain a column index for each value in @values
 *
 * Returns: proxy row number if the row has been identified, or -1 otherwise
 */
gint
gda_data_proxy_find_row_from_values (GdaDataProxy *proxy, GSList *values,
				     gint *cols_index)
{
	gboolean found = FALSE;
	gint proxy_row;
	gint current_nb_rows;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (values, FALSE);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/* FIXME: use a virtual connection here with some SQL, it'll be much easier and will avoid
	 * much code
	 */
	TO_IMPLEMENT;

	/* if there are still some rows waiting to be added in the idle loop, then force them to be added
	 * first, otherwise we might not find what we are looking for!
	 */
	if (proxy->priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		proxy->priv->chunk_sync_idle_id = 0;
		while (chunk_sync_idle (proxy)) ;
	}

	current_nb_rows = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy);
	for (proxy_row = 0; proxy_row < current_nb_rows; proxy_row++) {
		gboolean allequal = TRUE;
		GSList *list;
		gint index;
		const GValue *value;
		
		list = values;
		index = 0;
		while (list && allequal) {
			if (cols_index)
				g_return_val_if_fail (cols_index [index] < proxy->priv->model_nb_cols, FALSE);
			value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy, 
							     cols_index ? cols_index [index] : 
							     index, proxy_row);
			if (gda_value_compare_ext ((GValue *) (list->data), (GValue *) value))
				allequal = FALSE;

			list = g_slist_next (list);
			index++;
		}
		if (allequal) {
			found = TRUE;
			break;
		}
	}
	
	return found ? proxy_row : -1;
}


/**
 * gda_data_proxy_get_model
 * @proxy: a #GdaDataProxy object
 *
 * Get the #GdaDataModel which holds the unmodified (reference) data of @proxy
 *
 * Returns: the #GdaDataModel
 */
GdaDataModel *
gda_data_proxy_get_model (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy->priv, NULL);

	return proxy->priv->model;
}

/**
 * gda_data_proxy_append
 * @proxy: a #GdaDataProxy object
 *
 * Appends a new row to the proxy. The operation can fail if either:
 * <itemizedlist>
 * <listitem><para>The INSERT operation is not accepted by the proxied data model</para></listitem>
 * <listitem><para>There is an unknown number of rows in the proxy</para></listitem>
 * </itemizedlist>
 *
 * Returns: the proxy row number of the new row, or -1 if the row could not be appended
 */
gint
gda_data_proxy_append (GdaDataProxy *proxy)
{
	RowModif *rm;
	gint col;
	gint proxy_row;
	gint abs_row;
	
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	g_return_val_if_fail (proxy->priv, -1);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (! (gda_data_model_get_access_flags ((GdaDataModel *) proxy) & GDA_DATA_MODEL_ACCESS_INSERT)) 
		return -1;
	if (proxy->priv->model_nb_rows == -1)
		return -1;

	/* RowModif structure */
	rm = row_modifs_new (proxy, -1);
	rm->model_row = -1;
	rm->orig_values = NULL; /* there is no original value */
	rm->orig_values_size = 0;

	proxy->priv->all_modifs = g_slist_prepend (proxy->priv->all_modifs, rm);
	proxy->priv->new_rows = g_slist_append (proxy->priv->new_rows, rm);

	/* new proxy row value */
	abs_row = row_modif_to_absolute_row (proxy, rm);
	if (proxy->priv->chunk) {
		proxy_row = proxy->priv->chunk->mapping->len;
		g_array_append_val (proxy->priv->chunk->mapping, abs_row);
		if (proxy->priv->add_null_entry)
			proxy_row++;
	}
	else 
		proxy_row = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy) - 1;
	
	/* for the columns which allow a default value, set them to the default value */
	for (col = 0; col < proxy->priv->model_nb_cols; col ++) {
		GdaColumn *column;
		const GValue *def;
		RowValue *rv;
		GdaValueAttribute flags = 0;
		
		/* create a new RowValue */
		rv = g_new0 (RowValue, 1);
		rv->row_modif = rm;
		rv->model_column = col;
		rv->attributes = gda_value_new (GDA_TYPE_VALUE_ATTRIBUTE);
		rv->value = NULL;
		rm->modify_values = g_slist_prepend (rm->modify_values, rv);	
		
		column = gda_data_model_describe_column (proxy->priv->model, col);
		def = gda_column_get_default_value (column);
		if (def) {
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);
			if (G_VALUE_TYPE (def) == gda_column_get_g_type (column))
				rv->value = gda_value_copy (def);
		}
		if (gda_column_get_allow_null (column)) {
			GdaValueAttribute attributes;
			
			attributes = gda_data_model_get_attributes_at (proxy->priv->model, col, -1);;
			if (attributes & GDA_VALUE_ATTR_CAN_BE_NULL)
				flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
		}

		if (gda_column_get_auto_increment (column))
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);

		g_value_set_flags (rv->attributes, flags);
	}

	/* signal row insertion */
	if (proxy->priv->notify_changes)
		gda_data_model_row_inserted ((GdaDataModel *) proxy, proxy_row); 	

	return proxy_row;
}

/**
 * gda_data_proxy_cancel_row_changes
 * @proxy: a #GdaDataProxy object
 * @proxy_row: the row to cancel changes
 * @col: the column to cancel changes, or less than 0 to cancel any change on the @row row
 *
 * Resets data at the corresponding row and column. If @proxy_row corresponds to a new row, then
 * that new row is deleted from @proxy.
 */
void
gda_data_proxy_cancel_row_changes (GdaDataProxy *proxy, gint proxy_row, gint col)
{
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (((col >= 0) && (col < proxy->priv->model_nb_cols)) ||
	    (col < 0)) {
		RowModif *rm;
		gboolean signal_update = FALSE;
		gboolean signal_delete = FALSE;

		rm = proxy_row_to_row_modif (proxy, proxy_row);
		if (rm && rm->modify_values) {
			/* there are some modifications to the row */
			GSList *list;
			RowValue *rv = NULL;
			
			list = rm->modify_values;
			while (list && (!rv || (col < 0))) {
				if ((col < 0) || (ROW_VALUE (list->data)->model_column == col)) {
					rv = ROW_VALUE (list->data);

					/* remove this RowValue from the RowList */
					rm->modify_values = g_slist_remove (rm->modify_values, rv);
					if (!rm->modify_values && !rm->to_be_deleted) {
						/* remove this RowList as well */
						proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
						if (rm->model_row < 0) {
							if (proxy->priv->chunk) {
								/* Update chunk */
								gint i, *v, abs_row;
								gint row_cmp = proxy_row - (proxy->priv->add_null_entry ? 1 : 0);
								abs_row = proxy_row_to_absolute_row (proxy, proxy_row);
								for (i = 0; i < proxy->priv->chunk->mapping->len; i++) {
									v = &g_array_index (proxy->priv->chunk->mapping, gint, i);
									if (*v > abs_row) 
										*v -= 1;
								}
								g_array_remove_index (proxy->priv->chunk->mapping, row_cmp);
							}
							signal_delete = TRUE;
							proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
						}
						else
							g_hash_table_remove (proxy->priv->modify_rows, 
									     GINT_TO_POINTER (rm->model_row));
						row_modifs_free (rm);
						rm = NULL;						
					}
					else {
						signal_update = TRUE;
					}
					if (rm)
						list = rm->modify_values;
					else
						list = NULL;
				}
				else
					list = list->next;
			}
		}

		if (proxy->priv->notify_changes) {
			if (signal_delete)
				gda_data_model_row_removed ((GdaDataModel *) proxy, proxy_row);
			else if (signal_update)
				gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		}
	}
	else
		g_warning ("GdaDataProxy column %d is not a modifiable data column", col);
}

static gboolean commit_row_modif (GdaDataProxy *proxy, RowModif *rm, GError **error);

/**
 * gda_data_proxy_apply_row_changes
 * @proxy: a #GdaDataProxy object
 * @proxy_row: the row number to commit
 * @error: place to store the error, or %NULL
 * 
 * Commits the modified data in the proxy back into the #GdaDataModel.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_proxy_apply_row_changes (GdaDataProxy *proxy, gint proxy_row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

#ifdef GDA_DEBUG_NO
	DEBUG_HEADER;
	gda_object_dump (proxy, 5);
#endif

	return commit_row_modif (proxy, proxy_row_to_row_modif (proxy, proxy_row), error);
}

/*
 * Commits the modifications held in one single RowModif structure.
 *
 * Returns: TRUE if no error occurred
 */
static gboolean
commit_row_modif (GdaDataProxy *proxy, RowModif *rm, GError **error)
{
	gboolean err = FALSE;
	gboolean ignore_proxied_changes;
	gboolean mod_ok;
	gint abs_row, proxy_row;

	if (!rm)
		return TRUE;

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/*
	 * Steps in this procedure:
	 * -1- disable handling of proxied model modifications
	 * -2- apply desired modification (which may trigger a "change" signal from
	 *     the proxied model, that's why we need to specifically ignore it)
	 * -3.1- if no error then destroy the RowModif which has just been applied
	 *       and refresh displayed chunks if RowModif was a delete
	 * -4- re-enable handling of proxied model modifications
	 */

	ignore_proxied_changes = proxy->priv->ignore_proxied_changes;
	proxy->priv->ignore_proxied_changes = TRUE;

	abs_row = model_row_to_absolute_row (proxy, rm->model_row);
	proxy_row = absolute_row_to_proxy_row (proxy, abs_row);

	/* validate the changes to this row */
        g_signal_emit (G_OBJECT (proxy),
                       gda_data_proxy_signals[PRE_CHANGES_APPLIED],
                       0, proxy_row, rm->model_row, &mod_ok);
	if (!mod_ok) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_COMMIT_CANCELLED,
			     _("Modifications cancelled"));
		return FALSE;
	}

	/* apply the changes */
	if (rm->to_be_deleted) {
		/* delete the row */
		g_assert (rm->model_row >= 0);
		if (!gda_data_model_remove_row (proxy->priv->model, rm->model_row, error))
			err = TRUE;
	}
	else {
		if (rm->model_row >= 0) {
			/* update the row */
			GSList *list;
			GList *values = NULL;
			gint i;
			gboolean newvalue_found;
			GValue *newvalue;
			GValue **free_val;
			
			g_assert (rm->modify_values);
			g_assert (rm->orig_values);
			free_val = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (i=0; i < rm->orig_values_size; i++) {
				newvalue_found = FALSE;
				newvalue = NULL;
				
				list = rm->modify_values;
				while (list && !newvalue_found) {
					if (ROW_VALUE (list->data)->model_column == i) {
						newvalue_found = TRUE;
						if (g_value_get_flags (ROW_VALUE (list->data)->attributes) &
						    GDA_VALUE_ATTR_IS_DEFAULT)
							newvalue = NULL;
						else {
							if (! ROW_VALUE (list->data)->value) {
								newvalue = gda_value_new_null ();
								free_val [i] = newvalue;
							}
							else
								newvalue = ROW_VALUE (list->data)->value;	
						}
					}
					list = g_slist_next (list);
				}
				if (!newvalue_found)
					newvalue = rm->orig_values[i];
				values = g_list_append (values, newvalue);
			}

			err = ! gda_data_model_set_values (proxy->priv->model, rm->model_row, 
							   values, error);
			g_list_free (values);
			for (i = 0; i < proxy->priv->model_nb_cols; i++)
				if (free_val [i])
					gda_value_free (free_val [i]);
			g_free (free_val);
		}
		else {
			/* insert a new row */
			GSList *list;
			GList *values = NULL;
			gint i;
			GValue *newvalue;
			GValue **free_val;
			
			g_assert (rm->modify_values);
			free_val = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (i = 0; i < proxy->priv->model_nb_cols; i++) {
				newvalue = NULL;
				
				list = rm->modify_values;
				while (list && !newvalue) {
					if (ROW_VALUE (list->data)->model_column == i) {
						if (g_value_get_flags (ROW_VALUE (list->data)->attributes) &
						    GDA_VALUE_ATTR_IS_DEFAULT)
							newvalue = NULL;
						else {
							if (! ROW_VALUE (list->data)->value) {
								newvalue = gda_value_new_null ();
								free_val [i] = newvalue;
							}
							else
								newvalue = ROW_VALUE (list->data)->value;
							
						}
					}
					list = g_slist_next (list);
				}
				values = g_list_append (values, newvalue);
			}
			
			err = gda_data_model_append_values (proxy->priv->model, 
							    values, error) >= 0 ? FALSE : TRUE;
			g_list_free (values);
			for (i = 0; i < proxy->priv->model_nb_cols; i++)
				if (free_val [i])
					gda_value_free (free_val [i]);
			g_free (free_val);
		}
	}

	if (!err) {
		/* signal row actually changed */
		g_signal_emit (G_OBJECT (proxy),
			       gda_data_proxy_signals[POST_CHANGES_APPLIED],
			       0, proxy_row, rm->model_row);
		
		/* get rid of the commited change */
		proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
		proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
		g_hash_table_remove (proxy->priv->modify_rows, GINT_TO_POINTER (rm->model_row));
		row_modifs_free (rm);
	}

	proxy->priv->ignore_proxied_changes = ignore_proxied_changes;
	if (proxy->priv->proxy_has_changed) 
		proxied_model_data_changed_cb (proxy->priv->model, proxy);
	else
		adjust_displayed_chunk (proxy);

	return !err;
}

/**
 * gda_data_proxy_has_changed
 * @proxy: a #GdaDataProxy object
 *
 * Tells if @proxy contains any modifications not applied to the proxied data model.
 *
 * Returns: TRUE if there are some modifications in @proxy
 */
gboolean
gda_data_proxy_has_changed (GdaDataProxy *proxy)
{
        g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
        g_return_val_if_fail (proxy->priv, FALSE);

        return proxy->priv->all_modifs ? TRUE : FALSE;
}

/**
 * gda_data_proxy_row_has_changed
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Tells if the row number @proxy_row has changed
 *
 * Returns:
 */
gboolean
gda_data_proxy_row_has_changed (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	return rm && (rm->modify_values || rm->to_be_deleted) ? TRUE : FALSE;
}

/**
 * gda_data_proxy_get_n_new_rows
 * @proxy: a #GdaDataProxy object
 *
 * Get the number of rows which have been added to @proxy and which are not part of
 * the proxied data model.
 *
 * Returns: the number of new rows
 */
gint
gda_data_proxy_get_n_new_rows (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
        g_return_val_if_fail (proxy->priv, 0);

        return g_slist_length (proxy->priv->new_rows);
}

/**
 * gda_data_proxy_get_n_modified_rows
 * @proxy: a #GdaDataProxy object
 *
 * Get the number of rows which have been modified in the proxy (the sum of rows existing in
 * the proxied data model which have been modified, and new rows).
 *
 * Returns: the number of modified rows
 */
gint
gda_data_proxy_get_n_modified_rows (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
        g_return_val_if_fail (proxy->priv, 0);

        return g_slist_length (proxy->priv->all_modifs);
}

#ifdef GDA_DEBUG
static void
gda_data_proxy_dump (GdaDataProxy *proxy, guint offset)
{
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);

	/* FIXME */
	gda_data_model_dump (GDA_DATA_MODEL (proxy), stdout);
}
#endif

/**
 * gda_data_proxy_set_sample_size
 * @proxy: a #GdaDataProxy object
 * @sample_size: the requested size of a chunk, or 0
 *
 * Sets the size of each chunk of fata to display: the maximum number of rows which
 * can be displayed at a time. The default value is arbitrary 300 as it is big enough to
 * be able to display quite a lot of data, but small enough to avoid too much data
 * displayed at the same time.
 *
 * Note: the rows which have been added but not yet commited will always be displayed
 * regardless of the current chunk of data, and the modified rows which are not visible
 * when the displayed chunk of data changes are still held as modified rows.
 *
 * To remove the chunking of the data to display, simply pass @sample_size the 0 value.
 */
void
gda_data_proxy_set_sample_size (GdaDataProxy *proxy, gint sample_size)
{
	gint new_sample_size;
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	new_sample_size = sample_size <= 0 ? 0 : sample_size;
	if (proxy->priv->sample_size != new_sample_size) {
		proxy->priv->sample_size = new_sample_size;
		adjust_displayed_chunk (proxy);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[SAMPLE_SIZE_CHANGED],
                               0, sample_size);		
	}
}

/**
 * gda_data_proxy_get_sample_size
 * @proxy: a #GdaDataProxy object
 *
 * Get the size of each chunk of data displayed at a time.
 *
 * Returns: the chunk (or sample) size, or 0 if chunking is disabled.
 */
gint
gda_data_proxy_get_sample_size (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);

	return proxy->priv->sample_size;
}

/**
 * gda_data_proxy_set_sample_start
 * @proxy: a #GdaDataProxy object
 * @sample_start: the number of the first row to be displayed
 *
 * Sets the number of the first row to be displayed.
 */
void
gda_data_proxy_set_sample_start (GdaDataProxy *proxy, gint sample_start)
{
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (sample_start >= 0);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (proxy->priv->sample_first_row != sample_start) {
		proxy->priv->sample_first_row = sample_start;
		adjust_displayed_chunk (proxy);
	}
}

/**
 * gda_data_proxy_get_sample_start
 * @proxy: a #GdaDataProxy object
 *
 * Get the row number of the first row to be displayed.
 *
 * Returns: the number of the first row being displayed.
 */
gint
gda_data_proxy_get_sample_start (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);

	return proxy->priv->sample_first_row;
}

/**
 * gda_data_proxy_get_sample_end
 * @proxy: a #GdaDataProxy object
 *
 * Get the row number of the last row to be displayed.
 *
 * Returns: the number of the last row being displayed.
 */
gint
gda_data_proxy_get_sample_end (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);

	return proxy->priv->sample_last_row;	
}

static DisplayChunck *
display_chunk_new (gint reserved_size)
{
	DisplayChunck *chunk;

	chunk = g_new0 (DisplayChunck, 1);
	chunk->mapping = g_array_sized_new (FALSE, TRUE, sizeof (gint), reserved_size);

	return chunk;
}

static void
display_chunk_free (DisplayChunck *chunk)
{
	if (chunk->mapping)
		g_array_free (chunk->mapping, TRUE);
	g_free (chunk);
}

#ifdef GDA_DEBUG
static void
display_chunks_dump (GdaDataProxy *proxy)
{
#define DUMP
#undef DUMP
#ifdef DUMP
	gint i, total1 = 0, total2 = 0;

	g_print ("================== CHUNCK=%p, TO=%p (mapping=%p), SEP=%d\n", proxy->priv->chunk, proxy->priv->chunk_to,
		 proxy->priv->chunk_to ? proxy->priv->chunk_to->mapping : NULL,
		 proxy->priv->chunk_sep);
	if (!proxy->priv->chunk && !proxy->priv->chunk_to)
		g_print ("No chunks at all\n");

	if (proxy->priv->chunk)
		total1 = proxy->priv->chunk->mapping->len;
	if (proxy->priv->chunk_to && proxy->priv->chunk_to->mapping)
		total2 = proxy->priv->chunk_to->mapping->len;

	g_print ("CHUNCK   CHUNCK_TO\n");
	for (i = 0; i < MAX (total1, total2); i++) {
		if (i < total1)
			g_print ("%03d", g_array_index (proxy->priv->chunk->mapping, gint, i));
		else
			g_print ("   ");
		g_print (" ==== ");
		if (i < total2)
			g_print ("%03d", g_array_index (proxy->priv->chunk_to->mapping, gint, i));
		else
			g_print ("   ");
		g_print ("\n");
	}
#endif
}
#else 
static void
display_chunks_dump (GdaDataProxy *proxy)
{}
#endif

/*
 * Makes sure the sync, if necessary, is finished
 */
static void
ensure_chunk_sync (GdaDataProxy *proxy)
{
	if (proxy->priv->chunk_sync_idle_id) {
		gboolean defer_sync = proxy->priv->defer_sync;
		proxy->priv->defer_sync = FALSE;
		
		chunk_sync_idle (proxy);
		proxy->priv->defer_sync = defer_sync;
	}
}

/*
 * Emit all the correct signals when switching from proxy->priv->chunk to
 * proxy->priv->chunk_to
 */
static gboolean
chunk_sync_idle (GdaDataProxy *proxy)
{
#define IDLE_STEP 50

	gboolean finished = FALSE;
	gboolean has_changed = FALSE;
	gint index;
	gint step, max_steps;
	GdaDataModelIter *iter = NULL;
	gint signal_row_offset = proxy->priv->add_null_entry ? 1 : 0;

	if (!proxy->priv->defer_sync) {
		if (proxy->priv->chunk_sync_idle_id) {
			g_idle_remove_by_data (proxy);
			proxy->priv->chunk_sync_idle_id = 0;
		}
		max_steps = G_MAXINT;
	}

	if (!proxy->priv->chunk_to)
		return FALSE; /* nothing to do */

	max_steps = 0;
	if (proxy->priv->chunk_proxy_nb_rows < 0)
		proxy->priv->chunk_proxy_nb_rows = proxy->priv->model_nb_rows + g_slist_length (proxy->priv->new_rows);
	if (proxy->priv->chunk_to->mapping)
		max_steps = MAX (max_steps, proxy->priv->chunk_to->mapping->len - proxy->priv->chunk_sep + 1);
	else
		max_steps = MAX (max_steps, proxy->priv->chunk_proxy_nb_rows - proxy->priv->chunk_sep + 1);
	if (proxy->priv->chunk)
		max_steps = MAX (max_steps, proxy->priv->chunk->mapping->len - proxy->priv->chunk_sep + 1);
	else
		max_steps = MAX (max_steps, proxy->priv->chunk_proxy_nb_rows - proxy->priv->chunk_sep + 1);
	if (proxy->priv->defer_sync)
		max_steps = MIN (max_steps, IDLE_STEP);

#ifdef DEBUG_SYNC
	g_print ("////////// %s(defer_sync = %d)\n", __FUNCTION__, proxy->priv->defer_sync);
	display_chunks_dump (proxy);
#endif

	/* disable the emision of the "changed" signal each time a "row_*" signal is
	 * emitted, and instead send that signal only once at the end */
	gda_object_block_changed (GDA_OBJECT (proxy));
	for (index = proxy->priv->chunk_sep, step = 0; 
	     step < max_steps && !finished; 
	     step++) {
		gint cur_row, repl_row;

		if (proxy->priv->chunk) {
			if (index < proxy->priv->chunk->mapping->len)
				cur_row = g_array_index (proxy->priv->chunk->mapping, gint, index);
			else
				cur_row = -1;
		}
		else {
			cur_row = index;
			if (cur_row >= proxy->priv->chunk_proxy_nb_rows)
				cur_row = -1;
		}

		if (proxy->priv->chunk_to->mapping) {
			if (index < proxy->priv->chunk_to->mapping->len)
				repl_row = g_array_index (proxy->priv->chunk_to->mapping, gint, index);
			else
				repl_row = -1;
		}
		else {
			repl_row = index;
			if (!iter) 
				iter = gda_data_model_create_iter (proxy->priv->model);
			if (!gda_data_model_iter_set_at_row (iter, repl_row))
				repl_row = -1;
		}

#ifdef DEBUG_SYNC
		g_print ("INDEX=%d step=%d max_steps=%d cur_row=%d repl_row=%d\n", index, step, max_steps, cur_row, repl_row);
#endif
		if ((cur_row >= 0) && (repl_row >= 0)) {
			/* emit the GdaDataModel::"row_updated" signal */
			if (proxy->priv->chunk) {
				g_array_insert_val (proxy->priv->chunk->mapping, index, repl_row);
				g_array_remove_index (proxy->priv->chunk->mapping, index + 1);
			}

			if (cur_row != repl_row) 
				if (proxy->priv->notify_changes) {
					gda_data_model_row_updated ((GdaDataModel *) proxy, index + signal_row_offset);
#ifdef DEBUG_SYNC
					g_print ("Signal: Update row %d\n", index + signal_row_offset);
#endif
				}
			index++;
			proxy->priv->chunk_sep++;
		}
		else if ((cur_row >= 0) && (repl_row < 0)) {
			/* emit the GdaDataModel::"row_removed" signal */
			if (proxy->priv->chunk) 
				g_array_remove_index (proxy->priv->chunk->mapping, index);
			if (proxy->priv->notify_changes) {
				gda_data_model_row_removed ((GdaDataModel *) proxy, index + signal_row_offset);
#ifdef DEBUG_SYNC
				g_print ("Signal: Remove row %d\n", index + signal_row_offset);
#endif
			}

			proxy->priv->chunk_proxy_nb_rows--;
		}
		else if ((cur_row < 0) && (repl_row >= 0)) {
			/* emit GdaDataModel::"row_inserted" insert signal */			
			if (proxy->priv->chunk) 
				g_array_insert_val (proxy->priv->chunk->mapping, index, repl_row);
			if (proxy->priv->notify_changes) {
#ifdef DEBUG_SYNC
				g_print ("Signal: Insert row %d\n", index + signal_row_offset);
#endif
				gda_data_model_row_inserted ((GdaDataModel *) proxy, index + signal_row_offset);
			}
			index++;
			proxy->priv->chunk_sep++;
		}
		else
			finished = TRUE;	
	}

	if (iter)
		g_object_unref (iter);
		
	/* re-enable the emision of the "changed" signal each time a "row_*" signal is
	 * emitted */
	gda_object_unblock_changed (GDA_OBJECT (proxy));
	if (has_changed)
		gda_data_model_signal_emit_changed ((GdaDataModel *) proxy);

	if (finished) {
		if (proxy->priv->chunk_sync_idle_id) {
			g_idle_remove_by_data (proxy);
			proxy->priv->chunk_sync_idle_id = 0;
		}

		if (! proxy->priv->chunk_to->mapping) {
			if (proxy->priv->chunk) {
				display_chunk_free (proxy->priv->chunk);
				proxy->priv->chunk = NULL;
			}
			display_chunk_free (proxy->priv->chunk_to);
			proxy->priv->chunk_to = NULL;
		}
		else {
			if (proxy->priv->chunk) 
				display_chunk_free (proxy->priv->chunk);
			proxy->priv->chunk = proxy->priv->chunk_to;
			proxy->priv->chunk_to = NULL;
		}
#ifdef DEBUG_SYNC
		g_print ("Sync. is now finished\n");
#endif
	}
#ifdef DEBUG_SYNC
	else
		g_print ("Sync. is NOT finished yet\n");
#endif

	return !finished;
}

static DisplayChunck *
compute_display_chunk (GdaDataProxy *proxy)
{
	DisplayChunck *ret_chunk = NULL;

	if (proxy->priv->filtered_rows) {
		/* REM: when there is a filter applied, the new rows are mixed with the
		 * existing ones => no need to treat them appart
		 */
		gint nb_rows = gda_data_model_get_n_rows (proxy->priv->filtered_rows);
		gint i, new_nb_rows = 0;

		g_assert (nb_rows >= 0); /* the number of rows IS known here */
		if (proxy->priv->sample_size > 0) {
			if (proxy->priv->sample_first_row >= nb_rows) 
				proxy->priv->sample_first_row = proxy->priv->sample_size * 
					((nb_rows - 1) / proxy->priv->sample_size);
			
			proxy->priv->sample_last_row = proxy->priv->sample_first_row + 
				proxy->priv->sample_size - 1;
			if (proxy->priv->sample_last_row >= nb_rows)
				proxy->priv->sample_last_row = nb_rows - 1;
			new_nb_rows = proxy->priv->sample_last_row - proxy->priv->sample_first_row + 1;
		}
		else {
			proxy->priv->sample_first_row = 0;
			proxy->priv->sample_last_row = nb_rows - 1;
			new_nb_rows = nb_rows;
		}

		ret_chunk = display_chunk_new (proxy->priv->sample_size > 0 ? 
						 proxy->priv->sample_size : nb_rows);
		for (i = 0; i < new_nb_rows; i++) {
			const GValue *value;
			gint val;

			g_assert (i + proxy->priv->sample_first_row < nb_rows);
			value = gda_data_model_get_value_at (proxy->priv->filtered_rows, 
							     0, i + proxy->priv->sample_first_row);
			g_assert (value);
			g_assert (G_VALUE_TYPE (value) == G_TYPE_INT);
			val = g_value_get_int (value);
			g_array_append_val (ret_chunk->mapping, val);
		}
	}
	else {
		gint i, new_nb_rows = 0;
		
		if (proxy->priv->model_nb_rows >= 0) {
			/* known number of rows */
			if (proxy->priv->sample_size > 0) {
				if (proxy->priv->sample_first_row >= proxy->priv->model_nb_rows) 
					proxy->priv->sample_first_row = proxy->priv->sample_size * 
						((proxy->priv->model_nb_rows - 1) / proxy->priv->sample_size);
				
				proxy->priv->sample_last_row = proxy->priv->sample_first_row + 
					proxy->priv->sample_size - 1;
				if (proxy->priv->sample_last_row >= proxy->priv->model_nb_rows)
					proxy->priv->sample_last_row = proxy->priv->model_nb_rows - 1;
				new_nb_rows = proxy->priv->sample_last_row - proxy->priv->sample_first_row + 1;
				ret_chunk = display_chunk_new (proxy->priv->sample_size);
			}
			else {
				/* no chunk_to->mapping needed */
				ret_chunk = g_new0 (DisplayChunck, 1);

				proxy->priv->sample_first_row = 0;
				proxy->priv->sample_last_row = proxy->priv->model_nb_rows - 1;
				new_nb_rows = proxy->priv->model_nb_rows;
			}
		}
		else {
			/* no chunk_to->mapping needed */
			ret_chunk = g_new0 (DisplayChunck, 1);

			if (proxy->priv->model_nb_rows == 0 ) {
				/* known number of rows */
				proxy->priv->sample_first_row = 0;
				proxy->priv->sample_last_row = 0;
				new_nb_rows = 0;
			}
			else {
				/* unknown number of rows */
				if (proxy->priv->sample_size > 0) {
					proxy->priv->sample_last_row = proxy->priv->sample_first_row + 
						proxy->priv->sample_size - 1;
					new_nb_rows = proxy->priv->sample_last_row - proxy->priv->sample_first_row + 1;
				}
				else {
					proxy->priv->sample_first_row = 0;
					proxy->priv->sample_last_row = G_MAXINT - 1;
					new_nb_rows = G_MAXINT;
				}
			}
		}
		/* fill @chunk_to is it exists */
		if (ret_chunk && ret_chunk->mapping) {
			for (i = 0; i < new_nb_rows; i++) {
				g_assert (i + proxy->priv->sample_first_row < proxy->priv->model_nb_rows);
				gint val = model_row_to_absolute_row (proxy, i + proxy->priv->sample_first_row);
				g_array_append_val (ret_chunk->mapping, val);
			}
			GSList *list;
			for (i++, list = proxy->priv->new_rows; list; list = list->next, i++) {
				gint val = row_modif_to_absolute_row (proxy, ROW_MODIF (list->data));
				g_array_append_val (ret_chunk->mapping, val);
			}
		}
	}

	return ret_chunk;
}

/*
 * Adjusts the values of the first and last rows to be displayed depending
 * on the sample size.
 *
 * Some rows may be added or removed during the adjustment.
 */
static void
adjust_displayed_chunk (GdaDataProxy *proxy)
{
	g_return_if_fail (proxy->priv->model);

	/*
	 * Stop idle adding of rows if necessary
	 */
	if (proxy->priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		proxy->priv->chunk_sync_idle_id = 0;
	}

	/* compute new DisplayChunck */
	if (proxy->priv->chunk_to) {
		display_chunk_free (proxy->priv->chunk_to);
		proxy->priv->chunk_to = NULL;
	}
	proxy->priv->chunk_to = compute_display_chunk (proxy);
	if (!proxy->priv->chunk_to)
		return; /* nothing to do */

	/* determine if chunking has changed */
	gboolean equal = FALSE;
	if (proxy->priv->chunk && proxy->priv->chunk_to->mapping) {
		/* compare the 2 chunks */
		if (proxy->priv->chunk->mapping->len == proxy->priv->chunk_to->mapping->len) {
			gint i;
			equal = TRUE;
			for (i = 0; i < proxy->priv->chunk->mapping->len; i++) {
				if (g_array_index (proxy->priv->chunk->mapping, gint, i) !=
				    g_array_index (proxy->priv->chunk_to->mapping, gint, i)) {
					equal = FALSE;
					break;
				}
			}
		}
	}
	else if (!proxy->priv->chunk && !proxy->priv->chunk_to->mapping)
		equal = TRUE;

	/* handle new display chunk (which may be NULL) */
	if (! equal) {
#ifdef DEBUG_SYNC
		g_print ("////////// %s(%d)\n", __FUNCTION__, __LINE__);
#endif
		display_chunks_dump (proxy);	
		/* signal sample changed if necessary */
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[SAMPLE_CHANGED],
                               0, proxy->priv->sample_first_row, proxy->priv->sample_last_row);	

		/* sync proxy->priv->chunk to proxy->priv->chunk_to */
		proxy->priv->chunk_sep = 0;
		proxy->priv->chunk_proxy_nb_rows = -1;
		if (!proxy->priv->defer_sync) 
			chunk_sync_idle (proxy);
		else
			proxy->priv->chunk_sync_idle_id = g_idle_add ((GSourceFunc) chunk_sync_idle, proxy);
	}
	else {
		/* nothing to adjust => destroy proxy->priv->chunk_to if necessary */
		if (proxy->priv->chunk_to) {
			display_chunk_free (proxy->priv->chunk_to);
			proxy->priv->chunk_to = NULL;
		}
	}
}

/**
 * gda_data_proxy_apply_all_changes
 * @proxy: a #GdaDataProxy object
 * @error: a place to store errors, or %NULL
 *
 * Apply all the changes stored in the proxy to the proxied data model. The changes are done row
 * after row, and if an error
 * occurs, then it is possible that not all the changes to all the rows have been applied.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_apply_all_changes (GdaDataProxy *proxy, GError **error)
{
	gboolean allok = TRUE;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	gda_data_model_send_hint (proxy->priv->model, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);

	proxy->priv->ignore_proxied_changes = TRUE;
	while (proxy->priv->all_modifs && allok)
		allok = commit_row_modif (proxy, ROW_MODIF (proxy->priv->all_modifs->data), error);
	proxy->priv->ignore_proxied_changes = FALSE;

	gda_data_model_send_hint (proxy->priv->model, GDA_DATA_MODEL_HINT_END_BATCH_UPDATE, NULL);
	if (proxy->priv->proxy_has_changed) 
		adjust_displayed_chunk (proxy);

	return allok;
}

/**
 * gda_data_proxy_cancel_all_changes
 * @proxy: a #GdaDataProxy object
 *
 * Cancel all the changes stored in the proxy (the @proxy will be reset to its state
 * as it was just after creation).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_cancel_all_changes (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);
	g_assert (!proxy->priv->chunk_to);

	/* new rows are first treated and removed (no memory de-allocation here, though) */
	if (proxy->priv->new_rows) {
		if (proxy->priv->chunk) {
			/* Using a chunk */
			proxy->priv->chunk_to = display_chunk_new (proxy->priv->chunk->mapping->len);
			g_array_append_vals (proxy->priv->chunk_to->mapping, 
					     proxy->priv->chunk->mapping->data, proxy->priv->chunk->mapping->len);
			
			while (proxy->priv->new_rows) {
				gint proxy_row;
				
				proxy_row = row_modif_to_proxy_row (proxy, (ROW_MODIF (proxy->priv->new_rows->data)));
				proxy->priv->new_rows = g_slist_delete_link (proxy->priv->new_rows, proxy->priv->new_rows);
				
				if ((proxy_row >= 0) && proxy->priv->chunk_to)
					g_array_remove_index (proxy->priv->chunk_to->mapping, 
							      proxy_row - (proxy->priv->add_null_entry ? 1 : 0));
			}
			
			if (proxy->priv->chunk_to) {
				/* sync proxy->priv->chunk to proxy->priv->chunk_to */
				gboolean defer_sync = proxy->priv->defer_sync;
				proxy->priv->defer_sync = FALSE;
				proxy->priv->chunk_sep = 0;
				proxy->priv->chunk_proxy_nb_rows = -1;
				chunk_sync_idle (proxy);
				proxy->priv->defer_sync = defer_sync;
			}
		}
		else {
			/* no chunk used */
			gint nrows = gda_data_proxy_get_n_rows ((GdaDataModel *) proxy);
			while (proxy->priv->new_rows) {
				proxy->priv->new_rows = g_slist_delete_link (proxy->priv->new_rows, proxy->priv->new_rows);
				if (proxy->priv->notify_changes) {
					gda_data_model_row_removed ((GdaDataModel *) proxy, nrows-1);
					nrows--;
				}
			}
		}
	}

	/* all modified rows are then treated (including memory de-allocation for new rows) */
	while (proxy->priv->all_modifs) {
		gint model_row = ROW_MODIF (proxy->priv->all_modifs->data)->model_row;
		gint proxy_row = -1;

		if (proxy->priv->notify_changes && (model_row >= 0))
			proxy_row = row_modif_to_proxy_row (proxy, (ROW_MODIF (proxy->priv->all_modifs->data)));
			
		row_modifs_free (ROW_MODIF (proxy->priv->all_modifs->data));
		if (model_row >= 0)
			g_hash_table_remove (proxy->priv->modify_rows, GINT_TO_POINTER (model_row));
		proxy->priv->all_modifs = g_slist_delete_link (proxy->priv->all_modifs, proxy->priv->all_modifs);
		
		if ((proxy_row >= 0) && proxy->priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
	}

	return TRUE;
}

/**
 * gda_data_proxy_set_filter_expr
 * @proxy: a #GdaDataProxy object
 * @filter_expr: an SQL based expression which will filter the contents of @proxy, or %NULL to remove any previous filter
 * @error: a place to store errors, or %NULL
 *
 * Sets a filter among the rows presented by @proxy. The filter is defined by a filter expression
 * which can be any SQL valid expression using @proxy's columns. For instance if @proxy has the "id" and
 * "name" columns, then a filter can be "length(name) < 5" to filter only the rows where the length of the
 * name is strictly inferior to 5, or "id >= 1000 and id < 2000 order by name limit 50" to filter only the rows where the id
 * is between 1000 and 2000, ordered by name and limited to 50 rows.
 *
 * Note that any previous filter expression is replaced with the new @filter_expr if no error occurs
 * (if an error occurs, then any previous filter is left unchanged).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_set_filter_expr (GdaDataProxy *proxy, const gchar *filter_expr, GError **error)
{
	static GdaVirtualProvider *provider = NULL;
	GdaConnection *vcnc;
	gchar *sql;
	GdaQuery *query;
	GError *lerror = NULL;
	GdaDataModel *filtered_rows = NULL;
	GdaDataModel *wrapper;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy->priv, FALSE);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (!filter_expr)
		goto clean_previous_filter;

	if (!provider) 
		provider = gda_vprovider_data_model_new ();

	vcnc = gda_server_provider_create_connection (NULL, GDA_SERVER_PROVIDER (provider), NULL, NULL, NULL, 0);
	if (! gda_connection_open (vcnc, NULL)) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			     _("Could not create virtual connection"));
		g_object_unref (vcnc);
		return FALSE;
	}

	/* Force direct data access where proxy_row <=> absolute_row */
	proxy->priv->force_direct_mapping = TRUE,

	/* REM: use a GdaDataModelWrapper to force the viewing of the un-modified columns of @proxy,
	 * otherwise the GdaVconnectionDataModel will just take into account the modified columns
	 */
	wrapper = gda_data_access_wrapper_new ((GdaDataModel*) proxy);
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (vcnc), wrapper,
						   "proxy", error)) {
		g_object_unref (vcnc);
		g_object_unref (wrapper);
		proxy->priv->force_direct_mapping = FALSE;
		return FALSE;
	}
	g_object_unref (wrapper);

	/* generate SQL with a special case if expression starts with "ORDER BY" */
	gchar *tmp, *ptr;
	gint i;
	tmp = g_strdup (filter_expr);
	for (i = 0, ptr = filter_expr; *ptr && (i < 7); ptr++) {
		if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')) {
		}
		else {
			tmp [i] = *ptr;
			i++;
		}
	}
	if (! g_ascii_strncasecmp (tmp, "orderby", 7)) 
		sql = g_strdup_printf ("SELECT __gda_row_nb FROM proxy %s", filter_expr);
	else
		sql = g_strdup_printf ("SELECT __gda_row_nb FROM proxy WHERE %s", filter_expr);
	g_free (tmp);

	query = gda_query_new_from_sql (gda_object_get_dict (GDA_OBJECT (proxy)), sql, &lerror);
	g_free (sql);
	if (lerror) {
		/* also catches problems with multiple statements in @filter_expr, such as SQL code injection */
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			     _("Incorrect filter expression"));
		g_object_unref (vcnc);
		g_object_unref (query);
		g_error_free (lerror);
		proxy->priv->force_direct_mapping = FALSE;
		return FALSE;
	}
	sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL, 0, NULL);
	g_object_unref (query);
	if (sql) {
		GdaCommand *command;
		command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, 0);
		filtered_rows = gda_connection_execute_select_command (vcnc, command, NULL, NULL);
	}
	g_object_unref (vcnc);
	if (!filtered_rows) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			     _("Could not apply filter"));
		proxy->priv->force_direct_mapping = FALSE;
		return FALSE;
	}
	g_assert (GDA_IS_DATA_MODEL (filtered_rows));
	/*gda_data_model_dump (filtered_rows, stdout);*/

	proxy->priv->force_direct_mapping = FALSE;


 clean_previous_filter:
	if (proxy->priv->filter_expr) {
		g_free (proxy->priv->filter_expr);
		proxy->priv->filter_expr = NULL;
	}
	if (proxy->priv->filtered_rows) {
		g_object_unref (proxy->priv->filtered_rows);
		proxy->priv->filtered_rows = NULL;
	}
	if (filtered_rows) {
		proxy->priv->filtered_rows = filtered_rows;
		proxy->priv->filter_expr = g_strdup (filter_expr);
	}

	g_signal_emit (G_OBJECT (proxy),
		       gda_data_proxy_signals[FILTER_CHANGED],
		       0);

	adjust_displayed_chunk (proxy);

	if (!filter_expr)
		return TRUE;
	else
		return filtered_rows ? TRUE : FALSE;
}

/**
 * gda_data_proxy_get_filter_expr
 * @proxy: a #GdaDataProxy object
 *
 * Get the current filter expression used by @proxy.
 *
 * Returns: the current filter expression or %NULL if no filter has been set
 */
const gchar *
gda_data_proxy_get_filter_expr (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy->priv, NULL);

	return proxy->priv->filter_expr;
}



/* 
 * GdaDataModel interface implementation
 */
static gint
gda_data_proxy_get_n_rows (GdaDataModel *model)
{
	gint nbrows;
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, -1);

	if (proxy->priv->chunk && !proxy->priv->force_direct_mapping)
		nbrows = proxy->priv->chunk->mapping->len;
	else {
		if (proxy->priv->model_nb_rows >= 0)
			nbrows = proxy->priv->model_nb_rows;
		else
			return -1; /* unknown number of rows */
		nbrows += g_slist_length (proxy->priv->new_rows);
	}
	if (proxy->priv->add_null_entry)
		nbrows += 1;

	return nbrows;
}

static gint
gda_data_proxy_get_n_columns (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, -1);

	return 2 * proxy->priv->model_nb_cols;
}


typedef struct {
	gchar *name;
	GType  type;
} ExtraColAttrs;

static void create_columns (GdaDataProxy *proxy)
{
	gint i;
	if (proxy->priv->columns)
		return;

	proxy->priv->columns = g_new0 (GdaColumn *, 2 * proxy->priv->model_nb_cols);

	/* current proxy's values */
	for (i = 0; i < proxy->priv->model_nb_cols; i++) {
		GdaColumn *orig;

		orig = gda_data_model_describe_column (proxy->priv->model, i);
		proxy->priv->columns[i] = gda_column_copy (orig);
		gda_column_set_position (proxy->priv->columns[i], i);
	}

	/* proxied data model's values (original values), again reference columns from proxied data model */
	for (; i < 2 * proxy->priv->model_nb_cols; i++) {
		GdaColumn *orig;
		gchar *newname;

		orig = gda_data_model_describe_column (proxy->priv->model, 
						       i -  proxy->priv->model_nb_cols);
		proxy->priv->columns[i] = gda_column_copy (orig);
		newname = g_strdup_printf ("_%s", gda_column_get_name (orig));
		gda_column_set_name (proxy->priv->columns[i], newname);
		gda_column_set_title (proxy->priv->columns[i], newname);
		g_free (newname);
		gda_column_set_position (proxy->priv->columns[i], i);
	}
}
	


static GdaColumn *
gda_data_proxy_describe_column (GdaDataModel *model, gint col)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), NULL);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, NULL);

	if (!proxy->priv->columns)
		create_columns (proxy);

	return proxy->priv->columns [col];
}

static const GValue *
gda_data_proxy_get_value_at (GdaDataModel *model, gint column, gint proxy_row)
{
	gint model_row;
	GValue *retval = NULL;
	GdaDataProxy *proxy;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), NULL);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, NULL);
	g_return_val_if_fail (proxy_row >= 0, NULL);

	if ((proxy_row == 0) && proxy->priv->add_null_entry) 
		return NULL;

	model_row = proxy_row_to_model_row (proxy, proxy_row);

	/* current proxy's values (values may be different than the ones in the proxied data model) */
	if (column < proxy->priv->model_nb_cols) {
		RowModif *rm;
		gint model_col = column % proxy->priv->model_nb_cols;
		gboolean value_has_modifs = FALSE;

		rm = proxy_row_to_row_modif (proxy, proxy_row);
		if (rm && rm->modify_values) {
			/* there are some modifications to the row, see if there are some for the column */
			GSList *list;
			RowValue *rv = NULL;
					
			list = rm->modify_values;
			while (list && !rv) {
				if (ROW_VALUE (list->data)->model_column == model_col)
					rv = ROW_VALUE (list->data);
				list = g_slist_next (list);
			}
			if (rv) {
				value_has_modifs = TRUE;
				retval = rv->value;
			}
		}

		if (!value_has_modifs) {
			/* value has not been modified */
			if (model_row >= 0) {
				/* existing row */
				retval = (GValue *) gda_data_model_get_value_at (proxy->priv->model, 
										   column, model_row);
			}
			else 
				/* empty row, return NULL */
				retval = NULL;
		}

		return retval;
	}
	
	/* proxied data model's values (original values) */
	if (column < 2 *proxy->priv->model_nb_cols) {
		RowModif *rm;
		gint model_col = column % proxy->priv->model_nb_cols;

		rm = proxy_row_to_row_modif (proxy, proxy_row);
		if (rm) {
			if (rm->orig_values)
				retval = rm->orig_values [model_col];
			else
				retval = NULL;
		}
		else {
			if (model_row >= 0)
				retval = (GValue *) gda_data_model_get_value_at (proxy->priv->model, 
										   model_col, model_row);
			else
				retval = NULL;
		}
		return retval;
	}
	
	g_warning (_("Unknown GdaDataProxy column: %d"), column);
	return NULL;
}

static GdaValueAttribute
gda_data_proxy_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	g_return_val_if_fail (((GdaDataProxy *) model)->priv, FALSE);

	return gda_data_proxy_get_value_attributes ((GdaDataProxy *) model, row, col);
}

static GdaDataModelIter *
gda_data_proxy_create_iter (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	GdaDataModelIter *iter, *iter2;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);

	iter = gda_data_model_create_iter (proxy->priv->model);
	g_object_set (G_OBJECT (iter), "forced_model", proxy, NULL);

	iter2 = gda_data_model_create_iter (proxy->priv->model);
	if (iter2) {
		GSList *plist1, *plist2;

		plist1 = GDA_PARAMETER_LIST (iter)->parameters;
		plist2 = GDA_PARAMETER_LIST (iter2)->parameters;
		for (; plist1 && plist2; plist1 = plist1->next, plist2 = plist2->next) {
			gchar *plugin;

			g_object_get (G_OBJECT (plist2->data), "entry_plugin", &plugin, NULL);
			if (plugin) {
				g_object_set (G_OBJECT (plist1->data), "entry_plugin", plugin, NULL);
				g_free (plugin);
			}
		}
		if (plist1 || plist2) 
			g_warning ("Proxy iterator does not have the same length as proxied model's iterator: %d/%d",
				   g_slist_length (GDA_PARAMETER_LIST (iter)->parameters),
				   g_slist_length (GDA_PARAMETER_LIST (iter2)->parameters));
		g_object_unref (iter2);
	}

	return iter;
}

static GdaDataModelAccessFlags
gda_data_proxy_get_access_flags (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), 0);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, 0);

	if (proxy->priv->model)
		return gda_data_model_get_access_flags (proxy->priv->model) | GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		return 0;
}

static gboolean
gda_data_proxy_set_value_at (GdaDataModel *model, gint col, gint proxy_row, const GValue *value, 
			     GError **error)
{
	GdaDataProxy *proxy;
	GdaValueAttribute att = 0;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	att = gda_data_proxy_get_value_attributes (proxy, proxy_row, col);
	if (att & GDA_VALUE_ATTR_NO_MODIF) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_VALUE,
			     _("Value is read-only"));
		return FALSE;
	}

	if ((proxy_row == 0) && proxy->priv->add_null_entry) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_ROW,
			     _("The first row is an empty row artificially prepended and cannot be altered"));
		return FALSE;
	}

	/* current proxy's values (values may be different than the ones in the proxied data model) */
	if ((col >= 0) && (col < proxy->priv->model_nb_cols)) {
		/* Storing a GValue value */
		RowModif *rm;
		RowValue *rv = NULL;
		const GValue *cmp_value;

		/* compare with the current stored value */
		cmp_value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy, col, proxy_row);
		if (! gda_value_compare_ext ((GValue *) value, (GValue *) cmp_value)) {
			/* nothing to do: values are equal */
			return TRUE;
		}

		/* from now on we have a new value for the row */
		rm = find_or_create_row_modif (proxy, proxy_row, col, &rv);

		if (rv) {
			/* compare with the original value (before modifications) and either delete the RowValue, 
			 * or alter it */
			if (rv->value) {
				gda_value_free (rv->value);
				rv->value = NULL;
			}

			if (rm->orig_values && (col < rm->orig_values_size) && 
			    ! gda_value_compare_ext ((GValue *) value, rm->orig_values [col])) {
				/* remove the RowValue */
				rm->modify_values = g_slist_remove (rm->modify_values, rv);
				g_free (rv);
				rv = NULL;
			}
			else {
				/* simply alter the RowValue */
				GdaValueAttribute flags = g_value_get_flags (rv->attributes);
				
				if (value && !gda_value_is_null ((GValue *) value)) {
					flags &= ~GDA_VALUE_ATTR_IS_NULL;
					rv->value = gda_value_copy ((GValue*) value);
				}
				else
					flags |= GDA_VALUE_ATTR_IS_NULL;
				g_value_set_flags (rv->attributes, flags);
			}
		}
		else {
			/* create a new RowValue */
			GdaValueAttribute flags = 0;

			rv = g_new0 (RowValue, 1);
			rv->row_modif = rm;
			rv->model_column = col;
			rv->attributes = gda_value_copy (proxy->priv->columns_attrs [col]);
			flags = g_value_get_flags (rv->attributes);

			if (value && !gda_value_is_null ((GValue*) value)) {
				rv->value = gda_value_copy ((GValue*) value);
				flags &= ~GDA_VALUE_ATTR_IS_NULL;
			}
			else
				flags |= GDA_VALUE_ATTR_IS_NULL;
			if (rm->model_row >= 0)
				flags |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;
			else
				flags &= ~GDA_VALUE_ATTR_HAS_VALUE_ORIG;
			g_value_set_flags (rv->attributes, flags);
			rm->modify_values = g_slist_prepend (rm->modify_values, rv);
		}
		
		if (rv) {
			GdaValueAttribute flags = g_value_get_flags (rv->attributes);
			flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
			flags &= ~GDA_VALUE_ATTR_IS_DEFAULT;
			g_value_set_flags (rv->attributes, flags);
		}

		if (!rm->to_be_deleted && !rm->modify_values && (rm->model_row >= 0)) {
			/* remove that RowModif, it's useless */
			g_hash_table_remove (proxy->priv->modify_rows, GINT_TO_POINTER (rm->model_row));
			proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
			row_modifs_free (rm);
		}

		if (proxy->priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
	}
	else {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_VALUE, 
			     _("Trying to change read-only column: %d"), col);
		return FALSE;
	}

#ifdef GDA_DEBUG_NO
	gda_object_dump (proxy, 5);
#endif

	return TRUE;
}

static gboolean
gda_data_proxy_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaDataProxy *proxy;
	gboolean notify_changes;
	gint col;
	GList *list;
	gboolean err = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);
	if (!values)
		return TRUE;

	g_return_val_if_fail (g_list_length (values) <= gda_data_proxy_get_n_columns (model) , FALSE);

	/* check values */
	col = 0;
	list = values;
	while (list && !err) {
		GValue *value = (GValue *)(list->data);

		if (value && !gda_value_is_null (value)) {
			GdaColumn *column;
			column = gda_data_model_describe_column (model, col);
			if (gda_column_get_g_type (column) != G_VALUE_TYPE (value)) {
				g_set_error (error, GDA_DATA_PROXY_ERROR, 0,
					     _("Value type mismatch %s instead of %s"),
					     gda_g_type_to_string (G_VALUE_TYPE (value)),
					     gda_g_type_to_string (gda_column_get_g_type (column)));
				err = TRUE;
			}
		}
		col++;
		list = g_list_next (list);
	}
	
	/* stop here if there is a value error */
	if (err)
		return FALSE;

	/* temporary disable changes notification */
	notify_changes = proxy->priv->notify_changes;
	proxy->priv->notify_changes = FALSE;

	col = 0;
	list = values;
	while (list && !err) {
		if (gda_data_proxy_set_value_at (model, col, row, (GValue *)(list->data), error)) {
			col++;
			list = g_list_next (list);
		}
		else
			err = TRUE;
	}

	proxy->priv->notify_changes = notify_changes;
	if (col && proxy->priv->notify_changes)
		/* at least one successfull value change occurred */
		gda_data_model_row_updated (model, row);

	return !err;
}

static gint
gda_data_proxy_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaDataProxy *proxy;
	gint newrow;
	gboolean notify_changes;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, -1);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/* temporary disable changes notification */
	notify_changes = proxy->priv->notify_changes;
	proxy->priv->notify_changes = FALSE;
	
	newrow = gda_data_proxy_append (proxy);
	if (! gda_data_proxy_set_values (model, newrow, (GList *) values, error)) {
		gda_data_proxy_remove_row (model, newrow, NULL);
		proxy->priv->notify_changes = notify_changes;
		return -1;
	}
	else {
		proxy->priv->notify_changes = notify_changes;
		if (proxy->priv->notify_changes)
			gda_data_model_row_inserted (model, newrow);
		return newrow;
	}
}

static gint
gda_data_proxy_append_row (GdaDataModel *model, GError **error)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, -1);

	return gda_data_proxy_append (proxy);
}

static gboolean
gda_data_proxy_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);

	if (proxy->priv->add_null_entry && row == 0) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_ROW,
			     _("The first row is an empty row artificially prepended and cannot be removed"));
		return FALSE;
	}

	gda_data_proxy_delete (proxy, row);
	return TRUE;
}

static void
gda_data_proxy_set_notify (GdaDataModel *model, gboolean do_notify_changes)
{
	GdaDataProxy *proxy;
	g_return_if_fail (GDA_IS_DATA_PROXY (model));
	proxy = GDA_DATA_PROXY (model);
	g_return_if_fail (proxy->priv);

	proxy->priv->notify_changes = do_notify_changes;
}

static gboolean
gda_data_proxy_get_notify (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);

	return proxy->priv->notify_changes;
}

static void
gda_data_proxy_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	GdaDataProxy *proxy;
	g_return_if_fail (GDA_IS_DATA_PROXY (model));
	proxy = GDA_DATA_PROXY (model);
	g_return_if_fail (proxy->priv);

	if (proxy->priv->model)
		gda_data_model_send_hint (proxy->priv->model, hint, hint_value);
}
