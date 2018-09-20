/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "gdaui-data-cell-renderer-boolean.h"
#include "marshallers/gdaui-custom-marshal.h"
#include "gdaui-data-cell-renderer-util.h"

static void gdaui_data_cell_renderer_boolean_get_property  (GObject *object,
							    guint param_id,
							    GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_boolean_set_property  (GObject *object,
							    guint param_id,
							    const GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_boolean_init       (GdauiDataCellRendererBoolean      *celltext);
static void gdaui_data_cell_renderer_boolean_class_init (GdauiDataCellRendererBooleanClass *class);
static void gdaui_data_cell_renderer_boolean_dispose    (GObject *object);
static void gdaui_data_cell_renderer_boolean_render     (GtkCellRenderer            *cell,
							 cairo_t                    *cr,
							 GtkWidget                  *widget,
							 const GdkRectangle         *background_area,
							 const GdkRectangle         *cell_area,
							 GtkCellRendererState        flags);
static void gdaui_data_cell_renderer_boolean_get_size   (GtkCellRenderer            *cell,
							 GtkWidget                  *widget,
							 const GdkRectangle         *cell_area,
							 gint                       *x_offset,
							 gint                       *y_offset,
							 gint                       *width,
							 gint                       *height);
static gboolean gdaui_data_cell_renderer_boolean_activate  (GtkCellRenderer            *cell,
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
        GType                 type;
        GValue               *value;
	gboolean              to_be_deleted;
	gboolean              invalid;

	gboolean              editable;
	gboolean              active;
	gboolean              null;
} GdauiDataCellRendererBooleanPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataCellRendererBoolean, gdaui_data_cell_renderer_boolean, GTK_TYPE_CELL_RENDERER_TOGGLE)

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED,
	PROP_DATA_HANDLER,
	PROP_TYPE
};

static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };


static void
gdaui_data_cell_renderer_boolean_init (GdauiDataCellRendererBoolean *cell)
{
	GdauiDataCellRendererBooleanPrivate *priv = gdaui_data_cell_renderer_boolean_get_instance_private (cell);
	priv->dh = NULL;
	priv->type = G_TYPE_BOOLEAN;
	priv->editable = FALSE;
	g_object_set (G_OBJECT (cell), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "xpad", 2, "ypad", 2, NULL);
}

static void
gdaui_data_cell_renderer_boolean_class_init (GdauiDataCellRendererBooleanClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->dispose = gdaui_data_cell_renderer_boolean_dispose;

	object_class->get_property = gdaui_data_cell_renderer_boolean_get_property;
	object_class->set_property = gdaui_data_cell_renderer_boolean_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_boolean_get_size;
	cell_class->render = gdaui_data_cell_renderer_boolean_render;
	cell_class->activate = gdaui_data_cell_renderer_boolean_activate;

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
							       _("The toggle button can be activated"),
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


	toggle_cell_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererBooleanClass, changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__STRING_VALUE,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_VALUE);
}

static void
gdaui_data_cell_renderer_boolean_dispose (GObject *object)
{
	GdauiDataCellRendererBoolean *datacell = GDAUI_DATA_CELL_RENDERER_BOOLEAN (object);
	GdauiDataCellRendererBooleanPrivate *priv = gdaui_data_cell_renderer_boolean_get_instance_private (datacell);

	if (priv->dh) {
		g_object_unref (G_OBJECT (priv->dh));
		priv->dh = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_boolean_parent_class)->dispose (object);
}

static void
gdaui_data_cell_renderer_boolean_get_property (GObject *object,
					       guint        param_id,
					       GValue *value,
					       GParamSpec *pspec)
{
	GdauiDataCellRendererBoolean *cell = GDAUI_DATA_CELL_RENDERER_BOOLEAN (object);
	GdauiDataCellRendererBooleanPrivate *priv = gdaui_data_cell_renderer_boolean_get_instance_private (cell);

	switch (param_id) {
	case PROP_VALUE:
		g_value_set_boxed (value, priv->value);
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
gdaui_data_cell_renderer_boolean_set_property (GObject *object,
					       guint         param_id,
					       const GValue *value,
					       GParamSpec *pspec)
{
	GdauiDataCellRendererBoolean *cell = GDAUI_DATA_CELL_RENDERER_BOOLEAN (object);
	GdauiDataCellRendererBooleanPrivate *priv = gdaui_data_cell_renderer_boolean_get_instance_private (cell);

	switch (param_id) {
	case PROP_VALUE:
		/* Because we don't have a copy of the value, we MUST NOT free it! */
                priv->value = NULL;
		if (value) {
                        GValue *gval = g_value_get_boxed (value);
			if (gval && !gda_value_is_null (gval)) {
				g_return_if_fail (G_VALUE_TYPE (gval) == priv->type);
				if (! gda_value_isa (gval, G_TYPE_BOOLEAN))
					g_warning ("GdauiDataCellRendererBoolean can only handle boolean values");
				else
					g_object_set (G_OBJECT (object),
						      "inconsistent", FALSE,
						      "active", g_value_get_boolean (gval), NULL);
			}
			else if (gval)
				g_object_set (G_OBJECT (object),
					      "inconsistent", TRUE,
					      "active", FALSE, NULL);
			else {
				priv->invalid = TRUE;
				g_object_set (G_OBJECT (object),
					      "inconsistent", TRUE,
					      "active", FALSE, NULL);
			}

                        priv->value = gval;
                }
		else {
			priv->invalid = TRUE;
			g_object_set (G_OBJECT (object),
				      "inconsistent", TRUE,
				      "active", FALSE, NULL);
		}

                g_object_notify (object, "value");
		break;
	case PROP_VALUE_ATTRIBUTES:
		priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_EDITABLE:
		priv->editable = g_value_get_boolean (value);
		g_object_set (G_OBJECT (object), "activatable", priv->editable, NULL);
		g_object_notify (G_OBJECT(object), "editable");
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
		priv->type = g_value_get_gtype(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_boolean_new:
 * @dh: a #GdaDataHandler object
 * @type: the #GType of the data to be displayed
 *
 * Creates a new #GdauiDataCellRendererBoolean. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "active" property on the cell renderer to a boolean value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 *
 * Returns: (transfer full): the new cell renderer
 */
GtkCellRenderer *
gdaui_data_cell_renderer_boolean_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;

        g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
        obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_BOOLEAN, "type", type,
                            "data-handler", dh, NULL);

        return GTK_CELL_RENDERER (obj);
}

static void
gdaui_data_cell_renderer_boolean_get_size (GtkCellRenderer *cell,
					   GtkWidget       *widget,
					   const GdkRectangle *cell_area,
					   gint            *x_offset,
					   gint            *y_offset,
					   gint            *width,
					   gint            *height)
{
	GtkCellRendererClass *toggle_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TOGGLE);

	(toggle_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_boolean_render (GtkCellRenderer      *cell,
					 cairo_t              *cr,
					 GtkWidget            *widget,
					 const GdkRectangle   *background_area,
					 const GdkRectangle   *cell_area,
					 GtkCellRendererState  flags)
{
	GdauiDataCellRendererBoolean *datacell = GDAUI_DATA_CELL_RENDERER_BOOLEAN (cell);
	GtkCellRendererClass *toggle_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TOGGLE);
	GdauiDataCellRendererBooleanPrivate *priv = gdaui_data_cell_renderer_boolean_get_instance_private (datacell);

	(toggle_class->render) (cell, cr, widget, background_area, cell_area, flags);

	if (priv->to_be_deleted) {
		GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
		guint xpad;
		g_object_get (G_OBJECT(widget), "xpad", &xpad, NULL);
		gdouble y = cell_area->y + cell_area->height / 2.;
		gtk_render_line (style_context,
				 cr,
				 cell_area->x + xpad, cell_area->x + cell_area->width - xpad,
				 y, y);

	}
	if (priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (cr, cell_area);
}

static gboolean
gdaui_data_cell_renderer_boolean_activate  (GtkCellRenderer            *cell,
					    GdkEvent                   *event,
					    GtkWidget                  *widget,
					    const gchar                *path,
					    const GdkRectangle         *background_area,
					    const GdkRectangle         *cell_area,
					    GtkCellRendererState        flags)
{
	gboolean editable;
	g_object_get (G_OBJECT(cell), "editable", &editable, NULL);
	if (editable) {
		gboolean retval, active;
		GValue *value;

		retval = GTK_CELL_RENDERER_CLASS (gdaui_data_cell_renderer_boolean_parent_class)->activate (cell, event,
									   widget, path,
									   background_area,
									   cell_area, flags);
		active = gtk_cell_renderer_toggle_get_active (GTK_CELL_RENDERER_TOGGLE (cell));

		value = gda_value_new (G_TYPE_BOOLEAN);
		g_value_set_boolean (value, ! active);
		g_signal_emit (G_OBJECT (cell), toggle_cell_signals[CHANGED], 0, path, value);
		gda_value_free (value);
		return retval;
	}

	return FALSE;
}


