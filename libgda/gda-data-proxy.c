/* gda-data-proxy.c
 *
 * Copyright (C) 2005 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
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
static guint                gda_data_proxy_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_data_proxy_create_iter     (GdaDataModel *model);


static guint                gda_data_proxy_get_access_flags(GdaDataModel *model);

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

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
        ROW_DELETE_CHANGED,
	SAMPLE_SIZE_CHANGED,
	SAMPLE_CHANGED,
        LAST_SIGNAL
};

static gint gda_data_proxy_signals[LAST_SIGNAL] = { 0, 0, 0 };


/* properties */
enum
{
	PROP_0,
	PROP_MODEL,
	PROP_AUTOCOMMIT,
	PROP_ADD_NULL_ENTRY,
};

/*
 * Structure to hold the status and all the modifications made to a single
 * row. It is not possible to have ((model_row < 0) && !modif_values)
 */
typedef struct
{
	gint           model_row;    /* row index in the GdaDataModel, -1 if new row */
	gboolean       to_be_deleted;/* TRUE if row is to be deleted */
	GSList        *modif_values; /* list of RowValue structures */
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
        GValue        *attributes;   /* holds a GUINT of GValueAttribute */
} RowValue;
#define ROW_VALUE(x) ((RowValue *)(x))

/*
 * NOTE about the row numbers:
 * 
 * There are 2 kinds of row numbers: the row numbers as identified in the proxy (named "proxy_row")
 * and the row numbers in the GdaDataModel being proxied (named "model_row")
 *
 * There may be more rows in the GtkTreeModel than in the GdaDataModel if:
 *  - some rows have been added in the proxy
 *    and are not yet in the GdaDataModel (RowModif->model_row = -1 in this case); or
 *  - there is a NULL row at the beginning
 *
 * The rule to go from one row numbering to the other is to use the model_row_to_proxy_row() and proxy_row_to_model_row()
 * functions.
 */
struct _GdaDataProxyPrivate
{
	GdaDataModel      *model; /* Gda model which is proxied */
	GValue           **columns_attrs; /* Each GValue holds a GUINT of GValueAttribute to proxy cols. attributes */

	gint               model_nb_cols; /* = gda_data_model_get_n_columns (model) */
	gboolean           autocommit;
	gboolean           notify_changes;
	guint              background_commit_id;

	GSList            *all_modifs; /* list of RowModif structures, for memory management */
	GSList            *new_rows;   /* list of RowModif, no data allocated in this list */
	GHashTable        *modif_rows;  /* key = model_row number, value = RowModif, NOT for new rows */

	gboolean           ignore_proxied_changes;
	gboolean           proxy_has_changed;

	gboolean           add_null_entry; /* artificially add a NULL entry at the beginning of the tree model */

	guint              idle_add_event_source; /* !=0 when data rows are being added */

	/* chunking: ONLY accounting for @model's rows, not the new rows */
	gint               sample_first_row;
	gint               sample_last_row;
	gint               sample_size;
	gint               current_nb_rows;
	
	/* for ALL the columns of proxy */
	GdaColumn        **columns;

	/* attributes to implement the gda_data_proxy_store_model_row_value() and similar functions */
	GHashTable        *extra_store; /* key = (model,col), value = a GValue */
};

/*
 * Functions specific for proxy->priv->extra_store
 */
typedef struct {
	GdaDataModel *model;
	gint          col;
} ExtraStore;

static guint
custom_hash_func (ExtraStore *es)
{
	guint ret;

	ret = GPOINTER_TO_UINT (es->model);
	ret += (es->col) << 16;

	return ret;
}

static gboolean
custom_equal_func (ExtraStore *es1, ExtraStore *es2)
{
	if ((es1->model == es2->model) && (es1->col == es2->col))
		return TRUE;
	else
		return FALSE;
}

/*
 * The model_row_to_proxy_row() and proxy_row_to_model_row() convert row numbers between GdaDataModel
 * and the proxy.
 *
 * The current implementation is that the first N rows are GdaDataModel rows, and the next M ones
 * are new rows. It has the inconvenient of not being able to mix existing rows in the GdaDataModel
 * with new rows.
 *
 * Also if there is a NULL row at the beginning, the whole schema is shifted by 1
 *
 * -1 is returned if there is no model row corresponding to the proxy row
 */
static gint
model_row_to_proxy_row (GdaDataProxy *proxy, gint row)
{
        if (proxy->priv->add_null_entry)
                return row - proxy->priv->sample_first_row + 1;
        else
                return row - proxy->priv->sample_first_row;
}

static gint
proxy_row_to_model_row (GdaDataProxy *proxy, gint row)
{
        if (proxy->priv->add_null_entry) {
                if (row == 0)
                        return -1;

                if (row < proxy->priv->current_nb_rows + 1)
                        return row + proxy->priv->sample_first_row - 1;
                else
                        return -1;
        }
        else {
                if (row < proxy->priv->current_nb_rows)
                        return row + proxy->priv->sample_first_row;
                else
                        return -1;
        }
}

static RowModif *
find_row_modif_for_proxy_row (GdaDataProxy *proxy, gint proxy_row)
{
	RowModif *rm = NULL;
	gint model_row;

	model_row = proxy_row_to_model_row (proxy, proxy_row);
	if (model_row >= 0)
		rm = g_hash_table_lookup (proxy->priv->modif_rows, GINT_TO_POINTER (model_row));
	else {
		gint offset = proxy->priv->current_nb_rows + 
			(proxy->priv->add_null_entry ? 1 : 0);

		offset = proxy_row - offset;
		rm = g_slist_nth_data (proxy->priv->new_rows, offset);
	}

	return rm;
}

static gint
find_proxy_row_for_row_modif (GdaDataProxy *proxy, RowModif *rm)
{
	if (rm->model_row >= 0)
		return model_row_to_proxy_row (proxy, rm->model_row);
	else {
		gint index;
		index = g_slist_index (proxy->priv->new_rows, rm);
		g_assert (index >= 0);
		return index + proxy->priv->current_nb_rows + 
			(proxy->priv->add_null_entry ? 1 : 0);
	}
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
		
	list = rm->modif_values;
	while (list) {
		if (ROW_VALUE (list->data)->value)
			gda_value_free (ROW_VALUE (list->data)->value);
		g_free (list->data);
		list = g_slist_next (list);
	}
	g_slist_free (rm->modif_values);
	
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
	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
	if (rm) 
		g_warning ("%s(): RowModif already exists for that proxy_row", __FUNCTION__);
#endif
	
	rm = g_new0 (RowModif, 1);
	if (proxy_row >= 0) {
		gint i, model_row;
		
		rm->orig_values = g_new0 (GValue *, proxy->priv->model_nb_cols);
		rm->orig_values_size = proxy->priv->model_nb_cols;
		model_row = proxy_row_to_model_row (proxy, proxy_row);
		
		for (i=0; i<proxy->priv->model_nb_cols; i++) {
			const GValue *oval;

			oval = gda_data_proxy_get_value_at ((GdaDataModel *)proxy, i, model_row);
			if (oval)
				rm->orig_values [i] = gda_value_copy ((GValue *) oval);
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

	if (!type) {
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
	
	class->row_delete_changed = NULL;
	class->sample_size_changed = NULL;
	class->sample_changed = NULL;

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
					 g_param_spec_pointer ("model", _("Data model"), NULL,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE |
								G_PARAM_CONSTRUCT_ONLY)));
	g_object_class_install_property (object_class, PROP_AUTOCOMMIT,
					 g_param_spec_boolean ("autocommit", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_ADD_NULL_ENTRY,
					 g_param_spec_boolean ("prepend_null_entry", NULL, NULL, FALSE,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static gboolean
background_commit_cb (GdaDataProxy *proxy)
{
	if (proxy->priv) {
		GError *error = NULL;

		if (!gda_data_proxy_apply_all_changes (proxy, &error)) {
			g_warning (_("Could not auto-commit row change: %s"), 
				   error && error->message ?  error->message : _("No detail"));
			if (error)
				g_error_free (error);
		}
		proxy->priv->background_commit_id = 0;
	}
	return FALSE;
}

static void
row_changed_signal (GdaDataModel *model, gint row)
{
	GdaDataProxy *proxy;

	proxy = GDA_DATA_PROXY (model);
	if (!proxy->priv)
		return;

	/* the changes will be committed later in an idle high priority loop */
	if (proxy->priv->autocommit && !proxy->priv->background_commit_id) {
		proxy->priv->background_commit_id = g_timeout_add_full (G_PRIORITY_HIGH_IDLE, 10,
									(GSourceFunc) background_commit_cb, 
									proxy, NULL);
	}
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

	iface->row_inserted = row_changed_signal;
	iface->row_updated = row_changed_signal;
	iface->row_removed = row_changed_signal;
}

static void
gda_data_proxy_init (GdaDataProxy *proxy)
{
	proxy->priv = g_new0 (GdaDataProxyPrivate, 1);
	proxy->priv->modif_rows = g_hash_table_new (NULL, NULL);
	proxy->priv->notify_changes = TRUE;
	proxy->priv->ignore_proxied_changes = FALSE;
	proxy->priv->proxy_has_changed = FALSE;

	proxy->priv->add_null_entry = FALSE;
	proxy->priv->idle_add_event_source = 0;
	proxy->priv->background_commit_id = 0;

	proxy->priv->sample_first_row = 0;
	proxy->priv->sample_last_row = 0;
	proxy->priv->sample_size = 300;
	proxy->priv->current_nb_rows = 0;
	proxy->priv->columns = NULL;

	proxy->priv->extra_store = g_hash_table_new_full ((GHashFunc) custom_hash_func, 
							  (GEqualFunc) custom_equal_func,
							  g_free, (GDestroyNotify) gda_value_free);
}

static void adjust_displayed_chunck (GdaDataProxy *proxy);
static gboolean idle_add_model_rows (GdaDataProxy *proxy);


static void proxied_model_data_changed_cb (GdaDataModel *model, GdaDataProxy *proxy);

/**
 * gda_data_proxy_new
 * @dict: a #GdaDict object
 * @type: the #GdaDictType requested
 *
 * Creates a new proxy of type @type
 *
 * Returns: a new #GdaDataProxy object
 */
GObject *
gda_data_proxy_new (GdaDataModel *model)
{
	GObject *obj;
	GdaDataProxy *proxy;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), NULL);

	obj = g_object_new (GDA_TYPE_DATA_PROXY, "dict", gda_object_get_dict (GDA_OBJECT (model)), 
			    "model", model, NULL);

	proxy = GDA_DATA_PROXY (obj);

	return obj;
}

static void
destroyed_object_cb (GdaObject *obj, GdaDataProxy *proxy)
{
	gda_object_destroy (GDA_OBJECT (proxy));
}

static void
gda_data_proxy_dispose (GObject *object)
{
	GdaDataProxy *proxy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_DATA_PROXY (object));

	proxy = GDA_DATA_PROXY (object);
	if (proxy->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		if (proxy->priv->idle_add_event_source) {
			g_idle_remove_by_data (proxy);
			proxy->priv->idle_add_event_source = 0;
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
		
		if (proxy->priv->modif_rows) {
			gda_data_proxy_cancel_all_changes (proxy);
			g_hash_table_destroy (proxy->priv->modif_rows);
			proxy->priv->modif_rows = NULL;
		}

		if (proxy->priv->extra_store) {
			g_hash_table_destroy (proxy->priv->extra_store);
			proxy->priv->extra_store = NULL;
		}
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
		if (proxy->priv->new_rows) {
			g_slist_free (proxy->priv->new_rows);
			proxy->priv->new_rows = NULL;
		}
			
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
			model = (GdaDataModel*) g_value_get_pointer (value);
			g_return_if_fail (GDA_IS_DATA_MODEL (model));

			if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
				g_warning (_("GdaDataProxy cant' handle non random access data models"));
				return;
			}
			proxy->priv->model = model;
			g_object_ref (model);
			gda_object_connect_destroy (GDA_OBJECT (model), G_CALLBACK (destroyed_object_cb), object);
			
			/* if (gda_data_model_get_status (model) & GDA_DATA_MODEL_NEEDS_INIT_REFRESH)  */
			/* gda_data_model_refresh (model, NULL); */
			
			proxy->priv->model_nb_cols = gda_data_model_get_n_columns (model);
			
			/* column attributes */
			proxy->priv->columns_attrs = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (col = 0; col < proxy->priv->model_nb_cols; col++) {
				GdaColumn *column;
				guint flags = GDA_VALUE_ATTR_IS_UNCHANGED;
				
				column = gda_data_model_describe_column (model, col);
				if (gda_column_get_allow_null (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
				if (gda_column_get_default_value (column))
					flags |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;
				proxy->priv->columns_attrs[col] = g_value_init (g_new0 (GValue, 1), G_TYPE_UINT);
				g_value_set_uint (proxy->priv->columns_attrs[col], flags);
			}
			
			g_signal_connect (G_OBJECT (model), "changed",
					  G_CALLBACK (proxied_model_data_changed_cb), proxy);
			
			adjust_displayed_chunck (proxy);
			break;
		case PROP_AUTOCOMMIT:
			proxy->priv->autocommit = g_value_get_boolean (value);
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
		case PROP_AUTOCOMMIT:
			g_value_set_boolean (value, proxy->priv->autocommit);
			break;
		case PROP_ADD_NULL_ENTRY:
			g_value_set_boolean (value, proxy->priv->add_null_entry);
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
	if (proxy->priv->ignore_proxied_changes) {
		proxy->priv->proxy_has_changed = TRUE;
		return;
	}
	proxy->priv->proxy_has_changed = FALSE;

	/* stop idle adding of rows */
	if (proxy->priv->idle_add_event_source) {
		g_idle_remove_by_data (proxy);
		proxy->priv->idle_add_event_source = 0;
	}

	/* Free memory for the modifications */
	while (proxy->priv->all_modifs) {
		gint model_row = ROW_MODIF (proxy->priv->all_modifs->data)->model_row;
		row_modifs_free (ROW_MODIF (proxy->priv->all_modifs->data));
		if (model_row >= 0)
			g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (model_row));
		proxy->priv->all_modifs = g_slist_delete_link (proxy->priv->all_modifs, proxy->priv->all_modifs);
	}
	if (proxy->priv->new_rows) {/* new rows removing (no memory de-allocation here, though) */
		gint i, j, rowsig;
		j = g_slist_length (proxy->priv->new_rows);
		rowsig = proxy->priv->current_nb_rows + (proxy->priv->add_null_entry ? 1 : 0);
		/* emit a "delete" signal USING the SAME row number for all of them */
		for (i = 0; i < j ; i++) {
			if (proxy->priv->notify_changes)
				gda_data_model_row_removed ((GdaDataModel *) proxy, rowsig);
		}
		g_slist_free (proxy->priv->new_rows);
		proxy->priv->new_rows = NULL;
	}

	/* take into account the new dimensions of the proxied data model */
	if (proxy->priv->model_nb_cols != gda_data_model_get_n_columns (model)) {
		proxy->priv->model_nb_cols = gda_data_model_get_n_columns (model);
		
		/* FIXME: reset all proxy's attributes linked to proxy->priv->model_nb_cols */
		TO_IMPLEMENT;

		/* WILL break as implementation is missing */
		g_assert_not_reached ();
	}

	adjust_displayed_chunck (proxy);
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
	RowModif *rm;
	RowValue *rv = NULL;
	gint model_row;

	model_row = proxy_row_to_model_row (proxy, proxy_row);
	rm = find_row_modif_for_proxy_row (proxy, proxy_row);

	if (!rm) {
		/* create a new RowModif */
		g_assert (model_row >= 0);
		rm = row_modifs_new (proxy, proxy_row);
		rm->model_row = model_row;
		if (model_row >= 0)
			g_hash_table_insert (proxy->priv->modif_rows, GINT_TO_POINTER (model_row), rm);
		proxy->priv->all_modifs = g_slist_prepend (proxy->priv->all_modifs, rm);
	}
	else {
		/* there are already some modifications to the row, try to catch the RowValue if available */
		GSList *list;
		
		list = rm->modif_values;
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
 * is an ORed value of #GValueAttribute flags
 *
 * Returns: the attribute
 */
guint
gda_data_proxy_get_value_attributes (GdaDataProxy *proxy, gint proxy_row, gint col)
{
	gint model_row;
	RowModif *rm;
	gboolean value_has_modifs = FALSE;
	guint flags = 0;
	gint model_column;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), 0);
	g_return_val_if_fail (proxy->priv, 0);
	g_return_val_if_fail (proxy_row >= 0, 0);

	model_column = col % proxy->priv->model_nb_cols;
	model_row = proxy_row_to_model_row (proxy, proxy_row);
	flags = gda_data_model_get_attributes_at (proxy->priv->model, model_column, model_row);
	if (model_row < 0)
		flags |= GDA_VALUE_ATTR_IS_NULL;

	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
	if (rm) {
		if (rm->modif_values) {
			/* there are some modifications to the row */
			GSList *list;
			RowValue *rv = NULL;
			
			list = rm->modif_values;
			while (list && !rv) {
				if (ROW_VALUE (list->data)->model_column == model_column)
					rv = ROW_VALUE (list->data);
				list = g_slist_next (list);
			}
			if (rv) {
				value_has_modifs = TRUE;
				flags |= g_value_get_uint (rv->attributes);
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
gda_data_proxy_alter_value_attributes (GdaDataProxy *proxy, gint proxy_row, gint col, guint alter_flags)
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
			guint flags;
			if (!rv) {
				/* create a new RowValue */
				/*g_print ("New RV\n");*/
				rv = g_new0 (RowValue, 1);
				rv->row_modif = rm;
				rv->model_column = model_col;
				rv->attributes = gda_value_copy (proxy->priv->columns_attrs [col]);
				flags = g_value_get_uint (rv->attributes);
							
				rv->value = NULL;
				flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
				if (rm->model_row >= 0)
					flags |= GDA_VALUE_ATTR_HAS_VALUE_ORIG;
				else
					flags &= ~GDA_VALUE_ATTR_HAS_VALUE_ORIG;
				
				rm->modif_values = g_slist_prepend (rm->modif_values, rv);	
			}
			else
				flags = g_value_get_uint (rv->attributes);
			flags |= GDA_VALUE_ATTR_IS_DEFAULT;
			g_value_set_uint (rv->attributes, flags);

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
	RowModif *rm;
	gboolean do_signal = FALSE;
	gint model_row;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	model_row = proxy_row_to_model_row (proxy, proxy_row);
	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
	if (rm) {
		if (! rm->to_be_deleted) {
			if (rm->model_row < 0) {
				/* remove the row completely because it does not exist in the data model */
				proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
				proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
				row_modifs_free (rm);

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
		g_hash_table_insert (proxy->priv->modif_rows, GINT_TO_POINTER (model_row), rm);
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
	RowModif *rm;
	gboolean do_signal = FALSE;
	gint model_row;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	model_row = proxy_row_to_model_row (proxy, proxy_row);
	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
	if (rm) {
		rm->to_be_deleted = FALSE;
		if (!rm->modif_values) {
			/* get rid of that RowModif */
			do_signal= TRUE;
			
			g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (model_row));
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

	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
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

	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
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

	/* if there are still some rows waiting to be added in the idle loop, then force them to be added
	 * first, otherwise we might not find what we are looking for!
	 */
	if (proxy->priv->idle_add_event_source) {
		g_idle_remove_by_data (proxy);
		while (idle_add_model_rows (proxy)) ;
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
 * Appends a new row to the proxy
 *
 * Returns: the proxy row number of the new row
 */
gint
gda_data_proxy_append (GdaDataProxy *proxy)
{
	RowModif *rm;
	gint col;
	gint proxy_row;
	
	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	g_return_val_if_fail (proxy->priv, -1);

	/* RowModif structure */
	rm = row_modifs_new (proxy, -1);
	rm->model_row = -1;
	rm->orig_values = NULL; /* there is no original value */
	rm->orig_values_size = 0;

	proxy->priv->all_modifs = g_slist_prepend (proxy->priv->all_modifs, rm);
	proxy->priv->new_rows = g_slist_append (proxy->priv->new_rows, rm);

	/* new proxy row value */
	proxy_row = proxy->priv->current_nb_rows + (proxy->priv->add_null_entry ? 1 : 0) +
		g_slist_length (proxy->priv->new_rows) - 1;
	
	/* for the columns which allow a default value, set them to the default value */
	for (col = 0; col < proxy->priv->model_nb_cols; col ++) {
		GdaColumn *column;
		const GValue *def;
		RowValue *rv;
		guint flags = 0;
		
		/* create a new RowValue */
		/*g_print ("New RV\n");*/
		rv = g_new0 (RowValue, 1);
		rv->row_modif = rm;
		rv->model_column = col;
		rv->attributes = gda_value_new (G_TYPE_UINT);
		rv->value = NULL;
		rm->modif_values = g_slist_prepend (rm->modif_values, rv);	
		
		column = gda_data_model_describe_column (proxy->priv->model, col);
		def = gda_column_get_default_value (column);
		if (def) {
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);
			if (G_VALUE_TYPE (def) == gda_column_get_g_type (column))
				rv->value = gda_value_copy (def);
		}
		if (gda_column_get_allow_null (column)) {
			guint attributes;
			
			attributes = gda_data_model_get_attributes_at (proxy->priv->model, col, -1);;
			if (attributes & GDA_VALUE_ATTR_CAN_BE_NULL)
				flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
		}

		if (gda_column_get_auto_increment (column))
			flags |= (GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT);

		g_value_set_uint (rv->attributes, flags);
	}

	/* signal row insertion */
	if (proxy->priv->notify_changes)
		gda_data_model_row_inserted ((GdaDataModel *) proxy, proxy_row); 	

	return proxy_row;
}

/**
 * gda_data_proxy_cancel_row_changes
 * @proxy: a #GdaDataProxy object
 * @col:
 * @row:
 *
 * Resets data at the corresponding row and column. @row and @col can be <0 if all the corresponding
 * rows and columns are to be resetted.
 */
void
gda_data_proxy_cancel_row_changes (GdaDataProxy *proxy, gint proxy_row, gint col)
{
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	g_return_if_fail (proxy_row >= 0);

	if ((col >= 0) && (col < proxy->priv->model_nb_cols)) {
		RowModif *rm;

		rm = find_row_modif_for_proxy_row (proxy, proxy_row);
		if (rm && rm->modif_values) {
			/* there are some modifications to the row */
			GSList *list;
			RowValue *rv = NULL;
			
			list = rm->modif_values;
			while (list && !rv) {
				if (ROW_VALUE (list->data)->model_column == col)
					rv = ROW_VALUE (list->data);
				list = g_slist_next (list);
			}
			if (rv) {
				/* remove this RowValue from the RowList */
				rm->modif_values = g_slist_remove (rm->modif_values, rv);
				if (!rm->modif_values && !rm->to_be_deleted) {
					/* remove this RowList as well */
					proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
					if (rm->model_row < 0) 
						proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
					else
						g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (rm->model_row));
					row_modifs_free (rm);

					if (proxy->priv->notify_changes)
						gda_data_model_row_removed ((GdaDataModel *) proxy, proxy_row);
				}
				else {
					if (proxy->priv->notify_changes)
						gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
				}
			}
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

	return commit_row_modif (proxy, find_row_modif_for_proxy_row (proxy, proxy_row), error);
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
	gboolean freedone = FALSE;
	gboolean ignore_proxied_changes;

	if (!rm)
		return TRUE;

	/*
	 * Steps in this procedure:
	 * -1- disable handling of proxied model modifications
	 * -2- apply desired modification (which may trigger a "change" signal from
	 *     the proxied model, that's why we need to specifically ignore it
	 * -3.1- if no error then destroy the RowModif which has just been applied
	 *       and refresh displayed chuncks if RowModif was a delete
	 * -4- re-enable handling of proxied model modifications
	 */

	ignore_proxied_changes = proxy->priv->ignore_proxied_changes;
	proxy->priv->ignore_proxied_changes = TRUE;

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
			
			g_assert (rm->modif_values);
			g_assert (rm->orig_values);
			free_val = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (i=0; i < rm->orig_values_size; i++) {
				newvalue_found = FALSE;
				newvalue = NULL;
				
				list = rm->modif_values;
				while (list && !newvalue_found) {
					if (ROW_VALUE (list->data)->model_column == i) {
						newvalue_found = TRUE;
						if (g_value_get_uint (ROW_VALUE (list->data)->attributes) &
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
			
			g_assert (rm->modif_values);
			free_val = g_new0 (GValue *, proxy->priv->model_nb_cols);
			for (i = 0; i < proxy->priv->model_nb_cols; i++) {
				newvalue = NULL;
				
				list = rm->modif_values;
				while (list && !newvalue) {
					if (ROW_VALUE (list->data)->model_column == i) {
						if (g_value_get_uint (ROW_VALUE (list->data)->attributes) &
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

			if (!err) {
				/* emit a "delete" signal for that row because when rm is
				 * removed it means the corresponding row will have been removed,
				 * and if we don't emit that signal, then there will be a problem. 
				 */
				gint rowsig = find_proxy_row_for_row_modif (proxy, rm);

				proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
				proxy->priv->new_rows = g_slist_remove (proxy->priv->new_rows, rm);
				row_modifs_free (rm);

				gda_data_model_row_removed ((GdaDataModel *) proxy, rowsig);
				freedone = TRUE;
			}
		}
	}
	
	if (!err && !freedone) {
		/* get rid of the commited change, where rm->model_row >= 0 */
		proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
		g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (rm->model_row));
		row_modifs_free (rm);
	}

	proxy->priv->ignore_proxied_changes = ignore_proxied_changes;
	if (proxy->priv->proxy_has_changed) 
		proxied_model_data_changed_cb (proxy->priv->model, proxy);

	adjust_displayed_chunck (proxy);

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

	rm = find_row_modif_for_proxy_row (proxy, proxy_row);
	return rm && (rm->modif_values || rm->to_be_deleted) ? TRUE : FALSE;
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
 * @sample_size: the requested size of a chunck, or 0
 *
 * Sets the size of each chunck of fata to display: the maximum number of rows which
 * can be displayed at a time. The default value is arbitrary 300 as it is big enough to
 * be able to display quite a lot of data, but small enough to avoid too much data
 * displayed at the same time.
 *
 * Note: the rows which have been added but not yet commited will always be displayed
 * regardless of the current chunck of data, and the modified rows which are not visible
 * when the displayed chunck of data changes are still held as modified rows.
 *
 * To remove the chuncking of the data to display, simply pass @sample_size the 0 value.
 */
void
gda_data_proxy_set_sample_size (GdaDataProxy *proxy, gint sample_size)
{
	gint new_sample_size;
	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);

	new_sample_size = sample_size <= 0 ? 0 : sample_size;
	if (proxy->priv->sample_size != new_sample_size) {
		proxy->priv->sample_size = new_sample_size;
		adjust_displayed_chunck (proxy);
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
 * Returns: the chunck (or sample) size, or 0 if chunking is disabled.
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

	if (proxy->priv->sample_first_row != sample_start) {
		proxy->priv->sample_first_row = sample_start;
		adjust_displayed_chunck (proxy);
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

/*
 * Adjusts the values of the first and last rows to be displayed depending
 * on the sample size
 */
static void
adjust_displayed_chunck (GdaDataProxy *proxy)
{
	gint i, old_nb_rows, new_nb_rows, old_start, old_end;
	gint model_nb_rows;

	g_return_if_fail (proxy->priv->model);

	/* disable the emision of the "changed" signal each time a "row_*" signal is
	 * emitted, and instead send that signal only once at the end */
	gda_object_block_changed (GDA_OBJECT (proxy));

	/*
	 * Stop idle adding of rows if necessary
	 */
	if (proxy->priv->idle_add_event_source) {
		g_idle_remove_by_data (proxy);
		proxy->priv->idle_add_event_source = 0;
	}

	/*
	 * Compute chuncks limits
	 */
	old_start = proxy->priv->sample_first_row;
	old_end = proxy->priv->sample_last_row;

	old_nb_rows = proxy->priv->current_nb_rows;
	model_nb_rows = gda_data_model_get_n_rows (proxy->priv->model);

	if (model_nb_rows > 0) {
		/* known number of rows */
		if (proxy->priv->sample_size > 0) {
			if (proxy->priv->sample_first_row >= model_nb_rows) 
				proxy->priv->sample_first_row = proxy->priv->sample_size * 
					((model_nb_rows - 1) / proxy->priv->sample_size);
			
			proxy->priv->sample_last_row = proxy->priv->sample_first_row + 
				proxy->priv->sample_size - 1;
			if (proxy->priv->sample_last_row >= model_nb_rows)
				proxy->priv->sample_last_row = model_nb_rows - 1;
			new_nb_rows = proxy->priv->sample_last_row - proxy->priv->sample_first_row + 1;
		}
		else {
			proxy->priv->sample_first_row = 0;
			proxy->priv->sample_last_row = model_nb_rows - 1;
			new_nb_rows = model_nb_rows;
		}
	}
	else {
		if (model_nb_rows == 0 ) {
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

	if ((old_start != proxy->priv->sample_first_row) ||
	    (old_end != proxy->priv->sample_last_row))
		g_signal_emit (G_OBJECT (proxy),
                               gda_data_proxy_signals[SAMPLE_CHANGED],
                               0, proxy->priv->sample_first_row, proxy->priv->sample_last_row);	

	/*
	 * signal rows addition or removal
	 */
	if (old_nb_rows < new_nb_rows) {
		/*
		 * insert the missing rows in the idle loop
		 */
		proxy->priv->idle_add_event_source = g_idle_add ((GSourceFunc) idle_add_model_rows, proxy);

		proxy->priv->current_nb_rows = old_nb_rows;
	}
	else {
		/*
		 * delete all the remaining rows:
		 * emit the GdaDataModel::"row_removed" signal for all the new rows 
		 * (using the same row number!) 
		 */
		i = old_nb_rows < new_nb_rows ? old_nb_rows : new_nb_rows;
		gint rownb = model_row_to_proxy_row (proxy, proxy->priv->sample_first_row + i);
		for (; i < old_nb_rows; i++) 
			if (proxy->priv->notify_changes) {
				proxy->priv->current_nb_rows = new_nb_rows + old_nb_rows  - i - 1;
				gda_data_model_row_removed ((GdaDataModel *) proxy, rownb);
			}
		proxy->priv->current_nb_rows = new_nb_rows;
	}

	/*
	 * emit the GdaDataModel::"row_updated" signal for all the rows which already existed
	 */
	for (i=0; (i < old_nb_rows) && (i < new_nb_rows); i++) {
		if (model_nb_rows < 0) {
			if (! gda_data_model_get_value_at (proxy->priv->model, 0,
							   proxy->priv->sample_first_row + i)) {
				new_nb_rows = i;
				proxy->priv->sample_last_row = proxy->priv->sample_first_row + i - 1;
				break;
			}
		}
		if (proxy->priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, 
						    model_row_to_proxy_row (proxy, proxy->priv->sample_first_row + i));
	}
	


	/* re-enable the emision of the "changed" signal each time a "row_*" signal is
	 * emitted */
	gda_object_unblock_changed (GDA_OBJECT (proxy));
	gda_data_model_signal_emit_changed ((GdaDataModel *) proxy);

#ifdef GDA_DEBUG_NO
	g_print ("//GdaDataProxy adjusted sample: %d<->%d (=%d / %d rows, %s)\n", proxy->priv->sample_first_row, 
		 proxy->priv->sample_last_row,
		 proxy->priv->current_nb_rows,
		 gda_data_model_get_n_rows (proxy->priv->model),
		 proxy->priv->idle_add_event_source ? "Idle set" : "no idle func");
#endif
}

/*
 * Called in an idle loop to "add" rows to the proxy. The only effect is to
 * "declare" more rows in the GtkTreeModel interface. Returns FALSE when
 * it does not need to be called anymore.
 *
 * Rem: must be called only by the adjust_displayed_chunck() function.
 */
static gboolean
idle_add_model_rows (GdaDataProxy *proxy)
{
	gint tmp_current_nb_rows;
	gint model_nb_rows;
	gint i = 0;

#define IDLE_STEP 50

	g_return_val_if_fail (proxy->priv->model, FALSE);

	model_nb_rows = gda_data_model_get_n_rows (proxy->priv->model);
	if (model_nb_rows >= 0) {
		/* known number of rows */
		if (proxy->priv->sample_size > 0) {
			tmp_current_nb_rows = proxy->priv->sample_size;
			if (tmp_current_nb_rows >= model_nb_rows)
				tmp_current_nb_rows = model_nb_rows;
		}
		else
			tmp_current_nb_rows = model_nb_rows;
	}
	else {
		/* unknown number of rows */
		if (proxy->priv->sample_size > 0)
			tmp_current_nb_rows = proxy->priv->sample_size;
		else
			tmp_current_nb_rows = G_MAXINT;
	}
	
	while ((i < IDLE_STEP) && (proxy->priv->current_nb_rows < tmp_current_nb_rows)) {
		if (model_nb_rows < 0) {
			if (! gda_data_model_get_value_at (proxy->priv->model, 0,
							   proxy->priv->sample_first_row + 
							   proxy->priv->current_nb_rows)) {
				i = IDLE_STEP;
				proxy->priv->sample_last_row = proxy->priv->sample_first_row + 
					proxy->priv->current_nb_rows - 1;
				break;
			}
		}
		
		proxy->priv->current_nb_rows ++;
		/*g_print ("Nr: %d (%d)\n", proxy->priv->current_nb_rows, proxy->priv->notify_changes);*/
		if (proxy->priv->notify_changes) {
			if (model_nb_rows < 0)
				/* Force to fetch the next row to see if we have come to the end */
				gda_data_model_get_value_at (proxy->priv->model, 0,
							     proxy->priv->sample_first_row + 
							     proxy->priv->current_nb_rows);
			gda_data_model_row_inserted ((GdaDataModel *) proxy,
						     model_row_to_proxy_row (proxy, 
							       proxy->priv->sample_first_row + proxy->priv->current_nb_rows - 1));
		}
		i++;
	}

	if (i < IDLE_STEP) {
		proxy->priv->idle_add_event_source = 0;
		return FALSE;
	}
	else
		return TRUE;
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

	gda_data_model_send_hint (proxy->priv->model, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);

	proxy->priv->ignore_proxied_changes = TRUE;
	while (proxy->priv->all_modifs && allok)
		allok = commit_row_modif (proxy, ROW_MODIF (proxy->priv->all_modifs->data), error);
	proxy->priv->ignore_proxied_changes = FALSE;

	gda_data_model_send_hint (proxy->priv->model, GDA_DATA_MODEL_HINT_END_BATCH_UPDATE, NULL);

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

	/* new rows are first treated and removed (no memory de-allocation here, though) */
	if (proxy->priv->new_rows) {
		while (proxy->priv->new_rows) {
			proxy->priv->new_rows = g_slist_delete_link (proxy->priv->new_rows, proxy->priv->new_rows);

			if (proxy->priv->notify_changes)
				gda_data_model_row_removed ((GdaDataModel *) proxy, 
							    proxy->priv->current_nb_rows + (proxy->priv->add_null_entry ? 1 : 0));
		}
	}

	/* all modified rows are then treated (including memory de-allocation for new rows) */
	while (proxy->priv->all_modifs) {
		gint model_row = ROW_MODIF (proxy->priv->all_modifs->data)->model_row;
		
		row_modifs_free (ROW_MODIF (proxy->priv->all_modifs->data));
		if (model_row >= 0)
			g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (model_row));
		proxy->priv->all_modifs = g_slist_delete_link (proxy->priv->all_modifs, proxy->priv->all_modifs);
		
		if (model_row >= 0) {
			/* not a new row, emit a signal only if model row is between proxy->priv->sample_first_row
			 * and  proxy->priv->sample_last_row */
			if ((model_row >= proxy->priv->sample_first_row) &&
			    (model_row <= proxy->priv->sample_last_row))
				if (proxy->priv->notify_changes)
					gda_data_model_row_updated ((GdaDataModel *) proxy, 
								    model_row_to_proxy_row (proxy, model_row));
		}
	}

	return TRUE;
}


static void
store_dump_foreach (ExtraStore *key, GValue *value, gpointer data)
{
	g_print ("%p, C=%d, val= %s\n", key->model, key->col, gda_value_stringify (value));
}

static void
dump_extra_store (GdaDataProxy *proxy)
{
	g_hash_table_foreach (proxy->priv->extra_store, (GHFunc) store_dump_foreach, NULL);
}

/**
 * gda_data_proxy_get_model_row_value
 * @proxy: a #GdaDataProxy object
 * @model: a #GdaDataModel object
 * @proxy_row: a valid row number in @proxy
 * @col:
 * 
 * Retreive a value stored in @proxy using the gda_data_proxy_assign_model_col() method
 * 
 * Returns: the stored #GValue, or %NULL if no value was stored.
 */
const GValue *
gda_data_proxy_get_model_row_value (GdaDataProxy *proxy, GdaDataModel *model, gint proxy_row, gint extra_col)
{
	ExtraStore es;
	const GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), NULL);
	g_return_val_if_fail (proxy->priv, NULL);

	es.model = model;
	es.col = extra_col;
	value = g_hash_table_lookup (proxy->priv->extra_store, &es);
	if (value) {
		gint proxy_col;
		proxy_col = g_value_get_int ((GValue *) value);
		value = gda_data_model_get_value_at ((GdaDataModel *) proxy, proxy_col, proxy_row);
	}
	
	return value;
}

/**
 * gda_data_proxy_set_model_row_value
 * @proxy: a #GdaDataProxy object
 * @model: a #GdaDataModel object
 * @proxy_row: a valid row number in @proxy
 * @col:
 * @value:
 * 
 * Retreive a value stored in @proxy using the gda_data_proxy_assign_model_col() method
 * 
 * Returns: the stored #GValue, or %NULL if no value was stored.
 */
void
gda_data_proxy_set_model_row_value (GdaDataProxy *proxy, GdaDataModel *model, 
				    gint proxy_row, gint extra_col, const GValue *value)
{
	ExtraStore es;
	const GValue *colval;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);

	es.model = model;
	es.col = extra_col;
	colval = g_hash_table_lookup (proxy->priv->extra_store, &es);
	if (colval) {
		gint proxy_col;

		proxy_col = g_value_get_int ((GValue *) colval);
		g_assert (gda_data_model_set_value_at ((GdaDataModel *) proxy, proxy_col, proxy_row, 
						       (GValue *) value, NULL));
	}
}

/**
 * gda_data_proxy_clear_model_row_value
 */
void
gda_data_proxy_clear_model_row_value (GdaDataProxy *proxy, GdaDataModel *model, 
				      gint proxy_row, gint extra_col)
{
	ExtraStore es;
	const GValue *colval;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);

	es.model = model;
	es.col = extra_col;
	colval = g_hash_table_lookup (proxy->priv->extra_store, &es);
	if (colval) {
		gint proxy_col;
		const GValue *value;

		proxy_col = g_value_get_int ((GValue *) colval);
		value = gda_data_model_get_value_at ((GdaDataModel *) proxy, 
						     proxy_col + proxy->priv->model_nb_cols, proxy_row);
		g_assert (gda_data_model_set_value_at ((GdaDataModel *) proxy, proxy_col, proxy_row, 
						       (GValue *) value, NULL));
	}
}


/**
 * gda_data_proxy_assign_model_col
 * @proxy: a #GdaDataProxy object
 * @model: a #GdaDataModel object
 * @proxy_col: a valid column number in @proxy
 * @model_col: a valid column number in @model
 *
 * Instructs @proxy about what to do if a call to gda_data_proxy_get_model_row_value(@proxy, @model, a_row, @model_col)
 * fails to return a non %NULL value. If that case appears, then @proxy will return the value stored in @proxy at
 * column @proxy_col and row 'a_row'.
 */
void
gda_data_proxy_assign_model_col (GdaDataProxy *proxy, GdaDataModel *model, gint proxy_col, gint model_col)
{
	ExtraStore *es;
	GValue *value;

	g_return_if_fail (GDA_IS_DATA_PROXY (proxy));
	g_return_if_fail (proxy->priv);
	if (proxy->priv->model_nb_cols > 0)
		g_return_if_fail (proxy_col < proxy->priv->model_nb_cols);

	es = g_new (ExtraStore, 1);
	es->model = model;
	es->col = model_col;

	value = g_value_init (g_new0 (GValue, 1), G_TYPE_INT);
	g_value_set_int (value, proxy_col);
	g_hash_table_insert (proxy->priv->extra_store, es, value);

	dump_extra_store (proxy);
}
 
/**
 * gda_data_proxy_get_assigned_model_col
 */
gint
gda_data_proxy_get_assigned_model_col (GdaDataProxy *proxy, GdaDataModel *model, gint model_col)
{
	ExtraStore es;
	const GValue *colval;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (proxy), -1);
	g_return_val_if_fail (proxy->priv, -1);

	es.model = model;
	es.col = model_col;
	colval = g_hash_table_lookup (proxy->priv->extra_store, &es);
	if (colval)
		return g_value_get_int ((GValue *) colval);
	else
		return -1;
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

	nbrows = proxy->priv->current_nb_rows;
	nbrows += g_slist_length (proxy->priv->new_rows);
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

		orig = gda_data_model_describe_column (proxy->priv->model, 
						       i -  proxy->priv->model_nb_cols);
		proxy->priv->columns[i] = gda_column_copy (orig);
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

		rm = find_row_modif_for_proxy_row (proxy, proxy_row);
		if (rm && rm->modif_values) {
			/* there are some modifications to the row, see if there are some for the column */
			GSList *list;
			RowValue *rv = NULL;
					
			list = rm->modif_values;
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

		rm = find_row_modif_for_proxy_row (proxy, proxy_row);
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

static guint
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

	iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER, 
						  "dict", gda_object_get_dict (GDA_OBJECT (proxy->priv->model)), 
						  "data_model", proxy->priv->model, NULL);
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
			g_warning ("Proxy iterator does not have the same length as proxied model's iterator");
		g_object_unref (iter2);
	}

	return iter;
}

static guint
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
	gint row;
	GdaDataProxy *proxy;
	guint att;

	g_return_val_if_fail (GDA_IS_DATA_PROXY (model), FALSE);
	proxy = GDA_DATA_PROXY (model);
	g_return_val_if_fail (proxy->priv, FALSE);
	g_return_val_if_fail (proxy_row >= 0, FALSE);

	att = gda_data_proxy_get_value_attributes (proxy, proxy_row, col);
	if (att & GDA_VALUE_ATTR_NO_MODIF)
		return FALSE;

	row = proxy_row;
	if ((row == 0) && proxy->priv->add_null_entry) {
		g_warning ("Trying to set read-only NULL row");
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
			/* g_print ("Use existing RV\n"); */

			if (rv->value) {
				gda_value_free (rv->value);
				rv->value = NULL;
			}

			if (rm->orig_values && (col < rm->orig_values_size) && 
			    ! gda_value_compare_ext ((GValue *) value, rm->orig_values [col])) {
				/* remove the RowValue */
				rm->modif_values = g_slist_remove (rm->modif_values, rv);
				g_free (rv);
				rv = NULL;
			}
			else {
				/* simply alter the RowValue */
				guint flags = g_value_get_uint (rv->attributes);
				
				if (value && !gda_value_is_null ((GValue *) value)) {
					flags &= ~GDA_VALUE_ATTR_IS_NULL;
					rv->value = gda_value_copy ((GValue*) value);
				}
				else
					flags |= GDA_VALUE_ATTR_IS_NULL;
				g_value_set_uint (rv->attributes, flags);
			}
		}
		else {
			/* create a new RowValue */
			/*g_print ("New RV\n");*/
			guint flags;

			rv = g_new0 (RowValue, 1);
			rv->row_modif = rm;
			rv->model_column = col;
			rv->attributes = gda_value_copy (proxy->priv->columns_attrs [col]);
			flags = g_value_get_uint (rv->attributes);

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
			g_value_set_uint (rv->attributes, flags);
			rm->modif_values = g_slist_prepend (rm->modif_values, rv);
		}
		
		if (rv) {
			guint flags = g_value_get_uint (rv->attributes);
			flags &= ~GDA_VALUE_ATTR_IS_UNCHANGED;
			flags &= ~GDA_VALUE_ATTR_IS_DEFAULT;
			g_value_set_uint (rv->attributes, flags);
		}

		if (!rm->to_be_deleted && !rm->modif_values && (rm->model_row >= 0)) {
			/* remove that RowModif, it's useless */
			/* g_print ("Removing RM\n"); */
			g_hash_table_remove (proxy->priv->modif_rows, GINT_TO_POINTER (rm->model_row));
			proxy->priv->all_modifs = g_slist_remove (proxy->priv->all_modifs, rm);
			row_modifs_free (rm);
		}

		if (proxy->priv->notify_changes)
			gda_data_model_row_updated ((GdaDataModel *) proxy, proxy_row);
	}
	else
		g_set_error (error, 0, 0, _("Trying to change read-only column: %d"), col);

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
				g_set_error (error, 0, 0,
					     _("Value type mismatch %s instead of %s"),
					     g_type_to_string (G_VALUE_TYPE (value)),
					     g_type_to_string (gda_column_get_g_type (column)));
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
