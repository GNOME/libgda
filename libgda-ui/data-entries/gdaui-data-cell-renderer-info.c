/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include "gdaui-data-cell-renderer-info.h"
#include "marshallers/gdaui-marshal.h"
#include <libgda-ui/internal/utility.h>
#include <libgda/libgda.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/gda-enum-types.h>

static void gdaui_data_cell_renderer_info_get_property  (GObject *object,
							    guint param_id,
							    GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_info_set_property  (GObject *object,
							    guint param_id,
							    const GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_info_init       (GdauiDataCellRendererInfo      *celltext);
static void gdaui_data_cell_renderer_info_class_init (GdauiDataCellRendererInfoClass *class);
static void gdaui_data_cell_renderer_info_dispose    (GObject *object);
static void gdaui_data_cell_renderer_info_finalize   (GObject *object);

static void gdaui_data_cell_renderer_info_get_size   (GtkCellRenderer            *cell,
						      GtkWidget                  *widget,
						      const GdkRectangle         *cell_area,
						      gint                       *x_offset,
						      gint                       *y_offset,
						      gint                       *width,
						      gint                       *height);
static void gdaui_data_cell_renderer_info_render     (GtkCellRenderer            *cell,
						      cairo_t                    *cr,
						      GtkWidget                  *widget,
						      const GdkRectangle         *background_area,
						      const GdkRectangle         *cell_area,
						      GtkCellRendererState        flags);
static gboolean gdaui_data_cell_renderer_info_activate  (GtkCellRenderer            *cell,
							    GdkEvent                   *event,
							    GtkWidget                  *widget,
							    const gchar                *path,
							    const GdkRectangle         *background_area,
							    const GdkRectangle         *cell_area,
							    GtkCellRendererState        flags);


enum {
	STATUS_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_ZERO,
	PROP_VALUE_ATTRIBUTES,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED,
	PROP_STORE,
	PROP_ITER,
	PROP_GROUP
};

struct _GdauiDataCellRendererInfoPriv {
	/* attributes valid for the while life of the object */
	GdauiDataStore      *store;
	GdaDataModelIter      *iter;
	GdauiSetGroup       *group;

	/* attribute valid only for drawing */
	gboolean               active;
	guint                  attributes;
};

#define INFO_WIDTH 6
#define INFO_HEIGHT 14
static GObjectClass *parent_class = NULL;
static guint info_cell_signals[LAST_SIGNAL] = { 0 };


GType
gdaui_data_cell_renderer_info_get_type (void)
{
	static GType cell_info_type = 0;

	if (!cell_info_type) {
		static const GTypeInfo cell_info_info = {
			sizeof (GdauiDataCellRendererInfoClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_data_cell_renderer_info_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiDataCellRendererInfo),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gdaui_data_cell_renderer_info_init,
			0
		};
		
		cell_info_type =
			g_type_register_static (GTK_TYPE_CELL_RENDERER, "GdauiDataCellRendererInfo",
						&cell_info_info, 0);
	}

	return cell_info_type;
}

static void
gdaui_data_cell_renderer_info_init (GdauiDataCellRendererInfo *cellinfo)
{
	cellinfo->priv = g_new0 (GdauiDataCellRendererInfoPriv, 1);

	g_object_set ((GObject*) cellinfo, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "xpad", 1, "ypad", 1, NULL);
}

static void
gdaui_data_cell_renderer_info_class_init (GdauiDataCellRendererInfoClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

    	object_class->dispose = gdaui_data_cell_renderer_info_dispose;
	object_class->finalize = gdaui_data_cell_renderer_info_finalize;

	object_class->get_property = gdaui_data_cell_renderer_info_get_property;
	object_class->set_property = gdaui_data_cell_renderer_info_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_info_get_size;
	cell_class->render = gdaui_data_cell_renderer_info_render;
	cell_class->activate = gdaui_data_cell_renderer_info_activate;
  
	g_object_class_install_property (object_class,
					 PROP_VALUE_ATTRIBUTES,
					 g_param_spec_flags ("value-attributes", NULL, NULL, GDA_TYPE_VALUE_ATTRIBUTE,
                                                            GDA_VALUE_ATTR_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       _("The information and status changer can be activated"),
							       TRUE,G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TO_BE_DELETED,
					 g_param_spec_boolean ("to-be-deleted", NULL, NULL, FALSE,
                                                               G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_STORE,
					 g_param_spec_object ("store", NULL, NULL, GDAUI_TYPE_DATA_STORE,
                                                               G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_ITER,
					 g_param_spec_object ("iter", NULL, NULL, GDA_TYPE_DATA_MODEL_ITER,
                                                               G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

	/* Ideally, GdaPropertyListGroup would be a boxed type, but it is not yet, so we use g_param_spec_pointer. */
	g_object_class_install_property (object_class,
					 PROP_GROUP,
					 g_param_spec_pointer ("group", NULL, NULL,
                                                               G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
	info_cell_signals[STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererInfoClass, status_changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__STRING_ENUM,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      GDA_TYPE_VALUE_ATTRIBUTE);
}

static void
gdaui_data_cell_renderer_info_dispose (GObject *object)
{
	GdauiDataCellRendererInfo *cellinfo = GDAUI_DATA_CELL_RENDERER_INFO (object);

	if (cellinfo->priv->store) {
		g_object_unref (cellinfo->priv->store);
		cellinfo->priv->store = NULL;
	}

	if (cellinfo->priv->iter) {
		g_object_unref (cellinfo->priv->iter);
		cellinfo->priv->iter = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_cell_renderer_info_finalize (GObject *object)
{
	GdauiDataCellRendererInfo *cellinfo = GDAUI_DATA_CELL_RENDERER_INFO (object);

	if (cellinfo->priv) {
		g_free (cellinfo->priv);
		cellinfo->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void
gdaui_data_cell_renderer_info_get_property (GObject *object,
					       guint        param_id,
					       GValue *value,
					       GParamSpec *pspec)
{
	GdauiDataCellRendererInfo *cellinfo = GDAUI_DATA_CELL_RENDERER_INFO (object);
  
	switch (param_id) {
	case PROP_VALUE_ATTRIBUTES:
		g_value_set_flags (value, cellinfo->priv->attributes);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, cellinfo->priv->active);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}


static void
gdaui_data_cell_renderer_info_set_property (GObject *object,
					       guint         param_id,
					       const GValue *value,
					       GParamSpec *pspec)
{
	GdauiDataCellRendererInfo *cellinfo = GDAUI_DATA_CELL_RENDERER_INFO (object);
  
	switch (param_id) {
	case PROP_VALUE_ATTRIBUTES:
		cellinfo->priv->attributes = g_value_get_flags (value);
		g_object_set (object, "sensitive", 
			      !(cellinfo->priv->attributes & GDA_VALUE_ATTR_NO_MODIF), NULL);
		break;
	case PROP_EDITABLE:
		cellinfo->priv->active = g_value_get_boolean (value);
		g_object_notify (G_OBJECT(object), "editable");
		break;
	case PROP_TO_BE_DELETED:
		break;
	case PROP_STORE:
		if (cellinfo->priv->store)
			g_object_unref (cellinfo->priv->store);

		cellinfo->priv->store = GDAUI_DATA_STORE(g_value_get_object(value));
		if (cellinfo->priv->store)
			g_object_ref(cellinfo->priv->store);
    		break;
	case PROP_ITER:
		if (cellinfo->priv->iter)
			g_object_unref(cellinfo->priv->iter);

		cellinfo->priv->iter = GDA_DATA_MODEL_ITER (g_value_get_object (value));
		if (cellinfo->priv->iter)
			g_object_ref (cellinfo->priv->iter);
    		break;
	case PROP_GROUP:
		cellinfo->priv->group = GDAUI_SET_GROUP (g_value_get_pointer(value));
   		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_info_new:
 * @store: a #GdauiDataStore
 * @iter: a #GdaDataModelIter
 * @group: a #GdauiSetGroup pointer
 * 
 * Creates a new #GdauiDataCellRendererInfo. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "active" property on the cell renderer to a boolean value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 * 
 * Returns: (transfer full): the new cell renderer
 **/
GtkCellRenderer *
gdaui_data_cell_renderer_info_new (GdauiDataStore *store, 
				   GdaDataModelIter *iter, GdauiSetGroup *group)
{
	GObject *obj;

	g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), NULL);
	g_return_val_if_fail (GDA_IS_SET (iter), NULL);
	g_return_val_if_fail (group, NULL);

	obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_INFO, 
			    "store", store, "iter", iter, "group", group, NULL);

	return (GtkCellRenderer *) obj;
}

static void
gdaui_data_cell_renderer_info_get_size (GtkCellRenderer *cell,
					G_GNUC_UNUSED GtkWidget *widget,
					const GdkRectangle *cell_area,
					gint            *x_offset,
					gint            *y_offset,
					gint            *width,
					gint            *height)
{
	gint calc_width;
	gint calc_height;
	guint xpad, ypad;

	g_object_get ((GObject*) cell, "xpad", &xpad, "ypad", &ypad, NULL);
	calc_width = (gint) xpad * 2 + INFO_WIDTH;
	calc_height = (gint) ypad * 2 + INFO_HEIGHT;

	if (width)
		*width = calc_width;

	if (height)
		*height = calc_height;

	if (cell_area) {
		if (x_offset) {
			gfloat xalign;
			g_object_get ((GObject*) cell, "xalign", &xalign, NULL);
			*x_offset = xalign * (cell_area->width - calc_width);
			*x_offset = MAX (*x_offset, 0);
		}
		if (y_offset) {
			gfloat yalign;
			g_object_get ((GObject*) cell, "yalign", &yalign, NULL);
			*y_offset = yalign * (cell_area->height - calc_height);
			*y_offset = MAX (*y_offset, 0);
		}
	}
}


static void
gdaui_data_cell_renderer_info_render (GtkCellRenderer      *cell,
				      cairo_t              *cr,
				      GtkWidget            *widget,
				      G_GNUC_UNUSED const GdkRectangle *background_area,
				      const GdkRectangle   *cell_area,
				      G_GNUC_UNUSED GtkCellRendererState  flags)
{
	GdauiDataCellRendererInfo *cellinfo = (GdauiDataCellRendererInfo *) cell;
	gint width, height;
	gint x_offset, y_offset;

	static GdkRGBA **colors = NULL;
	GdkRGBA statenormal, stateprelight;
	GdkRGBA *normal = NULL;


	if (!colors)
		colors = _gdaui_utility_entry_build_info_colors_array_a ();

	if (cellinfo->priv->attributes & GDA_VALUE_ATTR_DATA_NON_VALID) {
		normal = colors[4];
	}
	else if (cellinfo->priv->attributes & GDA_VALUE_ATTR_IS_DEFAULT) {
		normal = colors[2];
	}
	else if (cellinfo->priv->attributes & GDA_VALUE_ATTR_IS_NULL) {
		normal = colors[0];
	}
	else {
		GtkStyleContext *stc;
		stc = gtk_widget_get_style_context (widget);
		gtk_style_context_get_background_color (stc, GTK_STATE_FLAG_NORMAL, &statenormal);
		gtk_style_context_get_background_color (stc, GTK_STATE_FLAG_NORMAL, &stateprelight);
		normal = &statenormal;
	}

	gdaui_data_cell_renderer_info_get_size (cell, widget, cell_area,
						&x_offset, &y_offset,
						&width, &height);

	guint xpad, ypad;
	g_object_get ((GObject*) cell, "xpad", &xpad, "ypad", &ypad, NULL);
	width -= xpad*2;
	height -= ypad*2;

	if (width <= 0 || height <= 0)
		return;

	cairo_set_source_rgba (cr, normal->red, normal->green, normal->blue, normal->alpha);
	cairo_rectangle (cr, cell_area->x + x_offset + xpad,
			 cell_area->y + y_offset + ypad,
			 width - 1, height - 1);
	cairo_fill (cr);
}


static void mitem_activated_cb (GtkWidget *mitem, GdauiDataCellRendererInfo *cellinfo);
static gint
gdaui_data_cell_renderer_info_activate (GtkCellRenderer      *cell,
					G_GNUC_UNUSED GdkEvent             *event,
					G_GNUC_UNUSED GtkWidget            *widget,
					const gchar          *path,
					G_GNUC_UNUSED const GdkRectangle *background_area,
					G_GNUC_UNUSED const GdkRectangle *cell_area,
					G_GNUC_UNUSED GtkCellRendererState  flags)
{
	GdauiDataCellRendererInfo *cellinfo;
	gchar *tmp;

	cellinfo = GDAUI_DATA_CELL_RENDERER_INFO (cell);

	/* free any pre-allocated path */
	if ((tmp = g_object_get_data (G_OBJECT (cellinfo), "path"))) {
		g_free (tmp);
		g_object_set_data (G_OBJECT (cellinfo), "path", NULL);
	}

	if (cellinfo->priv->active) {
		GtkWidget *menu;
		guint attributes = 0;
		GtkTreeIter iter;
		GtkTreePath *treepath;

		treepath = gtk_tree_path_new_from_string (path);
		if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (cellinfo->priv->store), &iter, treepath)) {
			g_warning ("Can't set iter on model from path %s", path);
			gtk_tree_path_free (treepath);
			return FALSE;
		}
		gtk_tree_path_free (treepath);

		/* we want the attributes */
		if (! gda_set_group_get_source (gdaui_set_group_get_group (cellinfo->priv->group))) {
			gint col;
			GdaDataModel *proxied_model;
			GdaDataProxy *proxy;
			GdaSetGroup *sg;
			
			proxy = gdaui_data_store_get_proxy (cellinfo->priv->store);
			proxied_model = gda_data_proxy_get_proxied_model (proxy);
			sg = gdaui_set_group_get_group (cellinfo->priv->group);
			g_assert (gda_set_group_get_n_nodes (sg) == 1);
			col = g_slist_index (gda_set_get_holders (GDA_SET (cellinfo->priv->iter)),
					     gda_set_node_get_holder (  gda_set_group_get_node (sg)));

			gtk_tree_model_get (GTK_TREE_MODEL (cellinfo->priv->store), &iter, 
					    gda_data_model_get_n_columns (proxied_model) + col, 
					    &attributes, -1);
		}
		else 
			attributes = _gdaui_utility_proxy_compute_attributes_for_group (cellinfo->priv->group, 
											cellinfo->priv->store,
											cellinfo->priv->iter,
											&iter, NULL);
		
		/* build the popup menu */
		menu = _gdaui_utility_entry_build_actions_menu (G_OBJECT (cellinfo), attributes, 
								G_CALLBACK (mitem_activated_cb));
		g_object_set_data (G_OBJECT (cellinfo), "path", g_strdup (path));
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
				0, gtk_get_current_event_time ());
		return TRUE;		
	}

	return FALSE;
}

static void
mitem_activated_cb (GtkWidget *mitem, GdauiDataCellRendererInfo *cellinfo)
{
	guint action;
	gchar *path;

	action = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (mitem), "action"));
	path = g_object_get_data (G_OBJECT (cellinfo), "path");
#ifdef debug_signal
        g_print (">> 'STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (cellinfo, info_cell_signals[STATUS_CHANGED], 0, path, action); 
#ifdef debug_signal
        g_print ("<< 'STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_free (path);
	g_object_set_data (G_OBJECT (cellinfo), "path", NULL);
}
