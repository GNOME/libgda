/*
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <arminb@src.gnome.org>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 - 2012, 2018 Daniel Espinosa <despinosa@src.gnome.org>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
 * Copyright (C) 2018-2019 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-data-proxy"

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-data-proxy.h"
#include "gda-data-model.h"
#include "gda-data-model-array.h"
#include "gda-data-model-extra.h"
#include "gda-data-model-iter.h"
#include "gda-data-select.h"
#include "gda-holder.h"
#include "gda-set.h"
#include "gda-marshal.h"
#include "gda-enums.h"
#include "gda-util.h"
#include "gda-marshal.h"
#include "gda-data-access-wrapper.h"
#include "gda-enum-types.h"
#include <libgda/sqlite/virtual/libgda-virtual.h>
#include <libgda/sqlite/virtual/gda-virtual-provider.h>
#include <libgda/sqlite/virtual/gda-vconnection-data-model.h>
#include <libgda/sqlite/virtual/gda-vprovider-data-model.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-custom-marshal.h>

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
static void                 gda_data_proxy_data_model_init (GdaDataModelInterface *iface);

static gint                 gda_data_proxy_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_proxy_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_proxy_describe_column (GdaDataModel *model, gint col);
static const GValue        *gda_data_proxy_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_proxy_get_attributes_at (GdaDataModel *model, gint col, gint row);


static GdaDataModelAccessFlags gda_data_proxy_get_access_flags(GdaDataModel *model);

static gboolean             gda_data_proxy_set_value_at    (GdaDataModel *model, gint col, gint row,
							    const GValue *value, GError **error);
static gboolean             gda_data_proxy_set_values      (GdaDataModel *model, gint row,
							    GList *values, GError **error);
static gint                 gda_data_proxy_find_row_from_values (GdaDataModel *model, GSList *values,
						           gint *cols_index);

static gint                 gda_data_proxy_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_proxy_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_proxy_remove_row      (GdaDataModel *model, gint row, GError **error);

static void                 gda_data_proxy_freeze          (GdaDataModel *model);
static void                 gda_data_proxy_thaw            (GdaDataModel *model);
static gboolean             gda_data_proxy_get_notify      (GdaDataModel *model);
static void                 gda_data_proxy_send_hint       (GdaDataModel *model, GdaDataModelHint hint,
							    const GValue *hint_value);

/* cache management */
static void clean_cached_changes (GdaDataProxy *proxy);
static void migrate_current_changes_to_cache (GdaDataProxy *proxy);
static void fetch_current_cached_changes (GdaDataProxy *proxy);

#define DEBUG_SYNC
#undef DEBUG_SYNC

static GMutex parser_mutex;
static GdaSqlParser *internal_parser;
static GMutex provider_mutex;
static GdaVirtualProvider *virtual_provider = NULL;

/* signals */
enum
{
	ROW_DELETE_CHANGED,
	SAMPLE_SIZE_CHANGED,
	SAMPLE_CHANGED,
	VALIDATE_ROW_CHANGES,
	ROW_CHANGES_APPLIED,
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
	PROP_SAMPLE_SIZE,
	PROP_CACHE_CHANGES
};

/*
 * Structure to hold the status and all the modifications made to a single
 * row. It is not possible to have ((model_row < 0) && !modify_values)
 */
typedef struct
{
	gint           model_row;    /* row index in the proxied GdaDataModel, -1 if new row */
	gboolean       to_be_deleted;/* TRUE if row is to be deleted */
	GSList        *modify_values; /* list of RowValue structures */
	GValue       **orig_values;  /* array of the original GValues, indexed on the column numbers */
	gint           orig_values_size; /* size of @orig_values, OR (for new rows) the number of columns of the proxied data model the new row is for */
} RowModif;
#define ROW_MODIF(x) ((RowModif *)(x))

/*
 * Structure to hold the modifications made to a single value
 */
typedef struct
{
	RowModif      *row_modif;    /* RowModif in which this structure instance appears */
	gint           model_column; /* column index in the GdaDataModel */
        GValue        *value;        /* values are owned here */
	GdaValueAttribute attributes;   /* holds flags */
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
} DisplayChunk;
static DisplayChunk *display_chunk_new (gint reserved_size);
static void          display_chunk_free (DisplayChunk *chunk);

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
typedef struct {
	GRecMutex          mutex;

	GdaDataModel      *model; /* Gda model which is proxied */

	GdaConnection     *filter_vcnc;   /* virtual connection used for filtering */
	gchar             *filter_expr;   /* NULL if no filter applied */
	GdaStatement      *filter_stmt;   /* NULL if no filter applied */
	GdaDataModel      *filtered_rows; /* NULL if no filter applied. Lists rows (by their number) which must be displayed */

	GdaValueAttribute *columns_attrs; /* Each GValue holds a flag of GdaValueAttribute to proxy. cols. attributes */

	gint               model_nb_cols; /* = gda_data_model_get_n_columns (model) */
	gint               model_nb_rows; /* = gda_data_model_get_n_rows (model) */
	gboolean           notify_changes;

	GSList            *all_modifs; /* list of RowModif structures, for memory management */
	GSList            *new_rows;   /* list of RowModif, no data allocated in this list */
	GHashTable        *modify_rows;  /* key = model_row number, value = RowModif, NOT for new rows */

	gboolean           defer_proxied_model_insert;
	gint               catched_inserted_row;

	gboolean           add_null_entry; /* artificially add a NULL entry at the beginning of the tree model */

	gboolean           defer_sync;

	/* chunking */
	gboolean           force_direct_mapping;
	gint               sample_first_row;
	gint               sample_last_row;
	gint               sample_size;
	guint              chunk_sync_idle_id;
	DisplayChunk      *chunk; /* number of proxy_rows depends directly on chunk->mapping->len */
	DisplayChunk      *chunk_to; /* NULL if nothing to do */
	gint               chunk_sep;
	gint               chunk_proxy_nb_rows;

	/* for ALL the columns of proxy */
	GdaColumn        **columns;

	/* modifications cache */
	gboolean           cache_changes;
	GSList            *cached_modifs;
	GSList            *cached_inserts;
} GdaDataProxyPrivate;
G_DEFINE_TYPE_WITH_CODE(GdaDataProxy, gda_data_proxy, G_TYPE_OBJECT,
                        G_ADD_PRIVATE (GdaDataProxy)
                        G_IMPLEMENT_INTERFACE(GDA_TYPE_DATA_MODEL, gda_data_proxy_data_model_init))

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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (model_row < 0)
		return -1;
	if ((model_row >= priv->model_nb_rows) && (priv->model_nb_rows >= 0))
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (rm == NULL)
		return -1;
	if (rm->model_row == -1) {
		gint index;
		if (priv->model_nb_rows == -1)
			return -1;
		index = g_slist_index (priv->new_rows, rm);
		if (index < 0)
			return -1;
		return priv->model_nb_rows + index;
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (abs_row < 0)
		return -1;
	if ((abs_row < priv->model_nb_rows) || (priv->model_nb_rows < 0)) {
		if (rm) {
			gint tmp;
			tmp = abs_row;
			*rm = g_hash_table_lookup (priv->modify_rows, &tmp);
		}
		return abs_row;
	}
	else {
		if (rm)
			*rm = g_slist_nth_data (priv->new_rows, abs_row - priv->model_nb_rows);
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (proxy_row < 0)
		return -1;
	if (priv->force_direct_mapping)
		return proxy_row;

	if (priv->add_null_entry) {
		if (proxy_row == 0)
			return -1;
		else
			proxy_row--;
	}
	if (priv->chunk) {
		if ((guint)proxy_row < priv->chunk->mapping->len)
			return g_array_index (priv->chunk->mapping, gint, proxy_row);
		else
			return -1;
	}
	else {
		if (priv->chunk_to &&
		    priv->chunk_to->mapping &&
		    (proxy_row < priv->chunk_sep) &&
		    ((guint)proxy_row < priv->chunk_to->mapping->len))
			return g_array_index (priv->chunk_to->mapping, gint, proxy_row);
		else
			return proxy_row;
	}
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	gint proxy_row = -1;
	if (abs_row < 0)
		return -1;

	if (priv->force_direct_mapping) {
		gint proxy_n_rows;
		proxy_row = abs_row;
		proxy_n_rows = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy);
		if ((proxy_row >= proxy_n_rows) && (proxy_n_rows >= 0))
			proxy_row = -1;
		return proxy_row;
	}

	if (priv->chunk) {
		gsize i;
		for (i = 0; i < priv->chunk->mapping->len; i++) {
			if (g_array_index (priv->chunk->mapping, gint, i) == abs_row) {
				proxy_row = i;
				break;
			}
		}
		if ((proxy_row >= 0) && priv->add_null_entry)
			proxy_row ++;
	}
	else {
		if (priv->chunk_to && priv->chunk_to->mapping) {
			/* search in the priv->chunk_sep first rows of priv->chunk_to */
			gint i;
			for (i = 0; i < MIN ((gint)priv->chunk->mapping->len, priv->chunk_sep); i++) {
				if (g_array_index (priv->chunk_to->mapping, gint, i) == abs_row) {
					proxy_row = i;
					break;
				}
			}
			if ((proxy_row >= 0) && priv->add_null_entry)
				proxy_row ++;
		}
		if (proxy_row < 0) {
			gint proxy_n_rows;
			proxy_row = abs_row;
			if (priv->add_null_entry)
				proxy_row ++;
			proxy_n_rows = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy);
			if ((proxy_row >= proxy_n_rows) && (proxy_n_rows >= 0))
				proxy_row = -1;
		}
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
		for (i = 0; i < rm->orig_values_size; i++) {
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

#ifdef GDA_DEBUG
	rm = proxy_row_to_row_modif (proxy, proxy_row);
	if (rm)
		g_warning ("%s(): RowModif already exists for that proxy_row", __FUNCTION__);
#endif

	rm = g_new0 (RowModif, 1);
	if (proxy_row >= 0) {
		gint i, model_row;

		rm->orig_values = g_new0 (GValue *, priv->model_nb_cols);
		rm->orig_values_size = priv->model_nb_cols;
		model_row = proxy_row_to_model_row (proxy, proxy_row);

		if (model_row >= 0) {
			for (i=0; i<priv->model_nb_cols; i++) {
				const GValue *oval;

				oval = gda_data_model_get_value_at (priv->model, i, model_row, NULL);
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

static gboolean
validate_row_changes_accumulator (G_GNUC_UNUSED GSignalInvocationHint *ihint,
				  GValue *return_accu,
				  const GValue *handler_return,
				  G_GNUC_UNUSED gpointer data)
{
	GError *error;

        error = g_value_get_boxed (handler_return);
        g_value_set_boxed (return_accu, error);

        return error ? FALSE : TRUE; /* stop signal if 'thisvalue' is FALSE */
}

static GError *
m_validate_row_changes (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint row,
			G_GNUC_UNUSED gint proxied_row)
{
        return NULL; /* defaults allows changes */
}

static void
gda_data_proxy_class_init (GdaDataProxyClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	/* signals */
	/**
	 * GdaDataProxy::row-delete-changed:
	 * @proxy: the #GdaDataProxy
	 * @row: the concerned @proxy's row
	 * @to_be_deleted: tells if the @row is marked to be deleted
	 *
	 * Gets emitted whenever a row has been marked to be deleted, or has been unmarked to be deleted
	 */
	gda_data_proxy_signals [ROW_DELETE_CHANGED] =
		g_signal_new ("row-delete-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, row_delete_changed),
                              NULL, NULL,
			      _gda_marshal_VOID__INT_BOOLEAN, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_BOOLEAN);
	/**
	 * GdaDataProxy::sample-size-changed:
	 * @proxy: the #GdaDataProxy
	 * @sample_size: the new sample size
	 *
	 * Gets emitted whenever @proxy's sample size has been changed
	 */
	gda_data_proxy_signals [SAMPLE_SIZE_CHANGED] =
		g_signal_new ("sample-size-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, sample_size_changed),
                              NULL, NULL,
			      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	/**
	 * GdaDataProxy::sample-changed:
	 * @proxy: the #GdaDataProxy
	 * @sample_start: the first row of the sample
	 * @sample_end: the last row of the sample
	 *
	 * Gets emitted whenever @proxy's sample size has been changed. @sample_start and @sample_end are
	 * in reference to the proxied data model.
	 */
	gda_data_proxy_signals [SAMPLE_CHANGED] =
		g_signal_new ("sample-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, sample_changed),
                              NULL, NULL,
			      _gda_marshal_VOID__INT_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	/**
	 * GdaDataProxy::validate-row-changes:
	 * @proxy: the #GdaDataProxy
	 * @row: the proxy's row
	 * @proxied_row: the proxied data model's row
	 *
	 * Gets emitted when @proxy is about to commit a row change to the proxied data model. If any
	 * callback returns a non %NULL value, then the change commit fails with the returned #GError
	 *
	 * Returns: a new #GError if validation failed, or %NULL
	 */
	gda_data_proxy_signals [VALIDATE_ROW_CHANGES] =
		g_signal_new ("validate-row-changes",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, validate_row_changes),
                              validate_row_changes_accumulator, NULL,
                              _gda_marshal_ERROR__INT_INT, G_TYPE_ERROR, 2, G_TYPE_INT, G_TYPE_INT);
	/**
	 * GdaDataProxy::row-changes-applied:
	 * @proxy: the #GdaDataProxy
	 * @row: the proxy's row
	 * @proxied_row: the proxied data model's row
	 *
	 * Gets emitted when @proxy has committed a row change to the proxied data model.
	 */
	gda_data_proxy_signals [ROW_CHANGES_APPLIED] =
		g_signal_new ("row-changes-applied",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaDataProxyClass, row_changes_applied),
                              NULL, NULL,
			      _gda_marshal_VOID__INT_INT, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	/**
	 * GdaDataProxy::filter-changed:
	 * @proxy: the #GdaDataProxy
	 *
	 * Gets emitted when @proxy's filter has been changed
	 */
	gda_data_proxy_signals [FILTER_CHANGED] =
		g_signal_new ("filter-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaDataProxyClass, filter_changed),
                              NULL, NULL,
			      _gda_marshal_VOID__VOID, G_TYPE_NONE, 0);

	klass->row_delete_changed = NULL;
	klass->sample_size_changed = NULL;
	klass->sample_changed = NULL;
	klass->validate_row_changes = m_validate_row_changes;
	klass->row_changes_applied = NULL;
	klass->filter_changed = NULL;

	/* virtual functions */
	object_class->dispose = gda_data_proxy_dispose;
	object_class->finalize = gda_data_proxy_finalize;

	/* Properties */
	object_class->set_property = gda_data_proxy_set_property;
	object_class->get_property = gda_data_proxy_get_property;

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", NULL, "Proxied data model",
                                                               GDA_TYPE_DATA_MODEL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));
	g_object_class_install_property (object_class, PROP_ADD_NULL_ENTRY,
					 g_param_spec_boolean ("prepend-null-entry", NULL,
							       "Tells if a row composed of NULL values is inserted "
							       "as the proxy's first row", FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DEFER_SYNC,
					 g_param_spec_boolean ("defer-sync", NULL,
							       "Tells if changes to the sample of rows displayed "
							       "is done in background in several steps or if it's "
							       "done in one step.", TRUE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_SAMPLE_SIZE,
					 g_param_spec_int ("sample-size", NULL,
							   "Number of rows which the proxy will contain at any time, "
							   "like a sliding window on the proxied data model",
							   0, G_MAXINT - 1, 300,
							   (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	/**
	 * GdaDataProxy:cache-changes:
	 *
	 * Defines how changes kept in the data proxy are handled when the proxied data model
	 * is changed (using the "model" property). The default is to silently discard all the
	 * changes, but if this property is set to %TRUE, then the changes are cached.
	 *
	 * If set to %TRUE, each cached change will be re-applied to a newly set proxied data model if
	 * the change's number of columns match the proxied data model's number of columns and based on:
	 * <itemizedlist>
	 *   <listitem><para>the contents of the proxied data model's modified row for updates and deletes</para></listitem>
	 *   <listitem><para>the inserts are always kept</para></listitem>
	 * </itemizedlist>
	 *
	 * Since: 5.2
	 **/
	g_object_class_install_property (object_class, PROP_CACHE_CHANGES,
					 g_param_spec_boolean ("cache-changes", NULL,
							       "set to TRUE to keep track of changes even when the proxied data model is changed", FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_mutex_lock (&parser_mutex);
	internal_parser = gda_sql_parser_new ();
	g_mutex_unlock (&parser_mutex);
}

static void
gda_data_proxy_data_model_init (GdaDataModelInterface *iface)
{
	iface->get_n_rows = gda_data_proxy_get_n_rows;
	iface->get_n_columns = gda_data_proxy_get_n_columns;
	iface->describe_column = gda_data_proxy_describe_column;
	iface->get_access_flags = gda_data_proxy_get_access_flags;
	iface->get_value_at = gda_data_proxy_get_value_at;
	iface->get_attributes_at = gda_data_proxy_get_attributes_at;

	iface->create_iter = NULL;

	iface->set_value_at = gda_data_proxy_set_value_at;
	iface->set_values = gda_data_proxy_set_values;
        iface->append_values = gda_data_proxy_append_values;
	iface->append_row = gda_data_proxy_append_row;
	iface->remove_row = gda_data_proxy_remove_row;
	iface->find_row = gda_data_proxy_find_row_from_values;

	iface->freeze = gda_data_proxy_freeze;
	iface->freeze = gda_data_proxy_thaw;
	iface->get_notify = gda_data_proxy_get_notify;
	iface->send_hint = gda_data_proxy_send_hint;

	iface->row_inserted = NULL;
	iface->row_updated = NULL;
	iface->row_removed = NULL;
}

/*
 * REM: @add_null_entry, @defer_sync and @cache_changes are not defined
 */
static void
do_init (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_rec_mutex_init (& (priv->mutex)); /* REM: make sure clean_proxy() is called first because
						    * we must not call g_rec_mutex_init() on an already initialized
						    * GRecMutex, see doc. */

	priv->modify_rows = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, NULL);
	priv->notify_changes = TRUE;

	priv->force_direct_mapping = FALSE;
	priv->sample_size = 0;
	priv->chunk = NULL;
	priv->chunk_to = NULL;
	priv->chunk_sync_idle_id = 0;
	priv->columns = NULL;

	priv->defer_proxied_model_insert = FALSE;
	priv->catched_inserted_row = -1;
}

static void
gda_data_proxy_init (GdaDataProxy *proxy)
{

	do_init (proxy);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	priv->add_null_entry = FALSE;
	priv->defer_sync = FALSE;
	priv->cache_changes = FALSE;
}

static DisplayChunk *compute_display_chunk (GdaDataProxy *proxy);
static void adjust_displayed_chunk (GdaDataProxy *proxy);
static gboolean chunk_sync_idle (GdaDataProxy *proxy);
static void ensure_chunk_sync (GdaDataProxy *proxy);


static void proxied_model_row_inserted_cb (GdaDataModel *model, gint row, GdaDataProxy *proxy);
static void proxied_model_row_updated_cb (GdaDataModel *model, gint row, GdaDataProxy *proxy);
static void proxied_model_row_removed_cb (GdaDataModel *model, gint row, GdaDataProxy *proxy);
static void proxied_model_reset_cb (GdaDataModel *model, GdaDataProxy *proxy);
static void proxied_model_access_changed_cb (GdaDataModel *model, GdaDataProxy *proxy);


/**
 * gda_data_proxy_new:
 * @model: (transfer none): Data model to be proxied
 *
 * Creates a new proxy for @model. For bindings use @gda_data_proxy_new_with_data_model. 
 *
 * Returns: (transfer full): a new #GdaDataProxy object
 */
GObject *
gda_data_proxy_new (GdaDataModel *model)
{
	GObject *obj;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), NULL);

	obj = g_object_new (GDA_TYPE_DATA_PROXY, "model", model, NULL);

	return obj;
}

/**
 * gda_data_proxy_new_with_data_model:
 * @model: (transfer none): Data model to be proxied
 *
 * Creates a new proxy for @model. This is the preferred method to create 
 * #GdaDataProxy objects by bindings.
 *
 * Returns: (transfer full): a new #GdaDataProxy object
 *
 * Since: 5.2.0
 */
GdaDataProxy*
gda_data_proxy_new_with_data_model (GdaDataModel *model)
{
	GObject *obj;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), NULL);

	obj = g_object_new (GDA_TYPE_DATA_PROXY, "model", model, NULL);

	return (GdaDataProxy*) obj;
}

static void
clean_proxy (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (priv->all_modifs) {
		gda_data_proxy_cancel_all_changes (proxy);
		g_assert (! priv->all_modifs);
	}

	if (priv->modify_rows) {
		g_hash_table_destroy (priv->modify_rows);
		priv->modify_rows = NULL;
	}

	if (priv->filter_vcnc) {
		g_object_unref (priv->filter_vcnc);
		priv->filter_vcnc = NULL;
	}

	if (priv->filter_expr) {
		g_free (priv->filter_expr);
		priv->filter_expr = NULL;
	}

	if (priv->filter_stmt) {
		g_object_unref (priv->filter_stmt);
		priv->filter_stmt = NULL;
	}

	if (priv->filtered_rows) {
		g_object_unref (priv->filtered_rows);
		priv->filtered_rows = NULL;
	}

	priv->force_direct_mapping = FALSE;
	if (priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		priv->chunk_sync_idle_id = 0;
	}

	if (priv->chunk) {
		display_chunk_free (priv->chunk);
		priv->chunk = NULL;
	}
	if (priv->chunk_to) {
		display_chunk_free (priv->chunk_to);
		priv->chunk_to = NULL;
	}

	if (priv->columns) {
		gint i;
		for (i = 0; i < 2 * priv->model_nb_cols; i++)
			g_object_unref (G_OBJECT (priv->columns[i]));
		g_free (priv->columns);
		priv->columns = NULL;
	}

	if (priv->model) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->model),
						      G_CALLBACK (proxied_model_row_inserted_cb), proxy);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->model),
						      G_CALLBACK (proxied_model_row_updated_cb), proxy);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->model),
						      G_CALLBACK (proxied_model_row_removed_cb), proxy);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->model),
						      G_CALLBACK (proxied_model_reset_cb), proxy);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->model),
						      G_CALLBACK (proxied_model_access_changed_cb), proxy);
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->columns_attrs) {
		g_free (priv->columns_attrs);
		priv->columns_attrs = NULL;
	}

	g_rec_mutex_clear (& (priv->mutex));
}

static void
gda_data_proxy_dispose (GObject *object)
{
	GdaDataProxy *proxy;

	g_return_if_fail (GDA_IS_DATA_PROXY (object));

	proxy = GDA_DATA_PROXY (object);
	clean_proxy (proxy);

	clean_cached_changes (proxy);

	/* parent class */
	G_OBJECT_CLASS (gda_data_proxy_parent_class)->dispose (object);
}

static void
gda_data_proxy_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_PROXY (object));

	/* parent class */
	G_OBJECT_CLASS (gda_data_proxy_parent_class)->finalize (object);
}

static void
gda_data_proxy_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaDataProxy *proxy;

	proxy = GDA_DATA_PROXY (object);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (priv) {
		g_rec_mutex_lock (& (priv->mutex));
		switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *model;
			gint col;
			gboolean already_set = FALSE;

			if (priv->model) {
				if (priv->cache_changes)
					migrate_current_changes_to_cache (proxy);

				gboolean notify_changes;
				notify_changes = priv->notify_changes;

				priv->notify_changes = FALSE;
				clean_proxy (proxy);
				priv->notify_changes = notify_changes;

				do_init (proxy);
				already_set = TRUE;
				g_object_unref (priv->model);
				priv->model = NULL;
			}

			model = (GdaDataModel*) g_value_get_object (value);
			g_return_if_fail (GDA_IS_DATA_MODEL (model));

			if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
				g_warning (_("GdaDataProxy can't handle non random access data models"));
				g_rec_mutex_unlock (& (priv->mutex));
				return;
			}
			priv->model = g_object_ref (model);

			priv->model_nb_cols = gda_data_model_get_n_columns (model);
			priv->model_nb_rows = gda_data_model_get_n_rows (model);

			/* column attributes */
			priv->columns_attrs = g_new0 (GdaValueAttribute, priv->model_nb_cols);
			for (col = 0; col < priv->model_nb_cols; col++) {
				GdaColumn *column;
				GdaValueAttribute flags = GDA_VALUE_ATTR_IS_UNCHANGED;

				column = gda_data_model_describe_column (model, col);
				if (gda_column_get_allow_null (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
				if (gda_column_get_default_value (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;
				priv->columns_attrs[col] = flags;
			}

			g_signal_connect (G_OBJECT (model), "row-inserted",
					  G_CALLBACK (proxied_model_row_inserted_cb), proxy);
			g_signal_connect (G_OBJECT (model), "row-updated",
					  G_CALLBACK (proxied_model_row_updated_cb), proxy);
			g_signal_connect (G_OBJECT (model), "row-removed",
					  G_CALLBACK (proxied_model_row_removed_cb), proxy);
			g_signal_connect (G_OBJECT (model), "reset",
					  G_CALLBACK (proxied_model_reset_cb), proxy);
			g_signal_connect (G_OBJECT (model), "access-changed",
					  G_CALLBACK (proxied_model_access_changed_cb), proxy);

			/* initial chunk settings, no need to emit any signal as it's an initial state */
			priv->chunk = compute_display_chunk (proxy);
			if (!priv->chunk->mapping) {
				display_chunk_free (priv->chunk);
				priv->chunk = NULL;
			}

			if (priv->cache_changes)
				fetch_current_cached_changes (proxy);

			if (already_set)
				gda_data_model_reset (GDA_DATA_MODEL (proxy));
			break;
		}
		case PROP_ADD_NULL_ENTRY:
			if (priv->add_null_entry != g_value_get_boolean (value)) {
				priv->add_null_entry = g_value_get_boolean (value);

				if (priv->add_null_entry)
					gda_data_model_row_inserted ((GdaDataModel *) proxy, 0);
				else
					gda_data_model_row_removed ((GdaDataModel *) proxy, 0);
			}
			break;
		case PROP_DEFER_SYNC:
			priv->defer_sync = g_value_get_boolean (value);
			if (!priv->defer_sync && priv->chunk_sync_idle_id) {
				g_idle_remove_by_data (proxy);
				priv->chunk_sync_idle_id = 0;
				chunk_sync_idle (proxy);
			}
			break;
		case PROP_SAMPLE_SIZE:
			priv->sample_size = g_value_get_int (value);
			if (priv->sample_size < 0)
				priv->sample_size = 0;

			/* initial chunk settings, no need to emit any signal as it's an initial state */
			priv->chunk = compute_display_chunk (proxy);
			if (!priv->chunk->mapping) {
				display_chunk_free (priv->chunk);
				priv->chunk = NULL;
			}
			break;
		case PROP_CACHE_CHANGES:
			priv->cache_changes = g_value_get_boolean (value);
			if (! priv->cache_changes)
				clean_cached_changes (proxy);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
		g_rec_mutex_unlock (& (priv->mutex));
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (priv) {
		g_rec_mutex_lock (& (priv->mutex));
		switch (param_id) {
		case PROP_ADD_NULL_ENTRY:
			g_value_set_boolean (value, priv->add_null_entry);
			break;
		case PROP_DEFER_SYNC:
			g_value_set_boolean (value, priv->defer_sync);
			break;
		case PROP_SAMPLE_SIZE:
			g_value_set_int (value, priv->sample_size);
			break;
		case PROP_CACHE_CHANGES:
			g_value_set_boolean (value, priv->cache_changes);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
		g_rec_mutex_unlock (& (priv->mutex));
	}
}

static void
proxied_model_row_inserted_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	gint abs_row;
	gint signal_row_offset = priv->add_null_entry ? 1 : 0;
	abs_row = row; /* can't call model_row_to_absolute_row because @row is not *officially* part of the computations */

	/* internal cleanups: update chunk and chunk_to arrays */
	if (priv->chunk) {
		gsize i;
		gint *v;

		for (i = 0; i < priv->chunk->mapping->len; i++) {
			v = &g_array_index (priv->chunk->mapping, gint, i);
			if (*v >= abs_row)
				*v += 1;
		}
	}
	if (priv->chunk_to && priv->chunk->mapping) {
		gsize i;
		gint *v;

		for (i = 0; i < priv->chunk_to->mapping->len; i++) {
			v = &g_array_index (priv->chunk_to->mapping, gint, i);
			if (*v >= abs_row)
				*v -= 1;
		}
	}

	/* update all the RowModif where model_row > row */
	if (priv->all_modifs) {
		GSList *list;
		for (list = priv->all_modifs; list; list = list->next) {
			RowModif *tmprm;
			tmprm = ROW_MODIF (list->data);
			if (tmprm->model_row > row) {
				gint tmp;
				tmp = tmprm->model_row;
				g_hash_table_remove (priv->modify_rows, &tmp);
				tmprm->model_row ++;

				gint *ptr;
				ptr = g_new (gint, 1);
				*ptr = tmprm->model_row;
				g_hash_table_insert (priv->modify_rows, ptr, tmprm);
			}
		}
	}

	/* Note: if there is a chunk, then the new row will *not* be part of that chunk and so
	 * no signal will be emitted for its insertion */
	priv->model_nb_rows ++;
	if (priv->defer_proxied_model_insert)
		priv->catched_inserted_row = row;
	else if (!priv->chunk && !priv->chunk_to)
		gda_data_model_row_inserted ((GdaDataModel *) proxy, row + signal_row_offset);
}

static void
proxied_model_row_updated_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, GdaDataProxy *proxy)
{
	gint proxy_row, tmp;
	RowModif *rm;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	/* destroy any RowModif associated ro @row */
	tmp = row;
	rm = g_hash_table_lookup (priv->modify_rows, &tmp);
	if (rm) {
		/* FIXME: compare with the new value of the updated row and remove RowModif only if there
		 * are no more differences. For now we only get rid of that RowModif.
		 */
		g_hash_table_remove (priv->modify_rows, &tmp);
		priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
		row_modifs_free (rm);
	}

	/* if @row is a "visible" row, then emit the updated signal on it */
	proxy_row = absolute_row_to_proxy_row (proxy, model_row_to_absolute_row (proxy, row));
	if (proxy_row >= 0)
		gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
}

static void
proxied_model_row_removed_cb (G_GNUC_UNUSED GdaDataModel *model, gint row, GdaDataProxy *proxy)
{
	gint proxy_row, abs_row;
	RowModif *rm;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	gint signal_row_offset = priv->add_null_entry ? 1 : 0;
	abs_row = model_row_to_absolute_row (proxy, row);
	proxy_row = absolute_row_to_proxy_row (proxy, abs_row);

	/* internal cleanups: update chunk and chunk_to arrays */
	if (priv->chunk) {
		gsize i;
		gint *v, remove_index = -1;

		for (i = 0; i < priv->chunk->mapping->len; i++) {
			v = &g_array_index (priv->chunk->mapping, gint, i);
			if (*v > abs_row)
				*v -= 1;
			else if (*v == abs_row) {
				g_assert (remove_index == -1);
				remove_index = i;
			}
		}
		if (remove_index >= 0)
			g_array_remove_index (priv->chunk->mapping, remove_index);
		if ((proxy_row >= 0) && (priv->chunk_sep >= (proxy_row - signal_row_offset)))
			priv->chunk_sep--;
	}
	if (priv->chunk_to && priv->chunk->mapping) {
		guint i;
		gint *v, remove_index = -1;

		for (i = 0; i < priv->chunk_to->mapping->len; i++) {
			v = &g_array_index (priv->chunk_to->mapping, gint, i);
			if (*v > abs_row)
				*v -= 1;
			else if (*v == abs_row) {
				g_assert (remove_index == -1);
				remove_index = i;
			}
		}
		if (remove_index >= 0)
			g_array_remove_index (priv->chunk_to->mapping, remove_index);
	}
	priv->chunk_proxy_nb_rows--;
	priv->model_nb_rows --;

	/* destroy any RowModif associated ro @row */
	gint tmp;
	tmp = row;
	rm = g_hash_table_lookup (priv->modify_rows, &tmp);
	if (rm) {
		g_hash_table_remove (priv->modify_rows, &tmp);
		priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
		row_modifs_free (rm);
	}

	/* update all the RowModif where model_row > row */
	if (priv->all_modifs) {
		GSList *list;
		for (list = priv->all_modifs; list; list = list->next) {
			RowModif *tmprm;
			tmprm = ROW_MODIF (list->data);
			if (tmprm->model_row > row) {
				tmp = tmprm->model_row;
				g_hash_table_remove (priv->modify_rows, &tmp);
				tmprm->model_row --;

				gint *ptr;
				ptr = g_new (gint, 1);
				*ptr = tmprm->model_row;
				g_hash_table_insert (priv->modify_rows, ptr, tmprm);
			}
		}
	}

	/* actual signal emission if row is 'visible' */
	if (proxy_row >= 0)
		gda_data_model_row_removed ((GdaDataModel *) proxy, proxy_row);
}

/*
 * called when the proxied model emits a "reset" signal
 */
static void
proxied_model_reset_cb (GdaDataModel *model, GdaDataProxy *proxy)
{
	g_object_ref (G_OBJECT (model));
	clean_proxy (proxy);
	do_init (proxy);
	g_object_set (G_OBJECT (proxy), "model", model, NULL);
	g_object_unref (G_OBJECT (model));
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	if (priv->columns) {
		/* adjust column's types */
		gint i;
		GdaColumn *orig;
		for (i = 0; i < priv->model_nb_cols; i++) {
			orig = gda_data_model_describe_column (priv->model, i);
			gda_column_set_g_type (priv->columns[i], gda_column_get_g_type (orig));
		}
		for (; i < 2 * priv->model_nb_cols; i++) {
			orig = gda_data_model_describe_column (priv->model,
							       i -  priv->model_nb_cols);
			gda_column_set_g_type (priv->columns[i], gda_column_get_g_type (orig));
		}
	}

	gda_data_model_reset (GDA_DATA_MODEL (proxy));
}

static void
proxied_model_access_changed_cb (G_GNUC_UNUSED GdaDataModel *model, GdaDataProxy *proxy)
{
	g_signal_emit_by_name (proxy, "access-changed");
}

/**
 * gda_data_proxy_get_proxied_model:
 * @proxy: a #GdaDataProxy object
 *
 * Fetch the #GdaDataModel which @proxy does proxy
 *
 * Returns: (transfer none): the proxied data model
 */
GdaDataModel *
gda_data_proxy_get_proxied_model (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->model;
}

/**
 * gda_data_proxy_get_proxied_model_n_cols:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->model_nb_cols;
}

/**
 * gda_data_proxy_get_proxied_model_n_rows:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return gda_data_model_get_n_rows (priv->model);
}

/**
 * gda_data_proxy_is_read_only:
 * @proxy: a #GdaDataProxy object
 *
 * Returns: TRUE if the proxied data model is itself read-only
 */
gboolean
gda_data_proxy_is_read_only (GdaDataProxy *proxy)
{
	GdaDataModelAccessFlags flags;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), TRUE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	flags = gda_data_model_get_access_flags (priv->model);
	return ! (flags & GDA_DATA_MODEL_ACCESS_WRITE);
}


static RowModif *find_or_create_row_modif (GdaDataProxy *proxy, gint proxy_row, gint col, RowValue **ret_rv);


/*
 * Stores the new RowValue in @rv
 */
static RowModif *
find_or_create_row_modif (GdaDataProxy *proxy, gint proxy_row, gint col, RowValue **ret_rv)
{
	RowModif *rm = NULL;
	RowValue *rv = NULL;
	gint model_row;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_assert (proxy_row >= 0);

	model_row = absolute_row_to_model_row (proxy,
					       proxy_row_to_absolute_row (proxy, proxy_row), &rm);
	if (!rm) {
		/* create a new RowModif */
		g_assert (model_row >= 0);
		rm = row_modifs_new (proxy, proxy_row);
		rm->model_row = model_row;

		gint *ptr;
		ptr = g_new (gint, 1);
		*ptr = model_row;
		g_hash_table_insert (priv->modify_rows, ptr, rm);
		priv->all_modifs = g_slist_prepend (priv->all_modifs, rm);
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
 * gda_data_proxy_get_values:
 * @proxy: a #GdaDataProxy object
 * @proxy_row: a proxy row
 * @cols_index: (array length=n_cols): array containing the columns for which the values are requested
 * @n_cols: size of @cols_index
 *
 * Retrieve a whole list of values from the @proxy data model. This function
 * calls gda_data_proxy_get_value()
 * for each column index specified in @cols_index, and generates a #GSList on the way.
 *
 * Returns: (element-type GValue) (transfer container): a new list of values (the list must be freed, not the values),
 * or %NULL if an error occurred
 */
GSList *
gda_data_proxy_get_values (GdaDataProxy *proxy, gint proxy_row, gint *cols_index, gint n_cols)
{
	GSList *retval = NULL;
	gint i;
	const GValue *value;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy_row >= 0, NULL);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	for (i = 0; i < n_cols; i++) {
		value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy, cols_index[i], proxy_row, NULL);
		if (value)
			retval = g_slist_prepend (retval, (GValue *) value);
		else {
			g_slist_free (retval);
			g_rec_mutex_unlock (& (priv->mutex));
			return NULL;
		}
	}
	g_rec_mutex_unlock (& (priv->mutex));

	return g_slist_reverse (retval);
}

/**
 * gda_data_proxy_get_value_attributes:
 * @proxy: a #GdaDataProxy object
 * @proxy_row: a proxy row
 * @col: a valid proxy column
 *
 * Get the attributes of the value stored at (proxy_row, col) in @proxy, which
 * is an ORed value of #GdaValueAttribute flags
 *
 * Returns: a #GdaValueAttribute with the value's attributes at given position
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
	g_return_val_if_fail (proxy_row >= 0, 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	model_column = col % priv->model_nb_cols;
	model_row = proxy_row_to_model_row (proxy, proxy_row);
	flags = gda_data_model_get_attributes_at (priv->model, model_column, model_row);
	if (model_row < 0)
		flags |= GDA_VALUE_ATTR_IS_NULL;

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
				flags |= rv->attributes;
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

	g_rec_mutex_unlock (& (priv->mutex));

	/*g_print ("%s (%p, %d, %d) => %d\n", __FUNCTION__, proxy, col, proxy_row, flags);*/
	return flags;
}

/**
 * gda_data_proxy_alter_value_attributes:
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 * @col: a valid column number
 * @alter_flags: (transfer none): flags to alter the attributes
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
	g_return_if_fail (proxy_row >= 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	model_col = col % priv->model_nb_cols;
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
				rv->attributes = priv->columns_attrs [col];
				flags = rv->attributes;

				rv->value = NULL;
				flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
				if (rm->model_row >= 0)
					flags |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;
				else
					flags &= ~GDA_VALUE_ATTR_HAS_VALUE_ORIG;

				rm->modify_values = g_slist_prepend (rm->modify_values, rv);
			}
			else {
				flags = rv->attributes;
				if (rv->value) {
					gda_value_free (rv->value);
					rv->value = NULL;
				}
			}
			flags |= GDA_VALUE_ATTR_IS_DEFAULT;
			rv->attributes = flags;

			if (priv->notify_changes)
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

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_get_proxied_model_row:
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
	g_return_val_if_fail (proxy_row >= 0, 0);

	return proxy_row_to_model_row (proxy, proxy_row);
}

/**
 * gda_data_proxy_delete:
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
	g_return_if_fail (proxy_row >= 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (priv->add_null_entry && proxy_row == 0) {
		g_warning (_("The first row is an empty row artificially prepended and cannot be removed"));
		g_rec_mutex_unlock (& (priv->mutex));
		return;
	}

	if (! (gda_data_model_get_access_flags ((GdaDataModel*) proxy) & GDA_DATA_MODEL_ACCESS_DELETE)) {
		g_rec_mutex_unlock (& (priv->mutex));
		return;
	}

	abs_row = proxy_row_to_absolute_row (proxy, proxy_row);
	model_row = absolute_row_to_model_row (proxy, abs_row, &rm);
	if (rm) {
		if (! rm->to_be_deleted) {
			if (rm->model_row == -1) {
				/* remove the row completely because it does not exist in the data model */
				priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
				priv->new_rows = g_slist_remove (priv->new_rows, rm);
				row_modifs_free (rm);

				if (priv->chunk) {
					/* Update chunk */
					gsize i;
					gint *v;
					gint row_cmp = proxy_row - (priv->add_null_entry ? 1 : 0);
					for (i = 0; i < priv->chunk->mapping->len; i++) {
						v = &g_array_index (priv->chunk->mapping, gint, i);
						if (*v > abs_row)
							*v -= 1;
					}
					g_array_remove_index (priv->chunk->mapping, row_cmp);
				}

				if (priv->notify_changes)
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

		gint *ptr;
		ptr = g_new (gint, 1);
		*ptr = model_row;
		g_hash_table_insert (priv->modify_rows, ptr, rm);
		priv->all_modifs = g_slist_prepend (priv->all_modifs, rm);
		rm->to_be_deleted = TRUE;
		do_signal = TRUE;
	}

	if (do_signal && priv->notify_changes) {
		gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[ROW_DELETE_CHANGED],
                               0, proxy_row, TRUE);
	}

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_undelete:
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
	g_return_if_fail (proxy_row >= 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	model_row = absolute_row_to_model_row (proxy,
					       proxy_row_to_absolute_row (proxy, proxy_row), &rm);
	if (rm) {
		rm->to_be_deleted = FALSE;
		if (!rm->modify_values) {
			/* get rid of that RowModif */
			do_signal= TRUE;

			gint tmp;
			tmp = model_row;
			g_hash_table_remove (priv->modify_rows, &tmp);
			priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
			row_modifs_free (rm);
		}
		else
			do_signal= TRUE;
	}

	if (do_signal && priv->notify_changes) {
		gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[ROW_DELETE_CHANGED],
                               0, proxy_row, FALSE);
	}

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_row_is_deleted:
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
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	return rm && rm->to_be_deleted ? TRUE : FALSE;
}

/**
 * gda_data_proxy_row_is_inserted:
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
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	if (rm && (rm->model_row < 0))
		return TRUE;
	return FALSE;
}

/*
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
static gint
gda_data_proxy_append (GdaDataProxy *proxy)
{
	RowModif *rm;
	gint col;
	gint proxy_row;
	gint abs_row;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (! (gda_data_model_get_access_flags ((GdaDataModel *) proxy) & GDA_DATA_MODEL_ACCESS_INSERT))
		return -1;
	if (priv->model_nb_rows == -1)
		return -1;

	/* RowModif structure */
	rm = row_modifs_new (proxy, -1);
	rm->model_row = -1;
	rm->orig_values = NULL; /* there is no original value */
	rm->orig_values_size = priv->model_nb_cols;

	priv->all_modifs = g_slist_prepend (priv->all_modifs, rm);
	priv->new_rows = g_slist_append (priv->new_rows, rm);

	/* new proxy row value */
	abs_row = row_modif_to_absolute_row (proxy, rm);
	if (priv->chunk) {
		proxy_row = priv->chunk->mapping->len;
		g_array_append_val (priv->chunk->mapping, abs_row);
		if (priv->add_null_entry)
			proxy_row++;
	}
	else
		proxy_row = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy) - 1;

	/* for the columns which allow a default value, set them to the default value */
	for (col = 0; col < priv->model_nb_cols; col ++) {
		GdaColumn *column;
		const GValue *def;
		RowValue *rv;
		GdaValueAttribute flags = 0;

		/* create a new RowValue */
		rv = g_new0 (RowValue, 1);
		rv->row_modif = rm;
		rv->model_column = col;
		rv->attributes = GDA_VALUE_ATTR_NONE;
		rv->value = NULL;
		rm->modify_values = g_slist_prepend (rm->modify_values, rv);

		column = gda_data_model_describe_column (priv->model, col);
		def = gda_column_get_default_value (column);
		if (def) {
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);
			if (G_VALUE_TYPE (def) == gda_column_get_g_type (column))
				rv->value = gda_value_copy (def);
		}
		if (gda_column_get_allow_null (column)) {
			GdaValueAttribute attributes;

			attributes = gda_data_model_get_attributes_at (priv->model, col, -1);;
			if (attributes & GDA_VALUE_ATTR_CAN_BE_NULL)
				flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
		}

		if (gda_column_get_auto_increment (column))
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);

		rv->attributes = flags;
	}

	/* signal row insertion */
	if (priv->notify_changes)
		gda_data_model_row_inserted ((GdaDataModel *) proxy, proxy_row);

	return proxy_row;
}

/**
 * gda_data_proxy_cancel_row_changes:
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
	g_return_if_fail (proxy_row >= 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (((col >= 0) && (col < priv->model_nb_cols)) ||
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
						priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
						if (rm->model_row < 0) {
							if (priv->chunk) {
								/* Update chunk */
								gsize i;
								gint *v, abs_row;
								gint row_cmp = proxy_row - (priv->add_null_entry ? 1 : 0);
								abs_row = proxy_row_to_absolute_row (proxy, proxy_row);
								for (i = 0; i < priv->chunk->mapping->len; i++) {
									v = &g_array_index (priv->chunk->mapping, gint, i);
									if (*v > abs_row)
										*v -= 1;
								}
								g_array_remove_index (priv->chunk->mapping, row_cmp);
							}
							signal_delete = TRUE;
							priv->new_rows = g_slist_remove (priv->new_rows, rm);
						}
						else {
							gint tmp;
							tmp = rm->model_row;
							g_hash_table_remove (priv->modify_rows, &tmp);
						}
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

		if (priv->notify_changes) {
			if (signal_delete)
				gda_data_model_row_removed ((GdaDataModel *) proxy, proxy_row);
			else if (signal_update)
				gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
		}
	}
	else
		g_warning ("GdaDataProxy column %d is not a modifiable data column", col);

	g_rec_mutex_unlock (& (priv->mutex));
}

static gboolean commit_row_modif (GdaDataProxy *proxy, RowModif *rm, gboolean adjust_display, GError **error);

/**
 * gda_data_proxy_apply_row_changes:
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
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	return commit_row_modif (proxy, proxy_row_to_row_modif (proxy, proxy_row), TRUE, error);
}

/*
 * Commits the modifications held in one single RowModif structure.
 *
 * Returns: TRUE if no error occurred
 */
static gboolean
commit_row_modif (GdaDataProxy *proxy, RowModif *rm, gboolean adjust_display, GError **error)
{
	gboolean err = FALSE;
	gint proxy_row, model_row;
	GError *lerror = NULL;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	if (!rm)
		return TRUE;

	g_rec_mutex_lock (& (priv->mutex));

	model_row = rm->model_row;

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/*
	 * Steps in this procedure:
	 * -1- send the "validate-row-changes" signal, and abort if return value is FALSE
	 * -2- apply desired modification (which _should_ trigger "row_{inserted,removed,updated}" signals from
	 *     the proxied model)
	 * -3- if no error then destroy the RowModif which has just been applied
	 *     and refresh displayed chunks if @adjust_display is set to TRUE
	 * -4- send the "row-changes-applied" signal
	 */
	proxy_row = row_modif_to_proxy_row (proxy, rm);

	/* validate the changes to this row */
        g_signal_emit (G_OBJECT (proxy),
                       gda_data_proxy_signals[VALIDATE_ROW_CHANGES],
                       0, proxy_row, rm->model_row, &lerror);
	if (lerror) {
		g_propagate_error (error, lerror);
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	/* apply the changes */
	if (rm->to_be_deleted) {
		/* delete the row */
		g_assert (rm->model_row >= 0);
		if (!gda_data_model_remove_row (priv->model, rm->model_row, error))
			err = TRUE;
	}
	else {
		if (rm->model_row >= 0) {
			/* update the row */
			GList *values = NULL;
			gint i;

			g_assert (rm->modify_values);
			g_assert (rm->orig_values);
			for (i=0; i < rm->orig_values_size; i++) {
				gboolean newvalue_found = FALSE;
				GValue *newvalue = NULL;
				GSList *list;

				for (list = rm->modify_values; list; list = list->next) {
					if (ROW_VALUE (list->data)->model_column == i) {
						newvalue_found = TRUE;
						if (ROW_VALUE (list->data)->attributes &
						    GDA_VALUE_ATTR_IS_DEFAULT)
							newvalue = NULL;
						else {
							if (! ROW_VALUE (list->data)->value)
								newvalue = gda_value_new_null ();
							else
								newvalue = gda_value_copy (ROW_VALUE (list->data)->value);
						}
						break;
					}
				}
				if (!newvalue_found && rm->orig_values[i])
					newvalue = gda_value_copy (rm->orig_values[i]);
				values = g_list_append (values, newvalue);
			}

			err = ! gda_data_model_set_values (priv->model, rm->model_row,
							   values, error);
			g_list_free_full (values, (GDestroyNotify) gda_value_free);
		}
		else {
			/* insert a new row */
			GSList *list;
			GList *values = NULL;
			gint i;
			GValue *newvalue;
			GValue **free_val;
			gint new_row;

			g_assert (rm->modify_values);
			free_val = g_new0 (GValue *, priv->model_nb_cols);
			for (i = 0; i < priv->model_nb_cols; i++) {
				newvalue = NULL;

				list = rm->modify_values;
				while (list && !newvalue) {
					if (ROW_VALUE (list->data)->model_column == i) {
						if (ROW_VALUE (list->data)->attributes &
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

			priv->defer_proxied_model_insert = TRUE;
			priv->catched_inserted_row = -1;
			new_row = gda_data_model_append_values (priv->model, values, error);
			err = new_row >= 0 ? FALSE : TRUE;

			g_list_free (values);
			for (i = 0; i < priv->model_nb_cols; i++)
				if (free_val [i])
					gda_value_free (free_val [i]);
			g_free (free_val);
			if (!err) {
				if (priv->catched_inserted_row < 0) {
					g_warning (_("Proxied data model reports the modifications as accepted, yet did not emit the "
						     "corresponding \"row-inserted\", \"row-updated\" or \"row-removed\" signal. This "
						     "is a bug of the %s's implementation (please report a bug)."),
						   G_OBJECT_TYPE_NAME (priv->model));
				}

				priv->new_rows = g_slist_remove (priv->new_rows, rm);
				priv->all_modifs = g_slist_remove (priv->all_modifs, rm);

				gint tmp;
				tmp = rm->model_row;
				g_hash_table_remove (priv->modify_rows, &tmp);
				row_modifs_free (rm);
				rm = NULL;

				if (proxy_row >= 0)
					gda_data_model_row_updated ((GdaDataModel*) proxy, proxy_row);

				/* signal row actually changed */
				g_signal_emit (G_OBJECT (proxy),
					       gda_data_proxy_signals[ROW_CHANGES_APPLIED],
					       0, proxy_row, -1);
			}

			priv->catched_inserted_row = -1;
			priv->defer_proxied_model_insert = FALSE;
		}
	}

	if (!err && rm) {
		/* signal row actually changed */
		g_signal_emit (G_OBJECT (proxy),
			       gda_data_proxy_signals[ROW_CHANGES_APPLIED],
			       0, proxy_row, model_row);

		/* get rid of the committed change; if the changes have been applied correctly, @rm should
		 * have been removed from the priv->all_modifs list because the proxied model
		 * should habe emitted the "row_{inserted,removed,updated}" signals */
		if (rm && g_slist_find (priv->all_modifs, rm)) {
			g_warning (_("Proxied data model reports the modifications as accepted, yet did not emit the "
				     "corresponding \"row-inserted\", \"row-updated\" or \"row-removed\" signal. This "
				     "may be a bug of the %s's implementation (please report a bug)."),
				   G_OBJECT_TYPE_NAME (priv->model));
			priv->new_rows = g_slist_remove (priv->new_rows, rm);
			priv->all_modifs = g_slist_remove (priv->all_modifs, rm);

			gint tmp;
			tmp = rm->model_row;
			g_hash_table_remove (priv->modify_rows, &tmp);
			row_modifs_free (rm);
		}
	}

	if (adjust_display)
		adjust_displayed_chunk (proxy);

	g_rec_mutex_unlock (& (priv->mutex));

	return !err;
}

/**
 * gda_data_proxy_has_changed:
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
        GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

        return priv->all_modifs ? TRUE : FALSE;
}

/**
 * gda_data_proxy_row_has_changed:
 * @proxy: a #GdaDataProxy object
 * @proxy_row: A proxy row number
 *
 * Tells if the row number @proxy_row has changed
 *
 * Returns: TRUE if the row has changed
 */
gboolean
gda_data_proxy_row_has_changed (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	rm = proxy_row_to_row_modif (proxy, proxy_row);
	return rm && (rm->modify_values || rm->to_be_deleted) ? TRUE : FALSE;
}

/**
 * gda_data_proxy_get_n_new_rows:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return g_slist_length (priv->new_rows);
}

/**
 * gda_data_proxy_get_n_modified_rows:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return g_slist_length (priv->all_modifs);
}

/**
 * gda_data_proxy_set_sample_size:
 * @proxy: a #GdaDataProxy object
 * @sample_size: the requested size of a chunk, or 0
 *
 * Sets the size of each chunk of data to display: the maximum number of rows which
 * can be "displayed" at a time (the maximum number of rows which @proxy pretends to have).
 * The default value is arbitrary 300 as it is big enough to
 * be able to display quite a lot of data, but small enough to avoid too much data
 * displayed at the same time.
 *
 * Note: the rows which have been added but not yet committed will always be displayed
 * regardless of the current chunk of data, and the modified rows which are not visible
 * when the displayed chunk of data changes are still held as modified rows.
 *
 * To remove the chunking of the data to display, simply pass @sample_size the %0 value.
 */
void
gda_data_proxy_set_sample_size (GdaDataProxy *proxy, gint sample_size)
{
	gint new_sample_size;
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	new_sample_size = sample_size <= 0 ? 0 : sample_size;
	if (priv->sample_size != new_sample_size) {
		priv->sample_size = new_sample_size;
		adjust_displayed_chunk (proxy);
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[SAMPLE_SIZE_CHANGED],
                               0, sample_size);
	}

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_get_sample_size:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->sample_size;
}

/**
 * gda_data_proxy_set_sample_start:
 * @proxy: a #GdaDataProxy object
 * @sample_start: the number of the first row to be displayed
 *
 * Sets the number of the first row to be available in @proxy (in reference to the proxied data model)
 */
void
gda_data_proxy_set_sample_start (GdaDataProxy *proxy, gint sample_start)
{
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (sample_start >= 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (priv->sample_first_row != sample_start) {
		priv->sample_first_row = sample_start;
		adjust_displayed_chunk (proxy);
	}

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_get_sample_start:
 * @proxy: a #GdaDataProxy object
 *
 * Get the number of the first row to be available in @proxy (in reference to the proxied data model)
 *
 * Returns: the number of the first proxied model's row.
 */
gint
gda_data_proxy_get_sample_start (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->sample_first_row;
}

/**
 * gda_data_proxy_get_sample_end:
 * @proxy: a #GdaDataProxy object
 *
 * Get the number of the last row to be available in @proxy (in reference to the proxied data model)
 *
 * Returns: the number of the last proxied model's row.
 */
gint
gda_data_proxy_get_sample_end (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->sample_last_row;
}

static DisplayChunk *
display_chunk_new (gint reserved_size)
{
	DisplayChunk *chunk;

	chunk = g_new0 (DisplayChunk, 1);
	chunk->mapping = g_array_sized_new (FALSE, TRUE, sizeof (gint), reserved_size);

	return chunk;
}

static void
display_chunk_free (DisplayChunk *chunk)
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_print ("================== CHUNK=%p, TO=%p (mapping=%p), SEP=%d\n", priv->chunk, priv->chunk_to,
		 priv->chunk_to ? priv->chunk_to->mapping : NULL,
		 priv->chunk_sep);
	if (!priv->chunk && !priv->chunk_to)
		g_print ("No chunks at all\n");

	if (priv->chunk)
		total1 = priv->chunk->mapping->len;
	if (priv->chunk_to && priv->chunk_to->mapping)
		total2 = priv->chunk_to->mapping->len;

	g_print ("CHUNK   CHUNK_TO\n");
	for (i = 0; i < MAX (total1, total2); i++) {
		if (i < total1)
			g_print ("%03d", g_array_index (priv->chunk->mapping, gint, i));
		else
			g_print ("   ");
		g_print (" ==== ");
		if (i < total2)
			g_print ("%03d", g_array_index (priv->chunk_to->mapping, gint, i));
		else
			g_print ("   ");
		g_print ("\n");
	}
#endif
}
#else
static void
display_chunks_dump (G_GNUC_UNUSED GdaDataProxy *proxy)
{}
#endif

/*
 * Makes sure the sync, if necessary, is finished
 */
static void
ensure_chunk_sync (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_rec_mutex_lock (& (priv->mutex));
	if (priv->chunk_sync_idle_id) {
		gboolean defer_sync = priv->defer_sync;
		priv->defer_sync = FALSE;

		chunk_sync_idle (proxy);
		priv->defer_sync = defer_sync;
	}
	g_rec_mutex_unlock (& (priv->mutex));
}

/*
 * Emit all the correct signals when switching from priv->chunk to
 * priv->chunk_to
 */
static gboolean
chunk_sync_idle (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
#define IDLE_STEP 50
	if (! g_rec_mutex_trylock (& (priv->mutex)))
		return TRUE; /* we have not finished yet */

	gboolean finished = FALSE;
	guint index, max_steps, step;
	GdaDataModelIter *iter = NULL;
	gint signal_row_offset = priv->add_null_entry ? 1 : 0;

	if (!priv->defer_sync) {
		if (priv->chunk_sync_idle_id) {
			g_idle_remove_by_data (proxy);
			priv->chunk_sync_idle_id = 0;
		}
		max_steps = G_MAXINT;
	}

	if (!priv->chunk_to) {
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE; /* nothing to do */
	}

	max_steps = 0;
	if (priv->chunk_proxy_nb_rows < 0)
		priv->chunk_proxy_nb_rows = priv->model_nb_rows + g_slist_length (priv->new_rows);
	if (priv->chunk_to->mapping)
		max_steps = MAX (max_steps, priv->chunk_to->mapping->len - priv->chunk_sep + 1);
	else
		max_steps = MAX (max_steps, (guint)(priv->chunk_proxy_nb_rows - priv->chunk_sep + 1));
	if (priv->chunk)
		max_steps = MAX (max_steps, priv->chunk->mapping->len - priv->chunk_sep + 1);
	else
		max_steps = MAX (max_steps, (guint)(priv->chunk_proxy_nb_rows - priv->chunk_sep + 1));

	if (priv->defer_sync)
		max_steps = MIN (max_steps, IDLE_STEP);

#ifdef DEBUG_SYNC
	g_print ("////////// %s(defer_sync = %d)\n", __FUNCTION__, priv->defer_sync);
	display_chunks_dump (proxy);
#endif

	for (index = priv->chunk_sep, step = 0;
	     step < max_steps && !finished;
	     step++) {
		gint cur_row, repl_row;

		if (priv->chunk) {
			if (index < priv->chunk->mapping->len)
				cur_row = g_array_index (priv->chunk->mapping, gint, index);
			else
				cur_row = -1;
		}
		else {
			cur_row = index;
			if (cur_row >= priv->chunk_proxy_nb_rows)
				cur_row = -1;
		}

		if (priv->chunk_to->mapping) {
			if (index < priv->chunk_to->mapping->len)
				repl_row = g_array_index (priv->chunk_to->mapping, gint, index);
			else
				repl_row = -1;
		}
		else {
			repl_row = index;
			if (!iter)
				iter = gda_data_model_create_iter (priv->model);
			if (!gda_data_model_iter_move_to_row (iter, repl_row)) {
				if (gda_data_model_iter_get_row (iter) != repl_row)
					repl_row = -1;
			}
		}

#ifdef DEBUG_SYNC
		g_print ("INDEX=%d step=%d max_steps=%d cur_row=%d repl_row=%d\n", index, step, max_steps, cur_row, repl_row);
#endif
		if ((cur_row >= 0) && (repl_row >= 0)) {
			/* emit the GdaDataModel::"row-updated" signal */
			if (priv->chunk) {
				g_array_insert_val (priv->chunk->mapping, index, repl_row);
				g_array_remove_index (priv->chunk->mapping, index + 1);
			}
			priv->chunk_sep++;

			if (cur_row != repl_row)
				if (priv->notify_changes) {
#ifdef DEBUG_SYNC
					g_print ("Signal: Update row %d\n", index + signal_row_offset);
#endif
					gda_data_model_row_updated ((GdaDataModel *) proxy, index + signal_row_offset);
				}
			index++;
		}
		else if ((cur_row >= 0) && (repl_row < 0)) {
			/* emit the GdaDataModel::"row-removed" signal */
			if (priv->chunk)
				g_array_remove_index (priv->chunk->mapping, index);
			priv->chunk_proxy_nb_rows--;
			if (priv->notify_changes) {
#ifdef DEBUG_SYNC
				g_print ("Signal: Remove row %d\n", index + signal_row_offset);
#endif
				gda_data_model_row_removed ((GdaDataModel *) proxy, index + signal_row_offset);
			}
		}
		else if ((cur_row < 0) && (repl_row >= 0)) {
			/* emit GdaDataModel::"row-inserted" insert signal */
			if (priv->chunk)
				g_array_insert_val (priv->chunk->mapping, index, repl_row);
			priv->chunk_sep++;
			if (priv->notify_changes) {
#ifdef DEBUG_SYNC
				g_print ("Signal: Insert row %d\n", index + signal_row_offset);
#endif
				gda_data_model_row_inserted ((GdaDataModel *) proxy, index + signal_row_offset);
			}
			index++;
		}
		else
			finished = TRUE;
	}

	if (iter)
		g_object_unref (iter);

	if (finished) {
		if (priv->chunk_sync_idle_id) {
			g_idle_remove_by_data (proxy);
			priv->chunk_sync_idle_id = 0;
		}

		if (! priv->chunk_to->mapping) {
			if (priv->chunk) {
				display_chunk_free (priv->chunk);
				priv->chunk = NULL;
			}
			display_chunk_free (priv->chunk_to);
			priv->chunk_to = NULL;
		}
		else {
			if (priv->chunk)
				display_chunk_free (priv->chunk);
			priv->chunk = priv->chunk_to;
			priv->chunk_to = NULL;
		}
#ifdef DEBUG_SYNC
		g_print ("Sync. is now finished\n");
#endif
	}
#ifdef DEBUG_SYNC
	else
		g_print ("Sync. is NOT finished yet\n");
#endif
	g_rec_mutex_unlock (& (priv->mutex));
	return !finished;
}

static DisplayChunk *
compute_display_chunk (GdaDataProxy *proxy)
{
	DisplayChunk *ret_chunk = NULL;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	if (priv->filtered_rows) {
		/* REM: when there is a filter applied, the new rows are mixed with the
		 * existing ones => no need to treat them appart
		 */
		gint nb_rows = gda_data_model_get_n_rows (priv->filtered_rows);
		gint i, new_nb_rows = 0;

		g_assert (nb_rows >= 0); /* the number of rows IS known here */
		if (priv->sample_size > 0) {
			if (priv->sample_first_row >= nb_rows)
				priv->sample_first_row = priv->sample_size *
					((nb_rows - 1) / priv->sample_size);

			priv->sample_last_row = priv->sample_first_row +
				priv->sample_size - 1;
			if (priv->sample_last_row >= nb_rows)
				priv->sample_last_row = nb_rows - 1;
			new_nb_rows = priv->sample_last_row - priv->sample_first_row + 1;
		}
		else {
			priv->sample_first_row = 0;
			priv->sample_last_row = nb_rows - 1;
			new_nb_rows = nb_rows;
		}

		ret_chunk = display_chunk_new (priv->sample_size > 0 ?
						 priv->sample_size : nb_rows);
		for (i = 0; i < new_nb_rows; i++) {
			const GValue *value;
			gint val;

			g_assert (i + priv->sample_first_row < nb_rows);
			value = gda_data_model_get_value_at (priv->filtered_rows,
							     0, i + priv->sample_first_row, NULL);
			g_assert (value);
			g_assert (G_VALUE_TYPE (value) == G_TYPE_INT);
			val = g_value_get_int (value);
			g_array_append_val (ret_chunk->mapping, val);
		}
	}
	else {
		gint i, new_nb_rows = 0;

		if (priv->model_nb_rows >= 0) {
			/* known number of rows */
			if (priv->sample_size > 0) {
				if (priv->sample_first_row >= priv->model_nb_rows)
					priv->sample_first_row = priv->sample_size *
						((priv->model_nb_rows - 1) / priv->sample_size);

				priv->sample_last_row = priv->sample_first_row +
					priv->sample_size - 1;
				if (priv->sample_last_row >= priv->model_nb_rows)
					priv->sample_last_row = priv->model_nb_rows - 1;
				new_nb_rows = priv->sample_last_row - priv->sample_first_row + 1;
				ret_chunk = display_chunk_new (priv->sample_size);
			}
			else {
				/* no chunk_to->mapping needed */
				ret_chunk = g_new0 (DisplayChunk, 1);

				priv->sample_first_row = 0;
				priv->sample_last_row = priv->model_nb_rows - 1;
				new_nb_rows = priv->model_nb_rows;
			}
		}
		else {
			/* no chunk_to->mapping needed */
			ret_chunk = g_new0 (DisplayChunk, 1);

			if (priv->model_nb_rows == 0 ) {
				/* known number of rows */
				priv->sample_first_row = 0;
				priv->sample_last_row = 0;
				new_nb_rows = 0;
			}
			else {
				/* unknown number of rows */
				if (priv->sample_size > 0) {
					priv->sample_last_row = priv->sample_first_row +
						priv->sample_size - 1;
					new_nb_rows = priv->sample_last_row - priv->sample_first_row + 1;
				}
				else {
					priv->sample_first_row = 0;
					priv->sample_last_row = G_MAXINT - 1;
					new_nb_rows = G_MAXINT;
				}
			}
		}
		/* fill @chunk_to is it exists */
		if (ret_chunk && ret_chunk->mapping) {
			for (i = 0; i < new_nb_rows; i++) {
				g_assert (i + priv->sample_first_row < priv->model_nb_rows);
				gint val = model_row_to_absolute_row (proxy, i + priv->sample_first_row);
				g_array_append_val (ret_chunk->mapping, val);
			}
			GSList *list;
			for (i++, list = priv->new_rows; list; list = list->next, i++) {
				gint val = row_modif_to_absolute_row (proxy, ROW_MODIF (list->data));
				g_array_append_val (ret_chunk->mapping, val);
			}
		}
	}

	g_rec_mutex_unlock (& (priv->mutex));
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_return_if_fail (priv->model);

	g_rec_mutex_lock (& (priv->mutex));

	/*
	 * Stop idle adding of rows if necessary
	 */
	if (priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		priv->chunk_sync_idle_id = 0;
	}

	/* compute new DisplayChunk */
	if (priv->chunk_to) {
		display_chunk_free (priv->chunk_to);
		priv->chunk_to = NULL;
	}
	priv->chunk_to = compute_display_chunk (proxy);
	if (!priv->chunk_to) {
		g_rec_mutex_unlock (& (priv->mutex));
		return; /* nothing to do */
	}

	/* determine if chunking has changed */
	gboolean equal = FALSE;
	if (priv->chunk && priv->chunk_to->mapping) {
		/* compare the 2 chunks */
		if (priv->chunk->mapping->len == priv->chunk_to->mapping->len) {
			gsize i;
			equal = TRUE;
			for (i = 0; i < priv->chunk->mapping->len; i++) {
				if (g_array_index (priv->chunk->mapping, gint, i) !=
				    g_array_index (priv->chunk_to->mapping, gint, i)) {
					equal = FALSE;
					break;
				}
			}
		}
	}
	else if (!priv->chunk && !priv->chunk_to->mapping)
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
                               0, priv->sample_first_row, priv->sample_last_row);

		/* sync priv->chunk to priv->chunk_to */
		priv->chunk_sep = 0;
		priv->chunk_proxy_nb_rows = -1;
		if (!priv->defer_sync)
			chunk_sync_idle (proxy);
		else
			priv->chunk_sync_idle_id = g_idle_add ((GSourceFunc) chunk_sync_idle, proxy);
	}
	else {
		/* nothing to adjust => destroy priv->chunk_to if necessary */
		if (priv->chunk_to) {
			display_chunk_free (priv->chunk_to);
			priv->chunk_to = NULL;
		}
	}

	g_rec_mutex_unlock (& (priv->mutex));
}

/**
 * gda_data_proxy_apply_all_changes:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	gda_data_model_send_hint (priv->model, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);

	while (priv->all_modifs && allok)
		allok = commit_row_modif (proxy, ROW_MODIF (priv->all_modifs->data), FALSE, error);

	gda_data_model_send_hint (priv->model, GDA_DATA_MODEL_HINT_END_BATCH_UPDATE, NULL);
	adjust_displayed_chunk (proxy);

	g_rec_mutex_unlock (& (priv->mutex));

	return allok;
}

/**
 * gda_data_proxy_cancel_all_changes:
 * @proxy: a #GdaDataProxy object
 *
 * Cancel all the changes stored in the proxy (the @proxy will be reset to its state
 * as it was just after creation). Note that if there are some cached changes (i.e. not applied
 * to the current proxied data model), then these cached changes are not cleared (set the "cache-changes"
 * property to %FALSE for this).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_cancel_all_changes (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);
	g_assert (!priv->chunk_to);

	/* new rows are first treated and removed (no memory de-allocation here, though) */
	if (priv->new_rows) {
		if (priv->chunk) {
			/* Using a chunk */
			priv->chunk_to = display_chunk_new (priv->chunk->mapping->len);
			g_array_append_vals (priv->chunk_to->mapping,
					     priv->chunk->mapping->data, priv->chunk->mapping->len);

			while (priv->new_rows) {
				gint proxy_row;

				proxy_row = row_modif_to_proxy_row (proxy, (ROW_MODIF (priv->new_rows->data)));
				priv->new_rows = g_slist_delete_link (priv->new_rows, priv->new_rows);

				if ((proxy_row >= 0) && priv->chunk_to)
					g_array_remove_index (priv->chunk_to->mapping,
							      proxy_row - (priv->add_null_entry ? 1 : 0));
			}

			if (priv->chunk_to) {
				/* sync priv->chunk to priv->chunk_to */
				gboolean defer_sync = priv->defer_sync;
				priv->defer_sync = FALSE;
				priv->chunk_sep = 0;
				priv->chunk_proxy_nb_rows = -1;
				chunk_sync_idle (proxy);
				priv->defer_sync = defer_sync;
			}
		}
		else {
			/* no chunk used */
			gint nrows = gda_data_proxy_get_n_rows ((GdaDataModel *) proxy);
			while (priv->new_rows) {
				priv->new_rows = g_slist_delete_link (priv->new_rows, priv->new_rows);
				if (priv->notify_changes) {
					gda_data_model_row_removed ((GdaDataModel *) proxy, nrows-1);
					nrows--;
				}
			}
		}
	}

	/* all modified rows are then treated (including memory de-allocation for new rows) */
	while (priv->all_modifs) {
		gint model_row = ROW_MODIF (priv->all_modifs->data)->model_row;
		gint proxy_row = -1;

		if (priv->notify_changes && (model_row >= 0))
			proxy_row = row_modif_to_proxy_row (proxy, (ROW_MODIF (priv->all_modifs->data)));

		row_modifs_free (ROW_MODIF (priv->all_modifs->data));
		if (model_row >= 0) {
			gint tmp;
			tmp = model_row;
			g_hash_table_remove (priv->modify_rows, &tmp);
		}
		priv->all_modifs = g_slist_delete_link (priv->all_modifs, priv->all_modifs);

		if ((proxy_row >= 0) && priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
	}

	g_rec_mutex_unlock (& (priv->mutex));

	return TRUE;
}

static gboolean
sql_where_foreach (GdaSqlAnyPart *part, GdaDataProxy *proxy, G_GNUC_UNUSED GError **error)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (part->type == GDA_SQL_ANY_EXPR) {
		GdaSqlExpr *expr = (GdaSqlExpr*) part;
		if (expr->value && (G_VALUE_TYPE (expr->value) == G_TYPE_STRING)) {
			const gchar *cstr = g_value_get_string (expr->value);
			if (*cstr == '_') {
				const gchar *ptr;
				for (ptr = cstr+1; *ptr; ptr++)
					if ((*ptr < '0') || (*ptr > '9'))
						break;
				if (!*ptr) {
					/* column name is "_<number>", use column: <number> - 1 */
					gint colnum;
					colnum = atoi (cstr+1) - 1; /* Flawfinder: ignore */
					if ((colnum >= 0) &&
					    (colnum < gda_data_model_get_n_columns ((GdaDataModel*) proxy))) {
						GdaColumn *col = gda_data_model_describe_column ((GdaDataModel*) proxy,
												 colnum);
						const gchar *cname = gda_column_get_name (col);
						if (cname && *cname) {
							g_value_take_string (expr->value,
									     gda_sql_identifier_quote (cname,
												       priv->filter_vcnc,
												       NULL,
												       FALSE, FALSE));
						}
					}
				}
			}
		}
	}
	return TRUE;
}

/*
 * Applies priv->filter_stmt
 *
 * Make sure priv->mutex is locked
 */
static gboolean
apply_filter_statement (GdaDataProxy *proxy, GError **error)
{
	GdaConnection *vcnc;
	GdaDataModel *filtered_rows = NULL;
	GdaStatement *stmt = NULL;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	if (priv->filter_stmt) {
		stmt = priv->filter_stmt;
		priv->filter_stmt = NULL;
	}

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if (!stmt)
		goto clean_previous_filter;

	g_mutex_lock (&provider_mutex);
	if (!virtual_provider)
		virtual_provider = gda_vprovider_data_model_new ();
	g_mutex_unlock (&provider_mutex);

	/* Force direct data access where proxy_row <=> absolute_row */
	priv->force_direct_mapping = TRUE;

	vcnc = priv->filter_vcnc;
	if (!vcnc) {
		GError *lerror = NULL;
		vcnc = gda_virtual_connection_open (virtual_provider, GDA_CONNECTION_OPTIONS_NONE, &lerror);
		if (! vcnc) {
			g_print ("Virtual ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
			if (lerror)
				g_error_free (lerror);
			g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
				      "%s", _("Could not create virtual connection"));
			priv->force_direct_mapping = FALSE;
			goto clean_previous_filter;
		}
		GMainContext *ctx;
		ctx = gda_connection_get_main_context (NULL, NULL);
		if (ctx)
			gda_connection_set_main_context (vcnc, NULL, ctx);
		priv->filter_vcnc = vcnc;
	}

	/* Add the @proxy to the virtual connection.
	 *
	 * REM: use a GdaDataModelWrapper to force the viewing of the un-modified columns of @proxy,
	 * otherwise the GdaVconnectionDataModel will just take into account the modified columns
	 */
	GdaDataModel *wrapper;
	wrapper = gda_data_access_wrapper_new ((GdaDataModel*) proxy);
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (vcnc), wrapper,
						   "proxy", error)) {
		g_object_unref (wrapper);
		priv->force_direct_mapping = FALSE;
		goto clean_previous_filter;
	}
	g_object_unref (wrapper);

	/* remork the statement for column names */
	GdaSqlStatement *sqlst;
	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT);
	gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sqlst->contents), (GdaSqlForeachFunc) sql_where_foreach, proxy, NULL);
	g_object_set (G_OBJECT (stmt), "structure", sqlst, NULL);
#ifdef GDA_DEBUG_NO
	gchar *ser;
	ser = gda_sql_statement_serialize (sqlst);
	g_print ("Modified Filter: %s\n", ser);
	g_free (ser);
#endif
	gda_sql_statement_free (sqlst);

	/* execute statement */
	GError *lerror = NULL;
	g_rec_mutex_unlock (& (priv->mutex));
	filtered_rows = gda_connection_statement_execute_select (vcnc, stmt, NULL, &lerror);
     	if (!filtered_rows) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			     _("Error in filter expression: %s"), lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
		priv->force_direct_mapping = FALSE;
		gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (vcnc), "proxy", NULL);
		g_rec_mutex_lock (& (priv->mutex));
		goto clean_previous_filter;
	}

	/* copy filtered_rows and remove virtual table */
	GdaDataModel *copy;
	copy = (GdaDataModel*) gda_data_model_array_copy_model (filtered_rows, NULL);
	gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (vcnc), "proxy", NULL);
	g_object_unref (filtered_rows);
	if (!copy) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			      "%s", _("Error in filter expression"));
		priv->force_direct_mapping = FALSE;
		filtered_rows = NULL;
		g_rec_mutex_lock (& (priv->mutex));
		goto clean_previous_filter;
	}
	filtered_rows = copy;
	priv->force_direct_mapping = FALSE;
	g_rec_mutex_lock (& (priv->mutex));

 clean_previous_filter: /* NEED TO BE LOCKED HERE */
	if (priv->filter_expr) {
		g_free (priv->filter_expr);
		priv->filter_expr = NULL;
	}
	if (priv->filtered_rows) {
		g_object_unref (priv->filtered_rows);
		priv->filtered_rows = NULL;
	}
#define FILTER_SELECT_WHERE "SELECT __gda_row_nb FROM proxy WHERE "
#define FILTER_SELECT_NOWHERE "SELECT __gda_row_nb FROM proxy "
	if (filtered_rows) {
		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		if (sql) {
			if (!g_ascii_strncasecmp (sql, FILTER_SELECT_WHERE, strlen (FILTER_SELECT_WHERE)))
				priv->filter_expr = g_strdup (sql + strlen (FILTER_SELECT_WHERE));
			else if (!g_ascii_strncasecmp (sql, FILTER_SELECT_NOWHERE, strlen (FILTER_SELECT_NOWHERE)))
				priv->filter_expr = g_strdup (sql + strlen (FILTER_SELECT_NOWHERE));
			g_free (sql);
		}
		priv->filtered_rows = filtered_rows;
		priv->filter_stmt = stmt;
	}
	else if (stmt)
		g_object_unref (stmt);

	g_signal_emit (G_OBJECT (proxy),
		       gda_data_proxy_signals[FILTER_CHANGED],
		       0);
	gda_data_model_reset (GDA_DATA_MODEL (proxy));

	adjust_displayed_chunk (proxy);
	g_rec_mutex_unlock (& (priv->mutex));

	if (!stmt)
		return TRUE;
	else
		return filtered_rows ? TRUE : FALSE;
}

/**
 * gda_data_proxy_set_filter_expr:
 * @proxy: a #GdaDataProxy object
 * @filter_expr: (nullable): an SQL based expression which will filter the contents of @proxy, or %NULL to remove any previous filter
 * @error: a place to store errors, or %NULL
 *
 * Sets a filter among the rows presented by @proxy. The filter is defined by a filter expression
 * which can be any SQL valid expression using @proxy's columns. For instance if @proxy has the "id" and
 * "name" columns, then a filter can be "length(name) < 5" to filter only the rows where the length of the
 * name is strictly inferior to 5, or "id >= 1000 and id < 2000 order by name limit 50" to filter only the rows where the id
 * is between 1000 and 2000, ordered by name and limited to 50 rows.
 *
 * Note about column names: real column names can be used (double quoted if necessary), but columns can also be named
 * "_&lt;column number&gt;" with column numbers starting at 1.
 *
 * Note that any previous filter expression is replaced with the new @filter_expr if no error occurs
 * (if an error occurs, then any previous filter is left unchanged).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_set_filter_expr (GdaDataProxy *proxy, const gchar *filter_expr, GError **error)
{
	gchar *sql;
	GdaStatement *stmt = NULL;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	if (!filter_expr) {
		if (priv->filter_stmt)
			g_object_unref (priv->filter_stmt);
		priv->filter_stmt = NULL;

		gboolean retval = apply_filter_statement (proxy, error);
		return retval;
	}

	/* generate SQL with a special case if expression starts with "ORDER BY" */
	gchar *tmp;
	const gchar *ptr;
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
		sql = g_strdup_printf (FILTER_SELECT_NOWHERE "%s", filter_expr);
	else
		sql = g_strdup_printf (FILTER_SELECT_WHERE "%s", filter_expr);
	g_free (tmp);

	g_mutex_lock (&parser_mutex);
	stmt = gda_sql_parser_parse_string (internal_parser, sql, &ptr, NULL);
	g_mutex_unlock (&parser_mutex);
	g_free (sql);
	if (ptr || !stmt || (gda_statement_get_statement_type (stmt) != GDA_SQL_STATEMENT_SELECT)) {
		/* also catches problems with multiple statements in @filter_expr, such as SQL code injection */
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_FILTER_ERROR,
			      "%s", _("Incorrect filter expression"));
		if (stmt)
			g_object_unref (stmt);
		priv->force_direct_mapping = FALSE;

		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	if (priv->filter_stmt)
		g_object_unref (priv->filter_stmt);
	priv->filter_stmt = stmt;

	gboolean retval = apply_filter_statement (proxy, error);
	return retval;
}

/**
 * gda_data_proxy_set_ordering_column:
 * @proxy: a #GdaDataProxy object
 * @col: the column number to order from
 * @error: a place to store errors, or %NULL
 *
 * Orders by the @col column
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_proxy_set_ordering_column (GdaDataProxy *proxy, gint col, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (col >= 0, FALSE);
	g_return_val_if_fail (col < gda_data_model_get_n_columns ((GdaDataModel*) proxy), FALSE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	if (priv->filter_stmt) {
		GdaSqlStatement *sqlst;
		GdaSqlStatementSelect *selst;
		gboolean replaced = FALSE;
		const gchar *cname;
		gchar *colname;

		cname = gda_column_get_name (gda_data_model_describe_column ((GdaDataModel*) proxy, col));
		if (cname && *cname)
			colname = gda_sql_identifier_quote (cname, priv->filter_vcnc, NULL, FALSE, FALSE);
		else
			colname = g_strdup_printf ("_%d", col + 1);

		g_object_get (G_OBJECT (priv->filter_stmt), "structure", &sqlst, NULL);
		g_assert (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT);
		g_free (sqlst->sql);
		sqlst->sql = NULL;
		selst = (GdaSqlStatementSelect*) sqlst->contents;

		/* test if we can actually toggle the sort ASC <-> DESC */
		if (selst->order_by && !selst->order_by->next) {
			GdaSqlSelectOrder *order_by = (GdaSqlSelectOrder*) selst->order_by->data;
			if (order_by->expr && order_by->expr->value &&
			    (G_VALUE_TYPE (order_by->expr->value) == G_TYPE_STRING) &&
			    gda_identifier_equal (g_value_get_string (order_by->expr->value), colname)) {
				order_by->asc = !order_by->asc;
				replaced = TRUE;
				g_free (colname);
			}
		}

		if (!replaced) {
			/* replace the whole ordering part */
			if (selst->order_by) {
				g_slist_free_full (selst->order_by, (GDestroyNotify) gda_sql_select_order_free);
				selst->order_by = NULL;
			}

			GdaSqlSelectOrder *order_by;
			GdaSqlExpr *expr;
			order_by = gda_sql_select_order_new (GDA_SQL_ANY_PART (selst));
			selst->order_by = g_slist_prepend (NULL, order_by);
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (order_by));
			order_by->expr = expr;
			order_by->asc = TRUE;
			expr->value = gda_value_new (G_TYPE_STRING);
			g_value_take_string (expr->value, colname);
		}

		g_object_set (G_OBJECT (priv->filter_stmt), "structure", sqlst, NULL);
#ifdef GDA_DEBUG_NO
		gchar *ser;
		ser = gda_sql_statement_serialize (sqlst);
		g_print ("Modified Filter: %s\n", ser);
		g_free (ser);
#endif
		gda_sql_statement_free (sqlst);
		retval = apply_filter_statement (proxy, error);
	}
	else {
		gchar *str;
		str = g_strdup_printf ("ORDER BY _%d", col + 1);
		g_rec_mutex_unlock (& (priv->mutex));
		retval = gda_data_proxy_set_filter_expr (proxy, str, error);
		g_free (str);
	}

	return retval;
}

/**
 * gda_data_proxy_get_filter_expr:
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->filter_expr;
}

/**
 * gda_data_proxy_get_filtered_n_rows:
 * @proxy: a #GdaDataProxy object
 *
 * Get the total number of filtered rows in @proxy if a filter has been applied. As new rows
 * (rows added to the proxy and not yet added to the proxied data model) and rows to remove
 * (rows marked for removal but not yet removed from the proxied data model) are also filtered,
 * the returned number also contains references to new rows and rows to be removed.
 *
 * Returns: the number of filtered rows in @proxy, or -1 if no filter has been applied
 */
gint
gda_data_proxy_get_filtered_n_rows (GdaDataProxy *proxy)
{
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	if (! priv->filtered_rows) {
		g_rec_mutex_unlock (& (priv->mutex));
		return -1;
	}
	else {
		gint n = gda_data_model_get_n_rows (priv->filtered_rows);
		g_rec_mutex_unlock (& (priv->mutex));
		return n;
	}
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_return_val_if_fail (priv, -1);

	g_rec_mutex_lock (& (priv->mutex));

	if (priv->chunk && !priv->force_direct_mapping)
		nbrows = priv->chunk->mapping->len;
	else {
		if (priv->model_nb_rows >= 0) {
			if (priv->chunk_to && priv->chunk_to->mapping) {
				nbrows = priv->chunk_proxy_nb_rows;
			}
			else
				nbrows = priv->model_nb_rows +
					g_slist_length (priv->new_rows);
		}
		else
			return -1; /* unknown number of rows */
	}
	if (!priv->force_direct_mapping && priv->add_null_entry)
		nbrows += 1;

	g_rec_mutex_unlock (& (priv->mutex));

	return nbrows;
}

static gint
gda_data_proxy_get_n_columns (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return 2 * priv->model_nb_cols;
}


typedef struct {
	gchar *name;
	GType  type;
} ExtraColAttrs;

static void create_columns (GdaDataProxy *proxy)
{
	gint i;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (priv->columns)
		return;

	priv->columns = g_new0 (GdaColumn *, 2 * priv->model_nb_cols);

	/* current proxy's values */
	for (i = 0; i < priv->model_nb_cols; i++) {
		GdaColumn *orig;

		orig = gda_data_model_describe_column (priv->model, i);
		priv->columns[i] = gda_column_copy (orig);
		gda_column_set_position (priv->columns[i], i);
	}

	/* proxied data model's values (original values), again reference columns from proxied data model */
	for (; i < 2 * priv->model_nb_cols; i++) {
		GdaColumn *orig;
		const gchar *cname;
		gchar *newname, *id;
		gint k;

		orig = gda_data_model_describe_column (priv->model,
						       i -  priv->model_nb_cols);
		priv->columns[i] = gda_column_copy (orig);
		g_object_get ((GObject*) priv->columns[i], "id", &id, NULL);
		if (id) {
			gchar *newid;
			newid = g_strdup_printf ("pre%s", id);
			g_object_set ((GObject*) priv->columns[i], "id", newid, NULL);

			/* make sure there is no duplicate ID */
			for (k = 0; ; k++) {
				gint j;
				for (j = 0; j < i; j++) {
					gchar *id2;
					g_object_get ((GObject*) priv->columns[j], "id", &id2, NULL);
					if (id2 && *id2 && !strcmp (id2, newid)) {
						g_free (id2);
						break;
					}
				}
				if (j == i)
					break;

				g_free (newid);
				newid = g_strdup_printf ("pre%s_%d", id, k);
				g_object_set ((GObject*) priv->columns[i], "id", newid, NULL);
			}
			g_free (newid);
			g_free (id);
		}

		cname =  gda_column_get_name (orig);
		if (cname && *cname)
			newname = g_strdup_printf ("pre%s", cname);
		else
			newname = g_strdup_printf ("pre%d", i);

		/* make sure there is no duplicate name */
		for (k = 0; ; k++) {
			gint j;
			for (j = 0; j < i; j++) {
				const gchar *cname2;
				cname2 =  gda_column_get_name (priv->columns[j]);
				if (cname2 && *cname2 && !strcmp (cname2, newname))
					break;
			}
			if (j == i)
				break;
			g_free (newname);
			if (cname && *cname)
				newname = g_strdup_printf ("pre%s_%d", cname, k);
			else
				newname = g_strdup_printf ("pre%d_%d", i, k);
		}
		gda_column_set_name (priv->columns[i], newname);
		gda_column_set_description (priv->columns[i], newname);
		g_free (newname);
		gda_column_set_position (priv->columns[i], i);
	}
}

static GdaColumn *
gda_data_proxy_describe_column (GdaDataModel *model, gint col)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), NULL);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	if (!priv->columns)
		create_columns (proxy);
	g_rec_mutex_unlock (& (priv->mutex));
	if ((col < 0) || (col >= 2 * priv->model_nb_cols)) {
		g_warning (_("Column %d out of range (0-%d)"), col,
			   gda_data_model_get_n_columns (model) - 1);
		return NULL;
	}
	else
		return priv->columns [col];
}

static const GValue *
gda_data_proxy_get_value_at (GdaDataModel *model, gint column, gint proxy_row, GError **error)
{
	gint model_row;
	GValue *retval = NULL;
	GdaDataProxy *proxy;
	static GValue *null_value = NULL;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), NULL);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy_row >= 0, NULL);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	if ((proxy_row == 0) && priv->add_null_entry) {
		if (!null_value)
			null_value = gda_value_new_null ();
		g_rec_mutex_unlock (& (priv->mutex));
		return null_value;
	}

	model_row = proxy_row_to_model_row (proxy, proxy_row);

	/* current proxy's values (values may be different than the ones in the proxied data model) */
	if (column < priv->model_nb_cols) {
		RowModif *rm;
		gint model_col = column % priv->model_nb_cols;
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
				if (!retval) {
					if (!null_value)
						null_value = gda_value_new_null ();
					retval = null_value;
				}
			}
		}

		if (!value_has_modifs) {
			/* value has not been modified */
			if (model_row >= 0) {
				/* existing row */
				retval = (GValue *) gda_data_model_get_value_at (priv->model, column, model_row, error);
			}
			else {
				/* non existing row, return NULL */
				gint n;
				n = gda_data_model_get_n_rows (model);
				if (n > 0)
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
						     _("Row %d out of range (0-%d)"), proxy_row, n - 1);
				else
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
						     _("Row %d not found (empty data model)"), proxy_row);
				retval = NULL;
			}
		}

		g_rec_mutex_unlock (& (priv->mutex));
		return retval;
	}

	/* proxied data model's values (original values) */
	if (column < 2 *priv->model_nb_cols) {
		RowModif *rm;
		gint model_col = column % priv->model_nb_cols;

		rm = proxy_row_to_row_modif (proxy, proxy_row);
		if (rm) {
			if (rm->orig_values)
				retval = rm->orig_values [model_col];
			else {
				if (!null_value)
					null_value = gda_value_new_null ();
				retval = null_value;
			}
		}
		else {
			if (model_row >= 0) {
				/* existing row */
				retval = (GValue *) gda_data_model_get_value_at (priv->model, model_col, model_row, error);
			}
			else {
				/* non existing row, return NULL */
				gint n;
				n = gda_data_model_get_n_rows (model);
				if (n > 0)
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
						     _("Row %d out of range (0-%d)"), proxy_row, n - 1);
				else
					g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
						     _("Row %d not found (empty data model)"), proxy_row);
				retval = NULL;
			}
		}

		g_rec_mutex_unlock (& (priv->mutex));
		return retval;
	}

	g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
		     _("Column %d out of range (0-%d)"), column, 2 *priv->model_nb_cols - 1);

	g_rec_mutex_unlock (& (priv->mutex));
	return NULL;
}

static GdaValueAttribute
gda_data_proxy_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute attrs;
	GdaDataProxy *proxy;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = (GdaDataProxy*) model;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	attrs = gda_data_proxy_get_value_attributes ((GdaDataProxy *) model, row, col);
	g_rec_mutex_unlock (& (priv->mutex));
	return attrs;
}

static gint
gda_data_proxy_find_row_from_values (GdaDataModel *model, GSList *values, gint *cols_index)
{
	gboolean found = FALSE;
	gint proxy_row;
	gint current_nb_rows;
	GdaDataProxy *proxy;

	proxy = (GdaDataProxy*) model;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), FALSE);
	g_return_val_if_fail (values, FALSE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/* FIXME: use a virtual connection here with some SQL, it'll be much easier and will avoid
	 * much code
	 */
	/*TO_IMPLEMENT;*/

	/* if there are still some rows waiting to be added in the idle loop, then force them to be added
	 * first, otherwise we might not find what we are looking for!
	 */
	if (priv->chunk_sync_idle_id) {
		g_idle_remove_by_data (proxy);
		priv->chunk_sync_idle_id = 0;
		while (chunk_sync_idle (proxy)) ;
	}

	current_nb_rows = gda_data_proxy_get_n_rows ((GdaDataModel*) proxy);
	for (proxy_row = 0; proxy_row < current_nb_rows; proxy_row++) {
		gboolean allequal = TRUE;
		GSList *list;
		gint index;
		const GValue *value;

		for (list = values, index = 0;
		     list;
		     list = list->next, index++) {
			if (cols_index)
				g_return_val_if_fail (cols_index [index] < priv->model_nb_cols, FALSE);
			value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy,
							     cols_index ? cols_index [index] :
							     index, proxy_row, NULL);
			if ((!value) || !list->data ||
			    (G_VALUE_TYPE (value) != G_VALUE_TYPE ((GValue *) list->data)) ||
			    gda_value_compare ((GValue *) (list->data), (GValue *) value)) {
				allequal = FALSE;
				break;
			}
		}
		if (allequal) {
			found = TRUE;
			break;
		}
	}

	g_rec_mutex_unlock (& (priv->mutex));
	return found ? proxy_row : -1;
}

static GdaDataModelAccessFlags
gda_data_proxy_get_access_flags (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), 0);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	if (priv->model) {
		GdaDataModelAccessFlags flags;
		g_rec_mutex_lock (& (priv->mutex));
		flags = gda_data_model_get_access_flags (priv->model) | GDA_DATA_MODEL_ACCESS_RANDOM;
		g_rec_mutex_unlock (& (priv->mutex));
		return flags;
	}
	else
		return 0;
}

static gboolean
gda_data_proxy_set_value_at (GdaDataModel *model, gint col, gint proxy_row, const GValue *value,
			     GError **error)
{
	GdaDataProxy *proxy;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy_row >= 0, FALSE);
	g_return_val_if_fail (value, FALSE);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	if ((proxy_row == 0) && priv->add_null_entry) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_ROW,
			      "%s", _("The first row is an empty row artificially prepended and cannot be altered"));
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	/* current proxy's values (values may be different than the ones in the proxied data model) */
	if ((col >= 0) && (col < priv->model_nb_cols)) {
		/* Storing a GValue value */
		RowModif *rm;
		RowValue *rv = NULL;
		const GValue *cmp_value;

		/* compare with the current stored value */
		cmp_value = gda_data_proxy_get_value_at ((GdaDataModel *) proxy, col, proxy_row, error);
		if (!cmp_value) {
			GdaValueAttribute attrs;
			attrs = gda_data_proxy_get_value_attributes (proxy, proxy_row, col);
			if (attrs & GDA_VALUE_ATTR_NO_MODIF) {
				g_rec_mutex_unlock (& (priv->mutex));
				return FALSE;
			}
			else {
				GType exptype;
				exptype = gda_column_get_g_type (gda_data_model_describe_column ((GdaDataModel *) proxy,
												 col));
				if ((G_VALUE_TYPE (value) != GDA_TYPE_NULL) &&
				    (exptype != GDA_TYPE_NULL) &&
				    (exptype != G_VALUE_TYPE (value))) {
					g_rec_mutex_unlock (& (priv->mutex));
					g_warning (_("Wrong value type: expected '%s' and got '%s'"),
						   g_type_name (exptype),
						   g_type_name (G_VALUE_TYPE (value)));
					return FALSE;
				}
			}
		}
		else if ((G_VALUE_TYPE (cmp_value) != GDA_TYPE_NULL) &&
			 (G_VALUE_TYPE (value) != GDA_TYPE_NULL) &&
			 (G_VALUE_TYPE (value) != G_VALUE_TYPE (cmp_value))) {
			g_rec_mutex_unlock (& (priv->mutex));
			g_warning (_("Wrong value type: expected '%s' and got '%s'"),
				   g_type_name (G_VALUE_TYPE (cmp_value)),
				   g_type_name (G_VALUE_TYPE (value)));
			return FALSE;
		}
		else if (! gda_value_compare ((GValue *) value, (GValue *) cmp_value)) {
			/* nothing to do: values are equal */
			g_rec_mutex_unlock (& (priv->mutex));
			return TRUE;
		}

		/* from now on we have a new value for the row */
		rm = find_or_create_row_modif (proxy, proxy_row, col, &rv);

		if (rv) {
			/* compare with the original value (before modifications) and either
			 * delete the RowValue or alter it */
			if (rv->value) {
				gda_value_free (rv->value);
				rv->value = NULL;
			}

			if (rm->orig_values && (col < rm->orig_values_size) &&
			    rm->orig_values [col] &&
			    ! gda_value_compare ((GValue *) value, rm->orig_values [col])) {
				/* remove the RowValue */
				rm->modify_values = g_slist_remove (rm->modify_values, rv);
				g_free (rv);
				rv = NULL;
			}
			else {
				/* simply alter the RowValue */
				GdaValueAttribute flags = rv->attributes;

				if (value && !gda_value_is_null ((GValue *) value)) {
					flags &= ~GDA_VALUE_ATTR_IS_NULL;
					rv->value = gda_value_copy ((GValue*) value);
				}
				else
					flags |= GDA_VALUE_ATTR_IS_NULL;
				rv->attributes = flags;
			}
		}
		else {
			/* create a new RowValue */
			GdaValueAttribute flags = 0;

			rv = g_new0 (RowValue, 1);
			rv->row_modif = rm;
			rv->model_column = col;
			rv->attributes = priv->columns_attrs [col];
			flags = rv->attributes;

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
			rv->attributes = flags;
			rm->modify_values = g_slist_prepend (rm->modify_values, rv);
		}

		if (rv) {
			GdaValueAttribute flags = rv->attributes;
			flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
			flags &= ~GDA_VALUE_ATTR_IS_DEFAULT;
			rv->attributes = flags;
		}

		if (!rm->to_be_deleted && !rm->modify_values && (rm->model_row >= 0)) {
			/* remove that RowModif, it's useless */
			gint tmp;
			tmp = rm->model_row;
			g_hash_table_remove (priv->modify_rows, &tmp);
			priv->all_modifs = g_slist_remove (priv->all_modifs, rm);
			row_modifs_free (rm);
		}

		if (priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
	}
	else {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_VALUE,
			     _("Trying to change read-only column: %d"), col);
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	g_rec_mutex_unlock (& (priv->mutex));
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	if (!values)
		return TRUE;

	g_return_val_if_fail ((gint)g_list_length (values) <= gda_data_proxy_get_n_columns (model), FALSE);

	/* check values */
	col = 0;
	list = values;
	while (list && !err) {
		GValue *value = (GValue *)(list->data);

		if (value && !gda_value_is_null (value)) {
			GdaColumn *column;
			column = gda_data_model_describe_column (model, col);
			if (gda_column_get_g_type (column) != G_VALUE_TYPE (value)) {
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
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

	g_rec_mutex_lock (& (priv->mutex));

	/* temporary disable changes notification */
	notify_changes = priv->notify_changes;
	priv->notify_changes = FALSE;

	for (col = 0, list = values; list; col ++, list = list->next) {
		if (list->data &&
		    !gda_data_proxy_set_value_at (model, col, row, (GValue *)(list->data), error)) {
			err = TRUE;
			break;
		}
	}

	priv->notify_changes = notify_changes;
	if (col && priv->notify_changes)
		/* at least one successful value change occurred */
		gda_data_model_row_updated (model, row);

	g_rec_mutex_unlock (& (priv->mutex));
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
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	/* ensure that there is no sync to be done */
	ensure_chunk_sync (proxy);

	/* temporary disable changes notification */
	notify_changes = priv->notify_changes;
	priv->notify_changes = FALSE;

	newrow = gda_data_proxy_append (proxy);
	if (! gda_data_proxy_set_values (model, newrow, (GList *) values, error)) {
		gda_data_proxy_remove_row (model, newrow, NULL);
		priv->notify_changes = notify_changes;
		g_rec_mutex_unlock (& (priv->mutex));
		return -1;
	}
	else {
		priv->notify_changes = notify_changes;
		if (priv->notify_changes)
			gda_data_model_row_inserted (model, newrow);
		g_rec_mutex_unlock (& (priv->mutex));
		return newrow;
	}
}

static gint
gda_data_proxy_append_row (GdaDataModel *model, G_GNUC_UNUSED GError **error)
{
	GdaDataProxy *proxy;
	gint i;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), -1);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	i = gda_data_proxy_append (proxy);
	g_rec_mutex_unlock (& (priv->mutex));
	return i;
}

static gboolean
gda_data_proxy_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));

	if (priv->add_null_entry && row == 0) {
		g_set_error (error, GDA_DATA_PROXY_ERROR, GDA_DATA_PROXY_READ_ONLY_ROW,
			      "%s", _("The first row is an empty row artificially prepended and cannot be removed"));
		g_rec_mutex_unlock (& (priv->mutex));
		return FALSE;
	}

	gda_data_proxy_delete (proxy, row);
	g_rec_mutex_unlock (& (priv->mutex));
	return TRUE;
}

static void
gda_data_proxy_freeze (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_if_fail (GDA_IS_DATA_PROXY (model));
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	priv->notify_changes = FALSE;
	g_rec_mutex_unlock (& (priv->mutex));
}

static void
gda_data_proxy_thaw (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_if_fail (GDA_IS_DATA_PROXY (model));
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	g_rec_mutex_lock (& (priv->mutex));
	priv->notify_changes = TRUE;
	g_rec_mutex_unlock (& (priv->mutex));
}

static gboolean
gda_data_proxy_get_notify (GdaDataModel *model)
{
	GdaDataProxy *proxy;
	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	return priv->notify_changes;
}

static void
gda_data_proxy_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	GdaDataProxy *proxy;
	g_return_if_fail (GDA_IS_DATA_PROXY (model));
	proxy = GDA_DATA_PROXY (model);
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);

	if (priv->model)
		gda_data_model_send_hint (priv->model, hint, hint_value);
}

/*
 * Cache management
 */
static void
clean_cached_changes (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	while (priv->cached_modifs) {
		row_modifs_free (ROW_MODIF (priv->cached_modifs->data));
		priv->cached_modifs = g_slist_delete_link (priv->cached_modifs,
								  priv->cached_modifs);
	}
	while (priv->cached_inserts) {
		row_modifs_free (ROW_MODIF (priv->cached_inserts->data));
		priv->cached_inserts = g_slist_delete_link (priv->cached_inserts,
								  priv->cached_inserts);
	}
}

static void
migrate_current_changes_to_cache (GdaDataProxy *proxy)
{
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	while (priv->all_modifs) {
		RowModif *rm;
		rm = (RowModif*) priv->all_modifs->data;
#ifdef GDA_DEBUG_NO
		g_print ("=== cached RM %p for row %d\n", rm, rm->model_row);
#endif
		if (rm->model_row == -1)
			priv->cached_inserts = g_slist_prepend (priv->cached_inserts, rm);
		else
			priv->cached_modifs = g_slist_prepend (priv->cached_modifs, rm);
		priv->all_modifs = g_slist_delete_link (priv->all_modifs,
							       priv->all_modifs);
	}
	g_hash_table_remove_all (priv->modify_rows);
	if (priv->new_rows) {
		g_slist_free (priv->new_rows);
		priv->new_rows = NULL;
	}
}

static void
fetch_current_cached_changes (GdaDataProxy *proxy)
{
	GSList *list;
	gint ncols;
	GdaDataProxyPrivate *priv = gda_data_proxy_get_instance_private (proxy);
	g_return_if_fail (priv->model);

	ncols = priv->model_nb_cols;

	/* handle INSERT Row Modifs */
	for (list = priv->cached_inserts; list;) {
		RowModif *rm = (RowModif*) list->data;
		if (rm->orig_values_size != ncols) {
			list = list->next;
			continue;
		}

		gint i;
		for (i = 0; i < ncols; i++) {
			GdaColumn *gcol;
			const GValue *cv = NULL;
			gcol = gda_data_model_describe_column (priv->model, i);

			GSList *rvl;
			for (rvl = rm->modify_values; rvl; rvl = rvl->next) {
				RowValue *rv = ROW_VALUE (rvl->data);
				if (rv->model_column == i) {
					cv = rv->value;
					break;
				}
			}

			if (cv && (G_VALUE_TYPE (cv) != GDA_TYPE_NULL) &&
			    G_VALUE_TYPE (cv) != gda_column_get_g_type (gcol))
				break;
		}

		if (i == ncols) {
			/* Matched! => move that Row Modif from cache */
			GSList *nlist;
			nlist = list->next;
			priv->cached_inserts = g_slist_delete_link (priv->cached_inserts,
									   list);
			priv->all_modifs = g_slist_prepend (priv->all_modifs, rm);
			priv->new_rows = g_slist_append (priv->new_rows, rm);
#ifdef GDA_DEBUG_NO
			g_print ("=== fetched RM %p for row %d\n", rm, rm->model_row);
#endif
			list = nlist;
		}
		else
			list = list->next;
	}

	/* handle UPDATE and DELETE Row Modifs */
	if (! priv->cached_modifs)
		return;

	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (priv->model);
	while (gda_data_model_iter_move_next (iter)) {
		for (list = priv->cached_modifs; list; list = list->next) {
			RowModif *rm = (RowModif*) list->data;
			const GValue *v1, *v2;
			if (rm->orig_values_size != ncols)
				continue;

			gint i;
			for (i = 0; i < ncols; i++) {
				v1 = gda_data_model_iter_get_value_at (iter, i);
				v2 = rm->orig_values [i];

				if ((v1 && !v2) || (!v1 && v2))
					break;
				else if (v1 && v2) {
					if ((G_VALUE_TYPE (v1) != G_VALUE_TYPE (v2)) ||
					    gda_value_differ (v1, v2))
						break;
				}
			}

			if (i == ncols) {
				/* Matched! => move that Row Modif from cache */
				priv->cached_modifs = g_slist_delete_link (priv->cached_modifs,
										  list);
				priv->all_modifs = g_slist_prepend (priv->all_modifs, rm);

				gint *ptr;
				ptr = g_new (gint, 1);
#ifdef GDA_DEBUG_NO
				g_print ("=== fetched RM %p for row %d (old %d)\n", rm, gda_data_model_iter_get_row (iter), rm->model_row);
#endif
				rm->model_row = gda_data_model_iter_get_row (iter);
				*ptr = rm->model_row;
				g_hash_table_insert (priv->modify_rows, ptr, rm);
				break; /* the FOR over cached_modifs, as there can only be 1 Row Modif
					* for the row iter is on */
			}
		}
	}
	g_object_unref (iter);
}
	/* RowModif structurdata */
