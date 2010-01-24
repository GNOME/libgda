/* gdaui-data-cell-renderer-cgrid.c
 *
 * Copyright (C) 2007 - 2009 Carlos Savoretti
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

#include <glib/gi18n-lib.h>

#include <libgda-ui/gdaui-data-entry.h>

#include "gdaui-entry-cgrid.h"
#include "gdaui-data-cell-renderer-cgrid.h"

struct _GdauiDataCellRendererCGridPrivate {
	GdaDataHandler        *data_handler;    /* Data handler. */
	GType                  gtype;           /* Cgrid gtype. */
	gchar                 *options;         /* Cgrid options. */
	gboolean               editable;
	gboolean               to_be_deleted;

	GValue                *value;
	GdaValueAttribute      value_attributes;
};

enum {
	PROP_0,
	PROP_DATA_HANDLER,
	PROP_GTYPE,
	PROP_OPTIONS,
	PROP_EDITABLE,
	PROP_TO_BE_DELETED,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint cgrid_signals[SIGNAL_LAST];

G_DEFINE_TYPE (GdauiDataCellRendererCGrid, gdaui_data_cell_renderer_cgrid, GTK_TYPE_CELL_RENDERER_TEXT)

static GObjectClass *parent_class;

/**
 * gdaui_data_cell_renderer_cgrid_new:
 *
 * Creates a new #GdauiDataCellRendererCGrid.
 *
 * Returns the newly created #GdauiDataCellRendererCGrid.
 */
GdauiDataCellRendererCGrid *
gdaui_data_cell_renderer_cgrid_new (GdaDataHandler  *data_handler,
				       GType            gtype,
				       const gchar     *options)
{
	return (GdauiDataCellRendererCGrid *) g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_CGRID,
							      "data-handler", data_handler,
							      "gtype", gtype,
							      "options", g_strdup (options),
							      NULL);
}

static void
gdaui_data_cell_renderer_cgrid_init (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));
	
	cgrid->priv = g_new0 (GdauiDataCellRendererCGridPrivate, 1);
	cgrid->priv->data_handler = NULL;
	cgrid->priv->gtype = G_TYPE_INVALID;
	cgrid->priv->options = NULL;
}

static void
gdaui_data_cell_renderer_cgrid_finalize (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));

	if (cgrid->priv) {
		if (cgrid->priv->data_handler) { 
			g_object_unref (G_OBJECT(cgrid->priv->data_handler)); 
			cgrid->priv->data_handler = NULL; 
		} 
		if (cgrid->priv->options) {
			g_free (G_OBJECT(cgrid->priv->options));
			cgrid->priv->options = NULL;
		}
		g_free (cgrid->priv);
		cgrid->priv = NULL;
	}

	if (parent_class->finalize)
		parent_class->finalize (G_OBJECT(cgrid));
}

/**
 * gdaui_data_cell_renderer_cgrid_get_data_handler
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * Get the data_handler for this cgrid.
 */
GdaDataHandler *
gdaui_data_cell_renderer_cgrid_get_data_handler (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), NULL);

	return cgrid->priv->data_handler;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_data_handler:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @data_handler: the cgrid data_handler.
 *
 * Set the data_handler for this cgrid.
 */
void
gdaui_data_cell_renderer_cgrid_set_data_handler (GdauiDataCellRendererCGrid  *cgrid,
						    GdaDataHandler          *data_handler)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));

	if (cgrid->priv->data_handler)
		g_object_unref (G_OBJECT(cgrid->priv->data_handler));

	cgrid->priv->data_handler = (GdaDataHandler *) data_handler;
	g_object_ref (G_OBJECT(cgrid->priv->data_handler));

	g_object_notify (G_OBJECT(cgrid), "data-handler");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_gtype
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * Get the gtype for this cgrid.
 */
GType
gdaui_data_cell_renderer_cgrid_get_gtype (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), G_TYPE_INVALID);

	return cgrid->priv->gtype;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_gtype:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @gtype: the cgrid gtype.
 *
 * Set the gtype for this cgrid.
 */
void
gdaui_data_cell_renderer_cgrid_set_gtype (GdauiDataCellRendererCGrid  *cgrid,
					     GType                    gtype)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));

	cgrid->priv->gtype = gtype;

	g_object_notify (G_OBJECT(cgrid), "gtype");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_options
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * Get the options for this cgrid.
 */
const gchar *
gdaui_data_cell_renderer_cgrid_get_options (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), NULL);

	return cgrid->priv->options;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_options:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @options: the cgrid options.
 *
 * Set the options for this cgrid.
 */
void
gdaui_data_cell_renderer_cgrid_set_options (GdauiDataCellRendererCGrid  *cgrid,
					       const gchar                   *options)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));

	if (cgrid->priv->options)
		g_free (G_OBJECT(cgrid->priv->options));

	cgrid->priv->options = g_strdup (options);

	g_object_notify (G_OBJECT(cgrid), "options");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_editable
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * TRUE if the cgrid is itself editable.
 */
gboolean
gdaui_data_cell_renderer_cgrid_get_editable (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), FALSE);
	return cgrid->priv->editable;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_editable:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @editable: the cgrid editable.
 *
 * Set to TRUE if this cgrid is editable.
 */
void
gdaui_data_cell_renderer_cgrid_set_editable (GdauiDataCellRendererCGrid  *cgrid,
						gboolean                 editable)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));
	cgrid->priv->editable = editable;
	g_object_notify (G_OBJECT(cgrid), "editable");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_to_be_deleted
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * TRUE if the cgrid is itself to_be_deleted.
 */
gboolean
gdaui_data_cell_renderer_cgrid_get_to_be_deleted (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), FALSE);
	return cgrid->priv->to_be_deleted;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_to_be_deleted:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @to_be_deleted: the cgrid to_be_deleted.
 *
 * Set to TRUE if this cgrid is to_be_deleted.
 */
void
gdaui_data_cell_renderer_cgrid_set_to_be_deleted (GdauiDataCellRendererCGrid  *cgrid,
						     gboolean                 to_be_deleted)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));
	cgrid->priv->to_be_deleted = to_be_deleted;
	g_object_notify (G_OBJECT(cgrid), "to-be-deleted");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_value
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * TRUE if the cgrid is itself value.
 */
GValue *
gdaui_data_cell_renderer_cgrid_get_value (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), FALSE);
	return cgrid->priv->value;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_value:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @value: the cgrid value.
 *
 * Set to TRUE if this cgrid is value.
 */
void
gdaui_data_cell_renderer_cgrid_set_value (GdauiDataCellRendererCGrid  *cgrid,
					     const GValue                  *value)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));

	if (cgrid->priv->value) {
		gda_value_free (cgrid->priv->value);
		cgrid->priv->value = NULL;
	}

	if (!value)
		return;

	cgrid->priv->value = gda_value_copy (value);
	g_object_notify (G_OBJECT(cgrid), "value");
}

/**
 * gdaui_data_cell_renderer_cgrid_get_value_attributes
 * @cgrid: a #GdauiDataCellRendererCGrid.
 *
 * TRUE if the cgrid is itself value_attributes.
 */
GdaValueAttribute
gdaui_data_cell_renderer_cgrid_get_value_attributes (GdauiDataCellRendererCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid), FALSE);
	return cgrid->priv->value_attributes;
}

/**
 * gdaui_data_cell_renderer_cgrid_set_value_attributes:
 * @cgrid: a #GdauiDataCellRendererCGrid.
 * @value_attributes: the cgrid value_attributes.
 *
 * Set to TRUE if this cgrid is value_attributes.
 */
void
gdaui_data_cell_renderer_cgrid_set_value_attributes (GdauiDataCellRendererCGrid  *cgrid,
							GdaValueAttribute        value_attributes)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID (cgrid));
	cgrid->priv->value_attributes = value_attributes;
	g_object_notify (G_OBJECT(cgrid), "value-attributes");
}

static void
gdaui_data_cell_renderer_cgrid_set_property (GObject       *object,
						guint          param_id,
						const GValue  *value,
						GParamSpec    *pspec)
{
	GdauiDataCellRendererCGrid *cgrid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID(object));

	cgrid = GDAUI_DATA_CELL_RENDERER_CGRID(object);

	switch (param_id) {
	case PROP_DATA_HANDLER:
		gdaui_data_cell_renderer_cgrid_set_data_handler (cgrid, g_value_get_object (value));
		break;
	case PROP_GTYPE:
		gdaui_data_cell_renderer_cgrid_set_gtype (cgrid, g_value_get_gtype (value));
		break;
	case PROP_OPTIONS:
		gdaui_data_cell_renderer_cgrid_set_options (cgrid, g_value_get_string (value));
		break;
	case PROP_EDITABLE:
		gdaui_data_cell_renderer_cgrid_set_editable (cgrid, g_value_get_boolean (value));
		break;
	case PROP_TO_BE_DELETED:
		gdaui_data_cell_renderer_cgrid_set_to_be_deleted (cgrid, g_value_get_boolean (value));
		break;
	case PROP_VALUE:
		gdaui_data_cell_renderer_cgrid_set_value (cgrid, g_value_get_pointer (value));
		break;
	case PROP_VALUE_ATTRIBUTES:
		gdaui_data_cell_renderer_cgrid_set_value_attributes (cgrid, g_value_get_flags (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

}

static void
gdaui_data_cell_renderer_cgrid_get_property (GObject     *object,
						guint        param_id,
						GValue      *value,
						GParamSpec  *pspec)
{
	GdauiDataCellRendererCGrid *cgrid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID(object));

	cgrid = GDAUI_DATA_CELL_RENDERER_CGRID (object);

	switch (param_id) {
	case PROP_DATA_HANDLER:
		g_value_set_object (value, cgrid->priv->data_handler);
		break;
	case PROP_GTYPE:
		g_value_set_gtype (value, cgrid->priv->gtype);
		break;
	case PROP_OPTIONS:
		g_value_set_string (value, cgrid->priv->options);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, cgrid->priv->editable);
		break;
	case PROP_TO_BE_DELETED:
		g_value_set_boolean (value, cgrid->priv->to_be_deleted);
		break;
	case PROP_VALUE:
		g_value_set_pointer (value, cgrid->priv->value);
		break;
	case PROP_VALUE_ATTRIBUTES:
		g_value_set_flags (value, cgrid->priv->value_attributes);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

}

static void
gdaui_data_cell_renderer_cgrid_get_size (GtkCellRenderer  *renderer,
					    GtkWidget        *widget,
					    GdkRectangle     *rectangle,
					    gint             *x_offset,
					    gint             *y_offset,
					    gint             *width,
					    gint             *height)
{
	GtkCellRendererClass *renderer_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);

	(renderer_class->get_size) (renderer, widget, rectangle, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_cgrid_render (GtkCellRenderer       *renderer,
					  GdkWindow             *window,
					  GtkWidget             *widget,
					  GdkRectangle          *background_rectangle,
					  GdkRectangle          *cell_rectangle,
					  GdkRectangle          *expose_rectangle,
					  GtkCellRendererState   flags)
{
	GtkCellRendererClass *renderer_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);

	(renderer_class->render) (renderer, window, widget, background_rectangle, cell_rectangle, expose_rectangle, flags);

	if (GDAUI_DATA_CELL_RENDERER_CGRID(renderer)->priv->to_be_deleted) {
		GtkStyle *style;
		guint xpad;

		g_object_get ((GObject*) widget, "style", &style, NULL);
		g_object_get ((GObject*) renderer, "xpad", &xpad, NULL);

		gtk_paint_hline (style,
                                 window, GTK_STATE_SELECTED,
                                 cell_rectangle,
                                 widget,
                                 "hline",
                                 cell_rectangle->x + xpad, 
				 cell_rectangle->x + cell_rectangle->width - xpad,
                                 cell_rectangle->y + cell_rectangle->height / 2.);
		g_object_unref (style);
	}
}

static void
gdaui_data_cell_renderer_cgrid_editing_done (GtkCellEditable  *editable,
						gpointer          data)
{
	g_return_if_fail (GDAUI_IS_DATA_CELL_RENDERER_CGRID(data));

	GValue *gvalue = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY(editable));

	g_signal_emit (G_OBJECT(data), cgrid_signals[SIGNAL_CHANGED], 0, gvalue);

	gda_value_free (gvalue);
}

static GtkCellEditable *
gdaui_data_cell_renderer_cgrid_start_editing (GtkCellRenderer       *renderer,
						 GdkEvent              *event,
						 GtkWidget             *widget,
						 const gchar           *path,
						 GdkRectangle          *background_rectangle,
						 GdkRectangle          *cell_rectangle,
						 GtkCellRendererState   flags)
{
	GdauiDataCellRendererCGrid *cgrid = GDAUI_DATA_CELL_RENDERER_CGRID(renderer);

	gboolean editable;
	g_object_get (G_OBJECT(renderer), "editable", &editable, NULL); 

	GtkWidget *entry = (GtkWidget *) gdaui_entry_cgrid_new (cgrid->priv->data_handler,
								   cgrid->priv->gtype,
								   cgrid->priv->options);

	g_object_set (G_OBJECT(entry),
		      "is-cell-renderer", TRUE,
		      "actions", FALSE,
		      NULL); 

	gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY(entry),
					     cgrid->priv->value);

	g_signal_connect (G_OBJECT(entry), "editing-done",
			  G_CALLBACK(gdaui_data_cell_renderer_cgrid_editing_done),
			  cgrid);

	gtk_widget_show (entry);

	return (GtkCellEditable *) entry;
}

static void
gdaui_data_cell_renderer_cgrid_class_init (GdauiDataCellRendererCGridClass  *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	renderer_class->get_size = gdaui_data_cell_renderer_cgrid_get_size;
	renderer_class->render = gdaui_data_cell_renderer_cgrid_render;
	renderer_class->start_editing = gdaui_data_cell_renderer_cgrid_start_editing;

	gobject_class->finalize = (GObjectFinalizeFunc) gdaui_data_cell_renderer_cgrid_finalize;
	gobject_class->set_property = (GObjectSetPropertyFunc) gdaui_data_cell_renderer_cgrid_set_property;
	gobject_class->get_property = (GObjectGetPropertyFunc) gdaui_data_cell_renderer_cgrid_get_property;

	klass->changed = NULL;

	g_object_class_install_property
		(gobject_class,
		 PROP_DATA_HANDLER,
		 g_param_spec_object ("data-handler", _("Cgrid data handler"),
				      _("The cgrid data handler"),
				      GDA_TYPE_DATA_HANDLER,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_GTYPE,
		 g_param_spec_gtype ("gtype", _("Cgrid gtype"),
				      _("The cgrid gtype"),
				      G_TYPE_NONE,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_OPTIONS,
		 g_param_spec_string ("options", _("Cgrid options"),
				      _("The cgrid options"),
				      NULL,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_EDITABLE,
		 g_param_spec_boolean ("editable", _("Cgrid is editable"),
				      _("Cgrid editable"),
				      TRUE,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_TO_BE_DELETED,
		 g_param_spec_boolean ("to-be-deleted", _("Cgrid is to be deleted"),
				      _("Cgrid to be deleted"),
				      TRUE,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_VALUE,
		 g_param_spec_pointer ("value", _("Cgrid value"),
				      _("Cgrid value"),
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_VALUE_ATTRIBUTES,
		 g_param_spec_flags ("value-attributes", _("Cgrid value attributes"),
				     _("Cgrid value attributes"),
				     GDA_TYPE_VALUE_ATTRIBUTE, GDA_VALUE_ATTR_NONE,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	cgrid_signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE(klass),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GdauiDataCellRendererCGridClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE, 1, G_TYPE_VALUE);
}
