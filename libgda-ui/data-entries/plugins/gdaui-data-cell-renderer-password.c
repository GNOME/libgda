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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include <libgda/libgda.h>
#include "gdaui-data-cell-renderer-password.h"
#include "custom-marshal.h"
#include "gdaui-entry-password.h"
#include <libgda/gda-enum-types.h>
#include "gdaui-data-cell-renderer-util.h"

static void gdaui_data_cell_renderer_password_init       (GdauiDataCellRendererPassword      *celltext);
static void gdaui_data_cell_renderer_password_class_init (GdauiDataCellRendererPasswordClass *class);
static void gdaui_data_cell_renderer_password_dispose    (GObject *object);
static void gdaui_data_cell_renderer_password_finalize   (GObject *object);

static void gdaui_data_cell_renderer_password_get_property  (GObject *object,
							     guint param_id,
							     GValue *value,
							     GParamSpec *pspec);
static void gdaui_data_cell_renderer_password_set_property  (GObject *object,
							     guint param_id,
							     const GValue *value,
							     GParamSpec *pspec);
static void gdaui_data_cell_renderer_password_get_size   (GtkCellRenderer          *cell,
							  GtkWidget                *widget,
							  const GdkRectangle       *cell_area,
							  gint                     *x_offset,
							  gint                     *y_offset,
							  gint                     *width,
							  gint                     *height);
static void gdaui_data_cell_renderer_password_render     (GtkCellRenderer          *cell,
							  cairo_t                  *cr,
							  GtkWidget                *widget,
							  const GdkRectangle       *background_area,
							  const GdkRectangle       *cell_area,
							  GtkCellRendererState      flags);

static GtkCellEditable *gdaui_data_cell_renderer_password_start_editing (GtkCellRenderer      *cell,
									 GdkEvent             *event,
									 GtkWidget            *widget,
									 const gchar          *path,
									 const GdkRectangle   *background_area,
									 const GdkRectangle   *cell_area,
									 GtkCellRendererState  flags);

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_TO_BE_DELETED,
	PROP_DATA_HANDLER,
	PROP_TYPE
};

struct _GdauiDataCellRendererPasswordPrivate
{
	GdaDataHandler *dh;
	GType           type;
	gboolean        type_forced; /* TRUE if ->type has been forced by a value and changed from what was specified */
	GValue         *value;
	gboolean        to_be_deleted;
	gchar          *options;
	gboolean        invalid;
};

typedef struct
{
	/* text renderer */
	gulong focus_out_id;
} GdauiDataCellRendererPasswordInfo;
#define GDAUI_DATA_CELL_RENDERER_PASSWORD_INFO_KEY "__info_key_P"



static GObjectClass *parent_class = NULL;
static guint text_cell_renderer_password_signals [LAST_SIGNAL];

#define GDAUI_DATA_CELL_RENDERER_PASSWORD_PATH "__path_P"

GType
gdaui_data_cell_renderer_password_get_type (void)
{
	static GType cell_text_type = 0;

	if (!cell_text_type) {
		static const GTypeInfo cell_text_info =	{
			sizeof (GdauiDataCellRendererPasswordClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_data_cell_renderer_password_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiDataCellRendererPassword),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gdaui_data_cell_renderer_password_init,
			0
		};

		cell_text_type =
			g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "GdauiDataCellRendererPassword",
						&cell_text_info, 0);
	}

	return cell_text_type;
}

static void
gdaui_data_cell_renderer_password_init (GdauiDataCellRendererPassword *datacell)
{
	datacell->priv = g_new0 (GdauiDataCellRendererPasswordPrivate, 1);
	datacell->priv->dh = NULL;
	datacell->priv->type = GDA_TYPE_NULL;
	datacell->priv->type_forced = FALSE;
	datacell->priv->value = NULL;
	datacell->priv->options = NULL;

	g_object_set ((GObject*) datacell, "xalign", 0., "yalign", 0.,
		      "xpad", 2, "ypad", 2, NULL);
}

static void
gdaui_data_cell_renderer_password_class_init (GdauiDataCellRendererPasswordClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

    	object_class->dispose = gdaui_data_cell_renderer_password_dispose;
	object_class->finalize = gdaui_data_cell_renderer_password_finalize;

	object_class->get_property = gdaui_data_cell_renderer_password_get_property;
	object_class->set_property = gdaui_data_cell_renderer_password_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_password_get_size;
	cell_class->render = gdaui_data_cell_renderer_password_render;
	cell_class->start_editing = gdaui_data_cell_renderer_password_start_editing;

	g_object_class_install_property (object_class,
					 PROP_VALUE,
					 g_param_spec_pointer ("value",
							       _("Value"),
							       _("GValue to render"),
							       G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_VALUE_ATTRIBUTES,
					 g_param_spec_flags ("value-attributes", NULL, NULL, GDA_TYPE_VALUE_ATTRIBUTE,
							     GDA_VALUE_ATTR_NONE, G_PARAM_READWRITE));

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

	text_cell_renderer_password_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererPasswordClass, changed),
			      NULL, NULL,
			      _marshal_VOID__STRING_VALUE,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_VALUE);

}

static void
gdaui_data_cell_renderer_password_dispose (GObject *object)
{
	GdauiDataCellRendererPassword *datacell = GDAUI_DATA_CELL_RENDERER_PASSWORD (object);

	if (datacell->priv->dh) {
		g_object_unref (G_OBJECT (datacell->priv->dh));
		datacell->priv->dh = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_cell_renderer_password_finalize (GObject *object)
{
	GdauiDataCellRendererPassword *datacell = GDAUI_DATA_CELL_RENDERER_PASSWORD (object);

	if (datacell->priv) {
		g_free (datacell->priv->options);
		g_free (datacell->priv);
		datacell->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static void
gdaui_data_cell_renderer_password_get_property (GObject *object,
						guint param_id,
						G_GNUC_UNUSED GValue *value,
						GParamSpec *pspec)
{
	switch (param_id) {
	case PROP_VALUE_ATTRIBUTES:
		/* nothing to do */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_data_cell_renderer_password_set_property (GObject *object,
						guint         param_id,
						const GValue *value,
						GParamSpec *pspec)
{
	GdauiDataCellRendererPassword *datacell = GDAUI_DATA_CELL_RENDERER_PASSWORD (object);

	switch (param_id) {
	case PROP_VALUE:
		/* FIXME: is it necessary to make a copy of value, couldn't we just use the
		 * value AS IS for performances reasons ? */
		if (datacell->priv->value) {
			gda_value_free (datacell->priv->value);
			datacell->priv->value = NULL;
		}

		if (value) {
			GValue *gval = g_value_get_pointer (value);
			if (gval && !gda_value_is_null (gval)) {
				gchar *str;

				if (G_VALUE_TYPE (gval) != datacell->priv->type) {
					if (!datacell->priv->type_forced) {
						datacell->priv->type_forced = TRUE;
						g_warning (_("Data cell renderer's specified type (%s) differs from actual "
							     "value to display type (%s)"),
							   g_type_name (datacell->priv->type),
							   g_type_name (G_VALUE_TYPE (gval)));
					}
					else
						g_warning (_("Data cell renderer asked to display values of different "
							     "data types, at least %s and %s, which means the data model has "
							     "some incoherencies"),
							   g_type_name (datacell->priv->type),
							   g_type_name (G_VALUE_TYPE (gval)));
					datacell->priv->type = G_VALUE_TYPE (gval);
				}

				datacell->priv->value = gda_value_copy (gval);

				if (datacell->priv->dh) {
					gchar *ptr;
					str = gda_data_handler_get_str_from_value (datacell->priv->dh, gval);
					for (ptr = str; *ptr; ptr++)
						*ptr = '*';
					g_object_set (G_OBJECT (object), "text", str, NULL);
					g_free (str);
				}
				else
					g_object_set (G_OBJECT (object), "text", _("<non-printable>"), NULL);
			}
			else if (gval)
				g_object_set (G_OBJECT (object), "text", "", NULL);
			else {
				datacell->priv->invalid = TRUE;
				g_object_set (G_OBJECT (object), "text", "", NULL);
			}
		}
		else {
			datacell->priv->invalid = TRUE;
			g_object_set (G_OBJECT (object), "text", "", NULL);
		}

		g_object_notify (object, "value");
		break;
	case PROP_VALUE_ATTRIBUTES:
		datacell->priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_TO_BE_DELETED:
		datacell->priv->to_be_deleted = g_value_get_boolean (value);
		break;
	case PROP_DATA_HANDLER:
		if (datacell->priv->dh)
			g_object_unref (G_OBJECT (datacell->priv->dh));

		datacell->priv->dh = GDA_DATA_HANDLER(g_value_get_object(value));
		if (datacell->priv->dh)
			g_object_ref (G_OBJECT (datacell->priv->dh));
		break;
	case PROP_TYPE:
		datacell->priv->type = g_value_get_gtype(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_password_new
 * @dh: (nullable): a #GdaDataHandler object, or %NULL
 * @type: the #GType being edited
 *
 * Creates a new #GdauiDataCellRendererPassword.
 *
 * Returns: the new cell renderer
 **/
GtkCellRenderer *
gdaui_data_cell_renderer_password_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;

	g_return_val_if_fail (!dh || GDA_IS_DATA_HANDLER (dh), NULL);
	obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD,
			    "type", type, "data-handler", dh, NULL);

	if (options)
		GDAUI_DATA_CELL_RENDERER_PASSWORD (obj)->priv->options = g_strdup (options);

	return GTK_CELL_RENDERER (obj);
}


static void
gdaui_data_cell_renderer_password_get_size (GtkCellRenderer *cell,
					    GtkWidget       *widget,
					    const GdkRectangle *cell_area,
					    gint            *x_offset,
					    gint            *y_offset,
					    gint            *width,
					    gint            *height)
{
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_password_render (GtkCellRenderer      *cell,
					  cairo_t              *cr,
					  GtkWidget            *widget,
					  const GdkRectangle   *background_area,
					  const GdkRectangle   *cell_area,
					  GtkCellRendererState  flags)

{
	GdauiDataCellRendererPassword *datacell = GDAUI_DATA_CELL_RENDERER_PASSWORD (cell);
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->render) (cell, cr, widget, background_area, cell_area, flags);

	if (datacell->priv->to_be_deleted) {
		GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
		guint xpad;
		g_object_get ((GObject*) cell, "xpad", &xpad, NULL);

		gdouble y = cell_area->y + cell_area->height / 2.;
		gtk_render_line (style_context,
				 cr,
				 cell_area->x + xpad, cell_area->x + cell_area->width - xpad,
				 y, y);
	}
	if (datacell->priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (cr, cell_area);
}

static void
gdaui_data_cell_renderer_password_editing_done (GtkCellEditable *entry,
						gpointer         data)
{
	const gchar *path;
	GdauiDataCellRendererPasswordInfo *info;
	GValue *value;

	info = g_object_get_data (G_OBJECT (data),
				  GDAUI_DATA_CELL_RENDERER_PASSWORD_INFO_KEY);

	if (info->focus_out_id > 0) {
		g_signal_handler_disconnect (entry, info->focus_out_id);
		info->focus_out_id = 0;
	}

	if (g_object_class_find_property (G_OBJECT_GET_CLASS (entry), "editing-canceled")) {
		gboolean editing_canceled;

		g_object_get (G_OBJECT (entry), "editing-canceled", &editing_canceled, NULL);
		if (editing_canceled)
			return;
	}

	path = g_object_get_data (G_OBJECT (entry), GDAUI_DATA_CELL_RENDERER_PASSWORD_PATH);

	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (entry));
	g_signal_emit (data, text_cell_renderer_password_signals[CHANGED], 0, path, value);
	gda_value_free (value);
}

static gboolean
gdaui_data_cell_renderer_password_focus_out_event (GtkWidget *entry,
						   G_GNUC_UNUSED GdkEvent  *event,
						   gpointer   data)
{
	gdaui_data_cell_renderer_password_editing_done (GTK_CELL_EDITABLE (entry), data);

	/* entry needs focus-out-event */
	return FALSE;
}

static GtkCellEditable *
gdaui_data_cell_renderer_password_start_editing (GtkCellRenderer      *cell,
						 G_GNUC_UNUSED GdkEvent             *event,
						 G_GNUC_UNUSED GtkWidget            *widget,
						 const gchar          *path,
						 G_GNUC_UNUSED const GdkRectangle   *background_area,
						 G_GNUC_UNUSED const GdkRectangle   *cell_area,
						 G_GNUC_UNUSED GtkCellRendererState  flags)
{
	GdauiDataCellRendererPassword *datacell;
	GtkWidget *entry;
	GdauiDataCellRendererPasswordInfo *info;
	gboolean editable;

	datacell = GDAUI_DATA_CELL_RENDERER_PASSWORD (cell);

	/* If the cell isn't editable we return NULL. */
	g_object_get (G_OBJECT (cell), "editable", &editable, NULL);
	if (!editable)
		return NULL;
	/* If there is no data handler then the cell also is not editable */
	if (!datacell->priv->dh)
		return NULL;

	entry = gdaui_entry_password_new (datacell->priv->dh, datacell->priv->type, datacell->priv->options);

	g_object_set (G_OBJECT (entry), "is-cell-renderer", TRUE, NULL);

	gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), datacell->priv->value);

	info = g_new0 (GdauiDataCellRendererPasswordInfo, 1);
 	g_object_set_data_full (G_OBJECT (entry), GDAUI_DATA_CELL_RENDERER_PASSWORD_PATH, g_strdup (path), g_free);
	g_object_set_data_full (G_OBJECT (cell), GDAUI_DATA_CELL_RENDERER_PASSWORD_INFO_KEY, info, g_free);

	g_signal_connect (entry, "editing-done",
			  G_CALLBACK (gdaui_data_cell_renderer_password_editing_done), datacell);
	info->focus_out_id = g_signal_connect (entry, "focus-out-event",
					       G_CALLBACK (gdaui_data_cell_renderer_password_focus_out_event),
					       datacell);
	gtk_widget_show (entry);
	return GTK_CELL_EDITABLE (entry);
}
