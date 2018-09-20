/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-enum-types.h>
#include "gdaui-data-cell-renderer-bin.h"
#include "marshallers/gdaui-custom-marshal.h"
#include "common-bin.h"
#include "gdaui-data-cell-renderer-util.h"

static void gdaui_data_cell_renderer_bin_get_property  (GObject *object,
							guint param_id,
							GValue *value,
							GParamSpec *pspec);
static void gdaui_data_cell_renderer_bin_set_property  (GObject *object,
							guint param_id,
							const GValue *value,
							GParamSpec *pspec);
static void gdaui_data_cell_renderer_bin_dispose    (GObject *object);
static void gdaui_data_cell_renderer_bin_finalize   (GObject *object);
static void gdaui_data_cell_renderer_bin_render     (GtkCellRenderer            *cell,
						     cairo_t                    *cr,
						     GtkWidget                  *widget,
						     const GdkRectangle         *background_area,
						     const GdkRectangle         *cell_area,
						     GtkCellRendererState        flags);
static void gdaui_data_cell_renderer_bin_get_size   (GtkCellRenderer            *cell,
						     GtkWidget                  *widget,
						     const GdkRectangle         *cell_area,
						     gint                       *x_offset,
						     gint                       *y_offset,
						     gint                       *width,
						     gint                       *height);
static gboolean gdaui_data_cell_renderer_bin_activate  (GtkCellRenderer            *cell,
							GdkEvent                   *event,
							GtkWidget                  *widget,
							const gchar                *path,
							const GdkRectangle         *background_area,
							const GdkRectangle         *cell_area,
							GtkCellRendererState        flags);

enum {
	CHANGED,
	LAST_SIGNAL
};


typedef struct
{
	GdaDataHandler       *dh;
	BinMenu               menu;
        GType                 type;
	gboolean              to_be_deleted;

	gboolean              editable;
	gboolean              active;
	gboolean              null;
	gboolean              invalid;
} GdauiDataCellRendererBinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataCellRendererBin, gdaui_data_cell_renderer_bin, GTK_TYPE_CELL_RENDERER_PIXBUF)

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED,
	PROP_DATA_HANDLER,
	PROP_TYPE
};

static guint bin_cell_signals[LAST_SIGNAL] = { 0 };


static void
gdaui_data_cell_renderer_bin_init (GdauiDataCellRendererBin *cell)
{
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (cell);
	priv->dh = NULL;
	priv->type = GDA_TYPE_BLOB;
	priv->editable = FALSE;
	g_object_set (G_OBJECT (cell), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
									"xpad", 2, "ypad", 2, NULL);
}

static void
gdaui_data_cell_renderer_bin_class_init (GdauiDataCellRendererBinClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->dispose = gdaui_data_cell_renderer_bin_dispose;
	object_class->finalize = gdaui_data_cell_renderer_bin_finalize;

	object_class->get_property = gdaui_data_cell_renderer_bin_get_property;
	object_class->set_property = gdaui_data_cell_renderer_bin_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_bin_get_size;
	cell_class->render = gdaui_data_cell_renderer_bin_render;
	cell_class->activate = gdaui_data_cell_renderer_bin_activate;
	cell_class->start_editing = NULL;
  
	g_object_class_install_property (object_class,
					 PROP_VALUE,
					 g_param_spec_boxed ("value",
							     _("Value"),
							     _("GValue to render"),
							     G_TYPE_VALUE,
							     G_PARAM_READWRITE));
  
	g_object_class_install_property (object_class,
					 PROP_VALUE_ATTRIBUTES,
					 g_param_spec_flags ("value-attributes", NULL, NULL, GDA_TYPE_VALUE_ATTRIBUTE,
							     GDA_VALUE_ATTR_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       NULL,
							       TRUE,
							       G_PARAM_READABLE |
							       G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_TO_BE_DELETED,
					 g_param_spec_boolean ("to-be-deleted", NULL, NULL, FALSE,
							   G_PARAM_WRITABLE));
	g_object_class_install_property(object_class,
					PROP_DATA_HANDLER,
					g_param_spec_object("data-handler", NULL, NULL, GDA_TYPE_DATA_HANDLER,
							    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class,
					PROP_TYPE,
					g_param_spec_gtype("type", NULL, NULL, G_TYPE_NONE,
							   G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
  

	bin_cell_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererBinClass, changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__STRING_VALUE,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_VALUE);
}

static void
gdaui_data_cell_renderer_bin_dispose (GObject *object)
{
	GdauiDataCellRendererBin *cell = GDAUI_DATA_CELL_RENDERER_BIN (object);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (cell);

	if (priv->dh) {
		g_object_unref (G_OBJECT (priv->dh));
		priv->dh = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_bin_parent_class)->dispose (object);
}

static void
gdaui_data_cell_renderer_bin_finalize (GObject *object)
{
	GdauiDataCellRendererBin *datacell = GDAUI_DATA_CELL_RENDERER_BIN (object);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (datacell);

	if (priv) {
		common_bin_reset (&(priv->menu));
		g_free (priv);
		priv = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_bin_parent_class)->finalize (object);
}

static void
gdaui_data_cell_renderer_bin_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec)
{
	GdauiDataCellRendererBin *cell = GDAUI_DATA_CELL_RENDERER_BIN (object);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (cell);
  
	switch (param_id) {
	case PROP_VALUE:
		break;
	case PROP_VALUE_ATTRIBUTES:
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, priv->editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}


static void
gdaui_data_cell_renderer_bin_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec)
{
	GdauiDataCellRendererBin *cell = GDAUI_DATA_CELL_RENDERER_BIN (object);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (cell);
  
	switch (param_id) {
	case PROP_VALUE:
		/* Because we don't have a copy of the value, we MUST NOT free it! */
		if (value) {	
                        GValue *gval = g_value_get_boxed (value);
			if (gval && (G_VALUE_TYPE (gval) != GDA_TYPE_NULL)) {
				GError *error = NULL;
				GdkPixbuf *attach_pixbuf = gdk_pixbuf_new_from_resource ("/gdaui/bin-attachment.png", &error);
				if (!attach_pixbuf)
					g_warning (_("Could not find icon file bin-attachment.png in resources please report error to "
							 "https://gitlab.gnome.org/GNOME/libgda/issues. Error message: %s"),
										 error != NULL ? error->message != NULL ? error->message : _("No detail") : _("No detail"));
				g_object_set (object, "pixbuf", attach_pixbuf, NULL);
			} else if (gval)
				g_object_set (object, "pixbuf", NULL, NULL);
			else {
				priv->invalid = TRUE;
				g_object_set (object, "pixbuf", NULL, NULL);
			}
                }
		else {
			priv->invalid = TRUE;
			g_object_set (object, "pixbuf", NULL, NULL);
		}
		break;
	case PROP_VALUE_ATTRIBUTES:
		priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_EDITABLE:
		priv->editable = g_value_get_boolean (value);
		break;
	case PROP_TO_BE_DELETED:
		priv->to_be_deleted = g_value_get_boolean (value);
		break;
	case PROP_DATA_HANDLER:
		if(priv->dh)
			g_object_unref (G_OBJECT(priv->dh));

		priv->dh = GDA_DATA_HANDLER(g_value_get_object(value));
		if(priv->dh)
			g_object_ref (G_OBJECT (priv->dh));
		break;
	case PROP_TYPE:
		priv->type = g_value_get_gtype (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_bin_new:
 * @dh: a #GdaDataHandler object
 * @type: the #GType of the data to be displayed
 * 
 * Creates a new #GdauiDataCellRendererBin. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "active" property on the cell renderer to a bin value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 * 
 * Returns: (transfer full): the new cell renderer
 */
GtkCellRenderer *
gdaui_data_cell_renderer_bin_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;

        g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
        obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_BIN, "type", type, 
                            "data-handler", dh, 
			    "editable", FALSE, NULL);
        	
        return GTK_CELL_RENDERER (obj);
}

static void
gdaui_data_cell_renderer_bin_get_size (GtkCellRenderer *cell,
				       GtkWidget       *widget,
				       const GdkRectangle *cell_area,
				       gint            *x_offset,
				       gint            *y_offset,
				       gint            *width,
				       gint            *height)
{
	GtkCellRendererClass *pixbuf_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF);

	(pixbuf_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_bin_render (GtkCellRenderer      *cell,
				     cairo_t              *cr,
				     GtkWidget            *widget,
				     const GdkRectangle   *background_area,
				     const GdkRectangle   *cell_area,
				     GtkCellRendererState  flags)
{
	GdauiDataCellRendererBin *datacell = (GdauiDataCellRendererBin*) cell;
	GtkCellRendererClass *pixbuf_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (datacell);

	(pixbuf_class->render) (cell, cr, widget, background_area, cell_area, flags);
	
	if (priv->to_be_deleted) {
		GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
		guint xpad;
		g_object_get ((GObject*) cell, "xpad", &xpad, NULL);

		gdouble y = cell_area->y + cell_area->height / 2.;
		gtk_render_line (style_context,
				 cr,
				 cell_area->x + xpad, cell_area->x + cell_area->width - xpad,
				 y, y);
	}
	if (priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (cr, cell_area);
}

static void
bin_data_changed_cb (GdauiDataCellRendererBin *datacell, GValue *value)
{
        g_signal_emit (G_OBJECT (datacell), bin_cell_signals[CHANGED], 0,
                       g_object_get_data (G_OBJECT (datacell), "last-path"), value);
        gda_value_free (value);
}

static gboolean
gdaui_data_cell_renderer_bin_activate  (GtkCellRenderer            *cell,
					GdkEvent                   *event,
					GtkWidget                  *widget,
					const gchar                *path,
					G_GNUC_UNUSED const GdkRectangle *background_area,
					const GdkRectangle         *cell_area,
					G_GNUC_UNUSED GtkCellRendererState flags)
{
	GdauiDataCellRendererBin *datacell;
	GtkTreeModel *model;
	GtkTreePath *tpath;
	GtkTreeIter iter;

	datacell = GDAUI_DATA_CELL_RENDERER_BIN (cell);
	GdauiDataCellRendererBinPrivate *priv = gdaui_data_cell_renderer_bin_get_instance_private (datacell);

	g_object_set_data_full (G_OBJECT (datacell), "last-path", g_strdup (path), g_free);
	if (!priv->menu.popover)
		common_bin_create_menu (widget, &(priv->menu), priv->type,
					(BinCallback) bin_data_changed_cb, datacell);
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	tpath = gtk_tree_path_new_from_string (path);
	if (gtk_tree_model_get_iter (model, &iter, tpath)) {
		gint model_col;
		GValue *value;
		
		model_col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "model_col"));

		gtk_tree_model_get (model, &iter, 
				    model_col, &value, -1);
		common_bin_adjust (&(priv->menu), priv->editable,
				   value);
		gtk_widget_show (priv->menu.popover);
	}
	gtk_tree_path_free (tpath);

	return FALSE;
}


