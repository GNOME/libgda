/* gdaui-raw-grid.c
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-raw-grid.h"
#include "gdaui-data-widget.h"
#include "gdaui-data-widget-filter.h"
#include "internal/utility.h"
#include "marshallers/gdaui-marshal.h"
#include "gdaui-easy.h"
#include "data-entries/gdaui-data-cell-renderer-combo.h"
#include "data-entries/gdaui-data-cell-renderer-info.h"

static void gdaui_raw_grid_class_init (GdauiRawGridClass * class);
static void gdaui_raw_grid_init (GdauiRawGrid *wid);
static void gdaui_raw_grid_dispose (GObject *object);

static void gdaui_raw_grid_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gdaui_raw_grid_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

static void init_tree_view (GdauiRawGrid *grid);

static void proxy_sample_changed_cb (GdaDataProxy *proxy, gint sample_start, gint sample_end, GdauiRawGrid *grid);
static void proxy_row_updated_cb (GdaDataProxy *proxy, gint proxy_row, GdauiRawGrid *grid);
static void proxy_reset_cb (GdaDataProxy *proxy, GdauiRawGrid *grid);
static void paramlist_public_data_changed_cb (GdauiSet *paramlist, GdauiRawGrid *grid);
static void paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, 
					     const gchar *att_name, const GValue *att_value, GdauiRawGrid *grid);
static GError *iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawGrid *grid);
static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawGrid *grid);

/* GdauiDataWidget interface */
static void            gdaui_raw_grid_widget_init           (GdauiDataWidgetIface *iface);
static GdaDataProxy   *gdaui_raw_grid_get_proxy             (GdauiDataWidget *iface);
static void            gdaui_raw_grid_col_set_show          (GdauiDataWidget *iface, gint column, gboolean shown);
static void            gdaui_raw_grid_set_column_editable   (GdauiDataWidget *iface, gint column, gboolean editable);
static void            gdaui_raw_grid_show_column_actions   (GdauiDataWidget *iface, gint column, gboolean show_actions);
static GtkActionGroup *gdaui_raw_grid_get_actions_group     (GdauiDataWidget *iface);
static GdaDataModelIter *gdaui_raw_grid_widget_get_data_set   (GdauiDataWidget *iface);

static GdaDataModel   *gdaui_raw_grid_widget_get_gda_model             (GdauiDataWidget *iface);
static void            gdaui_raw_grid_widget_set_gda_model             (GdauiDataWidget *iface, GdaDataModel *model);
static gboolean        gdaui_raw_grid_widget_set_write_mode (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode);
static GdauiDataWidgetWriteMode gdaui_raw_grid_widget_get_write_mode (GdauiDataWidget *iface);
static void            gdaui_raw_grid_set_data_layout (GdauiDataWidget  *iface, gpointer  data);

typedef struct {
	GdauiSetGroup *group;
	GtkCellRenderer *data_cell;
	GtkCellRenderer *info_cell;
	gboolean         info_shown;
	gboolean         data_locked; /* TRUE if no modification allowed on that column */
        gchar           *tooltip_text;
} ColumnData;

#define COLUMN_DATA(x) ((ColumnData *)(x))

static ColumnData *get_column_data (GdauiRawGrid *grid, GdauiSetGroup *group);

struct _GdauiRawGridPriv
{
	GdaDataModel               *data_model;  /* data model provided by set_model() */
	GdaDataModelIter           *iter;        /* iterator for @store, used for its structure */
	GdauiSet                 *iter_info;
	GdauiDataStore           *store;       /* GtkTreeModel interface, using @proxy */
	GdaDataProxy               *proxy;       /* proxy data model, proxying @data_model */

	GSList                     *columns_data; /* list of ColumnData */

	GSList                     *reordered_indexes;  /* Indexes of the reordered columns. */

	gboolean                    default_show_info_cell;
	gboolean                    default_show_global_actions;

	GtkActionGroup             *actions_group;

	gint                        export_type; /* used by the export dialog */
	GdauiDataWidgetWriteMode  write_mode;

	GtkWidget                  *filter;
	GtkWidget                  *filter_window;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* signals */
enum
{
        SELECTION_CHANGED,
	DOUBLE_CLICKED,
	POPULATE_POPUP,
        LAST_SIGNAL
};

static gint gdaui_raw_grid_signals[LAST_SIGNAL] = { 0, 0, 0 };


/* properties */
enum
{
        PROP_0,
	PROP_MODEL,
	PROP_DATA_LAYOUT,
	PROP_INFO_CELL_VISIBLE,
	PROP_GLOBAL_ACTIONS_VISIBLE
};

/*Callbacks */

/*
 * Real initialization
 */

static gboolean tree_view_event_cb (GtkWidget *treeview, GdkEvent *event, GdauiRawGrid *grid);
static gint     tree_view_popup_button_pressed_cb (GtkWidget *widget, GdkEventButton *event, 
						   GdauiRawGrid *grid);

static void     tree_view_selection_changed_cb (GtkTreeSelection *selection, GdauiRawGrid *grid);
static void     tree_view_row_activated_cb     (GtkTreeView *tree_view, GtkTreePath *path, 
						GtkTreeViewColumn *column, GdauiRawGrid *grid);

static void action_new_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_delete_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_undelete_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_commit_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_reset_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_first_chunck_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_prev_chunck_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_next_chunck_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_last_chunck_cb (GtkAction *action, GdauiRawGrid *grid);
static void action_filter_cb (GtkAction *action, GdauiRawGrid *grid);

static GtkActionEntry ui_actions[] = {
	{ "ActionNew", GTK_STOCK_ADD, "_New", NULL, "Create a new data entry", G_CALLBACK (action_new_cb)},
	{ "ActionDelete", GTK_STOCK_REMOVE, "_Delete", NULL, "Delete the selected entry", G_CALLBACK (action_delete_cb)},
	{ "ActionUndelete", GTK_STOCK_UNDELETE, "_Undelete", NULL, "Cancels the deletion of the selected entry", 
	  G_CALLBACK (action_undelete_cb)},
	{ "ActionCommit", GTK_STOCK_SAVE, "_Commit", NULL, "Commit the latest changes", G_CALLBACK (action_commit_cb)},
	{ "ActionReset", GTK_STOCK_REFRESH, "_Reset", NULL, "Reset the data", G_CALLBACK (action_reset_cb)},
	{ "ActionFirstChunck", GTK_STOCK_GOTO_FIRST, "_First chunck", NULL, "Go to first chunck of records", 
	  G_CALLBACK (action_first_chunck_cb)},
	{ "ActionLastChunck", GTK_STOCK_GOTO_LAST, "_Last chunck", NULL, "Go to last chunck of records", 
	  G_CALLBACK (action_last_chunck_cb)},
	{ "ActionPrevChunck", GTK_STOCK_GO_BACK, "_Previous chunck", NULL, "Go to previous chunck of records", 
	  G_CALLBACK (action_prev_chunck_cb)},
	{ "ActionNextChunck", GTK_STOCK_GO_FORWARD, "Ne_xt chunck", NULL, "Go to next chunck of records",
	  G_CALLBACK (action_next_chunck_cb)},
	{ "ActionFilter", GTK_STOCK_FIND, "Filter", NULL, "Filter records",
	  G_CALLBACK (action_filter_cb)}
};

GType
gdaui_raw_grid_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiRawGridClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_raw_grid_class_init,
			NULL,
			NULL,
			sizeof (GdauiRawGrid),
			0,
			(GInstanceInitFunc) gdaui_raw_grid_init
		};		

		static const GInterfaceInfo data_widget_info = {
                        (GInterfaceInitFunc) gdaui_raw_grid_widget_init,
                        NULL,
                        NULL
                };
		
		type = g_type_register_static (GTK_TYPE_TREE_VIEW, "GdauiRawGrid", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_WIDGET, &data_widget_info);
	}

	return type;
}

static void
gdaui_raw_grid_widget_init (GdauiDataWidgetIface *iface)
{
	iface->get_proxy = gdaui_raw_grid_get_proxy;
	iface->col_set_show = gdaui_raw_grid_col_set_show;
	iface->set_column_editable = gdaui_raw_grid_set_column_editable;
	iface->show_column_actions = gdaui_raw_grid_show_column_actions;
	iface->get_actions_group = gdaui_raw_grid_get_actions_group;
	iface->get_data_set = gdaui_raw_grid_widget_get_data_set;
	iface->get_gda_model = gdaui_raw_grid_widget_get_gda_model;
	iface->set_gda_model = gdaui_raw_grid_widget_set_gda_model;
	iface->set_write_mode = gdaui_raw_grid_widget_set_write_mode;
	iface->get_write_mode = gdaui_raw_grid_widget_get_write_mode;	
	iface->set_data_layout = gdaui_raw_grid_set_data_layout;
}

static void
gdaui_raw_grid_class_init (GdauiRawGridClass *class)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	gdaui_raw_grid_signals[SELECTION_CHANGED] = 
		g_signal_new ("selection_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiRawGridClass, selection_changed),
                              NULL, NULL,
                              _gdaui_marshal_VOID__BOOLEAN, G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
	gdaui_raw_grid_signals[DOUBLE_CLICKED] = 
		g_signal_new ("double_clicked",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiRawGridClass, double_clicked),
                              NULL, NULL,
                              _gdaui_marshal_VOID__INT, G_TYPE_NONE,
                              1, G_TYPE_INT);
	gdaui_raw_grid_signals[POPULATE_POPUP] = 
		g_signal_new ("populate_popup",
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
	
	g_object_class_install_property (object_class, PROP_DATA_LAYOUT,
					 g_param_spec_pointer ("data_layout", 
							       _("Pointer to an XML data layout specification"), NULL,
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_INFO_CELL_VISIBLE,
                                         g_param_spec_boolean ("info_cell_visible", NULL, _("Info cell visible"), FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
															   
	g_object_class_install_property (object_class, PROP_GLOBAL_ACTIONS_VISIBLE,
                                         g_param_spec_boolean ("global_actions_visible", NULL, _("Global Actions visible"), FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gdaui_raw_grid_init (GdauiRawGrid *grid)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	
	grid->priv = g_new0 (GdauiRawGridPriv, 1);
	grid->priv->store = NULL;
	grid->priv->proxy = NULL;
	grid->priv->default_show_info_cell = FALSE;
	grid->priv->default_show_global_actions = TRUE;
	grid->priv->columns_data = NULL;
	grid->priv->export_type = 2;
	grid->priv->write_mode = GDAUI_DATA_WIDGET_WRITE_ON_DEMAND;
	
	tree_view = GTK_TREE_VIEW (grid);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree_view), TRUE);
	g_signal_connect (G_OBJECT (tree_view), "event",
			  G_CALLBACK (tree_view_event_cb), grid);
	g_signal_connect (G_OBJECT (tree_view), "button_press_event",
                          G_CALLBACK (tree_view_popup_button_pressed_cb), grid);
	gtk_tree_view_set_enable_search (tree_view, FALSE);

	/* selection and signal handling */
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (tree_view_selection_changed_cb), grid);
	g_signal_connect (G_OBJECT (tree_view), "row_activated",
			  G_CALLBACK (tree_view_row_activated_cb), grid);

	/* action group */
	grid->priv->actions_group = gtk_action_group_new ("Actions");
	gtk_action_group_add_actions (grid->priv->actions_group, ui_actions, G_N_ELEMENTS (ui_actions), grid);

	grid->priv->filter = NULL;
	grid->priv->filter_window = NULL;
}

/**
 * gdaui_raw_grid_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiRawGrid widget suitable to display the data in @model
 *
 *  Returns: the new widget
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

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_RAW_GRID (object));
	grid = GDAUI_RAW_GRID (object);

	if (grid->priv) {
		gdaui_raw_grid_clean (grid);

		if (grid->priv->actions_group) {
			g_object_unref (G_OBJECT (grid->priv->actions_group));
			grid->priv->actions_group = NULL;
		}

		if (grid->priv->filter) 
			gtk_widget_destroy (grid->priv->filter);
		if (grid->priv->filter_window) 
			gtk_widget_destroy (grid->priv->filter_window);

		/* the private area itself */
		g_free (grid->priv);
		grid->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
load_xml_data_layout_item (GdauiRawGrid  *grid,
			   xmlNodePtr       node,
			   gpointer         data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));

	gchar *name = NULL;
	gint sequence = 0;
	gboolean editable = FALSE;
	gboolean sort_ascending = FALSE;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	/* str = xmlGetProp (node, "relationship"); */
	/* if (str) { */
	/* 	g_print ("relationship: %s\n", str); */
	/* 	xmlFree (str); */
	/* } */

	/* str = xmlGetProp (node, "related_relationship"); */
	/* if (str) { */
	/* 	g_print ("related_relationship: %s\n", str); */
	/* 	xmlFree (str); */
	/* } */

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "editable");
	if (str) {
		editable = (*str == 't' || *str == 'T') ? TRUE : FALSE;
		g_print ("editable: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sort_ascending");
	if (str) {
		sort_ascending = (*str == 't' || *str == 'T') ? TRUE : FALSE;
		g_print ("sort_ascending: %s\n", str);
		xmlFree (str);
	}

	/* GSList *slist = grid->priv->set->holders; */
	/* while (slist != NULL) { */
	/* 	GdaHolder *holder = slist->data; */
	/* 	g_print ("SET HOLDER=%s\n", gda_holder_get_id (holder)); */
	/* 	slist = g_slist_next (slist); */
	/* } */

	//GdaHolder *holder = gda_set_get_holder (grid->priv->set, name);
	GdaHolder *holder = gda_set_get_holder (GDA_SET (grid->priv->iter), name);
	g_return_if_fail (holder != NULL);

	gint index = g_slist_index (GDA_SET (grid->priv->iter)->holders, holder);

	grid->priv->reordered_indexes = g_slist_insert (grid->priv->reordered_indexes,
						     GINT_TO_POINTER(index),
						     sequence - 1);

        GdauiSetGroup *group = _gdaui_set_get_group (grid->priv->iter_info, holder);
        if (!group)
		return;

        ColumnData *column_data = get_column_data (grid, group);
        g_return_if_fail (column_data);

        column_data->tooltip_text = g_strdup ((const gchar *) data);
        //g_print ("*** %s\n", column_data->tooltip_text);

	gint position = g_slist_index (GDA_SET (grid->priv->iter)->holders, holder);
	GtkTreeViewColumn *column = gtk_tree_view_get_column (GTK_TREE_VIEW (grid), position);
	g_return_if_fail (column != NULL);

	// ((GdaDataSelect *) model)->prep_stmt
	GdaDataModel *model = grid->priv->data_model;

	const gchar *text = NULL;
	if (GDA_IS_DATA_SELECT (model))
		text = gda_utility_data_model_find_column_description (GDA_DATA_SELECT (model), name);
	if (!text)
		text = gda_holder_get_id (holder);

	gtk_tree_view_column_set_title (column, text);

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		/* if (child->type == XML_ELEMENT_NODE && */
		/*     !xmlStrcmp (child->name, (const xmlChar *) "formatting")) { */
		/* } */

		/* if (child->type == XML_ELEMENT_NODE && */
		/*     !xmlStrcmp (child->name, (const xmlChar *) "title_custom")) { */
		/* } */
	}

}

static void
load_xml_data_layout_group (GdauiRawGrid  *grid,
			    xmlNodePtr       node,
			    gpointer         data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));

	gchar *name = NULL;
	gint sequence = 0;
	gint columns_count = 1;
	gchar *title = NULL;

	xmlChar *str;
	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "sequence");
	if (str) {
		sequence = atoi (str);
		g_print ("sequence: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "columns_count");
	if (str) {
		columns_count = atoi (str);
		g_print ("columns_count: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "title");
	if (str) {
		title = g_strdup (str);
		g_print ("title: %s\n", str);
		xmlFree (str);
	}

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_group")) {
			load_xml_data_layout_group (grid, child, data);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item")) {
			load_xml_data_layout_item (grid, child, /* data *//* table */ title);
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_portal")) {
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_notebook")) {
		}
		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "trans_set")) {
			/* load_xml_trans_set (grid, child, data); */
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_button")) {
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item_groupby")) {
		}

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_item_header")) {
		}
	}

	g_free (name);
	g_free (title);
}

static void
load_xml_data_layout_groups (GdauiRawGrid  *grid,
			     xmlNodePtr       node,
			     gpointer         data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_group")) {
			load_xml_data_layout_group (grid, child, /* data */ NULL);
		}
	}

}

static void
load_xml_data_layout (GdauiRawGrid  *grid,
		      xmlNodePtr       node,
		      gpointer         data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));

	gchar *parent_table = NULL;
	gchar *name = NULL;

	xmlChar *str;
	str = xmlGetProp (node, "parent_table");
	if (str) {
		parent_table = g_strdup (str);
		g_print ("parent_table: %s\n", str);
		xmlFree (str);
	}

	str = xmlGetProp (node, "name");
	if (str) {
		name = g_strdup (str);
		g_print ("name: %s\n", str);
		xmlFree (str);
	}

	// Don't recurse unnecessarily
	gboolean retval = FALSE;
	if (strcmp ((const gchar *) data, parent_table) != 0 ||
	    strcmp ("list", name) != 0) {
		retval = TRUE;
	}
	g_free (parent_table);
	g_free (name);
	if (retval)
		return;

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_groups")) {
			load_xml_data_layout_groups (grid, child, data);
		}
	}

}

static void
load_xml_data_layouts (GdauiRawGrid  *grid,
		       xmlNodePtr       node,
		       gpointer         data)
{
	g_print ("%s:\n", __func__);
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));

	xmlNodePtr child;
	for (child = node->children; child != NULL; child = child->next) {

		if (child->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp (child->name, (const xmlChar *) "data_layout")) {
			load_xml_data_layout (grid, child, data);
		}

	}

}

static gboolean
gdaui_raw_grid_query_tooltip (GtkWidget   *widget,
				 gint         x,
				 gint         y,
				 gboolean     keyboard_tip,
				 GtkTooltip  *tooltip,
				 gpointer     data)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW(widget);

	if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
						keyboard_tip,
						NULL, NULL, NULL))
		return FALSE;

	gint position = 0;
	guint col_x = 0;
	GList *columns = gtk_tree_view_get_columns (tree_view), *list = columns;
	while (list != NULL) {
		GtkTreeViewColumn *column = list->data;
		if (x >= col_x && x < (col_x + column->width)) {
			break;
		} else
			col_x += column->width;
		++position;
		list = g_list_next (list);
	}
	if (list == NULL)
		return FALSE;

	GdauiRawGrid *grid = GDAUI_RAW_GRID(tree_view);
	ColumnData *column_data = (ColumnData *)
		(g_slist_nth (grid->priv->columns_data, position)->data);
	g_return_val_if_fail (column_data, FALSE);

	g_list_free (columns);

	if (column_data->tooltip_text == NULL)
		return FALSE;

	gchar *markup = g_strdup_printf ("<i>%s</i> <b>%s</b>",
					 _("Group:"), column_data->tooltip_text);
	gtk_tooltip_set_markup (tooltip, markup);
	g_free (markup);
        /* GSList *slist = grid->priv->columns_data; */
        /* while (slist) { */
        /*         g_print ("--- %s\n", COLUMN_DATA (slist->data)->tooltip_text); */
        /*         slist = g_slist_next (slist); */
        /* } */

	return TRUE;
}

static void
gdaui_raw_grid_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiRawGrid *grid;

        grid = GDAUI_RAW_GRID (object);
        if (grid->priv) {
                switch (param_id) {
		case PROP_MODEL: {
			GdaDataModel *model = GDA_DATA_MODEL (g_value_get_object (value));
			if (model)
				g_return_if_fail (GDA_IS_DATA_MODEL (model));
			
			gdaui_raw_grid_clean (grid);
			if (!model)
				return;
			
			grid->priv->store = GDAUI_DATA_STORE (gdaui_data_store_new (model));
			grid->priv->proxy = gdaui_data_store_get_proxy (grid->priv->store);
			grid->priv->data_model = gda_data_proxy_get_proxied_model (grid->priv->proxy);
			
			g_object_ref (G_OBJECT (grid->priv->proxy));
			g_signal_connect (grid->priv->proxy, "sample_changed",
					  G_CALLBACK (proxy_sample_changed_cb), grid);
			g_signal_connect (grid->priv->proxy, "row_updated",
					  G_CALLBACK (proxy_row_updated_cb), grid);
			g_signal_connect (grid->priv->proxy, "reset",
					  G_CALLBACK (proxy_reset_cb), grid);
				
			grid->priv->iter = gda_data_model_create_iter (GDA_DATA_MODEL (grid->priv->proxy));
			grid->priv->iter_info = gdaui_set_new (GDA_SET (grid->priv->iter));
				
			g_signal_connect (grid->priv->iter_info, "public_data_changed",
					  G_CALLBACK (paramlist_public_data_changed_cb), grid);
			g_signal_connect (grid->priv->iter, "holder-attr-changed",
					  G_CALLBACK (paramlist_param_attr_changed_cb), grid);

			g_signal_connect (grid->priv->iter, "row_changed",
					  G_CALLBACK (iter_row_changed_cb), grid);
			g_signal_connect (grid->priv->iter, "validate-set",
					  G_CALLBACK (iter_validate_set_cb), grid);

			gda_data_model_iter_invalidate_contents (grid->priv->iter);
							
			gtk_tree_view_set_model ((GtkTreeView *) grid, GTK_TREE_MODEL (grid->priv->store));
			init_tree_view (grid);
	
			g_signal_emit_by_name (object, "proxy_changed", grid->priv->proxy);
			g_signal_emit_by_name (object, "iter_changed", grid->priv->iter);
				
			break;
		}
				
		case PROP_DATA_LAYOUT: {
			xmlNodePtr node = g_value_get_pointer (value);

			xmlNodePtr child;
			for (child = node->children; child != NULL; child = child->next) {

				if (child->type == XML_ELEMENT_NODE &&
				    !xmlStrcmp (child->name, (const xmlChar *) "data_layout_groups")) {
					load_xml_data_layout_groups (grid, child, NULL);
				}
			}

			if (grid->priv->reordered_indexes != NULL) {
				g_print ("Loaded XML file, reinit interface\n");

				GList *columns = gtk_tree_view_get_columns (GTK_TREE_VIEW(grid));

				GtkTreeViewColumn *prev_column = NULL;
				GSList *columns_data = NULL;

				GSList *reordered_indexes = grid->priv->reordered_indexes;
				while (reordered_indexes != NULL) {
					gint position = GPOINTER_TO_INT(reordered_indexes->data);

					GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN
						(g_list_nth (columns, position)->data);

					gtk_tree_view_move_column_after (GTK_TREE_VIEW(grid),
									 column, prev_column);
					prev_column = column;

					// Data columns are handled
					ColumnData *column_data = (ColumnData *)
						(g_slist_nth (grid->priv->columns_data, position)->data);
					columns_data = g_slist_append (columns_data, column_data);

					reordered_indexes = g_slist_next (reordered_indexes);
				}
				g_list_free (columns);

				g_slist_free (grid->priv->columns_data);
				grid->priv->columns_data = columns_data;

				// Remove unnecessary columns according layout
				columns = gtk_tree_view_get_columns (GTK_TREE_VIEW(grid));

				gint num_columns = g_list_length (columns);
				gint num_reordered_indexes = g_slist_length (grid->priv->reordered_indexes);
				if (num_reordered_indexes < num_columns) {
					gint i;
					for (i = num_reordered_indexes; i < num_columns; ++i) {
						GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN
							(g_list_nth (columns, i)->data);
						gtk_tree_view_remove_column (GTK_TREE_VIEW(grid), column);
					}
				}
				g_list_free (columns);

				g_object_set (G_OBJECT(grid), "has-tooltip", TRUE, NULL);
				g_signal_connect (G_OBJECT(grid), "query-tooltip",
						  G_CALLBACK(gdaui_raw_grid_query_tooltip), NULL);
			}

		}

			break;
				
		case PROP_INFO_CELL_VISIBLE: {
			GSList *list = grid->priv->columns_data;
			gboolean show = g_value_get_boolean (value);
			grid->priv->default_show_info_cell = show;
	
			while (list) {
				COLUMN_DATA (list->data)->info_shown = show;
				g_object_set (G_OBJECT (COLUMN_DATA (list->data)->info_cell), "visible", 
					      show, NULL);
				list = g_slist_next (list);
			}
		}
			break;
			
		case PROP_GLOBAL_ACTIONS_VISIBLE:
			gtk_action_group_set_visible (grid->priv->actions_group, g_value_get_boolean (value));
			break;
		
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
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
        if (grid->priv) {
                switch (param_id) {
		case PROP_MODEL:
			g_value_set_object (value, grid->priv->data_model);
			break;
		case PROP_INFO_CELL_VISIBLE:
			g_value_set_boolean(value, grid->priv->default_show_info_cell);
			break;
		case PROP_GLOBAL_ACTIONS_VISIBLE:
			g_value_set_boolean(value, grid->priv->default_show_global_actions);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }	
}

/**
 * gdaui_raw_grid_get_selection
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
gdaui_raw_grid_get_selection (GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	GList *selected_rows;

	g_return_val_if_fail (grid && GDAUI_IS_RAW_GRID (grid), NULL);
	g_return_val_if_fail (grid->priv, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (selected_rows) {
		GList *list, *retlist = NULL;
		GtkTreeIter iter;
		gint row;

		list = selected_rows;
		while (list) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (grid->priv->store), &iter,
						     (GtkTreePath *)(list->data))) {
				gtk_tree_model_get (GTK_TREE_MODEL (grid->priv->store), &iter, 
						    DATA_STORE_COL_MODEL_ROW, &row, -1);
				retlist = g_list_prepend (retlist, GINT_TO_POINTER (row));
			}
			list = g_list_next (list);
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
replace_double_underscores (gchar *str)
{
        gchar **arr;
        gchar *ret;
	
        arr = g_strsplit (str, "_", 0);
        ret = g_strjoinv ("__", arr);
        g_strfreev (arr);
	g_free (str);
	
        return ret;
}

static void     cell_renderer_value_set_attributes (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
						    GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiRawGrid *grid);
static void     cell_renderer_info_set_attributes  (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
						    GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiRawGrid *grid);

static void     data_cell_value_changed (GtkCellRenderer *renderer, const gchar *path, 
					 const GValue *new_value, GdauiRawGrid *grid);
static void     data_cell_values_changed (GtkCellRenderer *renderer, const gchar *path, 
					  GSList *new_values, GSList *all_new_values, GdauiRawGrid *grid);
static void     data_cell_status_changed (GtkCellRenderer *renderer, const gchar *path, 
					  GdaValueAttribute requested_action, GdauiRawGrid *grid);
static void     treeview_column_clicked_cb (GtkTreeViewColumn *tree_column, GdauiRawGrid *grid);

/*
 * Creates the GtkTreeView's columns, from the grid->priv->store GtkTreeModel
 */
static void
init_tree_view (GdauiRawGrid *grid)
{
	gint i;
	GtkTreeView *tree_view;
	GSList *list;

	tree_view = GTK_TREE_VIEW (grid);

	/* Creation of the columns in the treeview, to fit the parameters in grid->priv->iter param list */
	for (i = 0, list = grid->priv->iter_info->groups_list;
	     list;
	     i++, list = list->next) {
		GdaHolder *param;
		GdauiSetGroup *group;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;
		ColumnData *column_data;

		group = GDAUI_SET_GROUP (list->data);

		/* update the list of columns data */
		column_data = get_column_data (grid, group);
		if (!column_data) {
			column_data = g_new0 (ColumnData, 1);
			column_data->group = group;
			column_data->info_shown = grid->priv->default_show_info_cell;
			column_data->data_locked = FALSE;
			column_data->tooltip_text = NULL;
			grid->priv->columns_data = g_slist_append (grid->priv->columns_data, column_data);
		}

		/* create renderers */
		if (group->source) {
			/* parameters depending on a GdaDataModel */
			gchar *title;
			gboolean nullok = TRUE;
			GSList *nodes;
			
			nodes = group->group->nodes;
			while (nodes && nullok) {
				if (gda_holder_get_not_null (GDA_HOLDER (GDA_SET_NODE (nodes->data)->holder)))
					nullok = FALSE;
				nodes = g_slist_next (nodes);
			}
			
			/* determine title */
			if (g_slist_length (group->group->nodes) == 1)
				title = (gchar *) g_object_get_data (G_OBJECT (GDA_SET_NODE (group->group->nodes->data)->holder),
								     "name");
			else 
				title = (gchar *) g_object_get_data (G_OBJECT (group->group->nodes_source->data_model),
								     "name");

			if (title)
				title = replace_double_underscores (g_strdup (title));
			else 
				/* FIXME: find a better label */
				title = g_strdup (_("Value"));

			/* FIXME: if nullok is FALSE, then set the column title in bold */

			renderer = gdaui_data_cell_renderer_combo_new (grid->priv->iter_info, group->source);
			column_data->data_cell = renderer;
			gtk_tree_view_insert_column_with_data_func (tree_view, i, title, renderer,
								    (GtkTreeCellDataFunc) cell_renderer_value_set_attributes, 
								    grid, NULL);
			column = gtk_tree_view_get_column (tree_view, i);
			g_free (title);

			g_signal_connect (G_OBJECT (renderer), "changed", 
					  G_CALLBACK (data_cell_values_changed), grid);

			g_object_set_data (G_OBJECT (column), "source", group->group->nodes_source);
		}
		else {
			/* single direct parameter */
			GType g_type;
			const gchar *plugin = NULL;
			const GValue *plugin_val;
			gchar *title;
			gint model_col;
			
			param = GDA_HOLDER (GDA_SET_NODE (group->group->nodes->data)->holder);
			g_type = gda_holder_get_g_type (param);

			g_object_get (G_OBJECT (param), "name", &title, NULL);
			if (title && *title)
				title = replace_double_underscores (title);
			else
				title = NULL;
			if (!title)
				title = g_strdup (_("No title"));
			
			plugin_val = gda_holder_get_attribute (param, GDAUI_ATTRIBUTE_PLUGIN);
			if (plugin_val) {
				if (G_VALUE_TYPE (plugin_val) == G_TYPE_STRING)
					plugin = g_value_get_string (plugin_val);
				else
					g_warning (_("The '%s' attribute should be a G_TYPE_STRING value"),
						   GDAUI_ATTRIBUTE_PLUGIN);
			}
			renderer = _gdaui_new_cell_renderer (g_type, plugin);
			column_data->data_cell = renderer;
			gtk_tree_view_insert_column_with_data_func (tree_view, i, title, renderer,
								    (GtkTreeCellDataFunc) cell_renderer_value_set_attributes, 
								    grid, NULL);
			column = gtk_tree_view_get_column (tree_view, i);
			g_free (title);

			model_col = g_slist_index (((GdaSet *)grid->priv->iter)->holders, param);
			g_object_set_data (G_OBJECT (renderer), "model_col", GINT_TO_POINTER (model_col));

			g_signal_connect (G_OBJECT (renderer), "changed", 
					  G_CALLBACK (data_cell_value_changed), grid);
		}

		g_object_set_data (G_OBJECT (column), "data_renderer", renderer);
		g_object_set (G_OBJECT (renderer), "editable", !column_data->data_locked, NULL);

		/* settings and signals */
		g_object_set (G_OBJECT (renderer), "editable", !column_data->data_locked, NULL);
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), "set_default_if_invalid"))
			g_object_set (G_OBJECT (renderer), "set_default_if_invalid", TRUE, NULL);
		g_object_set_data (G_OBJECT (renderer), "__gdaui_group", group);
		g_object_set_data (G_OBJECT (column), "__gdaui_group", group);

		/* Adding the GValue's information cell as another GtkCellRenderer */
		renderer = gdaui_data_cell_renderer_info_new (grid->priv->store, grid->priv->iter, group);
		column_data->info_cell = renderer;
		gtk_tree_view_column_pack_end (column, renderer, FALSE);
		gtk_tree_view_column_set_cell_data_func (column, renderer, 
							 (GtkTreeCellDataFunc) cell_renderer_info_set_attributes, 
							 grid, NULL);
		g_signal_connect (G_OBJECT (renderer), "status_changed",
				  G_CALLBACK (data_cell_status_changed), grid);
		g_object_set (G_OBJECT (renderer), "visible", column_data->info_shown, NULL);
		g_object_set_data (G_OBJECT (renderer), "__gdaui_group", group);

		/* Sorting data */
		gtk_tree_view_column_set_clickable (column, TRUE);
		g_signal_connect (G_OBJECT (column), "clicked",
				  G_CALLBACK (treeview_column_clicked_cb), grid);
	}
}

/*
 * Set the attributes for each cell renderer which is not the information cell renderer, 
 * called by each cell renderer before actually displaying anything.
 */
static void
cell_renderer_value_set_attributes (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer *cell,
				    GtkTreeModel *tree_model,
				    GtkTreeIter *iter, GdauiRawGrid *grid)
{
	GdauiSetGroup *group;
	guint attributes;
	gboolean to_be_deleted = FALSE;
	ColumnData *column_data;

	group = g_object_get_data (G_OBJECT (tree_column), "__gdaui_group");
	column_data = get_column_data (grid, group);
	
	if (group->group->nodes_source) {
		/* parameters depending on a GdaDataModel */
		GList *values = NULL;
		GdaSetSource *source;

		source = g_object_get_data (G_OBJECT (tree_column), "source");

		/* NOTE:
		 * For performances reasons we want to provide, if possible, all the values required by the combo cell
		 * renderer to draw whatever it wants, without further making it search for the values it wants in
		 * source->data_model.
		 *
		 * For this purpose, we try to get a complete list of values making one row of the node->data_for_params
		 * data model, so that the combo cell renderer has all the values it wants.
		 *
		 * NOTE2:
		 * This optimization is required anyway when source->data_model can be changed depending on
		 * external events and we can't know when it has changed.
		 */
		attributes = _gdaui_utility_proxy_compute_attributes_for_group (group, grid->priv->store, grid->priv->iter,
										  iter, &to_be_deleted);
		values = _gdaui_utility_proxy_compute_values_for_group (group, grid->priv->store, grid->priv->iter, iter,
									  TRUE);
		if (!values) {
			values = _gdaui_utility_proxy_compute_values_for_group (group, grid->priv->store, 
									 grid->priv->iter, iter, FALSE);
			g_object_set (G_OBJECT (cell), 
				      "values_display", values,
				      "value_attributes", attributes,
				      "editable", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "show_expander", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "cell_background", GDAUI_COLOR_NORMAL_MODIF,
				      "cell_background-set", 
				      ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
				      "to_be_deleted", to_be_deleted, 
				      "visible", !(attributes & GDA_VALUE_ATTR_UNUSED),
				      NULL);
			g_list_free (values);
		}
		else {
			g_object_set (G_OBJECT (cell), 
				      "values_display", values,
				      "value_attributes", attributes,
				      "editable", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "show_expander", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
				      "cell_background", GDAUI_COLOR_NORMAL_MODIF,
				      "cell_background-set", 
				      ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
				      "to_be_deleted", to_be_deleted, 
				      "visible", !(attributes & GDA_VALUE_ATTR_UNUSED),
				      NULL);
			g_list_free (values);
		}
	}
	else {
		/* single direct parameter */
		gint col;
		gint offset;
		GValue *value;
		gint row;

		offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (grid->priv->proxy));

		g_assert (g_slist_length (group->group->nodes) == 1);
		col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
				     GDA_SET_NODE (group->group->nodes->data)->holder);
		gtk_tree_model_get (GTK_TREE_MODEL (grid->priv->store), iter, 
				    DATA_STORE_COL_TO_DELETE, &to_be_deleted,
				    DATA_STORE_COL_MODEL_ROW, &row,
				    col, &value, 
				    offset + col, &attributes, -1);
		g_object_set (G_OBJECT (cell), 
			      "value", value,
			      "value_attributes", attributes,
			      "editable", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "cell_background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set", ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to_be_deleted", to_be_deleted, 
			      "visible", !(attributes & GDA_VALUE_ATTR_UNUSED),
			      NULL);
	}
}

/*
 * Set the attributes for each information cell renderer, 
 * called by each cell renderer before actually displaying anything.
 */
static void
cell_renderer_info_set_attributes (GtkTreeViewColumn *tree_column,
				   GtkCellRenderer *cell,
				   GtkTreeModel *tree_model,
				   GtkTreeIter *iter, GdauiRawGrid *grid)
{
	GdauiSetGroup *group;
	guint attributes;
	gboolean to_be_deleted = FALSE;
	ColumnData *column_data;

	group = g_object_get_data (G_OBJECT (tree_column), "__gdaui_group");
	column_data = get_column_data (grid, group);
	
	if (group->group->nodes_source) {
		/* parameters depending on a GdaDataModel */
		GdaSetSource *source;

		source = g_object_get_data (G_OBJECT (tree_column), "source");

		attributes = _gdaui_utility_proxy_compute_attributes_for_group (group, grid->priv->store, grid->priv->iter, 
										  iter, &to_be_deleted);
		g_object_set (G_OBJECT (cell), 
			      "editable", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "value_attributes", attributes,
			      "cell_background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set", ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to_be_deleted", to_be_deleted, 
			      "visible", column_data->info_shown && !(attributes & GDA_VALUE_ATTR_UNUSED),
			      NULL);
	}
	else {
		/* single direct parameter */
		gint col;
		gint offset;
		gint row;
		
		offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (grid->priv->proxy));

		g_assert (g_slist_length (group->group->nodes) == 1);
		col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
				     GDA_SET_NODE (group->group->nodes->data)->holder);
		gtk_tree_model_get (GTK_TREE_MODEL (grid->priv->store), iter, 
				    DATA_STORE_COL_TO_DELETE, &to_be_deleted,
				    DATA_STORE_COL_MODEL_ROW, &row,
				    offset + col, &attributes, -1);
		g_object_set (G_OBJECT (cell), 
			      "editable", !column_data->data_locked && !(attributes & GDA_VALUE_ATTR_NO_MODIF),
			      "value_attributes", attributes,
			      "cell_background", GDAUI_COLOR_NORMAL_MODIF,
			      "cell_background-set", ! (attributes & GDA_VALUE_ATTR_IS_UNCHANGED) || to_be_deleted,
			      "to_be_deleted", to_be_deleted, 
			      "visible", column_data->info_shown && !(attributes & GDA_VALUE_ATTR_UNUSED),
			      NULL);
	}
}

static gboolean set_iter_from_path (GdauiRawGrid *grid, const gchar *path, GtkTreeIter *iter);

/*
 * Callback when a value displayed as a GtkCellRenderer has been changed, for single parameter cell renderers
 *
 * Apply the new_value to the GTkTreeModel (grid->priv->store)
 */
static void
data_cell_value_changed (GtkCellRenderer *renderer, const gchar *path, const GValue *new_value, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GdauiSetGroup *group;
	
	group = g_object_get_data (G_OBJECT (renderer), "__gdaui_group");
	g_assert (g_slist_length (group->group->nodes) == 1);

	if (set_iter_from_path (grid, path, &iter)) {
		    gint col;
		    col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
					 GDA_SET_NODE (group->group->nodes->data)->holder);
		    gdaui_data_store_set_value (grid->priv->store, &iter, col, new_value);
	}
}


/*
 * Callback when a value displayed as a GdauiDataCellRendererCombo has been changed.
 *
 * Apply the new_value to the GTkTreeModel (grid->priv->store)
 */
static void
data_cell_values_changed (GtkCellRenderer *renderer, const gchar *path, 
			  GSList *new_values, GSList *all_new_values, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GdauiSetGroup *group;
	
	group = g_object_get_data (G_OBJECT (renderer), "__gdaui_group");
	g_assert (group->group->nodes_source);

	if (new_values)
		g_return_if_fail (g_slist_length (group->group->nodes) == g_slist_length (new_values));
	else
		/* the reason for not having any value is that the GdauiDataCellRendererCombo had no selected item */
		return;
	
	if (set_iter_from_path (grid, path, &iter)) {
		GSList *list, *params;
		gint col, proxy_row;

		proxy_row = gdaui_data_store_get_row_from_iter (grid->priv->store, &iter);

		/* update the GdauiDataStore */
		for (params = group->group->nodes, list = new_values; 
		     list; 
		     params = params->next, list = list->next) {
			col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
					     GDA_SET_NODE (params->data)->holder);
			gdaui_data_store_set_value (grid->priv->store, &iter, col, (GValue *)(list->data));
		}
		
#ifdef PROXY_STORE_EXTRA_VALUES
		/* call gda_data_proxy_set_model_row_value() */
		gint i;
		for (i = 0; i < group->source->shown_n_cols; i++) {
			GValue *value;

			col = group->nodes_source->shown_cols_index[i];
			value = (GValue *) g_slist_nth_data (all_new_values, col);
			gda_data_proxy_set_model_row_value (grid->priv->proxy,
							    group->group->nodes_source->data_model,
							    proxy_row, col, value);
		}
#endif
	}
}

static gboolean
set_iter_from_path (GdauiRawGrid *grid, const gchar *path, GtkTreeIter *iter)
{
	GtkTreePath *treepath;

	g_assert (path);

	treepath = gtk_tree_path_new_from_string (path);
	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (grid->priv->store), iter, treepath)) {
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
	GSList *nodes;

	group = g_object_get_data (G_OBJECT (tree_column), "__gdaui_group");
	g_assert (group);

	for (nodes = group->group->nodes; nodes; nodes = nodes->next) {
		GdaHolder *param = ((GdaSetNode*) nodes->data)->holder;
		gint pos;
		g_assert (param);
		
		pos = g_slist_index (GDA_SET (grid->priv->iter)->holders, param);
		if (pos >= 0) {
			gda_data_proxy_set_ordering_column (grid->priv->proxy, pos, NULL);
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
	GtkTreeModel *tree_model;
	gint col;
	gint offset;
	GValue *attribute;

	offset = gda_data_model_get_n_columns (gda_data_proxy_get_proxied_model (grid->priv->proxy));

	group = g_object_get_data (G_OBJECT (renderer), "__gdaui_group");
	tree_model = GTK_TREE_MODEL (grid->priv->store);

	treepath = gtk_tree_path_new_from_string (path);
	if (! gtk_tree_model_get_iter (tree_model, &iter, treepath)) {
		gtk_tree_path_free (treepath);
		g_warning ("Can't get iter for path %s", path);
		return;
	}
	gtk_tree_path_free (treepath);
	
	g_value_set_uint (attribute = gda_value_new (G_TYPE_UINT), requested_action);
	if (group->group->nodes_source) {
		/* parameters depending on a GdaDataModel */
		gint proxy_row;
		GSList *list;
		proxy_row = gdaui_data_store_get_row_from_iter (grid->priv->store, &iter);

		for (list = group->group->nodes; list; list = list->next) {
			col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
					     GDA_SET_NODE (list->data)->holder);
			gdaui_data_store_set_value (grid->priv->store, &iter, offset + col, attribute);
		}

#ifdef PROXY_STORE_EXTRA_VALUES
		/* call gda_data_proxy_set_model_row_value() */
		gint i;
		for (i = 0; i < group->nodes_source->shown_n_cols; i++) {
			col = group->nodes_source->shown_cols_index[i];
			
			if (requested_action & GDA_VALUE_ATTR_IS_NULL)
				gda_data_proxy_set_model_row_value (grid->priv->proxy, 
								    group->nodes_source->data_model,
								    proxy_row, col, NULL);
			else {
				if (requested_action & GDA_VALUE_ATTR_IS_UNCHANGED)
					gda_data_proxy_clear_model_row_value (grid->priv->proxy, 
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

		g_assert (g_slist_length (group->group->nodes) == 1);
		col = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
				     GDA_SET_NODE (group->group->nodes->data)->holder);
		gdaui_data_store_set_value (grid->priv->store, &iter, offset + col, attribute);
	}
	gda_value_free (attribute);
}

static void
action_new_cb (GtkAction *action, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	if (gdaui_data_store_append (grid->priv->store, &iter)) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (grid->priv->store), &iter);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (grid), path, NULL, FALSE);
		gtk_tree_path_free (path);
	}
}

static void
action_delete_cb (GtkAction *action, GdauiRawGrid *grid)
{
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
			gdaui_data_store_delete (grid->priv->store, &iter);
			g_list_foreach (sel_rows, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (sel_rows);
			sel_rows = gtk_tree_selection_get_selected_rows (select, &model); 
		}
		else
			sel_rows = sel_rows->next;
	}
}

static void
action_undelete_cb (GtkAction *action, GdauiRawGrid *grid)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GList *sel_rows, *cur_row;
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	sel_rows = gtk_tree_selection_get_selected_rows (select, &model);
	cur_row = sel_rows;
	while (cur_row) {
		gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (cur_row->data));
		gdaui_data_store_undelete (grid->priv->store, &iter);
		cur_row = g_list_next (cur_row);
	}
	g_list_foreach (sel_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (sel_rows);
}

static void
action_commit_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gint row;
	GError *error = NULL;
	gboolean allok = TRUE;
	gint mod1, mod2;

	mod1 = gda_data_proxy_get_n_modified_rows (grid->priv->proxy);
	row = gda_data_model_iter_get_row (grid->priv->iter);
	if (grid->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) {
		gint newrow;

		allok = gda_data_proxy_apply_row_changes (grid->priv->proxy, row, &error);
		if (allok) {
			newrow = gda_data_model_iter_get_row (grid->priv->iter);
			if (row != newrow) /* => current row has changed because the proxy had to emit a "row_removed" when
					      actually succeeded the commit 
					      => we need to come back to that row
					   */
				gda_data_model_iter_move_to_row (grid->priv->iter, row);
		}
	}
	else
		allok = gda_data_proxy_apply_all_changes (grid->priv->proxy, &error);

	mod2 = gda_data_proxy_get_n_modified_rows (grid->priv->proxy);
	if (!allok) {
		if (mod1 != mod2)
			/* the data model has changed while doing the writing */
			_gdaui_utility_display_error ((GdauiDataWidget *) grid, FALSE, error);
		else
			_gdaui_utility_display_error ((GdauiDataWidget *) grid, TRUE, error);
		g_error_free (error);
	}
}

static void
action_reset_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gda_data_proxy_cancel_all_changes (grid->priv->proxy);
	gda_data_model_send_hint (GDA_DATA_MODEL (grid->priv->proxy), GDA_DATA_MODEL_HINT_REFRESH, NULL);
}

static void
action_first_chunck_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gda_data_proxy_set_sample_start (grid->priv->proxy, 0);
}

static void
action_prev_chunck_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gint sample_size, sample_start;

	sample_size = gda_data_proxy_get_sample_size (grid->priv->proxy);
	if (sample_size > 0) {
		sample_start = gda_data_proxy_get_sample_start (grid->priv->proxy);
		sample_start -= sample_size;
		gda_data_proxy_set_sample_start (grid->priv->proxy, sample_start);
	}
}

static void
action_next_chunck_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gint sample_size, sample_start;

	sample_size = gda_data_proxy_get_sample_size (grid->priv->proxy);
	if (sample_size > 0) {
		sample_start = gda_data_proxy_get_sample_start (grid->priv->proxy);
		sample_start += sample_size;
		gda_data_proxy_set_sample_start (grid->priv->proxy, sample_start);
	}
}

static void
action_last_chunck_cb (GtkAction *action, GdauiRawGrid *grid)
{
	gda_data_proxy_set_sample_start (grid->priv->proxy, G_MAXINT);
}

static void
hide_filter_window (GdauiRawGrid *grid)
{
	gtk_widget_hide (grid->priv->filter_window);
	gtk_grab_remove (grid->priv->filter_window);
}

static gboolean
filter_event (GtkWidget *widget, GdkEventAny *event, GdauiRawGrid *grid)
{
	hide_filter_window (grid);
	return TRUE;
}

static gboolean
key_press_filter_event (GtkWidget *widget, GdkEventKey *event, GdauiRawGrid *grid)
{
	if (event->keyval == GDK_Escape ||
	    event->keyval == GDK_Tab ||
            event->keyval == GDK_KP_Tab ||
            event->keyval == GDK_ISO_Left_Tab) {
		hide_filter_window (grid);
		return TRUE;
	}
	return FALSE;
}

static void
filter_position_func (GtkWidget *widget,
		      GtkWidget *search_dialog,
		      gpointer   user_data)
{
	gint x, y;
	gint tree_x, tree_y;
	gint tree_width, tree_height;
	GdkWindow *window = GTK_WIDGET (widget)->window;
	GdkScreen *screen = gdk_drawable_get_screen (window);
	GtkRequisition requisition;
	gint monitor_num;
	GdkRectangle monitor;

	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
	gtk_widget_realize (search_dialog);

	gdk_window_get_origin (window, &tree_x, &tree_y);
	gdk_drawable_get_size (window,
			       &tree_width,
			       &tree_height);
	gtk_widget_size_request (search_dialog, &requisition);

	if (tree_x + tree_width > gdk_screen_get_width (screen))
		x = gdk_screen_get_width (screen) - requisition.width;
	else if (tree_x + tree_width - requisition.width < 0)
		x = 0;
	else
		x = tree_x + tree_width - requisition.width;

	if (tree_y + tree_height + requisition.height > gdk_screen_get_height (screen))
		y = gdk_screen_get_height (screen) - requisition.height;
	else if (tree_y + tree_height < 0) /* isn't really possible ... */
		y = 0;
	else
		y = tree_y + tree_height;

	gtk_window_move (GTK_WINDOW (search_dialog), x, y);
}

static gboolean
popup_grab_on_window (GdkWindow  *window,
                      guint32     activate_time) 
{
        if (gdk_pointer_grab (window, TRUE,
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
                              NULL, NULL,
                              activate_time) == 0) {

                if (gdk_keyboard_grab (window, TRUE, activate_time) == 0) 
                        return TRUE;
		else {
                        gdk_pointer_ungrab (activate_time);
                        return FALSE;
                }
        }

        return FALSE;
}

static void
action_filter_cb (GtkAction *action, GdauiRawGrid *grid)
{
	GtkWidget *toplevel;
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (grid));
	
	if (!grid->priv->filter_window) {
		/* create filter window */
		GtkWidget *frame, *vbox;
	       
		grid->priv->filter_window = gtk_window_new (GTK_WINDOW_POPUP);

		gtk_widget_set_events (grid->priv->filter_window,
				       gtk_widget_get_events (grid->priv->filter_window) | GDK_KEY_PRESS_MASK);

		if (GTK_WIDGET_TOPLEVEL (toplevel) && GTK_WINDOW (toplevel)->group)
			gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
						     GTK_WINDOW (grid->priv->filter_window));
		
		g_signal_connect (grid->priv->filter_window, "delete_event",
				  G_CALLBACK (filter_event), grid);
		g_signal_connect (grid->priv->filter_window, "button_press_event",
				  G_CALLBACK (filter_event), grid);
		g_signal_connect (grid->priv->filter_window, "key_press_event",
				  G_CALLBACK (key_press_filter_event), grid);
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
		gtk_widget_show (frame);
		gtk_container_add (GTK_CONTAINER (grid->priv->filter_window), frame);
		
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (vbox);
		gtk_container_add (GTK_CONTAINER (frame), vbox);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);
		
		/* add real filter widget */
		if (! grid->priv->filter) {
			grid->priv->filter = gdaui_data_widget_filter_new (GDAUI_DATA_WIDGET (grid));
			gtk_widget_show (grid->priv->filter);
		}
		gtk_container_add (GTK_CONTAINER (vbox), grid->priv->filter);
	}
	else if (GTK_WIDGET_TOPLEVEL (toplevel)) {
		if (GTK_WINDOW (toplevel)->group)
			gtk_window_group_add_window (GTK_WINDOW (toplevel)->group,
						     GTK_WINDOW (grid->priv->filter_window));
		else if (GTK_WINDOW (grid->priv->filter_window)->group)
			gtk_window_group_remove_window (GTK_WINDOW (grid->priv->filter_window)->group,
							GTK_WINDOW (grid->priv->filter_window));
	}

	/* move the filter window to a correct location */
	/* FIXME: let the user specify the position function like GtkTreeView -> search_position_func() */
	gtk_grab_add (grid->priv->filter_window);
	filter_position_func (GTK_WIDGET (grid), grid->priv->filter_window, NULL);
	gtk_widget_show (grid->priv->filter_window);
	popup_grab_on_window (grid->priv->filter_window->window,
			      gtk_get_current_event_time ());	
}

/*
 * Catch any event in the GtkTreeView widget
 */
static gboolean 
tree_view_event_cb (GtkWidget *treeview, GdkEvent *event, GdauiRawGrid *grid)
{
	gboolean done = FALSE;

	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey *ekey = (GdkEventKey *) event;
		guint modifiers = gtk_accelerator_get_default_mod_mask ();		

		/* Tab to move one column left or right */
		if (ekey->keyval == GDK_Tab) {
			GtkTreeViewColumn *column;
			GtkTreePath *path;

			/* FIXME: if a column is currently edited, then make sure the editing of that cell is not cancelled */
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
		if (ekey->keyval == GDK_Delete) {
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
					gdaui_data_store_undelete (grid->priv->store, &iter);
				else
					gdaui_data_store_delete (grid->priv->store, &iter);
				cur_row = g_list_next (cur_row);
			}
			g_list_foreach (sel_rows, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (sel_rows);
			
			done = TRUE;
		}
	}

	return done;
}

static void menu_select_all_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_unselect_all_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_show_columns_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_save_as_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_set_filter_cb (GtkWidget *widget, GdauiRawGrid *grid);
static void menu_unset_filter_cb (GtkWidget *widget, GdauiRawGrid *grid);
static GtkWidget *new_menu_item (const gchar *label,
				 gboolean pixmap,
				 GCallback cb_func,
				 gpointer user_data);
static GtkWidget *new_check_menu_item (const gchar *label,
				       gboolean active,
				       GCallback cb_func,
				       gpointer user_data);

static gint
tree_view_popup_button_pressed_cb (GtkWidget *widget, GdkEventButton *event, GdauiRawGrid *grid)
{
	GtkWidget *menu;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkSelectionMode sel_mode;

        if (event->button != 3)
                return FALSE;

	tree_view = GTK_TREE_VIEW (grid);
	selection = gtk_tree_view_get_selection (tree_view);
	sel_mode = gtk_tree_selection_get_mode (selection);

        /* create the menu */
        menu = gtk_menu_new ();
	if (sel_mode == GTK_SELECTION_MULTIPLE)
		gtk_menu_append (GTK_MENU (menu),
				 new_menu_item (_("Select _All"), FALSE, 
							 G_CALLBACK (menu_select_all_cb), grid));

	if ((sel_mode == GTK_SELECTION_SINGLE) || (sel_mode == GTK_SELECTION_MULTIPLE))
		gtk_menu_append (GTK_MENU (menu),
				 new_menu_item (_("_Clear Selection"), FALSE,
							 G_CALLBACK (menu_unselect_all_cb), grid));
        gtk_menu_append (GTK_MENU (menu),
			 new_check_menu_item (_("Show Column _Titles"),
					      gtk_tree_view_get_headers_visible (tree_view),
					      G_CALLBACK (menu_show_columns_cb), grid));

	gtk_menu_append (GTK_MENU (menu),
				 new_menu_item (_("_Set filter"), FALSE,
							 G_CALLBACK (menu_set_filter_cb), grid));
	gtk_menu_append (GTK_MENU (menu),
				 new_menu_item (_("_Unset filter"), FALSE,
							 G_CALLBACK (menu_unset_filter_cb), grid));

	if (sel_mode != GTK_SELECTION_NONE) {
		gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());
		gtk_menu_append (GTK_MENU (menu), new_menu_item (GTK_STOCK_SAVE_AS, TRUE,
					       G_CALLBACK (menu_save_as_cb), grid));
	}

	/* allow listeners to add their custom menu items */
        g_signal_emit (G_OBJECT (grid), gdaui_raw_grid_signals [POPULATE_POPUP], 0, GTK_MENU (menu));
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
        gtk_widget_show_all (menu);
	
        return TRUE;
}

static GtkWidget *
new_menu_item (const gchar *label,
	       gboolean pixmap,
	       GCallback cb_func,
	       gpointer user_data)
{
	GtkWidget *item;

	if (pixmap)
		item = gtk_image_menu_item_new_from_stock (label, NULL);
	else
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
menu_select_all_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	
	gtk_tree_selection_select_all (selection);
}

static void
menu_unselect_all_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	
	gtk_tree_selection_unselect_all (selection);
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

static void save_as_response_cb (GtkDialog *dialog, guint response_id, GdauiRawGrid *grid);

static void
menu_save_as_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *filename;
	GtkWidget *types;
	GtkWidget *hbox, *table, *check;
	char *str;
	GtkTreeSelection *sel;
	gint selrows;

	/* create dialog box */
	dialog = gtk_dialog_new_with_buttons (_("Saving Data"),
					      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (grid))), 0,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	str = g_strdup_printf ("<big><b>%s:</b></big>\n%s", _("Saving data to a file"), 
			       _("The data will be exported without any of the modifications which may "
				 "have been made and have not been committed."));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        g_free (str);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, TRUE, 2);

	str = g_strdup_printf ("<b>%s:</b>", _("File name"));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
        g_free (str);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, TRUE, 2);

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 5);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	filename = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	g_object_set_data (G_OBJECT (dialog), "filename", filename);
	gtk_box_pack_start (GTK_BOX (hbox), filename, TRUE, TRUE, 0);

	str = g_strdup_printf ("<b>%s:</b>", _("Details")); 
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
        g_free (str);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, TRUE, 2);

	hbox = gtk_hbox_new (FALSE, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 5);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_widget_show (label);
	
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
	gtk_widget_show (table);

	label = gtk_label_new (_("Limit to selection?"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	selrows = gtk_tree_selection_count_selected_rows (sel);
	if (selrows <= 0)
		gtk_widget_set_sensitive (label, FALSE);

	check = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (table), check, 1, 2, 0, 1);
	gtk_widget_show (check);
	if (selrows <= 0)
		gtk_widget_set_sensitive (check, FALSE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
	g_object_set_data (G_OBJECT (dialog), "sel_only", check);

	label = gtk_label_new (_("File type:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show (label);

	types = gtk_combo_box_new_text ();
	gtk_table_attach_defaults (GTK_TABLE (table), types, 1, 2, 1, 2);
	gtk_widget_show (label);
	g_object_set_data (G_OBJECT (dialog), "types", types);

	gtk_combo_box_append_text (GTK_COMBO_BOX (types), _("Tab-delimited"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (types), _("Comma-delimited"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (types), _("XML"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (types), grid->priv->export_type);
	
	/* run the dialog */
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (save_as_response_cb), grid);
	gtk_widget_show_all (dialog);
}

static gboolean confirm_file_overwrite (GtkWindow *parent, const gchar *path);

static void
save_as_response_cb (GtkDialog *dialog, guint response_id, GdauiRawGrid *grid)
{
	GtkWidget *types;
	gint export_type;
	GtkWidget *filename;
	gboolean selection_only = FALSE;
	
	if (response_id == GTK_RESPONSE_OK) {
		gchar *body;
		gchar *path;
		GList *columns, *list;
		gint *cols, nb_cols;
		gint *rows = NULL, nb_rows = 0;
		GdaHolder *param;
		GdaSet *paramlist;

		types = g_object_get_data (G_OBJECT (dialog), "types");
		filename = g_object_get_data (G_OBJECT (dialog), "filename");
		selection_only = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON 
							       (g_object_get_data (G_OBJECT (dialog), "sel_only")));

		/* output columns computation */
		columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
		cols = g_new (gint, gda_data_model_get_n_columns (GDA_DATA_MODEL (grid->priv->data_model)));
		nb_cols = 0;
		list = columns;
		while (list) {
			if (gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data))) {
				GdauiSetGroup *group;
				GSList *params;

				group = g_object_get_data (G_OBJECT (list->data), "__gdaui_group");
				for (params = group->group->nodes; params; nb_cols ++, params = params->next) 
					cols [nb_cols] = g_slist_index (((GdaSet *)grid->priv->iter)->holders,
									GDA_SET_NODE (params->data)->holder);
			}
			list = g_list_next (list);
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
		grid->priv->export_type = export_type;
		switch (export_type) {
		case 0:
			param = gda_holder_new_string ("SEPARATOR", "\t");
			paramlist = gda_set_new (NULL);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (grid->priv->data_model), 
								GDA_DATA_MODEL_IO_TEXT_SEPARATED, 
								cols, nb_cols, rows, nb_rows, paramlist);
			g_object_unref (paramlist);
			break;
		case 1:
			param = gda_holder_new_string ("SEPARATOR", ",");
			paramlist = gda_set_new (NULL);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (grid->priv->data_model), 
								GDA_DATA_MODEL_IO_TEXT_SEPARATED, 
								cols, nb_cols, rows, nb_rows, paramlist);
			g_object_unref (paramlist);
			break;
		case 2:
			param = NULL;
			body = (gchar *) g_object_get_data (G_OBJECT (grid->priv->data_model), "name");
			if (body)
				param = gda_holder_new_string ("NAME", body);
			paramlist = gda_set_new (NULL);
			gda_set_add_holder (paramlist, param);
			g_object_unref (param);
			body = gda_data_model_export_to_string (GDA_DATA_MODEL (grid->priv->data_model), 
								GDA_DATA_MODEL_IO_DATA_ARRAY_XML, 
								cols, nb_cols, rows, nb_rows, paramlist);
			g_object_unref (paramlist);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
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
	gchar *msg, *str;

	msg = g_strdup_printf (_("File '%s' already exists.\n"
				 "Do you want to overwrite it?"), path);

	/* build the dialog */
	str = g_strconcat ("<span weight=\"bold\">%s</span>\n",
			   msg,
			   _("If you choose yes, the contents will be lost."),
			   NULL);
	g_free (msg);
	dialog = gtk_message_dialog_new_with_markup (parent, 
						     GTK_DIALOG_DESTROY_WITH_PARENT |
						     GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_CLOSE, "%s", str);
	g_free (str);
		
	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_NO);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      gtk_button_new_from_stock (GTK_STOCK_YES),
				      GTK_RESPONSE_YES);
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
menu_set_filter_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	action_filter_cb (NULL, grid);
}

static void
menu_unset_filter_cb (GtkWidget *widget, GdauiRawGrid *grid)
{
	gda_data_proxy_set_filter_expr (grid->priv->proxy, NULL, NULL);
}


static void
tree_view_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GdauiRawGrid *grid)
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
	gboolean row_selected = FALSE;
	gint has_selection;
	
	/* block the GdaDataModelIter' "changed" signal */
	g_signal_handlers_block_by_func (grid->priv->iter,
					 G_CALLBACK (iter_row_changed_cb), grid);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE) {
		has_selection = gtk_tree_selection_count_selected_rows (selection);
		if (has_selection == 1) {
			GList *sel_rows;

			sel_rows = gtk_tree_selection_get_selected_rows (selection, &model);
			has_selection = gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) (sel_rows->data)) ? 1 : 0;
			g_list_foreach (sel_rows, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (sel_rows);
		}
	}
	else
		has_selection = gtk_tree_selection_get_selected (selection, &model, &iter) ? 1 : 0;

	if (has_selection == 1) {
		if (!gda_data_model_iter_move_to_row (grid->priv->iter, 
						     gdaui_data_store_get_row_from_iter (grid->priv->store, 
											    &iter))) {
			/* selection changing is refused, return to the current selected row */
			GtkTreePath *path;
			path = gtk_tree_path_new_from_indices (gda_data_model_iter_get_row (grid->priv->iter), -1);
			g_signal_handlers_block_by_func (G_OBJECT (selection),
							 G_CALLBACK (tree_view_selection_changed_cb), grid);

			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);
			g_signal_handlers_unblock_by_func (G_OBJECT (selection),
							   G_CALLBACK (tree_view_selection_changed_cb), grid);

			gtk_tree_path_free (path);
		}
		row_selected = TRUE;
	}
	else {
		/* render all the parameters invalid, and make the iter point to row -1 */
		gda_data_model_iter_invalidate_contents (grid->priv->iter);
		gda_data_model_iter_move_to_row (grid->priv->iter, -1);
	}

#ifdef debug_signal
	g_print (">> 'SELECTION_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (grid), grid);
#endif
	g_signal_emit (G_OBJECT (grid), gdaui_raw_grid_signals[SELECTION_CHANGED], 0, row_selected);
#ifdef debug_signal
	g_print ("<< 'SELECTION_CHANGED' from %s %p\n", G_OBJECT_TYPE_NAME (grid), grid);
#endif

	/* unblock the GdaDataModelIter' "changed" signal */
	g_signal_handlers_unblock_by_func (grid->priv->iter,
					   G_CALLBACK (iter_row_changed_cb), grid);
}

static ColumnData *
get_column_data (GdauiRawGrid *grid, GdauiSetGroup *group)
{
	GSList *list;
	for (list = grid->priv->columns_data; list; list = list->next) {
		if (COLUMN_DATA (list->data)->group == group)
			return COLUMN_DATA (list->data);
	}

	return NULL;
}

/**
 * gdaui_raw_grid_set_sample_size
 * @grid:
 * @sample_size:
 *
 */
void
gdaui_raw_grid_set_sample_size (GdauiRawGrid *grid, gint sample_size)
{
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));
	g_return_if_fail (grid->priv);

	gda_data_proxy_set_sample_size (grid->priv->proxy, sample_size);
}

/**
 * gdaui_raw_grid_set_sample_start
 * @grid:
 * @sample_start:
 *
 */
void
gdaui_raw_grid_set_sample_start (GdauiRawGrid *grid, gint sample_start)
{
	g_return_if_fail (grid && GDAUI_IS_RAW_GRID (grid));
	g_return_if_fail (grid->priv);
	
	gda_data_proxy_set_sample_start (grid->priv->proxy, sample_start);
}


/*
 * GdauiDataWidget interface
 */

static GdaDataProxy *
gdaui_raw_grid_get_proxy (GdauiDataWidget *iface)
{
	GdauiRawGrid *grid;
	g_return_val_if_fail (iface && GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, NULL);

	return grid->priv->proxy;
}

static void
gdaui_raw_grid_set_column_editable (GdauiDataWidget *iface, gint column, gboolean editable)
{
	GdauiRawGrid *grid;
	GdaHolder *param;
	ColumnData *column_data;
	GdauiSetGroup *group;

	g_return_if_fail (iface && GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	g_return_if_fail (grid->priv);
	
	if (grid->priv->data_model) {
		editable = editable && !gda_data_proxy_is_read_only (grid->priv->proxy);

		param = gda_data_model_iter_get_holder_for_field (grid->priv->iter, column);
		g_return_if_fail (param);

		group = _gdaui_set_get_group (grid->priv->iter_info, param);
		g_return_if_fail (group);

		column_data = get_column_data (grid, group);
		g_return_if_fail (column_data);
		
		if (editable && !gda_data_proxy_is_read_only (grid->priv->proxy))
			column_data->data_locked = FALSE;
		else
			column_data->data_locked = TRUE;
	}
}

static void
gdaui_raw_grid_show_column_actions (GdauiDataWidget *iface, gint column, gboolean show_actions)
{
	GdauiRawGrid *grid;

	g_return_if_fail (iface && GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	g_return_if_fail (grid->priv);

	if (column >= 0) {
		GdaHolder *param;
		GdauiSetGroup *group;
		ColumnData *cdata;

		/* setting applies only to the @column column */
		param = gda_data_model_iter_get_holder_for_field (grid->priv->iter, column);
		g_return_if_fail (param);
		
		group = _gdaui_set_get_group (grid->priv->iter_info, param);
		g_return_if_fail (group);

		cdata = get_column_data (grid, group);
		g_return_if_fail (cdata);
		
		if (show_actions != cdata->info_shown) {
			cdata->info_shown = show_actions;
			g_object_set (G_OBJECT (cdata->info_cell), "visible", cdata->info_shown, NULL);
		}
	}
	else {
		/* setting applies to all columns */
		GSList *list;
		for (list = grid->priv->columns_data; list; list = list->next) {
			ColumnData *cdata = (ColumnData*)(list->data);
			if (show_actions != cdata->info_shown) {
				cdata->info_shown = show_actions;
				g_object_set (G_OBJECT (cdata->info_cell), "visible", cdata->info_shown, NULL);
			}
		}
		grid->priv->default_show_info_cell = show_actions;
	}
}

static void
gdaui_raw_grid_col_set_show (GdauiDataWidget *iface, gint column, gboolean shown)
{
	GdauiRawGrid *grid;
	gint pos = -1;
	GtkTreeViewColumn *viewcol;
	GdaSetGroup *group;
	GdaHolder *param;

	g_return_if_fail (iface && GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	g_return_if_fail (grid->priv);

	param = gda_data_model_iter_get_holder_for_field (grid->priv->iter, column);
	g_return_if_fail (param);

	group = gda_set_get_group ((GdaSet *)grid->priv->iter, param);	
	pos = g_slist_index (((GdaSet *)grid->priv->iter)->groups_list, group);	
	g_assert (pos >= 0);

	viewcol = gtk_tree_view_get_column (GTK_TREE_VIEW (grid), pos);

	/* Sets the column's visibility */
	gtk_tree_view_column_set_visible (viewcol, shown);
}


static GtkActionGroup *
gdaui_raw_grid_get_actions_group (GdauiDataWidget *iface)
{
	GdauiRawGrid *grid;
	
	g_return_val_if_fail (iface && GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, NULL);

	return grid->priv->actions_group;
}

static GdaDataModelIter *
gdaui_raw_grid_widget_get_data_set (GdauiDataWidget *iface)
{
	GdauiRawGrid *grid;
	
	g_return_val_if_fail (iface && GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, NULL);

	return grid->priv->iter;
}


static GdaDataModel *
gdaui_raw_grid_widget_get_gda_model (GdauiDataWidget *iface)
{
	GdauiRawGrid *grid;
	g_return_val_if_fail (iface && GDAUI_IS_RAW_GRID (iface), NULL);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, NULL);

	/* warning: here we are returning the *original* GdaDataModel */

	return GDA_DATA_MODEL (grid->priv->data_model);
}

static void
paramlist_public_data_changed_cb (GdauiSet *info, GdauiRawGrid *grid)
{
	GList *columns, *list;

	/* info cells */
	if (grid->priv->columns_data) {
		GSList *list = grid->priv->columns_data;
		while (list) {
                        g_free (((ColumnData *) (list->data))->tooltip_text);
			g_free (list->data);
			list = g_slist_next (list);
		}
		g_slist_free (grid->priv->columns_data);
		grid->priv->columns_data = NULL;
	}

	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
	if (columns) {
		list = columns;
		while (list) {
			gtk_tree_view_remove_column (GTK_TREE_VIEW (grid),
						     (GtkTreeViewColumn*) (list->data));
			list = g_list_next (list);
		}
		g_list_free (columns);
	}

	init_tree_view (grid);
}

static void
paramlist_param_attr_changed_cb (GdaSet *paramlist, GdaHolder *param, 
				 const gchar *att_name, const GValue *att_value, GdauiRawGrid *grid)
{
	if (!strcmp (att_name, GDAUI_ATTRIBUTE_PLUGIN)) {
		/* TODO: be more specific and change only the cell renderer corresponding to @param */

		/* keep a list of hidden columns */
		gint i;
		gboolean *hidden_cols;
		GList *cols, *list;
		cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
		hidden_cols = g_new0 (gboolean, g_list_length (cols));
		for (list = cols, i = 0; list; list = list->next, i++) {
			if (!gtk_tree_view_column_get_visible (GTK_TREE_VIEW_COLUMN (list->data)))
				hidden_cols [i] = TRUE;
		}
		g_list_free (cols);

		/* re-create columns */
		paramlist_public_data_changed_cb (grid->priv->iter_info, grid);

		/* hide columns which were hidden */
		cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
		for (list = cols, i = 0; list; list = list->next, i++) {
			if (hidden_cols [i])
				gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (list->data), FALSE);
		}
		g_list_free (cols);
		g_free (hidden_cols);
	}
}

static GError *
iter_validate_set_cb (GdaDataModelIter *iter, GdauiRawGrid *grid)
{
GError *error = NULL;
	gint row = gda_data_model_iter_get_row (iter);
	
	if (row < 0)
		return NULL;

	if ((grid->priv->write_mode >= GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) &&
		/* write back the current row */
	    gda_data_proxy_row_has_changed (grid->priv->proxy, row) &&
	    !gda_data_proxy_apply_row_changes (grid->priv->proxy, row, &error)) {
		if (_gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataWidget *) grid, 
										error)) {
			gda_data_proxy_cancel_row_changes (grid->priv->proxy, row, -1);
			if (error) {
				g_error_free (error);
				error = NULL;
			}
		}
	}
	
	return error;	
}

static void
iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiRawGrid *grid)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
	
	if (row >= 0) {
		GtkSelectionMode mode;
		GtkTreeIter treeiter;

		mode = gtk_tree_selection_get_mode (selection);
		if (mode != GTK_SELECTION_SINGLE)
			gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
		path = gtk_tree_path_new_from_indices (row, -1);
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (grid->priv->store), &treeiter, path)) {
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
proxy_sample_changed_cb (GdaDataProxy *proxy, gint sample_start, gint sample_end, GdauiRawGrid *grid)
{
	/* bring back the vertical scrollbar to the top */
	gtk_adjustment_set_value (gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (grid)), 0.);
}

static void
proxy_row_updated_cb (GdaDataProxy *proxy, gint proxy_row, GdauiRawGrid *grid)
{
	if (grid->priv->write_mode == GDAUI_DATA_WIDGET_WRITE_ON_VALUE_ACTIVATED) {
		gint row;
		
		/* REM: this may lead to problems in the proxy because it is not re-entrant, solution: add a very short timeout */
		row = gda_data_model_iter_get_row (grid->priv->iter);
		if ((row >= 0) && (row == proxy_row)) {
			/* write back the current row */
			if (gda_data_proxy_row_has_changed (grid->priv->proxy, row)) {
				GError *error = NULL;

				/* If the write fails, the proxy sets the fields back to was they were. We do not want
				 * te get notified about this because we would the try again to write to the database,
				 * which would fail again, and so on. */
				g_signal_handlers_block_by_func(G_OBJECT(proxy), G_CALLBACK(proxy_row_updated_cb), grid);

				if (!gda_data_proxy_apply_row_changes (grid->priv->proxy, row, &error)) {
					gboolean discard;
					discard = _gdaui_utility_display_error_with_keep_or_discard_choice ((GdauiDataWidget *) grid, 
												     error);
					if (discard)
						gda_data_proxy_cancel_row_changes (grid->priv->proxy, row, -1);
					g_error_free (error);
				}

				g_signal_handlers_unblock_by_func(G_OBJECT(proxy), G_CALLBACK(proxy_row_updated_cb), grid);
			}
		}
	}
}

static void
proxy_reset_cb (GdaDataProxy *proxy, GdauiRawGrid *grid)
{
	g_object_ref (G_OBJECT (proxy));
	g_object_set (G_OBJECT (grid), "model", proxy, NULL);
	g_object_unref (G_OBJECT (proxy));
}

static void
gdaui_raw_grid_widget_set_gda_model (GdauiDataWidget *iface, GdaDataModel *model)
{
	GdauiRawGrid *grid;

	g_return_if_fail (iface && GDAUI_IS_RAW_GRID (iface));
	grid = GDAUI_RAW_GRID (iface);
	g_return_if_fail (grid->priv);
	
	g_object_set (grid, "model", model, NULL);
}

static void
gdaui_raw_grid_clean (GdauiRawGrid *grid)
{
	GList *columns, *list;

	/* columns */
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (grid));
	list = columns;
	while (list) {
		gtk_tree_view_remove_column ((GtkTreeView *) grid,
					     (GtkTreeViewColumn *) list->data);
		list = g_list_next (list);
	}
	g_list_free (columns);

	/* info cells */
	if (grid->priv->columns_data) {
		GSList *list = grid->priv->columns_data;
		while (list) {
                        g_free (((ColumnData *) (list->data))->tooltip_text);
			g_free (list->data);
			list = g_slist_next (list);
		}
		g_slist_free (grid->priv->columns_data);
		grid->priv->columns_data = NULL;
	}
	
	/* private data set */
	if (grid->priv->iter) {
		g_signal_handlers_disconnect_by_func (grid->priv->iter_info,
						      G_CALLBACK (paramlist_public_data_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (grid->priv->iter,
						      G_CALLBACK (paramlist_param_attr_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (grid->priv->iter,
						      G_CALLBACK (iter_row_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (grid->priv->iter,
						      G_CALLBACK (iter_validate_set_cb), grid);
		g_object_unref (grid->priv->iter);
		g_object_unref (grid->priv->iter_info);
		grid->priv->iter = NULL;
		grid->priv->iter_info = NULL;
	}
	
	/* proxy */
	if (grid->priv->proxy) {
		g_signal_handlers_disconnect_by_func (grid->priv->proxy, G_CALLBACK (proxy_sample_changed_cb), grid);
		g_signal_handlers_disconnect_by_func (grid->priv->proxy, G_CALLBACK (proxy_row_updated_cb), grid);
		g_signal_handlers_disconnect_by_func (grid->priv->proxy, G_CALLBACK (proxy_reset_cb), grid);
		g_object_unref (grid->priv->proxy);
		grid->priv->proxy = NULL;
	}

	/* store */
	if (grid->priv->store) {
		g_object_unref (grid->priv->store);
		grid->priv->store = NULL;
	}
}

static gboolean
gdaui_raw_grid_widget_set_write_mode (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode)
{
	GdauiRawGrid *grid;
	
	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), FALSE);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, FALSE);

	grid->priv->write_mode = mode;
	if (mode == GDAUI_DATA_WIDGET_WRITE_ON_VALUE_CHANGE) {
		grid->priv->write_mode = GDAUI_DATA_WIDGET_WRITE_ON_VALUE_ACTIVATED;
		return FALSE;
	}

	if (mode == GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE) {
		GtkTreeSelection *selection;
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (grid));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	}
	return TRUE;
}

static GdauiDataWidgetWriteMode
gdaui_raw_grid_widget_get_write_mode (GdauiDataWidget *iface)
{
	GdauiRawGrid *grid;
	
	g_return_val_if_fail (GDAUI_IS_RAW_GRID (iface), GDAUI_DATA_WIDGET_WRITE_ON_DEMAND);
	grid = GDAUI_RAW_GRID (iface);
	g_return_val_if_fail (grid->priv, GDAUI_DATA_WIDGET_WRITE_ON_DEMAND);

	return grid->priv->write_mode;
}

static void
gdaui_raw_grid_set_data_layout (GdauiDataWidget  *iface, gpointer  data)
{
	GdauiRawGrid *raw_grid;
	
	g_return_if_fail (GDAUI_IS_RAW_GRID (iface));
	raw_grid = GDAUI_RAW_GRID (iface);
	g_return_if_fail (raw_grid->priv);

	g_object_set (G_OBJECT (raw_grid), "data_layout", data, NULL);
}

