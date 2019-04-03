/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2013, 2018 Daniel Espinosa <esodan@gmail.com>
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

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <libgda-ui.h>
#include <libgda/gda-blob-op.h>
#include "internal/utility.h"
#include "marshallers/gdaui-marshal.h"
#include "data-entries/gdaui-data-cell-renderer-combo.h"
#include "data-entries/gdaui-data-cell-renderer-info.h"
#include <libgda/binreloc/gda-binreloc.h>
#include <gtk/gtk.h>
#include <libgda/gda-debug-macros.h>

static void gdaui_raw_grid_dispose (GObject *object);

static void gdaui_raw_grid_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gdaui_raw_grid_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static void create_columns_data (GdauiRawGrid *grid);

static void proxy_sample_changed_cb (GdaDataProxy *proxy, gint sample_start, gint sample_end, GdauiRawGrid *grid);
static void proxy_row_updated_cb (GdaDataProxy *proxy, gint proxy_row, GdauiRawGrid *grid);
static void proxy_reset_pre_cb (GdaDataProxy *proxy, GdauiRawGrid *grid);
static void proxy_reset_cb (GdaDataProxy *proxy, GdauiRawGrid *grid);
static void paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiRawGrid *grid);
static void paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param,
					     const gchar *att_name, const GValue *att_value, GdauiRawGrid *grid);
static GError *iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawGrid *grid);
static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawGrid *grid);

static void remove_all_columns (GdauiRawGrid *grid);
static void reset_columns_default (GdauiRawGrid *grid);
static void reset_columns_in_xml_layout (GdauiRawGrid *grid, xmlNodePtr grid_node);


/* GdauiDataProxy interface */
static void            gdaui_raw_grid_widget_init           (GdauiDataProxyInterface *iface);
static GdaDataProxy   *gdaui_raw_grid_get_proxy             (GdauiDataProxy *iface);
static void            gdaui_raw_grid_set_column_editable   (GdauiDataProxy *iface, gint column, gboolean editable);
static gboolean        gdaui_raw_grid_supports_action       (GdauiDataProxy *iface, GdauiAction action);
static void            gdaui_raw_grid_perform_action        (GdauiDataProxy *iface, GdauiAction action);
static gboolean        gdaui_raw_grid_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
static GdauiDataProxyWriteMode gdaui_raw_grid_widget_get_write_mode (GdauiDataProxy *iface);

/* GdauiDataSelector interface */
static void              gdaui_raw_grid_selector_init (GdauiDataSelectorInterface *iface);
static GdaDataModel     *gdaui_raw_grid_selector_get_model (GdauiDataSelector *iface);
static void              gdaui_raw_grid_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *gdaui_raw_grid_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *gdaui_raw_grid_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          gdaui_raw_grid_selector_select_row (GdauiDataSelector *iface, gint row);
static void              gdaui_raw_grid_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              gdaui_raw_grid_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

typedef struct {
	GtkCellRenderer   *data_cell;
	GtkCellRenderer   *info_cell;
	GtkTreeViewColumn *column; /* no ref held */

	gboolean           prog_hidden; /* status as requested by the programmer */
	gboolean           hidden; /* real status of the column */
	gchar             *title;

	GdaHolder         *single_param;
	GdauiSetGroup     *group;

	gboolean           info_shown;
	gboolean           data_locked; /* TRUE if no modification allowed on that column */
        gchar             *tooltip_text;
} ColumnData;

#define COLUMN_DATA(x) ((ColumnData *)(x))

static void destroy_column_data (GdauiRawGrid *grid);
static ColumnData *get_column_data_for_group (GdauiRawGrid *grid, GdauiSetGroup *group);
static ColumnData *get_column_data_for_holder (GdauiRawGrid *grid, GdaHolder *holder);
static ColumnData *get_column_data_for_id (GdauiRawGrid *grid, const gchar *id);

typedef struct {
	GdauiRawGridFormatFunc  func;
	gpointer                data;
	GDestroyNotify          dnotify;
} FormattingFuncData;
static FormattingFuncData *formatting_func_new (GdauiRawGridFormatFunc func,
						gpointer data, GDestroyNotify dnotify);
static void                formatting_func_destroy (FormattingFuncData *fd);

typedef struct
{
	GdaDataModel               *data_model;  /* data model provided by set_model() */
	GdaDataModelIter           *iter;        /* iterator for @store, used for its structure */
	GdauiSet                   *iter_info;
	gint                        iter_row;    /* @iter's last row in case of proxy reset */
	gboolean                    reset_soft;  /* tells if proxy rest was "soft" */
	GdauiDataStore             *store;       /* GtkTreeModel interface, using @proxy */
	GdaDataProxy               *proxy;       /* proxy data model, proxying @data_model */

	GSList                     *columns_data; /* list of ColumnData */
	GHashTable                 *columns_hash; /* key = a GtkCellRenderer, value = a #ColumnData (no ref held) */

	GSList                     *reordered_indexes;  /* Indexes of the reordered columns. */

	gboolean                    default_show_info_cell;

	gint                        export_type; /* used by the export dialog */
	GdauiDataProxyWriteMode     write_mode;

	/* store the position of the mouse for popup menu on button press event */
	gint                        bin_x;
	gint                        bin_y;

	GSList                     *formatting_funcs; /* list of #FormattingFuncData structures */
} GdauiRawGridPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiRawGrid, gdaui_raw_grid, GTK_TYPE_TREE_VIEW,
                         G_ADD_PRIVATE (GdauiRawGrid)
                         G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_PROXY, gdaui_raw_grid_widget_init)
                         G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_SELECTOR, gdaui_raw_grid_selector_init))

/* signals */
enum {
	DOUBLE_CLICKED,
	POPULATE_POPUP,
	LAST_SIGNAL
};

static gint gdaui_raw_grid_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum {
	PROP_0,
	PROP_MODEL,
	PROP_XML_LAYOUT,
	PROP_INFO_CELL_VISIBLE
};

/*
 * Real initialization
 */

static gboolean tree_view_event_cb (GtkWidget *treeview, GdkEvent *event, GdauiRawGrid *grid);
static gint     tree_view_popup_button_pressed_cb (GtkWidget *widget, GdkEventButton *event,
						   GdauiRawGrid *grid);

static void     tree_view_selection_changed_cb (GtkTreeSelection *selection, GdauiRawGrid *grid);
static void     tree_view_row_activated_cb     (GtkTreeView *tree_view, GtkTreePath *path,
						GtkTreeViewColumn *column, GdauiRawGrid *grid);

static void
gdaui_raw_grid_widget_init (GdauiDataProxyInterface *iface)
{
	iface->get_proxy = gdaui_raw_grid_get_proxy;
	iface->set_column_editable = gdaui_raw_grid_set_column_editable;
	iface->supports_action = gdaui_raw_grid_supports_action;
	iface->perform_action = gdaui_raw_grid_perform_action;
	iface->set_write_mode = gdaui_raw_grid_widget_set_write_mode;
	iface->get_write_mode = gdaui_raw_grid_widget_get_write_mode;
}

static void
gdaui_raw_grid_selector_init (GdauiDataSelectorInterface *iface)
{
	iface->get_model = gdaui_raw_grid_selector_get_model;
	iface->set_model = gdaui_raw_grid_selector_set_model;
	iface->get_selected_rows = gdaui_raw_grid_selector_get_selected_rows;
	iface->get_data_set = gdaui_raw_grid_selector_get_data_set;
	iface->select_row = gdaui_raw_grid_selector_select_row;
	iface->unselect_row = gdaui_raw_grid_selector_unselect_row;
	iface->set_column_visible = gdaui_raw_grid_selector_set_column_visible;
}

static void
gdaui_raw_grid_class_init (GdauiRawGridClass *klass)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (klass);
	/**
	 * GdauiRawGrid::double-clicked:
	 * @grid: GdauiRawGrid
	 * @row: the row that was double clicked
	 *
	 * Emitted when the user double clicks on a row
	 */
	gdaui_raw_grid_signals[DOUBLE_CLICKED] =
		g_signal_new ("double-clicked",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiRawGridClass, double_clicked),
                              NULL, NULL,
                              _gdaui_marshal_VOID__INT, G_TYPE_NONE,
                              1, G_TYPE_INT);

	/**
	 * GdauiRawGrid::populate-popup:
	 * @grid: GdauiRawGrid
	 * @menu: a #GtkMenu to modify
	 *
	 * Connect this signal and modify the popup menu.
	 */
	gdaui_raw_grid_signals[POPULATE_POPUP] =
		g_signal_new ("populate-popup",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiRawGridClass, populate_popup),
                              NULL, NULL,
                              _gdaui_marshal_VOID__OBJECT, G_TYPE_NONE,
                              1, GTK_TYPE_MENU);

	object_class->dispose = gdaui_raw_grid_dispose;

	/* Properties */
        object_class->set_property = gdaui_raw_grid_set_property;
        object_class->get_property = gdaui_raw_grid_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", _("Data to display"), NULL, GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_XML_LAYOUT,
					 g_param_spec_pointer ("xml-layout",
							       _("Pointer to an XML layout specification (as an xmlNodePtr to a <gdaui_grid> node)"), NULL,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_INFO_CELL_VISIBLE,
                                         g_param_spec_boolean ("info-cell-visible", NULL, _("Info cell visible"), FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static gboolean
gdaui_raw_grid_query_tooltip (GtkWidget   *widget,
			      gint         x,
			      gint         y,
			      gboolean     keyboard_tip,
			      GtkTooltip  *tooltip,
			      G_GNUC_UNUSED gpointer     data)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);

	if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
						keyboard_tip,
						NULL, NULL, NULL))
		return FALSE;

	gint position = 0;
	gint col_x = 0;
	GList *list, *columns = gtk_tree_view_get_columns (tree_view);
	for (list = columns; list; list = list->next) {
		GtkTreeViewColumn *column = list->data;
		if (x >= col_x && x < (col_x + gtk_tree_view_column_get_width (column)))
			break;
		else
			col_x += gtk_tree_view_column_get_width (column);
		++position;
	}
	if (columns)
		g_list_free (columns);
	if (!list)
		return FALSE;

	GdauiRawGrid *grid = GDAUI_RAW_GRID (tree_view);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	ColumnData *cdata = COLUMN_DATA (g_slist_nth (priv->columns_data, position)->data);
	g_return_val_if_fail (cdata, FALSE);

	if (! cdata->tooltip_text)
		return FALSE;

	gtk_tooltip_set_markup (tooltip, cdata->tooltip_text);

	return TRUE;
}

static void
gdaui_raw_grid_init (GdauiRawGrid *grid)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	priv->store = NULL;
	priv->iter = NULL;
	priv->iter_row = -1;
	priv->proxy = NULL;
	priv->default_show_info_cell = FALSE;
	priv->columns_data = NULL;
	priv->columns_hash = g_hash_table_new (NULL, NULL);
	priv->export_type = 1;
	priv->write_mode = GDAUI_DATA_PROXY_WRITE_ON_DEMAND;

	tree_view = GTK_TREE_VIEW (grid);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree_view), TRUE);
	g_signal_connect (G_OBJECT (tree_view), "event",
			  G_CALLBACK (tree_view_event_cb), grid);
	_gdaui_setup_right_click_selection_on_treeview (tree_view);
	g_signal_connect (G_OBJECT (tree_view), "button-press-event",
                          G_CALLBACK (tree_view_popup_button_pressed_cb), grid);
	gtk_tree_view_set_enable_search (tree_view, FALSE);

	/* selection and signal handling */
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (tree_view_selection_changed_cb), grid);
	g_signal_connect (G_OBJECT (tree_view), "row-activated",
			  G_CALLBACK (tree_view_row_activated_cb), grid);

	/* tooltip */
	g_object_set (G_OBJECT (grid), "has-tooltip", TRUE, NULL);
	g_signal_connect (grid, "query-tooltip",
			  G_CALLBACK (gdaui_raw_grid_query_tooltip), NULL);

	priv->formatting_funcs = NULL;
}

/**
 * gdaui_raw_grid_new:
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiRawGrid widget suitable to display the data in @model
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_raw_grid_new (GdaDataModel *model)
{
	GtkWidget *grid;

	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);

	grid = (GtkWidget *) g_object_new (GDAUI_TYPE_RAW_GRID, "model", model, NULL);

	return grid;
}

static void gdaui_raw_grid_clean (GdauiRawGrid *grid);
static void
gdaui_raw_grid_dispose (GObject *object)
{
	GdauiRawGrid *grid;

	g_return_if_fail (GDAUI_IS_RAW_GRID (object));
	grid = GDAUI_RAW_GRID (object);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	gdaui_raw_grid_clean (grid);

	if (priv->formatting_funcs) {
		g_slist_free_full (priv->formatting_funcs, (GDestroyNotify) formatting_func_destroy);
		priv->formatting_funcs = NULL;
	}

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_raw_grid_parent_class)->dispose (object);
}

static void
gdaui_raw_grid_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdauiRawGrid *grid;

	grid = GDAUI_RAW_GRID (object);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *model = (GdaDataModel*) g_value_get_object (value);

			if (model)
				g_return_if_fail (GDA_IS_DATA_MODEL (model));
			else
				return;

			if (priv->proxy) {
				/* data model has been changed */
				if (GDA_IS_DATA_PROXY (model)) {
					/* clean all */
					gdaui_raw_grid_clean (grid);
					g_assert (!priv->proxy);
				}
				else
					g_object_set (G_OBJECT (priv->proxy), "model", model, NULL);
			}

			if (!priv->proxy) {
				/* first time setting */
				if (GDA_IS_DATA_PROXY (model))
					priv->proxy = GDA_DATA_PROXY (g_object_ref (G_OBJECT (model)));
				else
					priv->proxy = GDA_DATA_PROXY (gda_data_proxy_new (model));

				g_signal_connect (priv->proxy, "reset",
						  G_CALLBACK (proxy_reset_pre_cb), grid);
				priv->data_model = gda_data_proxy_get_proxied_model (priv->proxy);

				g_signal_connect (priv->proxy, "sample-changed",
						  G_CALLBACK (proxy_sample_changed_cb), grid);
				g_signal_connect (priv->proxy, "row-updated",
						  G_CALLBACK (proxy_row_updated_cb), grid);
				g_signal_connect (priv->proxy, "reset",
						  G_CALLBACK (proxy_reset_cb), grid);

				priv->iter = gda_data_model_create_iter (GDA_DATA_MODEL (priv->proxy));
				priv->iter_row = -1;
				priv->iter_info = gdaui_set_new (GDA_SET (priv->iter));

				g_signal_connect (priv->iter_info, "public-data-changed",
						  G_CALLBACK (paramlist_public_data_changed_cb), grid);
				g_signal_connect (priv->iter, "holder-attr-changed",
						  G_CALLBACK (paramlist_param_attr_changed_cb), grid);

				g_signal_connect (priv->iter, "row-changed",
						  G_CALLBACK (iter_row_changed_cb), grid);
				g_signal_connect (priv->iter, "validate-set",
						  G_CALLBACK (iter_validate_set_cb), grid);

				gda_data_model_iter_invalidate_contents (priv->iter);

				priv->store = GDAUI_DATA_STORE (gdaui_data_store_new ((GdaDataModel*) priv->proxy));
				gtk_tree_view_set_model ((GtkTreeView *) grid,
							 GTK_TREE_MODEL (priv->store));

				create_columns_data (grid);
				reset_columns_default (grid);

				g_signal_emit_by_name (object, "proxy-changed", priv->proxy);
			}

			break;
		}

		case PROP_XML_LAYOUT: {
			/* node should be a "gdaui_grid" node */
			xmlNodePtr node = g_value_get_pointer (value);
			if (node) {
				g_return_if_fail (node);
				g_return_if_fail (!strcmp ((gchar*) node->name, "gdaui_grid"));
				
				reset_columns_in_xml_layout (grid, node);
			}
			else
				reset_columns_default (grid);
			break;
		}

		case PROP_INFO_CELL_VISIBLE: {
			GSList *list;
			gboolean show = g_value_get_boolean (value);
			priv->default_show_info_cell = show;

			for (list = priv->columns_data; list; list = list->next) {
				COLUMN_DATA (list->data)->info_shown = show;
				g_object_set (G_OBJECT (COLUMN_DATA (list->data)->info_cell), "visible",
					      show, NULL);
			}
			break;
		}

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gdaui_raw_grid_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdauiRawGrid *grid;

	grid = GDAUI_RAW_GRID (object);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	switch (param_id) {
		case PROP_MODEL:
			g_value_set_object (value, priv->data_model);
			break;
		case PROP_INFO_CELL_VISIBLE:
			g_value_set_boolean(value, priv->default_show_info_cell);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/*
 * _gdaui_raw_grid_get_selection
 * @grid: a #GdauiRawGrid widget
 *
 * Returns the list of the currently selected rows in a #GdauiRawGrid widget.
 * The returned value is a list of integers, which represent each of the selected rows.
 *
 * If new rows have been inserted, then those new rows will have a row number equal to -1.
 *
 * Returns: a new list, should be freed (by calling g_list_free) when no longer needed.
 */
GList *
_gdaui_raw_grid_get_selection (GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	GList *selected_rows;

	g_return_val_if_fail (grid && GDAUI_IS_RAW_GRID (grid), NULL);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (selected_rows) {
		GList *list, *retlist = NULL;
		GtkTreeIter iter;
		gint row;

		list = selected_rows;
		for (list = selected_rows; list; list = list->next) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter,
						     (GtkTreePath *)(list->data))) {
				gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
						    GDAUI_DATA_STORE_COL_MODEL_ROW, &row, -1);
				retlist = g_list_prepend (retlist, GINT_TO_POINTER (row));
			}
		}
		g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected_rows);
		return g_list_reverse (retlist);
	}
	else
		return NULL;
}



/*
 * creates a new string where underscores '_' are replaced by double underscores '__'
 * WARNING: the old string is free'ed so it is possible to do "str=double_underscores(str);"
 */
static gchar *
add_double_underscores (gchar *str)
{
        gchar **arr;
        gchar *ret;

        arr = g_strsplit (str, "_", 0);
        ret = g_strjoinv ("__", arr);
        g_strfreev (arr);
	g_free (str);

        return ret;
}

/*
 * Creates a NEW string
 */
static gchar *
remove_double_underscores (gchar *str)
{
        gchar *ptr1, *ptr2, *ret;

	ret = g_new (gchar, strlen (str) + 1);
	for (ptr1 = str, ptr2 = ret; *ptr1; ptr1++, ptr2++) {
		*ptr2 = *ptr1;
		if ((ptr1[0] == '_') && (ptr1[1] == '_'))
			ptr1++;
	}
	*ptr2 = 0;

        return ret;
}

static void     cell_value_set_attributes (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
					   GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiRawGrid *grid);
static void     cell_info_set_attributes  (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
					   GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiRawGrid *grid);

static void     data_cell_value_changed (GtkCellRenderer *renderer, const gchar *path,
					 const GValue *new_value, GdauiRawGrid *grid);
static void     data_cell_values_changed (GtkCellRenderer *renderer, const gchar *path,
					  GSList *new_values, GSList *all_new_values, GdauiRawGrid *grid);
static void     data_cell_status_changed (GtkCellRenderer *renderer, const gchar *path,
					  GdaValueAttribute requested_action, GdauiRawGrid *grid);
static void     treeview_column_clicked_cb (GtkTreeViewColumn *tree_column, GdauiRawGrid *grid);

/*
 * Creates the GtkTreeView's columns, from the priv->store GtkTreeModel
 */
static void
create_columns_data (GdauiRawGrid *grid)
{
	gint i;
	GSList *list;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	/* Creation of the columns in the treeview, to fit the parameters in priv->iter param list */
	for (i = 0, list = gdaui_set_get_groups (priv->iter_info);
	     list;
	     i++, list = list->next) {
		GdaHolder *param;
		GdauiSetGroup *group;
		GtkCellRenderer *renderer;
		ColumnData *cdata;
		GdaSetGroup *sg;

		group = GDAUI_SET_GROUP (list->data);
		sg = gdaui_set_group_get_group (group);
		/* update the list of columns data */
		cdata = get_column_data_for_group (grid, group);
		if (!cdata) {
			cdata = g_new0 (ColumnData, 1);
			cdata->group = group;
			cdata->info_shown = priv->default_show_info_cell;
			cdata->data_locked = FALSE;
			cdata->tooltip_text = NULL;
			priv->columns_data = g_slist_append (priv->columns_data, cdata);
		}

		/* create renderers */
		if (gdaui_set_group_get_source (group)) {
			/* parameters depending on a GdaDataModel */
			gchar *title;			
			/*
			GSList *nodes;

			for (nodes = group->group->nodes; nodes; nodes = nodes->next) {
				if (gda_holder_get_not_null (GDA_HOLDER (GDA_SET_NODE (nodes->data)->holder))) {
					nullok = FALSE;
					break;
				}
			}
			*/

			/* determine title */
			if (gda_set_group_get_n_nodes (sg) == 1)
				title = (gchar *) g_object_get_data (G_OBJECT (gda_set_node_get_holder (gda_set_group_get_node (sg))),
								     "name");
			else
				title = (gchar *) g_object_get_data (G_OBJECT (gda_set_source_get_data_model (gda_set_group_get_source (sg))),
								     "name");

			if (title)
				title = add_double_underscores (g_strdup (title));
			else
				/* FIXME: find a better label */
				title = g_strdup (_("Value"));

			/* FIXME: if nullok is FALSE, then set the column title in bold */
			cdata->tooltip_text = g_strdup (_("Can't be NULL"));
			renderer = gdaui_data_cell_renderer_combo_new (priv->iter_info, gdaui_set_group_get_source (group));
			cdata->data_cell = GTK_CELL_RENDERER (g_object_ref_sink ((GObject*) renderer));
			g_hash_table_insert (priv->columns_hash, renderer, cdata);
			g_free (cdata->title);
			cdata->title = title;

			g_signal_connect (G_OBJECT (renderer), "changed",
					  G_CALLBACK (data_cell_values_changed), grid);
		}
		else {
			/* single direct parameter */
			GType g_type;
			const gchar *plugin = NULL;
			gchar *title;
			gint model_col;

			param = gda_set_node_get_holder (gda_set_group_get_node (sg));
			cdata->single_param = param;
			g_type = gda_holder_get_g_type (param);

			if (gda_holder_get_not_null (param))
				cdata->tooltip_text = g_strdup (_("Can't be NULL"));

			g_object_get (G_OBJECT (param), "name", &title, NULL);
			if (title && *title)
				title = add_double_underscores (title);
			else
				title = NULL;
			if (!title)
				title = g_strdup (_("No title"));

			g_object_get (param, "plugin", &plugin, NULL);
			renderer = _gdaui_new_cell_renderer (g_type, plugin);
			cdata->data_cell = GTK_CELL_RENDERER (g_object_ref_sink ((GObject*) renderer));
			g_hash_table_insert (priv->columns_hash, renderer, cdata);
			g_free (cdata->title);
			cdata->title = title;

			model_col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter), param);
			g_object_set_data (G_OBJECT (renderer), "model_col", GINT_TO_POINTER (model_col));

			g_signal_connect (G_OBJECT (renderer), "changed",
					  G_CALLBACK (data_cell_value_changed), grid);
		}

		/* warning: when making modifications to renderer's attributes or signal connect,
		 * also make the same modifications to paramlist_param_attr_changed_cb() */

		/* settings and signals */
		g_object_set (G_OBJECT (renderer), "editable", !cdata->data_locked, NULL);
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), "set-default-if-invalid"))
			g_object_set (G_OBJECT (renderer), "set-default-if-invalid", TRUE, NULL);

		/* Adding the GValue's information cell as another GtkCellRenderer */
		renderer = gdaui_data_cell_renderer_info_new (priv->store, priv->iter, group);
		cdata->info_cell = GTK_CELL_RENDERER (g_object_ref_sink ((GObject*) renderer));
		g_hash_table_insert (priv->columns_hash, renderer, cdata);
		g_signal_connect (G_OBJECT (renderer), "status-changed",
				  G_CALLBACK (data_cell_status_changed), grid);
		g_object_set (G_OBJECT (renderer), "visible", cdata->info_shown, NULL);
	}
}

/* keep track of the columns shown and hidden */
static void
column_visibility_changed (GtkTreeViewColumn *column, G_GNUC_UNUSED GParamSpec *pspec, ColumnData *cdata)
{
	cdata->hidden = !gtk_tree_view_column_get_visible (column);
}

/*
 * Remove all columns from @grid, does not modify ColumnData
 */
static void
remove_all_columns (GdauiRawGrid *grid)
{
	GList *columns, *list;
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
	for (list = columns; list; list = list->next)
		gtk_tree_view_remove_column (GTK_TREE_VIEW (grid),
					     (GtkTreeViewColumn*) (list->data));
	if (columns)
		g_list_free (columns);
}

/*
 * Create a treeview column and places it at posision @pos, or las last position
 * if pos < 0
 */
static void
create_tree_view_column (GdauiRawGrid *grid, ColumnData *cdata, gint pos)
{
	GtkTreeViewColumn *column;
	gint n;
	n = gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (grid), pos,
							cdata->title,
							cdata->data_cell,
							(GtkTreeCellDataFunc) cell_value_set_attributes,
							grid, NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (grid), pos >= 0 ? pos : n-1);
	cdata->column = column;
	g_object_set_data (G_OBJECT (column), "data_renderer", cdata->data_cell);
	g_object_set_data (G_OBJECT (column), "__gdaui_group", cdata->group);
	
	gtk_tree_view_column_pack_end (column, cdata->info_cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cdata->info_cell,
						 (GtkTreeCellDataFunc) cell_info_set_attributes,
						 grid, NULL);
	
	g_signal_connect (column, "notify::visible",
			  G_CALLBACK (column_visibility_changed), cdata);

	/* Sorting data */
	gtk_tree_view_column_set_clickable (column, TRUE);
	g_signal_connect (G_OBJECT (column), "clicked",
			  G_CALLBACK (treeview_column_clicked_cb), grid);
}

static void
reset_columns_default (GdauiRawGrid *grid)
{
	GSList *list;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	remove_all_columns (grid);
	for (list = priv->columns_data; list; list = list->next) {
		ColumnData *cdata = COLUMN_DATA (list->data);
		if (cdata->prog_hidden)
			continue;

		/* create treeview column */
		create_tree_view_column (grid, cdata, -1);
	}
}

static void
reset_columns_in_xml_layout (GdauiRawGrid *grid, xmlNodePtr grid_node)
{
	remove_all_columns (grid);

	xmlNodePtr child;
	for (child = grid_node->children; child; child = child->next) {
		if (child->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrEqual (child->name, BAD_CAST "gdaui_entry")) {
			xmlChar *name;
			name = xmlGetProp (child, BAD_CAST "name");
			if (!name)
				continue;

			/* find column data */
			ColumnData *cdata;
			cdata = get_column_data_for_id (grid, (gchar*) name);
			if (! cdata) {
				g_warning ("Could not find column named '%s', ignoring",
					   (gchar*) name);
				xmlFree (name);
				continue;
			}
			xmlFree (name);

			/* create treeview column */
			cdata->prog_hidden = FALSE;
			cdata->hidden = FALSE;
			create_tree_view_column (grid, cdata, -1);

			/* plugin */
			xmlChar *plugin;
			plugin = xmlGetProp (child, BAD_CAST "plugin");
			if (plugin && cdata->single_param) {
				g_object_set (cdata->single_param, "plugin", plugin, NULL);
			}
			if (plugin)
				xmlFree (plugin);

			/* column label */
			xmlChar *prop;
			prop = xmlGetProp (child, BAD_CAST "label");
			if (prop) {
				g_free (cdata->title);
				cdata->title = g_strdup ((gchar*) prop);
				xmlFree (prop);
				gtk_tree_view_column_set_title (cdata->column, cdata->title);
			}
		}
	}
}

static gboolean
remove_formatting_function (GdauiRawGrid *grid, GdauiRawGridFormatFunc func)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	if (priv->formatting_funcs) {
		GSList *list;
		for (list = priv->formatting_funcs; list; list = list->next) {
			FormattingFuncData *fd = (FormattingFuncData*) list->data;
			if (fd->func == func) {
				priv->formatting_funcs = g_slist_remove (priv->formatting_funcs, fd);
				formatting_func_destroy (fd);				
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 * gdaui_raw_grid_add_formatting_function:
 * @grid: a #GdauiRawGrid widget
 * @func: a #GdauiRawGridFormatFunc function pointer
 * @data: (nullable): a pointer to pass to the @func function when called
 * @dnotify: (nullable): destroy notifier for @data
 *
 * This function allows you to specify that the @func function needs to be called
 * whenever the rendering of a cell in @grid needs to be done. It is similar in purpose
 * to the gtk_tree_view_column_set_cell_data_func() function.
 *
 * Since: 5.0.3
 */
void
gdaui_raw_grid_add_formatting_function (GdauiRawGrid *grid, GdauiRawGridFormatFunc func,
					gpointer data, GDestroyNotify dnotify)
{
	g_return_if_fail (GDAUI_IS_RAW_GRID (grid));
	g_return_if_fail (func);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	/* remove existing function */
	remove_formatting_function (grid, func);
	
	/* add new one */
	FormattingFuncData *fd;
	fd = formatting_func_new (func, data, dnotify);
	priv->formatting_funcs = g_slist_append (priv->formatting_funcs, fd);

	/* redraw grid to take new function into account */
	TO_IMPLEMENT;
}

/**
 * gdaui_raw_grid_remove_formatting_function:
 * @grid: a #GdauiRawGrid widget
 * @func: (scope notified): a #GdauiRawGridFormatFunc function pointer
 *
 * This function undoes what has been specified before by gdaui_raw_grid_add_formatting_function()
 *
 * Since: 5.0.3
 */
void
gdaui_raw_grid_remove_formatting_function (GdauiRawGrid *grid, GdauiRawGridFormatFunc func)
{
	g_return_if_fail (GDAUI_IS_RAW_GRID (grid));
	g_return_if_fail (func);

	remove_formatting_function (grid, func);

	/* redraw grid to take new function into account */
	TO_IMPLEMENT;
}

static FormattingFuncData *
formatting_func_new (GdauiRawGridFormatFunc func, gpointer data, GDestroyNotify dnotify)
{
	FormattingFuncData *fd;
	fd = g_new0 (FormattingFuncData, 1);
	fd->func = func;
	fd->data = data;
	fd->dnotify = dnotify;
	return fd;
}

static void
formatting_func_destroy (FormattingFuncData *fd)
{
	if (fd->dnotify)
		fd->dnotify (fd->data);
	g_free (fd);
}

/*
 * Set the attributes for each cell renderer which is not the information cell renderer,
 * called by each cell renderer before actually displaying anything.
 */
static void
cell_value_set_attributes (G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
			   GtkCellRenderer *cell,
			   G_GNUC_UNUSED GtkTreeModel *tree_model,
			   GtkTreeIter *iter, GdauiRawGrid *grid)
{
	GdauiSetGroup *group;
	GdaSetGroup *sg;
		guint attributes;
	gboolean to_be_deleted = FALSE;
	ColumnData *cdata;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	cdata = g_hash_table_lookup (priv->columns_hash, cell);
	if (!cdata) {
		g_warning ("Internal error: missing column data");
		return;
	}
	group = cdata->group;
	sg = gdaui_set_group_get_group (group);

	if (gda_set_group_get_source (sg)) {
		/* parameters depending on a GdaDataModel */
		GList *values = NULL;

		/* NOTE:
		 * For performances reasons we want to provide, if possible, all the values required by the combo cell
		 * renderer to draw whatever it wants, without further making it search for the values it wants in
		 * source->data_model.
		 *
		 * GdaSetSource *source;
		 * source = group->group->nodes_source;
		 *
		 * For this purpose, we try to get a complete list of values making one row of the node->data_for_params
		 * data model, so that the combo cell renderer has all the values it wants.
		 *
		 * NOTE2:
		 * This optimization is required anyway when source->data_model can be changed depending on
		 * external events and we can't know when it has changed.
		 */
		attributes = _gdaui_utility_proxy_compute_attributes_for_group (group,
										priv->store,
										priv->iter,
										iter, &to_be_deleted);
		values = _gdaui_utility_proxy_compute_values_for_group (group, priv->store,
									priv->iter, iter,
									TRUE);
		if (values) {
			g_object_set (G_OBJECT (cell),
				      "values-display", values,
				      "value-attributes", attributes,
				      "editable",
				      !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "show-expander",
				      !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "cell-background", GDAUI_COLOR_NORMAL_MODIF,
				      "cell_background-set",
				      ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
				      "to-be-deleted", to_be_deleted,
				      NULL);
		}
		else {
			values = _gdaui_utility_proxy_compute_values_for_group (group, priv->store,
										priv->iter, iter, FALSE);
			g_object_set (G_OBJECT (cell),
				      "values-display", values,
				      "value-attributes", attributes,
				      "editable",
				      !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "show-expander",
				      !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "cell-background", GDAUI_COLOR_NORMAL_MODIF,
				      "cell_background-set",
				      ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
				      "to-be-deleted", to_be_deleted,
				      NULL);
		}

		if (values)
			g_list_free (values);
	}
	else {
		/* single direct parameter */
		gint col;
		gint offset;
		GValue *value;

		offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (priv->proxy));

		g_assert (gda_set_group_get_n_nodes (sg) == 1);
		col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
				     gda_set_node_get_holder (gda_set_group_get_node (sg)));
		gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter,
				    GDAUI_DATA_STORE_COL_TO_DELETE, &to_be_deleted,
				    col, &value,
				    offset + col, &attributes, -1);
		g_object_set (G_OBJECT (cell),
			      "value-attributes", attributes,
			      "value", value,
			      "editable",
			      !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "cell-background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set",
			      ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to-be-deleted", to_be_deleted,
			      NULL);
	}

	if (priv->formatting_funcs) {
		gint row, col_index;
		GSList *list;

		col_index = g_slist_index (priv->columns_data, cdata);
		gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter,
				    GDAUI_DATA_STORE_COL_MODEL_ROW, &row, -1);
		for (list = priv->formatting_funcs; list; list = list->next) {
			FormattingFuncData *fd = (FormattingFuncData*) list->data;
			fd->func (cell, cdata->column, col_index, (GdaDataModel*) priv->proxy, row, fd->data);
		}
	}
}

/*
 * Set the attributes for each information cell renderer,
 * called by each cell renderer before actually displaying anything.
 */
static void
cell_info_set_attributes (GtkTreeViewColumn *tree_column,
			  GtkCellRenderer *cell,
			  G_GNUC_UNUSED GtkTreeModel *tree_model,
			  GtkTreeIter *iter, GdauiRawGrid *grid)
{
	GdauiSetGroup *group;
	GdaSetGroup *sg;
	guint attributes;
	gboolean to_be_deleted = FALSE;
	ColumnData *cdata;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	cdata = g_hash_table_lookup (priv->columns_hash, cell);
	if (!cdata) {
		g_warning ("Missing column data");
		return;
	}
	group = cdata->group;
	sg = gdaui_set_group_get_group (group);

	if (gda_set_group_get_source (sg)) {
		/* parameters depending on a GdaDataModel */
		attributes = _gdaui_utility_proxy_compute_attributes_for_group (group, priv->store,
										priv->iter,
										iter, &to_be_deleted);
		g_object_set (G_OBJECT (cell),
			      "editable", !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "value-attributes", attributes,
			      "cell-background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set", ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to-be-deleted", to_be_deleted,
			      "visible", cdata->info_shown,
			      NULL);
	}
	else {
		/* single direct parameter */
		gint col;
		gint offset;
		gint row;

		offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (priv->proxy));

		g_assert (gda_set_group_get_n_nodes (sg) == 1);
		col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
				     gda_set_node_get_holder (gda_set_group_get_node (sg)));
		gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter,
				    GDAUI_DATA_STORE_COL_TO_DELETE, &to_be_deleted,
				    GDAUI_DATA_STORE_COL_MODEL_ROW, &row,
				    offset + col, &attributes, -1);
		g_object_set (G_OBJECT (cell),
			      "editable", !cdata->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "value-attributes", attributes,
			      "cell-background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set", ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to-be-deleted", to_be_deleted,
			      "visible", cdata->info_shown,
			      NULL);
	}
}

static gboolean set_iter_from_path (GdauiRawGrid *grid, const gchar *path, GtkTreeIter *iter);

/*
 * Callback when a value displayed as a GtkCellRenderer has been changed, for single parameter cell renderers
 *
 * Apply the new_value to the GTkTreeModel (priv->store)
 */
static void
data_cell_value_changed (GtkCellRenderer *renderer, const gchar *path, const GValue *new_value, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GdaSetGroup *sg;
	ColumnData *cdata;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	cdata = g_hash_table_lookup (priv->columns_hash, renderer);
	g_assert (cdata);
	sg = gdaui_set_group_get_group (cdata->group);
	g_assert (gda_set_group_get_n_nodes (sg) == 1);

	if (set_iter_from_path (grid, path, &iter)) {
		gint col;
		col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
				     gda_set_node_get_holder (gda_set_group_get_node (sg)));
		gdaui_data_store_set_value (priv->store, &iter, col, new_value);
	}
}


/*
 * Callback when a value displayed as a GdauiDataCellRendererCombo has been changed.
 *
 * Apply the new_value to the GTkTreeModel (priv->store)
 */
static void
data_cell_values_changed (GtkCellRenderer *renderer, const gchar *path,
			  GSList *new_values, G_GNUC_UNUSED GSList *all_new_values, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GdauiSetGroup *group;
	GdaSetGroup *sg;
	ColumnData *cdata;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	cdata = g_hash_table_lookup (priv->columns_hash, renderer);
	g_assert (cdata);
	group = cdata->group;
	sg = gdaui_set_group_get_group (group);
	g_assert (gda_set_group_get_source (sg));

	if (new_values)
		g_return_if_fail (gda_set_group_get_n_nodes (sg) == (gint) g_slist_length (new_values));
	else
		/* the reason for not having any value is that the GdauiDataCellRendererCombo had no selected item */
		return;

	if (set_iter_from_path (grid, path, &iter)) {
		GSList *list, *params;
		gint col;

		/* update the GdauiDataStore */
		for (params = gda_set_group_get_nodes (sg), list = new_values;
		     list;
		     params = params->next, list = list->next) {
			col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
					     gda_set_node_get_holder (GDA_SET_NODE (params->data)));
			gdaui_data_store_set_value (priv->store, &iter, col, (GValue *)(list->data));
		}

#ifdef PROXY_STORE_EXTRA_VALUES
		/* call gda_data_proxy_set_model_row_value() */
		gint proxy_row;
		proxy_row = gdaui_data_store_get_row_from_iter (priv->store, &iter);

		gint i;
		for (i = 0; i < gdaui_set_source_get_shown_n_cols (gdaui_set_group_get_source (group)); i++) {
			GValue *value;

			col = group->nodes_source->shown_cols_index[i];
			value = (GValue *) g_slist_nth_data (all_new_values, col);
			gda_data_proxy_set_model_row_value (priv->proxy,
							    gda_set_source_get_data_model (gda_set_group_get_source (sg)),
							    proxy_row, col, value);
		}
#endif
	}
}

static gboolean
set_iter_from_path (GdauiRawGrid *grid, const gchar *path, GtkTreeIter *iter)
{
	GtkTreePath *treepath;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	g_assert (path);

	treepath = gtk_tree_path_new_from_string (path);
	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), iter, treepath)) {
		gtk_tree_path_free (treepath);
		g_warning ("Can't get iter for path %s", path);
		return FALSE;
	}
	gtk_tree_path_free (treepath);

	return TRUE;
}

static void
treeview_column_clicked_cb (GtkTreeViewColumn *tree_column, GdauiRawGrid *grid)
{
	GdauiSetGroup *group;
	GdaSetGroup *sg;
	GSList *nodes;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	group = g_object_get_data (G_OBJECT (tree_column), "__gdaui_group");
	g_assert (group);
	sg = gdaui_set_group_get_group (group);

	for (nodes = gda_set_group_get_nodes (sg); nodes; nodes = nodes->next) {
		GdaHolder *param = gda_set_node_get_holder (GDA_SET_NODE (nodes->data));
		gint pos;
		g_assert (param);

		pos = g_slist_index (gda_set_get_holders (GDA_SET (priv->iter)), param);
		if (pos >= 0) {
			gda_data_proxy_set_ordering_column (priv->proxy, pos, NULL);
			break;
		}
	}
}

/*
 * Actions
 */
static void
data_cell_status_changed (GtkCellRenderer *renderer, const gchar *path, GdaValueAttribute requested_action,
			  GdauiRawGrid *grid)
{
	GtkTreePath *treepath;
	GtkTreeIter iter;
	GdauiSetGroup *group;
	GdaSetGroup *sg;
	GtkTreeModel *tree_model;
	gint col;
	gint offset;
	GValue *attribute;
	ColumnData *cdata;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	cdata = g_hash_table_lookup (priv->columns_hash, renderer);
	g_assert (cdata);
	group = cdata->group;
	sg = gdaui_set_group_get_group (group);

	offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (priv->proxy));

	tree_model = GTK_TREE_MODEL (priv->store);

	treepath = gtk_tree_path_new_from_string (path);
	if (! gtk_tree_model_get_iter (tree_model, &iter, treepath)) {
		gtk_tree_path_free (treepath);
		g_warning ("Can't get iter for path %s", path);
		return;
	}
	gtk_tree_path_free (treepath);

	g_value_set_uint (attribute = gda_value_new (G_TYPE_UINT), requested_action);
	if (gda_set_group_get_source (sg)) {
		/* parameters depending on a GdaDataModel */
		GSList *list;

		for (list = gda_set_group_get_nodes (sg); list; list = list->next) {
			col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
					     gda_set_node_get_holder (GDA_SET_NODE (list->data)));
			gdaui_data_store_set_value (priv->store, &iter, offset + col, attribute);
		}

#ifdef PROXY_STORE_EXTRA_VALUES
		gint proxy_row;
		proxy_row = gdaui_data_store_get_row_from_iter (priv->store, &iter);

		/* call gda_data_proxy_set_model_row_value() */
		gint i;
		GdauiSetSource gs;
		gs = gdaui_set_group_get_source (group);
		for (i = 0; i < gdaui_set_source_get_shown_n_cols (gs); i++) {
			col = (gdaui_set_source_get_shown_columns (gs))[i];

			if (requested_action & GDA_VALUE_ATTR_IS_NULL)
				gda_data_proxy_set_model_row_value (priv->proxy,
								    group->nodes_source->data_model,
								    proxy_row, col, NULL);
			else {
				if (requested_action & GDA_VALUE_ATTR_IS_UNCHANGED)
					gda_data_proxy_clear_model_row_value (priv->proxy,
									      group->nodes_source->data_model,
									      proxy_row, col);
				else {
					if (requested_action & GDA_VALUE_ATTR_IS_DEFAULT) {
						TO_IMPLEMENT;
					}
					else
						TO_IMPLEMENT;
				}
			}
		}
#endif
	}
	else {
		/* single direct parameter */
		gint col;

		g_assert (gda_set_group_get_n_nodes (sg) == 1);
		col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
				     gda_set_node_get_holder (gda_set_group_get_node (sg)));
		gdaui_data_store_set_value (priv->store, &iter, offset + col, attribute);
	}
	gda_value_free (attribute);
}

/*
 * Catch any event in the GtkTreeView widget
 */
static gboolean
tree_view_event_cb (GtkWidget *treeview, GdkEvent *event, GdauiRawGrid *grid)
{
	gboolean done = FALSE;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey *ekey = (GdkEventKey *) event;
		guint modifiers = gtk_accelerator_get_default_mod_mask ();

		/* Tab to move one column left or right */
		if (ekey->keyval == GDK_KEY_Tab) {
			GtkTreeViewColumn *column;
			GtkTreePath *path;

			/* FIXME: if a column is currently edited, then make sure the editing of that cell is not canceled */
			gtk_tree_view_get_cursor (GTK_TREE_VIEW (treeview), &path, &column);
			if (column && path) {
				GList *columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (treeview));
				GList *col;
				GtkCellRenderer *renderer;

				/* change column */
				col = g_list_find (columns, column);
				g_return_val_if_fail (col, FALSE);

				if (((ekey->state & modifiers) == GDK_SHIFT_MASK) || ((ekey->state & modifiers) == GDK_CONTROL_MASK))
					col = g_list_previous (col); /* going to previous column */
				else
					col = g_list_next (col); /* going to next column */

				if (col) {
					renderer = g_object_get_data (G_OBJECT (col->data), "data_renderer");
					gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (treeview), path,
									  GTK_TREE_VIEW_COLUMN (col->data),
									  renderer, FALSE);
					gtk_widget_grab_focus (treeview);
					done = TRUE;
				}
				g_list_free (columns);
			}
			if (path)
				gtk_tree_path_free (path);
		}

		/* DELETE to delete the selected row */
		if (ekey->keyval == GDK_KEY_Delete) {
			GtkTreeIter iter;
			GtkTreeSelection *selection;
			GtkTreeModel *model;
			GList *sel_rows, *cur_row;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
			sel_rows = gtk_tree_selection_get_selected_rows (selection, &model);
			cur_row = sel_rows;
			while (cur_row) {
				gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (cur_row->data));
				if (((ekey->state & modifiers) == GDK_SHIFT_MASK) ||
				    ((ekey->state & modifiers) == GDK_CONTROL_MASK))
					gdaui_data_store_undelete (priv->store, &iter);
				else
					gdaui_data_store_delete (priv->store, &iter);
				cur_row = g_list_next (cur_row);
			}
			g_list_free_full (sel_rows, (GDestroyNotify) gtk_tree_path_free);

			done = TRUE;
		}
	}

	return done;
}

static void menu_select_all_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_unselect_all_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_show_columns_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_save_as_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_copy_row_cb (GtkWidget *widget, GdauiRawGrid *grid);
static GtkWidget *new_menu_item (const gchar *label,
				 GCallback cb_func,
				 gpointer user_data);
static GtkWidget *new_check_menu_item (const gchar *label,
				       gboolean active,
				       GCallback cb_func,
				       gpointer user_data);

static void
hidden_column_mitem_toggled_cb (GtkCheckMenuItem *check, G_GNUC_UNUSED GdauiRawGrid *grid)
{
	ColumnData *cdata;
	gboolean act;
	cdata = g_object_get_data (G_OBJECT (check), "c");
	g_assert (cdata);
	act = gtk_check_menu_item_get_active (check);
	gtk_tree_view_column_set_visible (cdata->column, act);
}

static gboolean
tree_view_popup_button_pressed_cb (G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, GdauiRawGrid *grid)
{
	GtkWidget *menu, *submenu;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkSelectionMode sel_mode;
	GSList *list;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	if (event->button != 3)
		return FALSE;

	tree_view = GTK_TREE_VIEW (grid);
	if (event->window != gtk_tree_view_get_bin_window (tree_view))
		return FALSE;

	selection = gtk_tree_view_get_selection (tree_view);
	sel_mode = gtk_tree_selection_get_mode (selection);

	priv->bin_x = (gint) event->x;
	priv->bin_y = (gint) event->y;

	/* create the menu */
	menu = gtk_menu_new ();

	/* shown columns */
	guint nentries = 0;
	for (list = priv->columns_data; list; list = list->next) {
		ColumnData *cdata = (ColumnData*) list->data;
		if (! cdata->prog_hidden)
			nentries ++;
	}
	if (nentries > 1) {
		GtkWidget *mitem;
		mitem = gtk_menu_item_new_with_label (_("Shown columns"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
		gtk_widget_show (mitem);

		submenu = gtk_menu_new ();
		gtk_widget_show (submenu);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), submenu);
		for (list = priv->columns_data; list; list = list->next) {
			ColumnData *cdata = (ColumnData*) list->data;
			if (cdata->prog_hidden)
				continue;

			gchar *tmp;
			tmp = remove_double_underscores (cdata->title);
			mitem = gtk_check_menu_item_new_with_label (tmp);
			g_free (tmp);
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), !cdata->hidden);
			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mitem);
			gtk_widget_show (mitem);

			g_object_set_data (G_OBJECT (mitem), "c", cdata);
			g_signal_connect (mitem, "toggled",
					  G_CALLBACK (hidden_column_mitem_toggled_cb), grid);
		}
	}

	gtk_menu_shell_append (GTK_MENU_SHELL (menu),
			       new_menu_item (_("_Copy"),
					      G_CALLBACK (menu_copy_row_cb), grid));
	

	if (sel_mode == GTK_SELECTION_MULTIPLE)
		gtk_menu_shell_append (GTK_MENU_SHELL (menu),
				       new_menu_item (_("Select _All"),
						      G_CALLBACK (menu_select_all_cb), grid));
	
	if ((sel_mode == GTK_SELECTION_SINGLE) || (sel_mode == GTK_SELECTION_MULTIPLE))
		gtk_menu_shell_append (GTK_MENU_SHELL (menu),
				       new_menu_item (_("_Clear Selection"),
						      G_CALLBACK (menu_unselect_all_cb), grid));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu),
			       new_check_menu_item (_("Show Column _Titles"),
						    gtk_tree_view_get_headers_visible (tree_view),
						    G_CALLBACK (menu_show_columns_cb), grid));
	
	if (sel_mode != GTK_SELECTION_NONE) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), new_menu_item (_("Save _As"),
									     G_CALLBACK (menu_save_as_cb),
									     grid));
	}

	/* allow listeners to add their custom menu items */
	g_signal_emit (G_OBJECT (grid), gdaui_raw_grid_signals [POPULATE_POPUP], 0, GTK_MENU (menu));
	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
	gtk_widget_show_all (menu);

	return TRUE;
}

static GtkWidget *
new_menu_item (const gchar *label,
	       GCallback cb_func,
	       gpointer user_data)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic (label);

	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (cb_func), user_data);

	return item;
}

static GtkWidget *
new_check_menu_item (const gchar *label,
		     gboolean active,
		     GCallback cb_func,
		     gpointer user_data)
{
	GtkWidget *item;

	item = gtk_check_menu_item_new_with_mnemonic (label);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), active);

	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (cb_func), user_data);

	return item;
}

static void
menu_select_all_cb (G_GNUC_UNUSED GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));

	gtk_tree_selection_select_all (selection);
}

static void
menu_unselect_all_cb (G_GNUC_UNUSED GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));

	gtk_tree_selection_unselect_all (selection);
}

static void
menu_copy_row_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkClipboard *cp;
	GtkTreeViewColumn *column;
	GList *columns;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	cp = gtk_clipboard_get (gdk_atom_intern_static_string ("CLIPBOARD"));
	if (!cp)
		return;

	if (! gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (grid),
					     priv->bin_x, priv->bin_y, &path,
					     &column, NULL, NULL))
		return;

	model = GTK_TREE_MODEL (priv->store);
	if (! gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);

	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
	const GValue *cvalue;
	gboolean cpset = TRUE;
	gtk_tree_model_get (model, &iter, g_list_index (columns, column), &cvalue, -1);
	g_list_free (columns);
	if (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)
		gtk_clipboard_set_text (cp, "", -1);
	else if ((G_VALUE_TYPE (cvalue) == GDA_TYPE_BINARY) ||
		 (G_VALUE_TYPE (cvalue) == GDA_TYPE_BLOB)) {
		GdaBinary *bin;
		
		cpset = FALSE;
		if (G_VALUE_TYPE (cvalue) == GDA_TYPE_BINARY)
			bin = g_value_get_boxed (cvalue);
		else {
			GdaBlob *blob;
			blob = (GdaBlob *) g_value_get_boxed ((GValue *) cvalue);
			g_assert (blob);
			bin = gda_blob_get_binary (blob);
			if (gda_blob_get_op (blob) &&
			    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
				gda_blob_op_read_all (gda_blob_get_op (blob), blob);
		}
		if (bin) {
			GdkPixbufLoader *loader;
			GdkPixbuf *pixbuf = NULL;
			loader = gdk_pixbuf_loader_new ();
			if (gdk_pixbuf_loader_write (loader, gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
				if (gdk_pixbuf_loader_close (loader, NULL)) {
					pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
					g_object_ref (pixbuf);
				}
				else
					gdk_pixbuf_loader_close (loader, NULL);
			}
			else
				gdk_pixbuf_loader_close (loader, NULL);
			g_object_unref (loader);
			
			if (pixbuf) {
				gtk_clipboard_set_image (cp, pixbuf);
				g_object_unref (pixbuf);
				cpset = TRUE;
			}
		}
	}
	else
		cpset = FALSE;

	if (!cpset) {
		gchar *str;
		GdaDataHandler *dh;
		dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
		if (dh)
			str = gda_data_handler_get_str_from_value (dh, cvalue);
		else
			str = gda_value_stringify (cvalue);
		gtk_clipboard_set_text (cp, str, -1);
		g_free (str);
	}
}

static void
menu_show_columns_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkCheckMenuItem *item;

	item = (GtkCheckMenuItem *) widget;

	g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (item));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (grid),
					   gtk_check_menu_item_get_active (item));
}

static void export_type_changed_cb (GtkComboBox *types, GtkWidget *dialog);
static void save_as_response_cb (GtkDialog *dialog, gint response_id, GdauiRawGrid *grid);
static void
menu_save_as_cb (G_GNUC_UNUSED GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *filename;
	GtkWidget *types, *scope;
	GtkWidget *hbox, *grid1, *check, *dbox;
	char *str;
	GtkTreeSelection *sel;
	gint selrows;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	/* create dialog box */
	dialog = gtk_dialog_new_with_buttons (_("Saving Data"),
					      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (grid))), 0,
					      _("_Cancel"), GTK_RESPONSE_CANCEL,
					      _("_Save"), GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	str = g_strdup_printf ("<big><b>%s:</b></big>\n%s", _("Saving data to a file"),
			       _("The data will be exported to the selected file."));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_free (str);

	dbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (dbox), label, FALSE, TRUE, 2);

	str = g_strdup_printf ("<b>%s:</b>", _("File name"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (dbox), label, FALSE, TRUE, 2);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
	gtk_box_pack_start (GTK_BOX (dbox), hbox, TRUE, TRUE, 5);
	gtk_widget_show (hbox);
	label = gtk_label_new ("    ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	filename = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	g_object_set_data (G_OBJECT (dialog), "filename", filename);
	gtk_box_pack_start (GTK_BOX (hbox), filename, TRUE, TRUE, 0);
	gtk_widget_show (filename);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filename),
					     gdaui_get_default_path ());

	str = g_strdup_printf ("<b>%s:</b>", _("Details"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	g_free (str);
	gtk_box_pack_start (GTK_BOX (dbox), label, FALSE, TRUE, 2);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
	gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show (hbox);
	label = gtk_label_new ("    ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	grid1 = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid1), 5);
	gtk_grid_set_column_spacing (GTK_GRID (grid1), 5);
	gtk_box_pack_start (GTK_BOX (hbox), grid1, TRUE, TRUE, 0);
	gtk_widget_show (grid1);

	/* file type */
	label = gtk_label_new (_("File type:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid1), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	types = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (grid1), types, 1, 0, 1, 1);
	gtk_widget_show (types);
	g_object_set_data (G_OBJECT (dialog), "types", types);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (types), _("Tab-delimited"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (types), _("Comma-delimited"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (types), _("XML"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (types), priv->export_type);

	g_signal_connect (types, "changed",
			  G_CALLBACK (export_type_changed_cb), dialog);

	/* data scope */
	label = gtk_label_new (_("Data to save:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid1), label, 0, 1, 1, 1);
	gtk_widget_show (label);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	selrows = gtk_tree_selection_count_selected_rows (sel);
	if (selrows <= 0)
		gtk_widget_set_sensitive (label, FALSE);

	scope = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (grid1), scope, 1, 1, 1, 1);
	gtk_widget_show (scope);
	g_object_set_data (G_OBJECT (dialog), "scope", scope);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (scope), _("All data (without any local modification)"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (scope), _("Only displayed data"));
	if (selrows > 0)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (scope), _("Only selected data"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (scope), 0);

	/* other options */
	GtkWidget *exp;
	exp = gtk_expander_new (_("Other options"));
	gtk_grid_attach (GTK_GRID (grid1), exp, 0, 2, 2, 1);

	GtkWidget *grid2;
	grid2 = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (exp), grid2);
	
	label = gtk_label_new (_("Empty string when NULL?"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid2), label, 0, 0, 1, 1);
	gtk_widget_set_tooltip_text (label, _("Export NULL values as an empty \"\" string"));

	check = gtk_check_button_new ();
	gtk_grid_attach (GTK_GRID (grid2), check, 1, 0, 1, 1);
	g_object_set_data (G_OBJECT (dialog), "null_as_empty", check);
	gtk_widget_set_tooltip_text (check, _("Export NULL values as an empty \"\" string"));

	label = gtk_label_new (_("Invalid data as NULL?"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid2), label, 2, 0, 1, 1);
	gtk_widget_set_tooltip_text (label, _("Don't export invalid data,\nbut export a NULL value instead"));

	check = gtk_check_button_new ();
	gtk_grid_attach (GTK_GRID (grid2), check, 3, 0, 1, 1);
	g_object_set_data (G_OBJECT (dialog), "invalid_as_null", check);
	gtk_widget_set_tooltip_text (check, _("Don't export invalid data,\nbut export a NULL value instead"));

	label = gtk_label_new (_("Field names on first row?"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid2), label, 0, 1, 1, 1);
	gtk_widget_set_tooltip_text (label, _("Add a row at beginning with columns names"));

	check = gtk_check_button_new ();
	gtk_grid_attach (GTK_GRID (grid2), check, 1, 1, 1, 1);
	g_object_set_data (G_OBJECT (dialog), "first_row", check);
	gtk_widget_set_tooltip_text (check, _("Add a row at beginning with columns names"));

	export_type_changed_cb (GTK_COMBO_BOX (types), dialog);

	/* run the dialog */
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (save_as_response_cb), grid);
	gtk_widget_show_all (dialog);
}

static void
export_type_changed_cb (GtkComboBox *types, GtkWidget *dialog)
{
	gboolean is_cvs = TRUE;
	GtkWidget *wid;
	if (gtk_combo_box_get_active (types) == 2) /* XML */
		is_cvs = FALSE;
	wid = g_object_get_data (G_OBJECT (dialog), "first_row");
	gtk_widget_set_sensitive (wid, is_cvs);
	wid = g_object_get_data (G_OBJECT (dialog), "invalid_as_null");
	gtk_widget_set_sensitive (wid, is_cvs);
	wid = g_object_get_data (G_OBJECT (dialog), "null_as_empty");
	gtk_widget_set_sensitive (wid, is_cvs);
}

static gboolean confirm_file_overwrite (GtkWindow *parent, const gchar *path);

static void
save_as_response_cb (GtkDialog *dialog, gint response_id, GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	if (response_id == GTK_RESPONSE_OK) {
		GtkWidget *types;
		gint export_type;
		GtkWidget *filename;
		gboolean selection_only = FALSE;
		gboolean null_as_empty = FALSE;
		gboolean invalid_as_null = FALSE;
		gboolean first_row = FALSE;
		gchar *body;
		gchar *path;
		GList *columns, *list;
		gint *cols, nb_cols;
		gint *rows = NULL, nb_rows = 0;
		GdaHolder *param;
		GdaSet *paramlist;
		GdaDataModel *model_to_use = (GdaDataModel*) priv->proxy;
		GtkWidget *scope;
		gint scope_v;

		model_to_use = model_to_use;
		scope = g_object_get_data (G_OBJECT (dialog), "scope");
		scope_v = gtk_combo_box_get_active (GTK_COMBO_BOX (scope));
		if (scope_v == 0)
			model_to_use = priv->data_model;
		else if (scope_v == 2)
			selection_only = TRUE;

		types = g_object_get_data (G_OBJECT (dialog), "types");
		filename = g_object_get_data (G_OBJECT (dialog), "filename");
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (filename)));
		null_as_empty = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
							      (g_object_get_data (G_OBJECT (dialog), "null_as_empty")));
		invalid_as_null = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
							      (g_object_get_data (G_OBJECT (dialog), "invalid_as_null")));
		first_row = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
							  (g_object_get_data (G_OBJECT (dialog), "first_row")));

		/* output columns computation */
		columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
		cols = g_new (gint, gda_data_model_get_n_columns (GDA_DATA_MODEL (model_to_use)));
		nb_cols = 0;
		for (list = columns; list; list = list->next) {
			if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data))) {
				GdauiSetGroup *group;
				GSList *params;

				group = g_object_get_data (G_OBJECT (list->data), "__gdaui_group");
				for (params = gda_set_group_get_nodes (gdaui_set_group_get_group (group))
				     ; params; nb_cols ++, params = params->next)
					cols [nb_cols] = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter),
									gda_set_node_get_holder (GDA_SET_NODE (params->data)));
			}
		}
		g_list_free (columns);

		/* output rows computation */
		if (selection_only) {
			GtkTreeSelection *selection;
			GList *sel_rows, *list;
			gint pos;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
			sel_rows = gtk_tree_selection_get_selected_rows (selection, NULL);

			nb_rows = g_list_length (sel_rows);
			rows = g_new0 (gint, nb_rows);

			for (pos = 0, list = sel_rows; list; list = list->next, pos++) {
				gint *ind = gtk_tree_path_get_indices ((GtkTreePath*) list->data);
				rows [pos] = *ind;
				gtk_tree_path_free ((GtkTreePath*) list->data);
			}
			g_list_free (sel_rows);
		}

		/* Actual ouput computations */
		export_type = gtk_combo_box_get_active (GTK_COMBO_BOX (types));
		priv->export_type = export_type;
		paramlist = gda_set_new (NULL);

		if (null_as_empty) {
			param = gda_holder_new_boolean ("NULL_AS_EMPTY", TRUE);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
		}
		if (invalid_as_null) {
			param = gda_holder_new_boolean ("INVALID_AS_NULL", TRUE);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
		}
		if (first_row) {
			param = gda_holder_new_boolean ("FIELDS_NAME", TRUE);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
		}

		switch (export_type) {
		case 0:
			param = gda_holder_new_string ("SEPARATOR", "\t");
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (model_to_use),
								GDA_DATA_MODEL_IO_TEXT_SEPARATED,
								cols, nb_cols, rows, nb_rows, paramlist);
			break;
		case 1:
			param = gda_holder_new_string ("SEPARATOR", ",");
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (model_to_use),
								GDA_DATA_MODEL_IO_TEXT_SEPARATED,
								cols, nb_cols, rows, nb_rows, paramlist);
			break;
		case 2:
			param = NULL;
			body = (gchar *) g_object_get_data (G_OBJECT (model_to_use), "name");
			if (body)
				param = gda_holder_new_string ("NAME", body);
			if (param) {
				gda_set_add_holder (paramlist, param);
				g_object_unref (param);
			}
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (model_to_use),
								GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
								cols, nb_cols, rows, nb_rows, paramlist);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		g_object_unref (paramlist);
		g_free (cols);
		if (rows)
			g_free (rows);

		/* saving */
		if (body) {
			path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filename));
			if (path) {
				if (g_file_test (path, G_FILE_TEST_EXISTS)) {
					if (! confirm_file_overwrite (GTK_WINDOW (dialog), path)) {
						g_free (body);
						g_free (path);
						return;
					}
				}

				if (! g_file_set_contents (path, body, strlen (body), NULL)) {
					_gdaui_utility_show_error (NULL, _("Could not save file %s"), path);
					g_free (body);
					g_free (path);
					return;
				}
				g_free (path);
			}
			else {
				_gdaui_utility_show_error (NULL, _("You must specify a file name"));
				g_free (body);
				return;
			}
			g_free (body);
		} else
			_gdaui_utility_show_error (NULL,_("Got empty file while converting the data"));
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
confirm_file_overwrite (GtkWindow *parent, const gchar *path)
{
	GtkWidget *dialog, *button;
	gboolean yes;
	gchar *msg;

	msg = g_strdup_printf (_("File '%s' already exists.\n"
				 "Do you want to overwrite it?"), path);

	/* build the dialog */
	dialog = gtk_message_dialog_new_with_markup (parent,
						     GTK_DIALOG_DESTROY_WITH_PARENT |
						     GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_YES_NO,
						     "<b>%s</b>\n%s",
						     msg,
						     _("If you choose yes, the contents will be lost."));
	g_free (msg);

	button = gtk_button_new_with_label (_("_Cancel"));
	gtk_widget_set_can_default (button, TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_NO);

	/* run the dialog */
	gtk_widget_show_all (dialog);
	yes = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;

	/* free memory */
	gtk_widget_destroy (dialog);

	return yes;
}

static void
tree_view_row_activated_cb (G_GNUC_UNUSED GtkTreeView *tree_view, GtkTreePath *path,
			    G_GNUC_UNUSED GtkTreeViewColumn *column, GdauiRawGrid *grid)
{
	gint *indices;

	indices = gtk_tree_path_get_indices (path);
#ifdef debug_signal
	g_print (">> 'DOUBLE_CLICKED' from %s %p\n", G_OBJECT_TYPE_NAME (grid), grid);
#endif
	g_signal_emit (G_OBJECT (grid), gdaui_raw_grid_signals[DOUBLE_CLICKED], 0, *indices);
#ifdef debug_signal
	g_print ("<< 'DOUBLE_CLICKED' from %s %p\n", G_OBJECT_TYPE_NAME (grid), grid);
#endif
}

/*
 * Synchronize the values of the parameters in priv->iter with the currently selected row
 */
static void
tree_view_selection_changed_cb (GtkTreeSelection *selection, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint has_selection = 0;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	/* block the GdaDataModelIter' "changed" signal */
	g_signal_handlers_block_by_func (priv->iter,
					 G_CALLBACK (iter_row_changed_cb), grid);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE) {
		has_selection = gtk_tree_selection_count_selected_rows (selection);
		if (has_selection == 1) {
			GList *sel_rows;

			sel_rows = gtk_tree_selection_get_selected_rows (selection, &model);
			has_selection = gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (sel_rows->data)) ? 1 : 0;
			g_list_free_full (sel_rows, (GDestroyNotify) gtk_tree_path_free);
		}
	}
	else
		has_selection = gtk_tree_selection_get_selected (selection, &model, &iter) ? 1 : 0;

	if (has_selection == 1) {
		if (!gda_data_model_iter_move_to_row (priv->iter,
						      gdaui_data_store_get_row_from_iter (priv->store,
											  &iter))) {
			/* selection changing is refused, return to the current selected row */
			GtkTreePath *path;
			path = gtk_tree_path_new_from_indices (gda_data_model_iter_get_row (priv->iter), -1);
			g_signal_handlers_block_by_func (G_OBJECT (selection),
							 G_CALLBACK (tree_view_selection_changed_cb), grid);

			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);
			g_signal_handlers_unblock_by_func (G_OBJECT (selection),
							   G_CALLBACK (tree_view_selection_changed_cb), grid);

			gtk_tree_path_free (path);
		}
	}
	else {
		/* render all the parameters invalid, and make the iter point to row -1 */
		gda_data_model_iter_invalidate_contents (priv->iter);
		gda_data_model_iter_move_to_row (priv->iter, -1);
	}

	g_signal_emit_by_name (G_OBJECT (grid), "selection-changed");

	/* unblock the GdaDataModelIter' "changed" signal */
	g_signal_handlers_unblock_by_func (priv->iter,
					   G_CALLBACK (iter_row_changed_cb), grid);
}

static void
destroy_column_data (GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	if (priv->columns_data) {
		GSList *list;
		for (list = priv->columns_data; list; list = list->next) {
			ColumnData *cdata = COLUMN_DATA (list->data);
			g_object_unref (cdata->data_cell);
			g_object_unref (cdata->info_cell);
			g_free (cdata->tooltip_text);
			g_free (cdata->title);
			g_free (cdata);
		}
		g_slist_free (priv->columns_data);
		priv->columns_data = NULL;
		g_hash_table_remove_all (priv->columns_hash);
	}
}

static ColumnData *
get_column_data_for_group (GdauiRawGrid *grid, GdauiSetGroup *group)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	GSList *list;
	for (list = priv->columns_data; list; list = list->next) {
		if (COLUMN_DATA (list->data)->group == group)
			return COLUMN_DATA (list->data);
	}

	return NULL;
}

static ColumnData *
get_column_data_for_holder (GdauiRawGrid *grid, GdaHolder *holder)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	GSList *list;
	for (list = priv->columns_data; list; list = list->next) {
		if (COLUMN_DATA (list->data)->single_param == holder)
			return COLUMN_DATA (list->data);
	}

	return NULL;
}

static ColumnData *
get_column_data_for_id (GdauiRawGrid *grid, const gchar *id)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	GSList *list;
	for (list = priv->columns_data; list; list = list->next) {
		ColumnData *cdata = COLUMN_DATA (list->data);
		if (cdata->title && !strcmp (cdata->title, id))
			return cdata;
		if (cdata->single_param) {
			const gchar *hid;
			hid = gda_holder_get_id (cdata->single_param);
			if (hid && !strcmp (hid, id))
				return cdata;
		}
	}

	return NULL;
}

/**
 * gdaui_raw_grid_set_sample_size:
 * @grid: a #GdauiRawGrid
 * @sample_size: the size of the sample displayed in @grid
 *
 * Sets the size of each chunk of data to display: the maximum number of rows which
 * can be displayed at a time. See gdaui_grid_set_sample_size() and gda_data_proxy_set_sample_size()
 *
 * Since: 4.2
 */
void
gdaui_raw_grid_set_sample_size (GdauiRawGrid *grid, gint sample_size)
{
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	gda_data_proxy_set_sample_size (priv->proxy, sample_size);
}

/**
 * gdaui_raw_grid_set_sample_start:
 * @grid: a #GdauiRawGrid
 * @sample_start:
 *
 *
 * Since: 4.2
 */
void
gdaui_raw_grid_set_sample_start (GdauiRawGrid *grid, gint sample_start)
{
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	gda_data_proxy_set_sample_start (priv->proxy, sample_start);
}

/**
 * gdaui_raw_grid_set_layout_from_file:
 * @grid: a #GdauiRawGrid
 * @file_name: XML file name to use
 * @grid_name: the name of the grid to use, in @file_name
 *
 * Sets a grid's columns layout according an XML description contained in @file_name, for the grid identified
 * by the @grid_name name (as an XML layout file can contain the descriptions of several forms and grids).
 *
 * Since: 4.2
 */
void
gdaui_raw_grid_set_layout_from_file (GdauiRawGrid *grid, const gchar *file_name, const gchar *grid_name)
{
	g_return_if_fail (GDAUI_IS_RAW_GRID (grid));
	g_return_if_fail (file_name);
        g_return_if_fail (grid_name);

	xmlDocPtr doc;
        doc = xmlParseFile (file_name);
        if (doc == NULL) {
                g_warning (_("'%s' document not parsed successfully"), file_name);
                return;
        }

        xmlDtdPtr dtd = NULL;

	GBytes *bytes;
	bytes = g_resources_lookup_data ("/gdaui/gdaui-layout.dtd", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		xmlParserInputBufferPtr	ibuffer;
		gsize size;
		const char *data;
		data = (const char *) g_bytes_get_data (bytes, &size);
		ibuffer = xmlParserInputBufferCreateMem (data, size, XML_CHAR_ENCODING_NONE);
		dtd = xmlIOParseDTD (NULL, ibuffer, XML_CHAR_ENCODING_NONE);
		/* No need to call xmlFreeParserInputBuffer (ibuffer), because xmlIOParseDTD() does it */
		g_bytes_unref (bytes);
	}

        if (! dtd)
                g_warning ("DTD not parsed successfully, please report error to "
                           "http://gitlab.gnome.org/GNOME/libgda/issues");

        /* Get the root element node */
        xmlNodePtr root_node = NULL;
        root_node = xmlDocGetRootElement (doc);

        /* Must have root element, a name and the name must be "gdaui_layouts" */
        if (!root_node || !root_node->name ||
            ! xmlStrEqual (root_node->name, BAD_CAST "gdaui_layouts")) {
                xmlFreeDoc (doc);
                return;
        }

        xmlNodePtr node;
        for (node = root_node->children; node; node = node->next) {
                if ((node->type == XML_ELEMENT_NODE) &&
                    xmlStrEqual (node->name, BAD_CAST "gdaui_grid")) {
                        xmlChar *str;
                        str = xmlGetProp (node, BAD_CAST "name");
			if (str) {
				if (!strcmp ((gchar*) str, grid_name)) {
					g_object_set (G_OBJECT (grid), "xml-layout", node, NULL);
					xmlFree (str);
					break;
				}
                                xmlFree (str);
                        }
                }
        }

        /* Free the document */
        xmlFreeDoc (doc);
}

/*
 * GdauiDataProxy interface
 */

static GdaDataProxy *
gdaui_raw_grid_get_proxy (GdauiDataProxy *iface)
{
	GdauiRawGrid *grid;
	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	return priv->proxy;
}

static void
gdaui_raw_grid_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable)
{
	GdauiRawGrid *grid;
	GdaHolder *param;
	ColumnData *cdata;
	GdauiSetGroup *group;

	g_return_if_fail (GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	if (priv->data_model) {
		editable = editable && !gda_data_proxy_is_read_only (priv->proxy);

		param = gda_data_model_iter_get_holder_for_field (priv->iter, column);
		g_return_if_fail (param);

		group = gdaui_set_get_group (priv->iter_info, param);
		g_return_if_fail (group);

		cdata = get_column_data_for_group (grid, group);
		g_return_if_fail (cdata);

		if (editable && !gda_data_proxy_is_read_only (priv->proxy))
			cdata->data_locked = FALSE;
		else
			cdata->data_locked = TRUE;
	}
}

static gboolean
gdaui_raw_grid_supports_action (GdauiDataProxy *iface, GdauiAction action)
{
	switch (action) {
	case GDAUI_ACTION_NEW_DATA:
	case GDAUI_ACTION_WRITE_MODIFIED_DATA:
	case GDAUI_ACTION_DELETE_SELECTED_DATA:
	case GDAUI_ACTION_UNDELETE_SELECTED_DATA:
	case GDAUI_ACTION_RESET_DATA:
	case GDAUI_ACTION_MOVE_FIRST_CHUNK:
        case GDAUI_ACTION_MOVE_PREV_CHUNK:
        case GDAUI_ACTION_MOVE_NEXT_CHUNK:
        case GDAUI_ACTION_MOVE_LAST_CHUNK:
		return TRUE;
	default:
		return FALSE;
	};
}

static void
gdaui_raw_grid_perform_action (GdauiDataProxy *iface, GdauiAction action)
{
	GdauiRawGrid *grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	switch (action) {
	case GDAUI_ACTION_NEW_DATA: {
		GtkTreeIter iter;
		GtkTreePath *path;

		if (gdaui_data_store_append (priv->store, &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (grid), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		break;
	}

	case GDAUI_ACTION_WRITE_MODIFIED_DATA: {
		gint row;
		GError *error = NULL;
		gboolean allok = TRUE;
		gint mod1, mod2;

		mod1 = gda_data_proxy_get_n_modified_rows (priv->proxy);
		row = gda_data_model_iter_get_row (priv->iter);
		if (priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) {
			gint newrow;

			allok = gda_data_proxy_apply_row_changes (priv->proxy, row, &error);
			if (allok) {
				newrow = gda_data_model_iter_get_row (priv->iter);
				if (row != newrow) /* => current row has changed because the
						      proxy had to emit a "row_removed" when
						      actually succeeded the commit
						      => we need to come back to that row
						   */
					gda_data_model_iter_move_to_row (priv->iter, row);
			}
		}
		else
			allok = gda_data_proxy_apply_all_changes (priv->proxy, &error);

		mod2 = gda_data_proxy_get_n_modified_rows (priv->proxy);
		if (!allok) {
			if (mod1 != mod2)
				/* the data model has changed while doing the writing */
				_gdaui_utility_display_error ((GdauiDataProxy *) grid, FALSE, error);
			else
				_gdaui_utility_display_error ((GdauiDataProxy *) grid, TRUE, error);
			g_error_free (error);
		}

		break;
	}

	case GDAUI_ACTION_DELETE_SELECTED_DATA: {
		GtkTreeIter iter;
		GtkTreeSelection *select;
		GtkTreeModel *model;
		GList *sel_rows;
		GdaDataProxy *proxy;

		select = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
		sel_rows = gtk_tree_selection_get_selected_rows (select, &model);
		proxy = gdaui_data_store_get_proxy (GDAUI_DATA_STORE (model));

		/* rem: get the list of selected rows after each row deletion because the data model might have changed and
		 * row numbers might also have changed */
		while (sel_rows) {
			gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (sel_rows->data));
			if (!gda_data_proxy_row_is_deleted (proxy,
							    gdaui_data_store_get_row_from_iter (GDAUI_DATA_STORE (model),
												&iter))) {
				gdaui_data_store_delete (priv->store, &iter);
				g_list_free_full (sel_rows, (GDestroyNotify) gtk_tree_path_free);
				sel_rows = gtk_tree_selection_get_selected_rows (select, &model);
			}
			else
				sel_rows = sel_rows->next;
		}

		break;
	}

	case GDAUI_ACTION_UNDELETE_SELECTED_DATA: {
		GtkTreeIter iter;
		GtkTreeSelection *select;
		GtkTreeModel *model;
		GList *sel_rows, *cur_row;

		select = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
		sel_rows = gtk_tree_selection_get_selected_rows (select, &model);
		cur_row = sel_rows;
		while (cur_row) {
			gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (cur_row->data));
			gdaui_data_store_undelete (priv->store, &iter);
			cur_row = g_list_next (cur_row);
		}
		g_list_free_full (sel_rows, (GDestroyNotify) gtk_tree_path_free);

		break;
	}

	case GDAUI_ACTION_RESET_DATA:
		gda_data_proxy_cancel_all_changes (priv->proxy);
		gda_data_model_send_hint (GDA_DATA_MODEL (priv->proxy), GDA_DATA_MODEL_HINT_REFRESH, NULL);
		break;

	case GDAUI_ACTION_MOVE_FIRST_CHUNK:
		gda_data_proxy_set_sample_start (priv->proxy, 0);
		break;

        case GDAUI_ACTION_MOVE_PREV_CHUNK: {
		gint sample_size, sample_start;

		sample_size = gda_data_proxy_get_sample_size (priv->proxy);
		if (sample_size > 0) {
			sample_start = gda_data_proxy_get_sample_start (priv->proxy);
			sample_start -= sample_size;
			if (sample_start >= 0)
				gda_data_proxy_set_sample_start (priv->proxy, sample_start);
		}

		break;
	}

        case GDAUI_ACTION_MOVE_NEXT_CHUNK: {
		gint sample_size, sample_start;

		sample_size = gda_data_proxy_get_sample_size (priv->proxy);
		if (sample_size > 0) {
			sample_start = gda_data_proxy_get_sample_start (priv->proxy);
			sample_start += sample_size;
			gda_data_proxy_set_sample_start (priv->proxy, sample_start);
		}
		break;
	}

        case GDAUI_ACTION_MOVE_LAST_CHUNK:
		gda_data_proxy_set_sample_start (priv->proxy, G_MAXINT);
		break;

	default:
		break;
	}
}

static void
paramlist_public_data_changed_cb (G_GNUC_UNUSED GdauiSet *info, GdauiRawGrid *grid)
{
	/* info cells */
	destroy_column_data (grid);	
	create_columns_data (grid);
	reset_columns_default (grid);
}

static void
paramlist_param_attr_changed_cb (G_GNUC_UNUSED GdaSet *paramlist, GdaHolder *param,
				 const gchar *att_name, const GValue *att_value, GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	if (!strcmp (att_name, GDAUI_ATTRIBUTE_PLUGIN)) {
		ColumnData *cdata;
		const gchar *plugin = NULL;
		cdata = get_column_data_for_holder (grid, param);
		if (!cdata)
			return;
		
		if (att_value) {
			if (G_VALUE_TYPE (att_value) == G_TYPE_STRING)
				plugin = g_value_get_string (att_value);
			else {
				g_warning (_("The '%s' attribute should be a G_TYPE_STRING value"),
					   GDAUI_ATTRIBUTE_PLUGIN);
				return;
			}
		}

		/* get rid of previous renderer */
		g_signal_handlers_disconnect_by_func (G_OBJECT (cdata->data_cell),
						      G_CALLBACK (data_cell_value_changed), grid);
		g_hash_table_remove (priv->columns_hash, cdata->data_cell);
		g_object_unref (cdata->data_cell);
		
		/* create new renderer */
		GtkCellRenderer *renderer;
		gint model_col;
		renderer = _gdaui_new_cell_renderer (gda_holder_get_g_type (param), plugin);
		cdata->data_cell = GTK_CELL_RENDERER (g_object_ref_sink ((GObject*) renderer));
		g_hash_table_insert (priv->columns_hash, renderer, cdata);

		model_col = g_slist_index (gda_set_get_holders ((GdaSet *)priv->iter), param);
		g_object_set_data (G_OBJECT (renderer), "model_col", GINT_TO_POINTER (model_col));

		g_object_set (G_OBJECT (renderer), "editable", !cdata->data_locked, NULL);		
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), "set-default-if-invalid"))
			g_object_set (G_OBJECT (renderer), "set-default-if-invalid", TRUE, NULL);

		g_signal_connect (G_OBJECT (renderer), "changed",
				  G_CALLBACK (data_cell_value_changed), grid);

		/* replace column */
		GtkTreeViewColumn *column;
		gint pos;
		pos = -1;
		column = cdata->column;
		if (column) {
			GList *cols;
			cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
			pos = g_list_index (cols, column);
			g_list_free (cols);
			gtk_tree_view_remove_column (GTK_TREE_VIEW (grid), column);
		}
		create_tree_view_column (grid, cdata, pos);
	}
	else if (!strcmp (att_name, GDA_ATTRIBUTE_NAME)) {
		ColumnData *cdata;
		cdata = get_column_data_for_holder (grid, param);
		if (!cdata)
			return;
		if (att_value) {
			if (G_VALUE_TYPE (att_value) == G_TYPE_STRING) {
				g_free (cdata->title);
				cdata->title = g_value_dup_string (att_value);
				gtk_tree_view_column_set_title (cdata->column, cdata->title);
			}
			else
				g_warning (_("The '%s' attribute should be a G_TYPE_STRING value"),
					   GDA_ATTRIBUTE_NAME);
		}
	}
}

static GError *
iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawGrid *grid)
{
	GError *error = NULL;
	gint row = gda_data_model_iter_get_row (iter);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	if (row < 0)
		return NULL;

	if ((priv->write_mode >= GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) &&
	    /* write back the current row */
	    gda_data_proxy_row_has_changed (priv->proxy, row) &&
	    !gda_data_proxy_apply_row_changes (priv->proxy, row, &error)) {
		if (_gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataProxy *) grid,
									      error)) {
			gda_data_proxy_cancel_row_changes (priv->proxy, row, -1);
			if (error) {
				g_error_free (error);
				error = NULL;
			}
		}
	}

	return error;
}

static void
iter_row_changed_cb (G_GNUC_UNUSED GdaDataModelIter *iter, gint row, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));

	if (row >= 0) {
		GtkSelectionMode mode;
		GtkTreeIter treeiter;

		mode = gtk_tree_selection_get_mode (selection);
		if (mode != GTK_SELECTION_SINGLE)
			gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
		path = gtk_tree_path_new_from_indices (row, -1);
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &treeiter, path)) {
			gtk_tree_selection_select_path (selection, path);
			gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (grid), path, NULL,
						      FALSE, 0., 0.);
		}
		gtk_tree_path_free (path);
		if (mode != GTK_SELECTION_SINGLE)
			gtk_tree_selection_set_mode (selection, mode);
	}
	else
		gtk_tree_selection_unselect_all (selection);
}

static void
proxy_sample_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint sample_start,
			 G_GNUC_UNUSED gint sample_end, GdauiRawGrid *grid)
{
	/* bring back the vertical scrollbar to the top */
	gtk_adjustment_set_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (grid)), 0.);
}

static void
proxy_row_updated_cb (GdaDataProxy *proxy, gint proxy_row, GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	if (priv->write_mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_ACTIVATED) {
		gint row;

		/* REM: this may lead to problems in the proxy because it is not re-entrant, solution: add a very short timeout */
		row = gda_data_model_iter_get_row (priv->iter);
		if ((row >= 0) && (row == proxy_row)) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (priv->proxy, row)) {
				GError *error = NULL;

				/* If the write fails, the proxy sets the fields back to was they were. We do not want
				 * te get notified about this because we would the try again to write to the database,
				 * which would fail again, and so on. */
				g_signal_handlers_block_by_func(G_OBJECT(proxy), G_CALLBACK(proxy_row_updated_cb), grid);

				if (!gda_data_proxy_apply_row_changes (priv->proxy, row, &error)) {
					gboolean discard;
					discard = _gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataProxy *) grid,
													    error);
					if (discard)
						gda_data_proxy_cancel_row_changes (priv->proxy, row, -1);
					g_error_free (error);
				}

				g_signal_handlers_unblock_by_func(G_OBJECT(proxy), G_CALLBACK(proxy_row_updated_cb), grid);
			}
		}
	}
}

static gboolean
model_reset_was_soft (GdauiRawGrid *grid, GdaDataModel *new_model)
{
	GdaDataModelIter *iter;
	gboolean retval = FALSE;
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	if (!new_model)
		return FALSE;
	else if (new_model == (GdaDataModel*) priv->proxy)
		return TRUE;
	else if (!priv->iter)
		return FALSE;

	iter = gda_data_model_create_iter (new_model);
	retval = ! _gdaui_utility_iter_differ (priv->iter, iter);
	g_object_unref (iter);
	return retval;
}

static void
proxy_reset_pre_cb (GdaDataProxy *proxy, GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	priv->iter_row = gda_data_model_iter_get_row (priv->iter);
	priv->reset_soft = model_reset_was_soft (grid, gda_data_proxy_get_proxied_model (priv->proxy));
}

static void
proxy_reset_cb (GdaDataProxy *proxy, GdauiRawGrid *grid)
{
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	GdkRectangle vis = {0, 0, 0, 0};
	if (gtk_widget_get_realized ((GtkWidget*) grid))
				gtk_tree_view_get_visible_rect ((GtkTreeView*) grid, &vis);

	if (priv->iter_row >= 0)
		gda_data_model_iter_move_to_row (priv->iter, priv->iter_row);
	else
		gda_data_model_iter_invalidate_contents (priv->iter);
	priv->iter_row = -1;
	priv->data_model = gda_data_proxy_get_proxied_model (priv->proxy);

	if (! priv->reset_soft)
		g_signal_emit_by_name (grid, "proxy-changed", priv->proxy);

	if (gtk_widget_get_realized ((GtkWidget*) grid))
		gtk_tree_view_scroll_to_point ((GtkTreeView*) grid, vis.x, vis.y);
}

static void
gdaui_raw_grid_clean (GdauiRawGrid *grid)
{
	/* column data */
	destroy_column_data (grid);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	/* store */
	if (priv->store) {
		g_object_unref (priv->store);
		priv->store = NULL;
	}

	/* private data set */
	if (priv->iter) {
		g_signal_handlers_disconnect_by_func (priv->iter_info,
						      G_CALLBACK (paramlist_public_data_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->iter,
						      G_CALLBACK (paramlist_param_attr_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->iter,
						      G_CALLBACK (iter_row_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->iter,
						      G_CALLBACK (iter_validate_set_cb), grid);
		g_object_unref (priv->iter);
		g_object_unref (priv->iter_info);
		priv->iter = NULL;
		priv->iter_info = NULL;
	}

	/* proxy */
	if (priv->proxy) {
		g_signal_handlers_disconnect_by_func (priv->proxy, G_CALLBACK (proxy_sample_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->proxy, G_CALLBACK (proxy_row_updated_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->proxy, G_CALLBACK (proxy_reset_pre_cb), grid);
		g_signal_handlers_disconnect_by_func (priv->proxy, G_CALLBACK (proxy_reset_cb), grid);
		g_object_unref (priv->proxy);
		priv->proxy = NULL;
	}
}

static gboolean
gdaui_raw_grid_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	GdauiRawGrid *grid;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), FALSE);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	g_return_val_if_fail (priv, FALSE);

	priv->write_mode = mode;
	if (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE) {
		priv->write_mode = GDAUI_DATA_PROXY_WRITE_ON_VALUE_ACTIVATED;
		return FALSE;
	}

	if (mode == GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE) {
		GtkTreeSelection *selection;
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	}
	return TRUE;
}

static GdauiDataProxyWriteMode
gdaui_raw_grid_widget_get_write_mode (GdauiDataProxy *iface)
{
	GdauiRawGrid *grid;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), GDAUI_DATA_PROXY_WRITE_ON_DEMAND);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);
	g_return_val_if_fail (priv, GDAUI_DATA_PROXY_WRITE_ON_DEMAND);

	return priv->write_mode;
}

/* GdauiDataSelector interface */
static GdaDataModel *
gdaui_raw_grid_selector_get_model (GdauiDataSelector *iface)
{
	GdauiRawGrid *grid;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	return GDA_DATA_MODEL (priv->proxy);
}

static void
gdaui_raw_grid_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	GdauiRawGrid *grid;

	g_return_if_fail (GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);

	g_object_set (grid, "model", model, NULL);
}

static GArray *
gdaui_raw_grid_selector_get_selected_rows (GdauiDataSelector *iface)
{
	GtkTreeSelection *selection;
	GList *selected_rows;
	GdauiRawGrid *grid;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (selected_rows) {
		GList *list;
		GArray *selarray = NULL;
		GtkTreeIter iter;
		gint row;

		for (list = selected_rows; list; list = list->next) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter,
						     (GtkTreePath *)(list->data))) {
				gint *ind;
				ind = gtk_tree_path_get_indices ((GtkTreePath *)(list->data));
				g_assert (ind);
				row = *ind;
				if (!selarray)
					selarray = g_array_new (FALSE, FALSE, sizeof (gint));
				g_array_append_val (selarray, row);
			}
		}
		g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
		return selarray;
	}
	else
		return NULL;
}

static GdaDataModelIter *
gdaui_raw_grid_selector_get_data_set (GdauiDataSelector *iface)
{
	GdauiRawGrid *grid;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	return priv->iter;
}

static gboolean
gdaui_raw_grid_selector_select_row (GdauiDataSelector *iface, gint row)
{
	GdauiRawGrid *grid;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	gboolean retval = TRUE;
	GtkTreeIter iter;

	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), FALSE);
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	path = gtk_tree_path_new_from_indices (row, -1);
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path))
		gtk_tree_selection_select_path (selection, path);
	else
		retval = FALSE;
	gtk_tree_path_free (path);

	return retval;
}

static void
gdaui_raw_grid_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	GdauiRawGrid *grid;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	path = gtk_tree_path_new_from_indices (row, -1);
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path))
		gtk_tree_selection_unselect_path (selection, path);
	gtk_tree_path_free (path);
}

static void
gdaui_raw_grid_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	GdauiRawGrid *grid;
	gint pos = -1;
	GtkTreeViewColumn *viewcol;
	GdaSetGroup *group;
	GdaHolder *param;

	g_return_if_fail (GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	GdauiRawGridPrivate *priv = gdaui_raw_grid_get_instance_private (grid);

	param = gda_data_model_iter_get_holder_for_field (priv->iter, column);
	g_return_if_fail (param);

	group = gda_set_get_group ((GdaSet *)priv->iter, param);
	pos = g_slist_index (gda_set_get_groups ((GdaSet *)priv->iter), group);
	g_assert (pos >= 0);

	viewcol = gtk_tree_view_get_column (GTK_TREE_VIEW (grid), pos);
	gtk_tree_view_column_set_visible (viewcol, visible);

	/* Sets the column's visibility */
	ColumnData *cdata;
	cdata = g_slist_nth_data (priv->columns_data, column);
	g_assert (cdata);
	cdata->prog_hidden = !visible;
}
