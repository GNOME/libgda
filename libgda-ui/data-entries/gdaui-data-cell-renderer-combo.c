/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include "gdaui-entry-combo.h"
#include "gdaui-data-cell-renderer-combo.h"
#include "marshallers/gdaui-custom-marshal.h"
#include <libgda-ui/gdaui-combo.h>
#include <libgda-ui/gdaui-data-entry.h>
#include <libgda/gda-enum-types.h>
#include "gdaui-data-cell-renderer-util.h"

#define GDAUI_DATA_CELL_RENDERER_COMBO_PATH "gdaui-data-cell-renderer-combo-path"

static void gdaui_data_cell_renderer_combo_dispose    (GObject *object);

static void gdaui_data_cell_renderer_combo_get_property  (GObject *object,
							  guint param_id,
							  GValue *value,
							  GParamSpec *pspec);
static void gdaui_data_cell_renderer_combo_set_property  (GObject *object,
							  guint param_id,
							  const GValue *value,
							  GParamSpec *pspec);
static void gdaui_data_cell_renderer_combo_get_size   (GtkCellRenderer          *cell,
						       GtkWidget                *widget,
						       const GdkRectangle       *cell_area,
						       gint                     *x_offset,
						       gint                     *y_offset,
						       gint                     *width,
						       gint                     *height);
static void gdaui_data_cell_renderer_combo_render     (GtkCellRenderer          *cell,
						       cairo_t                  *cr,
						       GtkWidget                *widget,
						       const GdkRectangle       *background_area,
						       const GdkRectangle       *cell_area,
						       GtkCellRendererState      flags);

static GtkCellEditable *gdaui_data_cell_renderer_combo_start_editing (GtkCellRenderer     *cell,
								      GdkEvent            *event,
								      GtkWidget           *widget,
								      const gchar         *path,
								      const GdkRectangle  *background_area,
								      const GdkRectangle  *cell_area,
								      GtkCellRendererState flags);

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_VALUES,
	PROP_VALUES_DISPLAY,
	PROP_VALUE_ATTRIBUTES,
	PROP_TO_BE_DELETED,
	PROP_SET_DEFAULT_IF_INVALID,
	PROP_SHOW_EXPANDER,
	PROP_PARAMLIST,
	PROP_PARAMLISTSOURCE
};

typedef struct
{
	GdauiSet       *paramlist;
	GdauiSetSource *source;
	guint         focus_out_id;
	guint         attributes;
	gboolean      to_be_deleted;
	gboolean      set_default_if_invalid;
	gboolean      show_expander;
	gboolean      invalid;
} GdauiDataCellRendererComboPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataCellRendererCombo, gdaui_data_cell_renderer_combo, GTK_TYPE_CELL_RENDERER_TEXT)


static guint text_cell_renderer_combo_signals [LAST_SIGNAL] = { 0 };

static void
gdaui_data_cell_renderer_combo_init (GdauiDataCellRendererCombo *datacell)
{
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);
	g_object_set ((GObject*) datacell, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
	priv->attributes = 0;
	priv->set_default_if_invalid = FALSE;
	priv->show_expander = TRUE;
}

static void
gdaui_data_cell_renderer_combo_class_init (GdauiDataCellRendererComboClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->dispose = gdaui_data_cell_renderer_combo_dispose;

	object_class->get_property = gdaui_data_cell_renderer_combo_get_property;
	object_class->set_property = gdaui_data_cell_renderer_combo_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_combo_get_size;
	cell_class->render = gdaui_data_cell_renderer_combo_render;
	cell_class->start_editing = gdaui_data_cell_renderer_combo_start_editing;
  
	g_object_class_install_property (object_class,
					 PROP_VALUES,
					 g_param_spec_pointer ("values",
							       _("Values limited to PK fields"),
							       _("GList of GValue to render, limited to PK fields"),
							       G_PARAM_WRITABLE));
	
	g_object_class_install_property (object_class,
					 PROP_VALUES_DISPLAY,
					 g_param_spec_pointer ("values-display",
							       _("Values"),
							       _("GList of GValue to render, not limited to PK fields "),
							       G_PARAM_WRITABLE));
  
	g_object_class_install_property (object_class,
					 PROP_VALUE_ATTRIBUTES,
					 g_param_spec_flags ("value-attributes", NULL, NULL, GDA_TYPE_VALUE_ATTRIBUTE,
							     GDA_VALUE_ATTR_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TO_BE_DELETED,
					 g_param_spec_boolean ("to-be-deleted", NULL, NULL, FALSE,
                                                               G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_EXPANDER,
					 g_param_spec_boolean ("show-expander", NULL, NULL, FALSE,
                                                               G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_SET_DEFAULT_IF_INVALID,
					 g_param_spec_boolean ("set-default-if-invalid", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
  
	g_object_class_install_property (object_class, PROP_PARAMLIST,
					 g_param_spec_object ("data-set", NULL, NULL, GDAUI_TYPE_SET,
							      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE)));

	/* Ideally, GdaSetSource would be a boxed type, but it is not yet, so we use g_param_spec_pointer(). */
	g_object_class_install_property (object_class, PROP_PARAMLISTSOURCE,
					 g_param_spec_pointer ("data-set-source", NULL, NULL,
                                                               (G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE)));
	  
	text_cell_renderer_combo_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererComboClass, changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__STRING_SLIST_SLIST,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);

}

static void
gdaui_data_cell_renderer_combo_dispose (GObject *object)
{
	GdauiDataCellRendererCombo *datacell = GDAUI_DATA_CELL_RENDERER_COMBO (object);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);

	if (priv->paramlist) {
		g_object_unref (priv->paramlist);
		priv->paramlist = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_combo_parent_class)->dispose (object);
}

static void
gdaui_data_cell_renderer_combo_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec)
{
	GdauiDataCellRendererCombo *datacell = GDAUI_DATA_CELL_RENDERER_COMBO (object);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);

	switch (param_id) {
	case PROP_VALUE_ATTRIBUTES:
		g_value_set_flags (value, priv->attributes);
		break;
	case PROP_SET_DEFAULT_IF_INVALID:
		g_value_set_boolean (value, priv->set_default_if_invalid);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static gchar *render_text_to_display_from_values (GList *values);

static void
gdaui_data_cell_renderer_combo_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec)
{
	GdauiDataCellRendererCombo *datacell = GDAUI_DATA_CELL_RENDERER_COMBO (object);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);

	switch (param_id) {
	case PROP_VALUES:
		priv->invalid = FALSE;
		if (value) {	
			GList *gvalues = g_value_get_pointer (value);
			if (gvalues) {
				GSList *values = NULL;
				gint length = 0, row;
				gboolean allnull = TRUE;

				/* copy gvalues into the values GSList */
				while (gvalues) {
					values = g_slist_append (values, gvalues->data);
					
					if (!gvalues->data || 
					    (gvalues->data && !gda_value_is_null ((GValue *)(gvalues->data))))
						allnull = FALSE;

					length ++;
					gvalues = g_list_next (gvalues);
				}

				g_return_if_fail (length == gdaui_set_source_get_ref_n_cols (priv->source));
				
				if (allnull) 
					g_object_set (G_OBJECT (object), "text", "", NULL);
				else {
					/* find the data model row for the values */
					/* if (gdaui_data_model_get_status (priv->data_model) &  */
					/* 					    GDAUI_DATA_MODEL_NEEDS_INIT_REFRESH) */
					/* 						gdaui_data_model_refresh (priv->data_model, NULL); */
					GdaDataModel *source_model;
					source_model = gda_set_source_get_data_model (gdaui_set_source_get_source (priv->source));
					row = gda_data_model_get_row_from_values (source_model,
										  values,
										  gdaui_set_source_get_ref_columns (priv->source));
					if (row >= 0) {
						GList *dsplay_values = NULL;
						gint i;
						gchar *str;
						
						for (i = 0; i < gdaui_set_source_get_shown_n_cols (priv->source); i++) {
							const GValue *value;
							gint* cols;
							cols = gdaui_set_source_get_shown_columns (priv->source);
							value = gda_data_model_get_value_at (source_model, cols [i], row, NULL);
							dsplay_values = g_list_append (dsplay_values, (GValue *) value);
						}
						str = render_text_to_display_from_values (dsplay_values);
						g_list_free (dsplay_values);
						g_object_set (G_OBJECT (object), "text", str, NULL);
						g_free (str);
					}
					else {
						if (priv->attributes & GDA_VALUE_ATTR_CAN_BE_NULL)
							g_object_set (G_OBJECT (object), "text", "", NULL);
						else
							g_object_set (G_OBJECT (object), "text", "???", NULL);
					}
				}

				g_slist_free (values);
			}
			else {
				priv->invalid = TRUE;
				g_object_set (G_OBJECT (object), "text", "", NULL);
			}
		}
		else {
			priv->invalid = TRUE;
			g_object_set (G_OBJECT (object), "text", "", NULL);
		}
		
		g_object_notify (object, "values");
		break;
	case PROP_VALUES_DISPLAY:
		if (value) {
			GList *gvalues = g_value_get_pointer (value);
			gchar *str;

			g_assert (g_list_length (gvalues) == (guint) gdaui_set_source_get_shown_n_cols (priv->source));
			str = render_text_to_display_from_values (gvalues);
			g_object_set (G_OBJECT (object), "text", str, NULL);
			g_free (str);
		}
		else
			g_object_set (G_OBJECT (object), "text", "", NULL);
		
		g_object_notify (object, "values-display");
		break;
	case PROP_VALUE_ATTRIBUTES:
		priv->attributes = g_value_get_flags (value);
		break;
	case PROP_TO_BE_DELETED:
		priv->to_be_deleted = g_value_get_boolean (value);
		break;
	case PROP_SHOW_EXPANDER:
		priv->show_expander = g_value_get_boolean (value);
		break;
	case PROP_SET_DEFAULT_IF_INVALID:
		priv->set_default_if_invalid = g_value_get_boolean (value);
		break;
	case PROP_PARAMLIST:
		if (priv->paramlist)
			g_object_unref (priv->paramlist);

		priv->paramlist = GDAUI_SET (g_value_get_object(value));
		if(priv->paramlist)
			g_object_ref(priv->paramlist);

		g_object_ref (G_OBJECT (priv->paramlist));
		break;
	case PROP_PARAMLISTSOURCE:
		priv->source = GDAUI_SET_SOURCE (g_value_get_pointer(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static gchar *
render_text_to_display_from_values (GList *values)
{
	GList *list = values;
	gboolean allnull = TRUE;
	GString *string = g_string_new ("");
	gchar *retval;
	gchar *str;
	
	while (list) {
		if (list->data && !gda_value_is_null ((GValue *)(list->data)))
			allnull = FALSE;

		if (list != values)
			g_string_append (string, " / ");
		if (list->data) {
			str = gda_value_stringify ((GValue *)(list->data));
			/* TODO: use GdaDataHandler */
			g_string_append (string, str);
			g_free (str);
		}
		else
			g_string_append (string, " ? ");

		list = g_list_next (list);
	}

	if (!allnull) {
		retval = string->str;
		g_string_free (string, FALSE);
	}
	else {
		retval = g_strdup ("");
		g_string_free (string, TRUE);
	}

	return retval;
}

/**
 * gdaui_data_cell_renderer_combo_new:
 * @paramlist: a #GdaSet object
 * @source: a #GdauiSetSource structure listed in @paramlist->sources_list
 * 
 * Creates a new #GdauiDataCellRendererCombo which will fill the parameters listed in
 * @source->nodes with values available from @source->data_model.
 * 
 * Returns: (transfer full): the new cell renderer
 **/
GtkCellRenderer *
gdaui_data_cell_renderer_combo_new (GdauiSet *paramlist, GdauiSetSource *source)
{
	GObject *obj;

	g_return_val_if_fail (GDAUI_IS_SET (paramlist), NULL);
	g_return_val_if_fail (source, NULL);
	g_return_val_if_fail (g_slist_find (gdaui_set_get_sources (paramlist), source), NULL);

	obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_COMBO, "data-set", paramlist, 
			    "data-set-source", source, NULL);
	
	return GTK_CELL_RENDERER (obj);
}

static void
gdaui_data_cell_renderer_combo_get_size (GtkCellRenderer *cell,
					 GtkWidget       *widget,
					 const GdkRectangle *cell_area,
					 gint            *x_offset,
					 gint            *y_offset,
					 gint            *width,
					 gint            *height)
{
	gint calc_width = 0;
	gint calc_height = 0;

	/* get the size as calculated by the GtkCellRendererText */
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (GDAUI_DATA_CELL_RENDERER_COMBO (cell));
	
	/* Add more space for the popdown menu symbol */
	if (priv->show_expander) {
		guint xpad, ypad;
		g_object_get ((GObject*) cell, "xpad", &xpad, "ypad", &ypad, NULL);
		gint expander_size;
		gtk_widget_style_get (widget, "expander-size", &expander_size, NULL);
		calc_width = (gint) xpad * 2 + expander_size;
		calc_height = (gint) ypad * 2 + expander_size;
	}
	
	if (width)
		*width += calc_width;
	
	if (height && (*height < calc_height))
		*height = calc_height;
}

static void
gdaui_data_cell_renderer_combo_render (GtkCellRenderer      *cell,
				       cairo_t              *cr,
				       GtkWidget            *widget,
				       const GdkRectangle   *background_area,
				       const GdkRectangle   *cell_area,
				       GtkCellRendererState  flags)
	
{
	GdauiDataCellRendererCombo *datacell = GDAUI_DATA_CELL_RENDERER_COMBO (cell);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);

	/* render the text as for the GtkCellRendererText */
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->render) (cell, cr, widget, background_area, cell_area, flags);

	/* render the popdown menu symbol */
	if (priv->show_expander) {
		gint expander_size;
		GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
		guint xpad, ypad;

		gtk_widget_style_get (widget, "expander-size", &expander_size, NULL);
		g_object_get ((GObject*) cell, "xpad", &xpad, "ypad", &ypad, NULL);
		
		
		gtk_render_expander (style_context,
				    cr,
				    cell_area->x + cell_area->width - xpad - expander_size/2.,
				    cell_area->y + cell_area->height - ypad - expander_size/2.,
                                    expander_size, expander_size);
	}

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

static void gdaui_data_cell_renderer_combo_editing_done (GtkCellEditable *combo, GdauiDataCellRendererCombo *datacell);
static gboolean gdaui_data_cell_renderer_combo_focus_out_event (GtkWidget *widget, GdkEvent *event, 
								GdauiDataCellRendererCombo *datacell);

static GtkCellEditable *
gdaui_data_cell_renderer_combo_start_editing (GtkCellRenderer     *cell,
					      G_GNUC_UNUSED GdkEvent            *event,
					      G_GNUC_UNUSED GtkWidget           *widget,
					      const gchar         *path,
					      G_GNUC_UNUSED const GdkRectangle        *background_area,
					      G_GNUC_UNUSED const GdkRectangle        *cell_area,
					      G_GNUC_UNUSED GtkCellRendererState flags)
{
	GdauiDataCellRendererCombo *datacell;
	GtkWidget *combo;
	gboolean editable;
	GdaDataModel *source_model;

	g_object_get ((GObject*) cell, "editable", &editable, NULL);
	if (editable == FALSE)
		return NULL;

	datacell = GDAUI_DATA_CELL_RENDERER_COMBO (cell);
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);
	source_model = gda_set_source_get_data_model (gdaui_set_source_get_source (priv->source));
	combo = gdaui_combo_new_with_model (source_model,
					    gdaui_set_source_get_shown_n_cols (priv->source),
					    gdaui_set_source_get_shown_columns (priv->source));
	
	g_object_set (combo, "has-frame", FALSE, NULL);
	g_object_set_data_full (G_OBJECT (combo),
				GDAUI_DATA_CELL_RENDERER_COMBO_PATH,
				g_strdup (path), g_free);
	gdaui_combo_add_null (GDAUI_COMBO (combo),
				      (priv->attributes & GDA_VALUE_ATTR_CAN_BE_NULL) ? TRUE : FALSE);
	gtk_widget_show (combo);

	g_signal_connect (GTK_CELL_EDITABLE (combo), "editing-done",
			  G_CALLBACK (gdaui_data_cell_renderer_combo_editing_done), datacell);
	priv->focus_out_id = g_signal_connect (combo, "focus-out-event",
							 G_CALLBACK (gdaui_data_cell_renderer_combo_focus_out_event),
							 datacell);
	
	return GTK_CELL_EDITABLE (combo);
}


static void
gdaui_data_cell_renderer_combo_editing_done (GtkCellEditable *combo, GdauiDataCellRendererCombo *datacell)
{
	const gchar *path;
	gboolean canceled;
	GSList *list, *list_all;
	GdauiDataCellRendererComboPrivate *priv = gdaui_data_cell_renderer_combo_get_instance_private (datacell);

	if (priv->focus_out_id > 0) {
		g_signal_handler_disconnect (combo, priv->focus_out_id);
		priv->focus_out_id = 0;
	}
	
	/*canceled = _gtk_combo_box_editing_canceled (GTK_COMBO_BOX (combo));*/
	canceled = FALSE; /* FIXME */
	gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (datacell), canceled);
	if (canceled)
		return;
	
	list = _gdaui_combo_get_selected_ext (GDAUI_COMBO (combo), 
					      gdaui_set_source_get_ref_n_cols (priv->source),
					      gdaui_set_source_get_ref_columns (priv->source));
	list_all = _gdaui_combo_get_selected_ext (GDAUI_COMBO (combo), 0, NULL);

	path = g_object_get_data (G_OBJECT (combo), GDAUI_DATA_CELL_RENDERER_COMBO_PATH);
	g_signal_emit (datacell, text_cell_renderer_combo_signals [CHANGED], 0, path, list, list_all);
	g_slist_free (list);
	g_slist_free (list_all);
}


static gboolean
gdaui_data_cell_renderer_combo_focus_out_event (GtkWidget *widget, G_GNUC_UNUSED GdkEvent  *event,
						GdauiDataCellRendererCombo *datacell)
{
  
	gdaui_data_cell_renderer_combo_editing_done (GTK_CELL_EDITABLE (widget), datacell);
	
	return FALSE;
}
