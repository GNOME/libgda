/* gdaui-data-cell-renderer-bin.c
 *
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
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
static void gdaui_data_cell_renderer_bin_init       (GdauiDataCellRendererBin      *cell);
static void gdaui_data_cell_renderer_bin_class_init (GdauiDataCellRendererBinClass *class);
static void gdaui_data_cell_renderer_bin_dispose    (GObject *object);
static void gdaui_data_cell_renderer_bin_finalize   (GObject *object);
static void gdaui_data_cell_renderer_bin_render     (GtkCellRenderer            *cell,
						     GdkWindow                  *window,
						     GtkWidget                  *widget,
						     GdkRectangle               *background_area,
						     GdkRectangle               *cell_area,
						     GdkRectangle               *expose_area,
						     GtkCellRendererState        flags);
static void gdaui_data_cell_renderer_bin_get_size   (GtkCellRenderer            *cell,
						     GtkWidget                  *widget,
						     GdkRectangle               *cell_area,
						     gint                       *x_offset,
						     gint                       *y_offset,
						     gint                       *width,
						     gint                       *height);
static gboolean gdaui_data_cell_renderer_bin_activate  (GtkCellRenderer            *cell,
							GdkEvent                   *event,
							GtkWidget                  *widget,
							const gchar                *path,
							GdkRectangle               *background_area,
							GdkRectangle               *cell_area,
							GtkCellRendererState        flags);

enum {
	CHANGED,
	LAST_SIGNAL
};


struct _GdauiDataCellRendererBinPrivate 
{
	GdaDataHandler       *dh;
	BinMenu               menu;
        GType                 type;
	gboolean              to_be_deleted;

	gboolean              editable;
	gboolean              active;
	gboolean              null;
	gboolean              invalid;
};

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED,
	PROP_DATA_HANDLER,
	PROP_TYPE
};

static GObjectClass *parent_class = NULL;
static guint bin_cell_signals[LAST_SIGNAL] = { 0 };
static GdkPixbuf *attach_pixbuf = NULL;


GType
gdaui_data_cell_renderer_bin_get_type (void)
{
	static GType cell_type = 0;

	if (!cell_type) {
		static const GTypeInfo cell_info = {
			sizeof (GdauiDataCellRendererBinClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_data_cell_renderer_bin_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiDataCellRendererBin),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gdaui_data_cell_renderer_bin_init,
		};
		
		cell_type =
			g_type_register_static (GTK_TYPE_CELL_RENDERER_PIXBUF, "GdauiDataCellRendererBin",
						&cell_info, 0);
	}

	return cell_type;
}

static void
gdaui_data_cell_renderer_bin_init (GdauiDataCellRendererBin *cell)
{
	cell->priv = g_new0 (GdauiDataCellRendererBinPrivate, 1);
	cell->priv->dh = NULL;
	cell->priv->type = GDA_TYPE_BLOB;
	cell->priv->editable = FALSE;
	g_object_set (G_OBJECT (cell), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
									"xpad", 2, "ypad", 2, NULL);
}

static void
gdaui_data_cell_renderer_bin_class_init (GdauiDataCellRendererBinClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

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

	if (! attach_pixbuf) {
		gchar *tmp;
		tmp = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "bin-attachment-16x16.png", NULL);
		attach_pixbuf = gdk_pixbuf_new_from_file (tmp, NULL);
		if (!attach_pixbuf)
			g_warning ("Could not find icon file %s", tmp);
		g_free (tmp);
	}
}

static void
gdaui_data_cell_renderer_bin_dispose (GObject *object)
{
	GdauiDataCellRendererBin *datacell = GDAUI_DATA_CELL_RENDERER_BIN (object);

	if (datacell->priv->dh) {
		g_object_unref (G_OBJECT (datacell->priv->dh));
		datacell->priv->dh = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_cell_renderer_bin_finalize (GObject *object)
{
	GdauiDataCellRendererBin *datacell = GDAUI_DATA_CELL_RENDERER_BIN (object);

	if (datacell->priv) {
		common_bin_reset (&(datacell->priv->menu));
		g_free (datacell->priv);
		datacell->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
gdaui_data_cell_renderer_bin_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec)
{
	GdauiDataCellRendererBin *cell = GDAUI_DATA_CELL_RENDERER_BIN (object);
  
	switch (param_id) {
	case PROP_VALUE:
		break;
	case PROP_VALUE_ATTRIBUTES:
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, cell->priv->editable);
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
  
	switch (param_id) {
	case PROP_VALUE:
		/* Because we don't have a copy of the value, we MUST NOT free it! */
		if (value) {	
                        GValue *gval = g_value_get_boxed (value);
			if (gval && (G_VALUE_TYPE (gval) != GDA_TYPE_NULL))
				g_object_set (object, "pixbuf", attach_pixbuf, NULL);
			else if (gval)
				g_object_set (object, "pixbuf", NULL, NULL);
			else {
				cell->priv->invalid = TRUE;
				g_object_set (object, "pixbuf", NULL, NULL);
			}
                }
		else {
			cell->priv->invalid = TRUE;
			g_object_set (object, "pixbuf", NULL, NULL);
		}
		break;
	case PROP_VALUE_ATTRIBUTES:
		cell->priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_EDITABLE:
		cell->priv->editable = g_value_get_boolean (value);
		break;
	case PROP_TO_BE_DELETED:
		cell->priv->to_be_deleted = g_value_get_boolean (value);
		break;
	case PROP_DATA_HANDLER:
		if(cell->priv->dh)
			g_object_unref (G_OBJECT(cell->priv->dh));

		cell->priv->dh = GDA_DATA_HANDLER(g_value_get_object(value));
		if(cell->priv->dh)
			g_object_ref (G_OBJECT (cell->priv->dh));
		break;
	case PROP_TYPE:
		cell->priv->type = g_value_get_gtype (value);
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
 * Return value: the new cell renderer
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
				       GdkRectangle    *cell_area,
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
				     GdkWindow            *window,
				     GtkWidget            *widget,
				     GdkRectangle         *background_area,
				     GdkRectangle         *cell_area,
				     GdkRectangle         *expose_area,
				     GtkCellRendererState  flags)
{
	GdauiDataCellRendererBin *datacell = (GdauiDataCellRendererBin*) cell;
	GtkCellRendererClass *pixbuf_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF);

	(pixbuf_class->render) (cell, window, widget, background_area, cell_area, expose_area, flags);
	
	if (datacell->priv->to_be_deleted) {
		GtkStyle *style;
		guint xpad;
		
		g_object_get ((GObject*) widget, "style", &style, NULL);
		g_object_get ((GObject*) cell, "xpad", &xpad, NULL);

		gtk_paint_hline (style,
				 window, GTK_STATE_SELECTED,
				 cell_area, 
				 widget,
				 "hline",
				 cell_area->x + xpad, cell_area->x + cell_area->width - xpad,
				 cell_area->y + cell_area->height / 2.);
		g_object_unref (style);
	}
	if (datacell->priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (window, cell_area);
}

static void
bin_data_changed_cb (GdauiDataCellRendererBin *bincell, GValue *value)
{
        g_signal_emit (G_OBJECT (bincell), bin_cell_signals[CHANGED], 0,
                       g_object_get_data (G_OBJECT (bincell), "last-path"), value);
        gda_value_free (value);
}

static void
popup_position (PopupContainer *container, gint *out_x, gint *out_y)
{
	GtkWidget *poswidget;
	GdkEvent *event;
	GdkRectangle *rect;
	gint x, y;

	poswidget = g_object_get_data (G_OBJECT (container), "__poswidget");
	event = g_object_get_data (G_OBJECT (container), "__event");
	rect = g_object_get_data (G_OBJECT (container), "__rect");

	if (event && (event->type == GDK_BUTTON_PRESS)) {
		GdkEventButton *rev = (GdkEventButton*) event;
		gdk_window_get_origin (rev->window, &x, &y);
		x += (gint) rev->x;
		y += (gint) rev->y;
	}
	else {
		g_assert (rect);
		gdk_window_get_origin (gtk_tree_view_get_bin_window (GTK_TREE_VIEW (poswidget)), &x, &y);
		x += rect->x;
		y += rect->y;
	}

	if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

	*out_x = x;
	*out_y = y;
}

static gboolean
gdaui_data_cell_renderer_bin_activate  (GtkCellRenderer            *cell,
					GdkEvent                   *event,
					GtkWidget                  *widget,
					const gchar                *path,
					GdkRectangle               *background_area,
					GdkRectangle               *cell_area,
					GtkCellRendererState        flags)
{
	GdauiDataCellRendererBin *bincell;
	GtkTreeModel *model;
	GtkTreePath *tpath;
	GtkTreeIter iter;

        bincell = GDAUI_DATA_CELL_RENDERER_BIN (cell);
	
	g_object_set_data_full (G_OBJECT (bincell), "last-path", g_strdup (path), g_free);
	if (!bincell->priv->menu.popup) {
		common_bin_create_menu (&(bincell->priv->menu), popup_position, bincell->priv->type,
					(BinCallback) bin_data_changed_cb, bincell);
		g_object_set_data (G_OBJECT (bincell->priv->menu.popup), "__poswidget", widget);
	}
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	tpath = gtk_tree_path_new_from_string (path);
	if (gtk_tree_model_get_iter (model, &iter, tpath)) {
		gint model_col;
		GValue *value;
		
		model_col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "model_col"));

		gtk_tree_model_get (model, &iter, 
				    model_col, &value, -1);
		common_bin_adjust_menu (&(bincell->priv->menu), bincell->priv->editable,
					value);
		g_object_set_data (G_OBJECT (bincell->priv->menu.popup), "__event", event);
		g_object_set_data (G_OBJECT (bincell->priv->menu.popup), "__rect", cell_area);
		gtk_widget_show (bincell->priv->menu.popup);
	}
	gtk_tree_path_free (tpath);

        return FALSE;
}


