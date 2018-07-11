/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "gdaui-data-cell-renderer-pict.h"
#include "custom-marshal.h"
#include "common-pict.h"
#include <libgda/gda-enum-types.h>
#include "gdaui-data-cell-renderer-util.h"

static void gdaui_data_cell_renderer_pict_get_property  (GObject *object,
							 guint param_id,
							 GValue *value,
							 GParamSpec *pspec);
static void gdaui_data_cell_renderer_pict_set_property  (GObject *object,
							 guint param_id,
							 const GValue *value,
							 GParamSpec *pspec);
static void gdaui_data_cell_renderer_pict_dispose       (GObject *object);

static void gdaui_data_cell_renderer_pict_init       (GdauiDataCellRendererPict      *celltext);
static void gdaui_data_cell_renderer_pict_class_init (GdauiDataCellRendererPictClass *class);
static void gdaui_data_cell_renderer_pict_render     (GtkCellRenderer            *cell,
						      cairo_t                    *cr,
						      GtkWidget                  *widget,
						      const GdkRectangle         *background_area,
						      const GdkRectangle         *cell_area,
						      GtkCellRendererState        flags);
static void gdaui_data_cell_renderer_pict_get_size   (GtkCellRenderer            *cell,
						      GtkWidget                  *widget,
						      const GdkRectangle        *cell_area,
						      gint                       *x_offset,
						      gint                       *y_offset,
						      gint                       *width,
						      gint                       *height);
static gboolean gdaui_data_cell_renderer_pict_activate  (GtkCellRenderer            *cell,
							 GdkEvent                   *event,
							 GtkWidget                  *widget,
							 const gchar                *path,
							 const GdkRectangle         *background_area,
							 const GdkRectangle         *cell_area,
							 GtkCellRendererState        flags);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

enum {
	CHANGED,
	LAST_SIGNAL
};


struct _GdauiDataCellRendererPictPrivate
{
	GdaDataHandler       *dh;
        GType                 type;
        GValue               *value;
	PictBinData           bindata;
	PictOptions           options;
	PictAllocation        size;
	PictMenu              popup_menu;
	gboolean              to_be_deleted;
	gboolean              invalid;

	gboolean              editable;
	gboolean              active;
	gboolean              null;
};

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED
};

static guint pixbuf_cell_signals[LAST_SIGNAL] = { 0 };


GType
gdaui_data_cell_renderer_pict_get_type (void)
{
	static GType cell_type = 0;

	if (!cell_type) {
		static const GTypeInfo cell_info = {
			sizeof (GdauiDataCellRendererPictClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_data_cell_renderer_pict_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiDataCellRendererPict),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gdaui_data_cell_renderer_pict_init,
			0
		};

		cell_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_PIXBUF, "GdauiDataCellRendererPict",
						    &cell_info, 0);
	}

	return cell_type;
}

static void
notify_property_cb (GtkCellRenderer *cell, GParamSpec *pspec, G_GNUC_UNUSED gpointer data)
{
	if (!strcmp (pspec->name, "stock-size")) {
		GdauiDataCellRendererPict *pictcell;
		guint size;

		pictcell = (GdauiDataCellRendererPict *) cell;
		g_object_get ((GObject *) cell, "stock-size", &size, NULL);
		gtk_icon_size_lookup (size, &(pictcell->priv->size.width), &(pictcell->priv->size.height));
		common_pict_clear_pixbuf_cache (&(pictcell->priv->options));
	}
}

static void
gdaui_data_cell_renderer_pict_init (GdauiDataCellRendererPict *cell)
{
	cell->priv = g_new0 (GdauiDataCellRendererPictPrivate, 1);
	cell->priv->dh = NULL;
	cell->priv->type = GDA_TYPE_BINARY;
	cell->priv->editable = FALSE;

	cell->priv->bindata.data = NULL;
	cell->priv->bindata.data_length = 0;
	cell->priv->options.encoding = ENCODING_NONE;
	common_pict_init_cache (&(cell->priv->options));

	gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &(cell->priv->size.width), &(cell->priv->size.height));

	g_object_set ((GObject*) cell, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "xpad", 2, "ypad", 2, NULL);
	g_signal_connect (G_OBJECT (cell), "notify",
			  G_CALLBACK (notify_property_cb), NULL);
}

static void
gdaui_data_cell_renderer_pict_class_init (GdauiDataCellRendererPictClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = gdaui_data_cell_renderer_pict_get_property;
	object_class->set_property = gdaui_data_cell_renderer_pict_set_property;
	object_class->dispose = gdaui_data_cell_renderer_pict_dispose;

	cell_class->get_size = gdaui_data_cell_renderer_pict_get_size;
	cell_class->render = gdaui_data_cell_renderer_pict_render;
	cell_class->activate = gdaui_data_cell_renderer_pict_activate;

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

	pixbuf_cell_signals[CHANGED] = g_signal_new ("changed",
						     G_OBJECT_CLASS_TYPE (object_class),
						     G_SIGNAL_RUN_LAST,
						     G_STRUCT_OFFSET (GdauiDataCellRendererPictClass, changed),
						     NULL, NULL,
						     _marshal_VOID__STRING_VALUE,
						     G_TYPE_NONE, 2,
						     G_TYPE_STRING,
						     G_TYPE_VALUE);
}

static void
gdaui_data_cell_renderer_pict_dispose (GObject *object)
{
	GdauiDataCellRendererPict *cell;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_PICT (object));
	cell = GDAUI_DATA_CELL_RENDERER_PICT (object);

	if (cell->priv) {
		g_hash_table_destroy (cell->priv->options.pixbuf_hash);

		/* the private area itself */
		g_free (cell->priv);
		cell->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_cell_renderer_pict_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec)
{
	GdauiDataCellRendererPict *cell = GDAUI_DATA_CELL_RENDERER_PICT (object);

	switch (param_id) {
	case PROP_VALUE:
		g_value_set_boxed (value, cell->priv->value);
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
gdaui_data_cell_renderer_pict_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
	GdauiDataCellRendererPict *cell = GDAUI_DATA_CELL_RENDERER_PICT (object);

	switch (param_id) {
	case PROP_VALUE:
		/* Because we don't have a copy of the value, we MUST NOT free it! */
                cell->priv->value = NULL;
		g_object_set (G_OBJECT (cell), "pixbuf", NULL, "icon-name", NULL, NULL);
		if (value) {
                        GValue *gval = g_value_get_boxed (value);
			GdkPixbuf *pixbuf = NULL;
			const gchar *icon_name = NULL;
			GError *error = NULL;

			if (!gval)
				cell->priv->invalid = TRUE;

			if (cell->priv->bindata.data) {
				g_free (cell->priv->bindata.data);
				cell->priv->bindata.data = NULL;
				cell->priv->bindata.data_length = 0;
			}

			/* fill in cell->priv->data */
			if (common_pict_load_data (&(cell->priv->options), gval, &(cell->priv->bindata),
						   &icon_name, &error)) {
				/* try to make a pixbuf */
				pixbuf = common_pict_fetch_cached_pixbuf (&(cell->priv->options), gval);
				if (pixbuf)
					g_object_ref (pixbuf);
				else {
					pixbuf = common_pict_make_pixbuf (&(cell->priv->options),
									  &(cell->priv->bindata), &(cell->priv->size),
									  &icon_name, &error);
					if (pixbuf)
						common_pict_add_cached_pixbuf (&(cell->priv->options), gval, pixbuf);
				}

				if (!pixbuf && !icon_name)
					icon_name = "image-missing";
			}

			/* display something */
			if (pixbuf) {
				g_object_set (G_OBJECT (cell), "pixbuf", pixbuf, NULL);
				g_object_unref (pixbuf);
			}

			if (icon_name)
				g_object_set (G_OBJECT (cell), "icon-name", icon_name, NULL);
			if (error)
				g_error_free (error);

                        cell->priv->value = gval;
                }
		else
			cell->priv->invalid = TRUE;

                g_object_notify (object, "value");
		break;
	case PROP_VALUE_ATTRIBUTES:
		cell->priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_EDITABLE:
		cell->priv->editable = g_value_get_boolean (value);
		/* FIXME */
		/*g_object_notify (G_OBJECT(object), "editable");*/
		break;
	case PROP_TO_BE_DELETED:
		cell->priv->to_be_deleted = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_pict_new:
 * @dh: a #GdaDataHandler object
 * @type:
 * @options: options string
 *
 * Creates a new #GdauiDataCellRendererPict. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "active" property on the cell renderer to a pict value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 *
 * Returns: the new cell renderer
 */
GtkCellRenderer *
gdaui_data_cell_renderer_pict_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
        GdauiDataCellRendererPict *cell;

        g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
        obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_PICT, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);

        cell = GDAUI_DATA_CELL_RENDERER_PICT (obj);
        cell->priv->dh = dh;
        g_object_ref (G_OBJECT (dh));
        cell->priv->type = type;

	common_pict_parse_options (&(cell->priv->options), options);

        return GTK_CELL_RENDERER (obj);
}

static void
gdaui_data_cell_renderer_pict_get_size (GtkCellRenderer *cell,
					GtkWidget       *widget,
					const GdkRectangle *cell_area,
					gint            *x_offset,
					gint            *y_offset,
					gint            *width,
					gint            *height)
{
	/* FIXME */
	/* GtkIconSize */
	GtkCellRendererClass *pixbuf_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF);

	(pixbuf_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_pict_render (GtkCellRenderer      *cell,
				      cairo_t              *cr,
				      GtkWidget            *widget,
				      const GdkRectangle   *background_area,
				      const GdkRectangle   *cell_area,
				      GtkCellRendererState  flags)
{
	GdauiDataCellRendererPict *datacell = GDAUI_DATA_CELL_RENDERER_PICT (cell);
	GtkCellRendererClass *pixbuf_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF);

	(pixbuf_class->render) (cell, cr, widget, background_area, cell_area, flags);

	if (datacell->priv->to_be_deleted) {
		GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
		guint xpad;
		g_object_get ((GObject*) cell, "xpad", &xpad, NULL);

		gdouble y = cell_area->y + cell_area->height / 2.;
		gtk_render_line (style_context,
				 cr,
				 cell_area->x + xpad, y, cell_area->x + cell_area->width - xpad, y);
	}
	if (datacell->priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (cr, cell_area);
}

static void
pict_data_changed_cb (PictBinData *bindata, GdauiDataCellRendererPict *pictcell)
{
	GValue *value;

	value = common_pict_get_value (bindata, &(pictcell->priv->options),
				       pictcell->priv->type);
	g_free (bindata->data);
	g_signal_emit (G_OBJECT (pictcell), pixbuf_cell_signals[CHANGED], 0,
		       g_object_get_data (G_OBJECT (pictcell), "last-path"), value);
	gda_value_free (value);
}

static gboolean
gdaui_data_cell_renderer_pict_activate  (GtkCellRenderer            *cell,
					 G_GNUC_UNUSED GdkEvent                   *event,
					 GtkWidget                  *widget,
					 const gchar                *path,
					 G_GNUC_UNUSED const GdkRectangle *background_area,
					 G_GNUC_UNUSED const GdkRectangle *cell_area,
					 G_GNUC_UNUSED GtkCellRendererState        flags)
{
	GdauiDataCellRendererPict *pictcell;

	pictcell = GDAUI_DATA_CELL_RENDERER_PICT (cell);
	if (pictcell->priv->editable) {
		g_object_set_data_full (G_OBJECT (pictcell), "last-path", g_strdup (path), g_free);
		if (pictcell->priv->popup_menu.menu) {
			gtk_widget_destroy (pictcell->priv->popup_menu.menu);
			pictcell->priv->popup_menu.menu = NULL;
		}
		common_pict_create_menu (&(pictcell->priv->popup_menu), widget, &(pictcell->priv->bindata),
					 &(pictcell->priv->options),
					 (PictCallback) pict_data_changed_cb, pictcell);

		common_pict_adjust_menu_sensitiveness (&(pictcell->priv->popup_menu), pictcell->priv->editable,
						       &(pictcell->priv->bindata));
		gtk_menu_popup_at_pointer (GTK_MENU (pictcell->priv->popup_menu.menu), NULL);
	}

	return FALSE;
}


