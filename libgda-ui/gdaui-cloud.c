/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-cloud.h"
#include <gdk/gdkkeysyms.h>
#include "gdaui-data-selector.h"
#include <libgda/gda-debug-macros.h>

static void gdaui_cloud_dispose (GObject *object);
static void gdaui_cloud_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec);
static void gdaui_cloud_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);

/* GdauiDataSelector interface */
static void              gdaui_cloud_selector_init (GdauiDataSelectorInterface *iface);
static GdaDataModel     *cloud_selector_get_model (GdauiDataSelector *iface);
static void              cloud_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *cloud_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *cloud_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          cloud_selector_select_row (GdauiDataSelector *iface, gint row);
static void              cloud_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              cloud_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

typedef struct
{
	GdaDataModel        *model;
	GdaDataModelIter    *iter;
	gint                 label_column;
	gint                 weight_column;
	GdauiCloudWeightFunc weight_func;
	gpointer             weight_func_data;

	gdouble              min_scale;
	gdouble              max_scale;

	GtkTextBuffer       *tbuffer;
        GtkWidget           *tview;
	GSList              *selected_tags;
	GtkSelectionMode     selection_mode;

        gboolean             hovering_over_link;
} GdauiCloudPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiCloud, gdaui_cloud, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GdauiCloud)
                         G_IMPLEMENT_INTERFACE(GDAUI_TYPE_DATA_SELECTOR, gdaui_cloud_selector_init))

enum {
	ACTIVATE,
        LAST_SIGNAL
};

static guint objects_cloud_signals[LAST_SIGNAL] = { 0 };

/* properties */
enum
{
        PROP_0,
	PROP_MODEL,
	PROP_LABEL_COLUMN,
	PROP_WEIGHT_COLUMN,
	PROP_MIN_SCALE,
	PROP_MAX_SCALE,
};

static void
cloud_map (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (gdaui_cloud_parent_class)->map)
        	GTK_WIDGET_CLASS (gdaui_cloud_parent_class)->map (widget);
}

static void
gdaui_cloud_class_init (GdauiCloudClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->dispose = gdaui_cloud_dispose;
	GTK_WIDGET_CLASS (object_class)->map = cloud_map;

	/* signals */
        objects_cloud_signals [ACTIVATE] =
                g_signal_new ("activate",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdauiCloudClass, activate),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
        klass->activate = NULL;

	/* Properties */
        object_class->set_property = gdaui_cloud_set_property;
        object_class->get_property = gdaui_cloud_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
	                                 g_param_spec_object ("model", NULL, NULL,
	                                                      GDA_TYPE_DATA_MODEL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_LABEL_COLUMN,
	                                 g_param_spec_int ("label-column", NULL,
							   "Column in the data model which contains the "
							   "text to display, the column must be a G_TYPE_STRING",
							   -1, G_MAXINT, -1, G_PARAM_READWRITE));
 	g_object_class_install_property (object_class, PROP_WEIGHT_COLUMN,
	                                 g_param_spec_int ("weight-column", NULL, NULL,
							   -1, G_MAXINT, -1, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MIN_SCALE,
	                                 g_param_spec_double ("min-scale", NULL, NULL,
							      .1, 10., .8, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MAX_SCALE,
	                                 g_param_spec_double ("max-scale", NULL, NULL,
							      .1, 10., 3., G_PARAM_READWRITE));
}

static void
gdaui_cloud_selector_init (GdauiDataSelectorInterface *iface)
{
	iface->get_model = cloud_selector_get_model;
	iface->set_model = cloud_selector_set_model;
	iface->get_selected_rows = cloud_selector_get_selected_rows;
	iface->get_data_set = cloud_selector_get_data_set;
	iface->select_row = cloud_selector_select_row;
	iface->unselect_row = cloud_selector_unselect_row;
	iface->set_column_visible = cloud_selector_set_column_visible;
}

static void
sync_iter_with_selection (GdauiCloud *cloud)
{
	GSList *list;
	gint selrow = -1;
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);

	if (! priv->iter)
		return;

	/* locate selected row */
	for (list = priv->selected_tags; list; list = list->next) {
		gint row;
		row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "row")) - 1;
		if (row >= 0) {
			if (selrow == -1)
				selrow = row;
			else {
				selrow = -1;
				break;
			}
		}
	}
	
	/* update iter */
	if ((selrow == -1) || !gda_data_model_iter_move_to_row (priv->iter, selrow)) {
		gda_data_model_iter_invalidate_contents (priv->iter);
		g_object_set (G_OBJECT (priv->iter), "current-row", -1, NULL);
	}
}

static void
update_display (GdauiCloud *cloud)
{
	GtkTextBuffer *tbuffer;
	GtkTextIter start, end;
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);

	/* clean all */
	tbuffer = priv->tbuffer;
	gtk_text_buffer_get_start_iter (tbuffer, &start);
	gtk_text_buffer_get_end_iter (tbuffer, &end);
	gtk_text_buffer_delete (tbuffer, &start, &end);
	if (priv->selected_tags) {
		g_slist_free_full (priv->selected_tags, (GDestroyNotify) g_object_unref);
		priv->selected_tags = NULL;
		sync_iter_with_selection (cloud);
		g_signal_emit_by_name (cloud, "selection-changed");
	}

	if (!priv->model)
		return;
	if (priv->label_column < 0)
		return;
	/* check for the data model's column type */
	GdaColumn *column;
	column = gda_data_model_describe_column (priv->model, priv->label_column);
	if (!column || (gda_column_get_g_type (column) != G_TYPE_STRING)) {
		g_warning (_("Wrong column type for label: expecting a string and got a %s"),
			   gda_g_type_to_string (gda_column_get_g_type (column)));
		return;
	}

	gint nrows, i;
	nrows = gda_data_model_get_n_rows (priv->model);

	/* compute scale range */
	gdouble min_weight = G_MAXDOUBLE, max_weight = G_MINDOUBLE, wrange;
	if ((priv->weight_column >= 0) || priv->weight_func) {
		for (i = 0; i < nrows; i++) {
			const GValue *cvalue;
			gdouble weight = 1.;
			if (priv->weight_func) {
				weight = priv->weight_func (priv->model, i,
								   priv->weight_func_data);
				min_weight = MIN (min_weight, weight);
				max_weight = MAX (max_weight, weight);
			}
			else {
				cvalue = gda_data_model_get_value_at (priv->model,
								      priv->weight_column, i, NULL);
				if (cvalue) {
					weight = g_ascii_strtod (gda_value_stringify (cvalue), NULL);
					min_weight = MIN (min_weight, weight);
					max_weight = MAX (max_weight, weight);
				}
			}
		}
	}

	if (max_weight > min_weight)
		wrange = (priv->max_scale - priv->min_scale) / (max_weight - min_weight);
	else
		wrange = 0.;
	
	gtk_text_buffer_get_start_iter (tbuffer, &start);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;
		gdouble weight = 1.;
		const gchar *ptr;
		GString *string;
		cvalue = gda_data_model_get_value_at (priv->model, priv->label_column, i, NULL);
		if (!cvalue) {
			TO_IMPLEMENT;
			continue;
		}
		if (!g_value_get_string (cvalue))
			continue;

		/* convert spaces to non breaking spaces (0xC2 0xA0 as UTF8) */
		string = g_string_new ("");
		for (ptr = g_value_get_string (cvalue); *ptr; ptr++) {
			if (*ptr == ' ') {
				g_string_append_c (string, 0xC2);
				g_string_append_c (string, 0xA0);
			}
			else
				g_string_append_c (string, *ptr);
		}

		if ((priv->weight_column >= 0) || priv->weight_func) {
			if (priv->weight_func) {
				weight = priv->weight_func (priv->model, i,
								   priv->weight_func_data);
				weight = priv->min_scale + wrange * (weight - min_weight);
			}
			else {
				cvalue = gda_data_model_get_value_at (priv->model,
								      priv->weight_column, i, NULL);
				if (cvalue) {
					weight = g_ascii_strtod (gda_value_stringify (cvalue), NULL);
					weight = priv->min_scale + wrange * (weight - min_weight);
				}
			}
		}

		GtkTextTag *tag;
		tag = gtk_text_buffer_create_tag (priv->tbuffer, NULL,
					  "foreground", "#6161F2", 
					  "scale", weight,
					  NULL);
		g_object_set_data ((GObject*) tag, "row", GINT_TO_POINTER (i) + 1);
		gtk_text_buffer_insert_with_tags (priv->tbuffer, &start, string->str, -1,
						  tag, NULL);
		g_string_free (string, TRUE);
		gtk_text_buffer_insert (priv->tbuffer, &start, "   ", -1);
	}
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, GdauiCloud *cloud);
static void event_after (GtkWidget *text_view, GdkEvent *ev, GdauiCloud *cloud);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, GdauiCloud *cloud);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, GdauiCloud *cloud);

static void
gdaui_cloud_init (GdauiCloud *cloud)
{
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	priv->min_scale = .8;
	priv->max_scale = 2.;
	priv->selected_tags = NULL;
	priv->selection_mode = GTK_SELECTION_SINGLE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (cloud), GTK_ORIENTATION_VERTICAL);

	/* text buffer */
        priv->tbuffer = gtk_text_buffer_new (NULL);
        gtk_text_buffer_create_tag (priv->tbuffer, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

	/* text view */
	GtkWidget *sw, *vbox, *vp;
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (cloud), sw, TRUE, TRUE, 0);

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	vp = gtk_viewport_new (NULL, NULL);
        gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);

        gtk_container_add (GTK_CONTAINER (sw), vp);
	gtk_container_add (GTK_CONTAINER (vp), vbox);

        priv->tview = gtk_text_view_new_with_buffer (priv->tbuffer);
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->tview), GTK_WRAP_WORD);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->tview), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (priv->tview), FALSE);
        gtk_box_pack_start (GTK_BOX (vbox), priv->tview, TRUE, TRUE, 0);
        gtk_widget_show_all (sw);

        g_signal_connect (priv->tview, "key-press-event",
                          G_CALLBACK (key_press_event), cloud);
        g_signal_connect (priv->tview, "event-after",
                          G_CALLBACK (event_after), cloud);
        g_signal_connect (priv->tview, "motion-notify-event",
                          G_CALLBACK (motion_notify_event), cloud);
        g_signal_connect (priv->tview, "visibility-notify-event",
                          G_CALLBACK (visibility_notify_event), cloud);
}

/**
 * gdaui_cloud_new:
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiCloud widget suitable to display the data in @model
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_cloud_new (GdaDataModel *model, gint label_column, gint weight_column)
{
	GdauiCloud *cloud;
	
	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);
	if (label_column < -1)
		label_column = -1;
	if (weight_column < -1)
		weight_column = -1;

	cloud = (GdauiCloud *) g_object_new (GDAUI_TYPE_CLOUD,
					     "label-column", label_column,
					     "weight-column", weight_column,
					     "model", model, NULL);
	return (GtkWidget *) cloud;
}

static void
gdaui_cloud_dispose (GObject *object)
{
	GdauiCloud *cloud;

	g_return_if_fail (GDAUI_IS_CLOUD (object));

	cloud = GDAUI_CLOUD (object);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);

	if (priv->selected_tags) {
		g_slist_free_full (priv->selected_tags, (GDestroyNotify) g_object_unref);
    priv->selected_tags = NULL;
	}
	if (priv->iter) {
		g_object_unref (priv->iter);
    priv->iter = NULL;
  }
	if (priv->model) {
		g_object_unref (priv->model);
    priv->model = NULL;
  }
	if (priv->tbuffer) {
		g_object_unref (priv->tbuffer);
    priv->tbuffer = NULL;
  }

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_cloud_parent_class)->dispose (object);
}

static void
model_reset_cb (G_GNUC_UNUSED GdaDataModel *model, GdauiCloud *cloud)
{
	update_display (cloud);
}

static void
gdaui_cloud_set_property (GObject *object,
			  guint param_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	GdauiCloud *cloud;
	GdaDataModel *model;
	
	cloud = GDAUI_CLOUD (object);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	
	switch (param_id) {
	case PROP_MODEL:
		model = (GdaDataModel*) g_value_get_object (value);
		if (priv->model != model) {
			if (priv->iter) {
				g_object_unref (priv->iter);
				priv->iter = NULL;
			}
			if (priv->model) {
				g_signal_handlers_disconnect_by_func (priv->model,
								      G_CALLBACK (model_reset_cb), cloud);
				g_object_unref (priv->model);
			}
			priv->model = model;
			if (model) {
				g_signal_connect (model, "reset",
						  G_CALLBACK (model_reset_cb), cloud);
				g_object_ref (G_OBJECT (model));
			}
			update_display (cloud);
		}
		break;
	case PROP_LABEL_COLUMN:
		if (priv->label_column !=  g_value_get_int (value)) {
			priv->label_column = g_value_get_int (value);
			update_display (cloud);
		}
		break;
	case PROP_WEIGHT_COLUMN:
		if (priv->weight_column !=  g_value_get_int (value)) {
			priv->weight_column = g_value_get_int (value);
			update_display (cloud);
		}
		break;
	case PROP_MIN_SCALE:
		if (priv->min_scale !=  g_value_get_double (value)) {
			priv->min_scale = g_value_get_double (value);
			update_display (cloud);
		}
		break;
	case PROP_MAX_SCALE:
		if (priv->max_scale !=  g_value_get_double (value)) {
			priv->max_scale = g_value_get_double (value);
			update_display (cloud);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_cloud_get_property (GObject *object,
			  guint param_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GdauiCloud *cloud;

	cloud = GDAUI_CLOUD (object);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	
	switch (param_id) {
	case PROP_MODEL:
		g_value_set_object (value, priv->model);
		break;
	case PROP_LABEL_COLUMN:
		g_value_set_int (value, priv->label_column);
		break;
	case PROP_WEIGHT_COLUMN:
		g_value_set_int (value, priv->weight_column);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}	
}

/**
 * gdaui_cloud_set_selection_mode:
 * @cloud: a #GdauiCloud widget
 * @mode: the desired selection mode
 *
 * Sets @cloud's selection mode
 *
 * Since: 4.2
 */
void
gdaui_cloud_set_selection_mode   (GdauiCloud *cloud, GtkSelectionMode mode)
{
	g_return_if_fail (GDAUI_IS_CLOUD (cloud));
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if (mode == priv->selection_mode)
		return;
	switch (mode) {
	case GTK_SELECTION_NONE:
		if (priv->selected_tags) {
			/* remove any selection */
			GSList *list;
			for (list = priv->selected_tags; list; list = list->next) {
				g_object_unref ((GObject*) list->data);
				g_object_set ((GObject*) list->data,
					      "background-set", FALSE,
					      NULL);
			}

			g_slist_free (priv->selected_tags);
			priv->selected_tags = NULL;
			sync_iter_with_selection (cloud);
			g_signal_emit_by_name (cloud, "selection-changed");
		}
		break;
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_BROWSE:
		if (priv->selected_tags && priv->selected_tags->next) {
			/* cut down to 1 selected node */
			GSList *newsel;
			newsel = priv->selected_tags;
			priv->selected_tags = g_slist_remove_link (priv->selected_tags,
									  priv->selected_tags);
			GSList *list;
			for (list = priv->selected_tags; list; list = list->next) {
				g_object_unref ((GObject*) list->data);
				g_object_set ((GObject*) list->data,
					      "background-set", FALSE,
					      NULL);
			}
			g_slist_free (priv->selected_tags);
			priv->selected_tags = newsel;
			sync_iter_with_selection (cloud);
			g_signal_emit_by_name (cloud, "selection-changed");
		}
		break;
	case GTK_SELECTION_MULTIPLE:
		break;
	default:
		g_warning ("Unknown selection mode");
		return;
	}
	priv->selection_mode = mode;
}

static void
row_clicked (GdauiCloud *cloud, gint row, GtkTextTag *tag)
{
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if (priv->selection_mode == GTK_SELECTION_NONE) {
		/* emit "activate" signal */
		g_signal_emit (cloud, objects_cloud_signals [ACTIVATE], 0, row);
		return;
	}

	/* toggle @rows's selection */
	if (g_slist_find (priv->selected_tags, tag)) {
		priv->selected_tags = g_slist_remove (priv->selected_tags, tag);
		g_object_set ((GObject*) tag,
			      "background-set", FALSE,
			      NULL);
		g_object_unref ((GObject*) tag);
	}
	else {
		priv->selected_tags = g_slist_prepend (priv->selected_tags, tag);
		g_object_ref ((GObject*) tag);
		g_object_set ((GObject*) tag,
			      "background", "yellow",
			      "background-set", TRUE,
			      NULL);

		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark (priv->tbuffer, &iter,
                                                  gtk_text_buffer_get_insert (priv->tbuffer));
		while (1) {
			gunichar guchar;
			guchar = gtk_text_iter_get_char (&iter);
			if (g_unichar_isspace (guchar) && (guchar != 0x00A0)) {
				gtk_text_iter_forward_char (&iter);
				break;
			}
			if (!gtk_text_iter_backward_char (&iter))
				break;
		}
		gtk_text_buffer_place_cursor (priv->tbuffer, &iter);
	}

	if ((priv->selection_mode == GTK_SELECTION_SINGLE) ||
	    (priv->selection_mode == GTK_SELECTION_BROWSE)) {
		/* no more than 1 element can be selected */
		if (priv->selected_tags && priv->selected_tags->next) {
			GtkTextTag *tag2;
			tag2 = GTK_TEXT_TAG (priv->selected_tags->next->data);
			priv->selected_tags = g_slist_remove (priv->selected_tags, tag2);
			g_object_set ((GObject*) tag2,
				      "background-set", FALSE,
				      NULL);
			g_object_unref ((GObject*) tag2);
		}
	}
	if (priv->selection_mode == GTK_SELECTION_BROWSE) {
		/* one element is always selected */
		if (! priv->selected_tags) {
			priv->selected_tags = g_slist_prepend (priv->selected_tags, tag);
			g_object_ref ((GObject*) tag);
			g_object_set ((GObject*) tag,
				      "background", "yellow",
				      "background-set", TRUE,
				      NULL);
		}
	}

	sync_iter_with_selection (cloud);
	g_signal_emit_by_name (cloud, "selection-changed");
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;

/* Looks at all tags covering the position (x, y) in the text view, 
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view, gint x, gint y, GdauiCloud *cloud)
{
	GSList *tags = NULL, *tagp;
	GtkTextIter iter;
	gboolean hovering = FALSE;
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	
	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		gint row;
		row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
		if (row >= 0) {
			hovering = TRUE;
			break;
		}
	}
	
	if (hovering != priv->hovering_over_link) {
		priv->hovering_over_link = hovering;
		
		if (priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new_for_display (
							gtk_widget_get_display (GTK_WIDGET (text_view)),
							GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new_for_display (
							gtk_widget_get_display (GTK_WIDGET (text_view)),
							GDK_XTERM);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       regular_cursor);
		}
	}
	
	if (tags) 
		g_slist_free (tags);
}

/* 
 * Also update the cursor image if the window becomes visible
 * (e.g. when a window covering it got iconified).
 */
static gboolean
visibility_notify_event (GtkWidget *text_view, G_GNUC_UNUSED GdkEventVisibility *event, GdauiCloud *cloud)
{
	gint wx, wy, bx, by;
	GdkSeat *seat;
        GdkDevice *pointer;

        seat = gdk_display_get_default_seat (gtk_widget_get_display (text_view));
        pointer = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position (gtk_widget_get_window (text_view), pointer, &wx, &wy, NULL);
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       wx, wy, &bx, &by);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), bx, by, cloud);
	
	return FALSE;
}

/*
 * Update the cursor image if the pointer moved. 
 */
static gboolean
motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, GdauiCloud *cloud)
{
	gint x, y;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), x, y, cloud);
	return FALSE;
}

/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (G_GNUC_UNUSED GtkWidget *text_view, GtkTextIter *iter, GdauiCloud *cloud)
{
	GSList *tags = NULL, *tagp;
	
	tags = gtk_text_iter_get_tags (iter);
	for (tagp = tags;  tagp;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		gint row;
		row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
		if (row >= 0) {
			row_clicked (cloud, row, tag);
			break;
		}
        }

	if (tags) 
		g_slist_free (tags);
}

/*
 * Links can also be activated by clicking.
 */
static void
event_after (GtkWidget *text_view, GdkEvent *ev, GdauiCloud *cloud)
{
	GtkTextIter start, end, iter;
	GtkTextBuffer *buffer;
	GdkEventButton *event;
	gint x, y;
	
	if (ev->type != GDK_BUTTON_RELEASE)
		return;
	
	event = (GdkEventButton *)ev;
	
	if (event->button != 1)
		return;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	
	/* we shouldn't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
		return;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);
	
	follow_if_link (text_view, &iter, cloud);
	
	return;
}

/* 
 * Links can be activated by pressing Enter.
 */
static gboolean
key_press_event (GtkWidget *text_view, GdkEventKey *event, GdauiCloud *cloud)
{
        GtkTextIter iter;
        GtkTextBuffer *buffer;
        GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);

        switch (event->keyval) {
        case GDK_KEY_Return:
        case GDK_KEY_space:
        case GDK_KEY_KP_Enter:
                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
                gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                                  gtk_text_buffer_get_insert (buffer));
                follow_if_link (text_view, &iter, cloud);
		return TRUE;
	case GDK_KEY_Up:
	case GDK_KEY_Down:
	case GDK_KEY_Left:
	case GDK_KEY_Right:
		if ((priv->selection_mode == GTK_SELECTION_SINGLE) ||
		    (priv->selection_mode == GTK_SELECTION_BROWSE)) {
			GtkTextIter iter;
			if (priv->selected_tags) {
				GtkTextMark *mark;
				mark = gtk_text_buffer_get_insert (priv->tbuffer);
				gtk_text_buffer_get_iter_at_mark (priv->tbuffer, &iter, mark);
			}
			else if ((event->keyval == GDK_KEY_Right) || (event->keyval == GDK_KEY_Down))
				gtk_text_buffer_get_start_iter (priv->tbuffer, &iter);
			else
				gtk_text_buffer_get_end_iter (priv->tbuffer, &iter);
			
			gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (priv->tview), TRUE);
			while (1) { /* loop to move the cursor enough positions to change the selected item */
				gboolean done = FALSE;
				GtkMovementStep mvt_type;
				gint mvt_amount;
				switch (event->keyval) {
				case GDK_KEY_Up:
					done = ! gtk_text_view_backward_display_line ((GtkTextView*)priv->tview, &iter);
					mvt_type = GTK_MOVEMENT_DISPLAY_LINES;
					mvt_amount = -1;
					break;
				case GDK_KEY_Down:
					done = ! gtk_text_view_forward_display_line ((GtkTextView*)priv->tview, &iter);
					mvt_type = GTK_MOVEMENT_DISPLAY_LINES;
					mvt_amount = 1;
					break;
				case GDK_KEY_Left:
					done = ! gtk_text_iter_backward_char (&iter);
					mvt_type = GTK_MOVEMENT_VISUAL_POSITIONS;
					mvt_amount = -1;
					break;
				default:
				case GDK_KEY_Right:
					done = ! gtk_text_iter_forward_char (&iter);
					mvt_type = GTK_MOVEMENT_VISUAL_POSITIONS;
					mvt_amount = 1;
					break;
				}
				if (done)
					break; /* end of treatment as no movement possible */
				g_signal_emit_by_name (priv->tview, "move-cursor",
						       mvt_type, mvt_amount, FALSE);

				GtkTextMark *mark;
				mark = gtk_text_buffer_get_insert (priv->tbuffer);
				gtk_text_buffer_get_iter_at_mark (priv->tbuffer, &iter, mark);

				GSList *tags, *tagp;
				done = FALSE;
				tags = gtk_text_iter_get_tags (&iter);
				for (tagp = tags;  tagp;  tagp = tagp->next) {
					GtkTextTag *tag = (GtkTextTag*) tagp->data;
					gint row;
					row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
					if (row >= 0) {
						if ((priv->selected_tags &&
						     (tag != priv->selected_tags->data)) ||
						    !priv->selected_tags) {
							row_clicked (cloud, row, tag);
							done = TRUE;
							break;
						}
					}
				}
				if (tags) 
					g_slist_free (tags);
				if (done) {
					GtkTextMark *mark;
					    
					mark = gtk_text_buffer_get_insert (priv->tbuffer);
					gtk_text_view_scroll_mark_onscreen ((GtkTextView*)priv->tview, mark);
					break;
				}
			}
			gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (priv->tview), FALSE);
			return TRUE;
		}
        default:
                break;
        }
        return FALSE;
}

typedef struct {
	GdauiCloud *cloud;
	const gchar *find;
} FilterData;

static gchar *
prepare_cmp_string (const gchar *str)
{
	GString *string;
	gchar *ptr, *tmp1, *tmp2;

	/* lower case */
	tmp1 = g_utf8_strdown (str, -1);
	
	/* normalize */
	tmp2 = g_utf8_normalize (tmp1, -1, G_NORMALIZE_DEFAULT);
	g_free (tmp1);

	/* remove accents */
	string = g_string_new ("");
	for (ptr = tmp2; *ptr; ptr = g_utf8_next_char (ptr)) {
		gunichar uc;
		uc = g_utf8_get_char (ptr);
		if (! g_unichar_ismark (uc))
			g_string_append_unichar (string, uc);
	}
	return g_string_free (string, FALSE);
}

static void
text_tag_table_foreach_cb (GtkTextTag *tag, FilterData *fdata)
{
	const GValue *cvalue;
	const gchar *label;
	gint row;
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (fdata->cloud);

	if (! priv->model)
		return;
	if (priv->label_column < 0)
		return;

	/* check for the data model's column type */
	GdaColumn *column;
	column = gda_data_model_describe_column (priv->model,
						 priv->label_column);
	if (!column || (gda_column_get_g_type (column) != G_TYPE_STRING)) {
		g_warning (_("Wrong column type for label: expecting a string and got a %s"),
			   gda_g_type_to_string (gda_column_get_g_type (column)));
		return;
	}

	row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
	if (row < 0)
		return;

	cvalue = gda_data_model_get_value_at (priv->model,
					      priv->label_column, row, NULL);
	if (!cvalue)
		return;

	label = g_value_get_string (cvalue);

	if (!(fdata->find) || !*(fdata->find)) {
		g_object_set (tag, "foreground", "#6161F2", NULL);
	}
	else {
		gchar *ptr, *lcname, *lcfind;
		lcname = prepare_cmp_string (label);
		lcfind = prepare_cmp_string (fdata->find);

		ptr = strstr (lcname, lcfind);
		if (!ptr) {
			/* string not present in name */
			g_object_set (tag, "foreground", "#DBDBDB", NULL);
		}
		else if ((ptr == lcname) ||
			 ((*label == '"') && (ptr == lcname+1))) {
			/* string present as start of name */
			g_object_set (tag, "foreground", "#6161F2", NULL);
		}
		else {
			/* string present in name but not at the start */
			g_object_set (tag, "foreground", "#A0A0A0", NULL);
		}

		g_free (lcname);
		g_free (lcfind);
	}
}

/**
 * gdaui_cloud_filter:
 * @cloud: a #GdauiCloud widget
 * @filter: (nullable): the filter to use, or %NULL to remove any filter
 *
 * Filters the elements displayed in @cloud, by altering their color.
 *
 * Since: 4.2
 */
void
gdaui_cloud_filter (GdauiCloud *cloud, const gchar *filter)
{
	g_return_if_fail (GDAUI_IS_CLOUD (cloud));
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);

	GtkTextTagTable *tags_table = gtk_text_buffer_get_tag_table (priv->tbuffer);

	FilterData fdata;
	fdata.cloud = cloud;
	fdata.find = filter;
	gtk_text_tag_table_foreach (tags_table, (GtkTextTagTableForeach) text_tag_table_foreach_cb,
				    (gpointer) &(fdata));
}

static void
find_entry_changed_cb (GtkWidget *entry, GdauiCloud *cloud)
{
	gchar *find = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	gdaui_cloud_filter (cloud, find);
	g_free (find);
}

/**
 * gdaui_cloud_create_filter_widget:
 * @cloud: a #GdauiCloud widget
 *
 * Creates a search widget linked directly to modify @cloud's appearance.
 *
 * Returns: (transfer full): a new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_cloud_create_filter_widget (GdauiCloud *cloud)
{
	GtkWidget *hbox, *label, *wid;
	g_return_val_if_fail (GDAUI_IS_CLOUD (cloud), NULL);
	
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	label = gtk_label_new (_("Find:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	wid = gtk_entry_new ();
	g_signal_connect (wid, "changed",
			  G_CALLBACK (find_entry_changed_cb), cloud);
	gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);
	gtk_widget_hide (hbox);

	return hbox;
}

/**
 * gdaui_cloud_set_weight_func:
 * @cloud: a #GdauiCloud widget
 * @func: (nullable) (scope notified): a #GdauiCloudWeightFunc function which computes weights, or %NULL to unset
 * @data: (nullable): a pointer to pass as last argument of @func each time it is called, or %NULL
 *
 * Specifies a function called by @cloud to compute each row's respective weight.
 *
 * Since: 4.2
 */
void
gdaui_cloud_set_weight_func (GdauiCloud *cloud, GdauiCloudWeightFunc func, gpointer data)
{
	g_return_if_fail (GDAUI_IS_CLOUD (cloud));
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if ((priv->weight_func != func) || (priv->weight_func_data != data)) {
		priv->weight_func = func;
		priv->weight_func_data = data;
		update_display (cloud);
	}
}

/* GdauiDataSelector interface */
static GdaDataModel *
cloud_selector_get_model (GdauiDataSelector *iface)
{
	GdauiCloud *cloud;
	cloud = GDAUI_CLOUD (iface);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	return priv->model;
}

static void
cloud_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	g_object_set (G_OBJECT (iface), "model", model, NULL);
}

static GArray *
cloud_selector_get_selected_rows (GdauiDataSelector *iface)
{
	GArray *retval = NULL;
	GdauiCloud *cloud;
	GSList *list;

	cloud = GDAUI_CLOUD (iface);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	for (list = priv->selected_tags; list; list = list->next) {
		gint row;
		row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "row")) - 1;
		if (row >= 0) {
			if (!retval)
				retval = g_array_new (FALSE, FALSE, sizeof (gint));
			g_array_append_val (retval, row);
		}
	}
	return retval;

}

static GdaDataModelIter *
cloud_selector_get_data_set (GdauiDataSelector *iface)
{
	GdauiCloud *cloud;

	cloud = GDAUI_CLOUD (iface);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if (! priv->iter && priv->model) {
		priv->iter = gda_data_model_create_iter (priv->model);
		sync_iter_with_selection (cloud);
	}

	return priv->iter;
}

typedef struct {
	gint row_to_find;
	GtkTextTag *tag;
} RowLookup;

static void
text_tag_table_foreach_cb2 (GtkTextTag *tag, RowLookup *rl)
{
	if (rl->tag)
		return; /* row already found */
	gint srow;
	srow = GPOINTER_TO_INT (g_object_get_data ((GObject*) tag, "row")) - 1;
	if (srow == rl->row_to_find)
		rl->tag = tag;
}

static gboolean
cloud_selector_select_row (GdauiDataSelector *iface, gint row)
{
	GdauiCloud *cloud;
	GSList *list;

	cloud = GDAUI_CLOUD (iface);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if (priv->selection_mode == GTK_SELECTION_NONE)
		return FALSE;

	/* test if row already selected */
	for (list = priv->selected_tags; list; list = list->next) {
		gint srow;
		srow = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "row")) - 1;
		if (srow == row)
			return TRUE;
	}

	/* try to select row */
	RowLookup rl;
	rl.row_to_find = row;
	rl.tag = NULL;
	gtk_text_tag_table_foreach (gtk_text_buffer_get_tag_table (priv->tbuffer),
				    (GtkTextTagTableForeach) text_tag_table_foreach_cb2,
				    (gpointer) &rl);
	if (rl.tag) {
		row_clicked (cloud, row, rl.tag);
		for (list = priv->selected_tags; list; list = list->next) {
			gint srow;
			srow = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "row")) - 1;
			if (srow == row)
				return TRUE;
		}
		return FALSE;
	}
	else
		return FALSE;
}

static void
cloud_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	GdauiCloud *cloud;
	GSList *list;

	cloud = GDAUI_CLOUD (iface);
	GdauiCloudPrivate *priv = gdaui_cloud_get_instance_private (cloud);
	if (priv->selection_mode == GTK_SELECTION_NONE)
		return;

	/* test if row already selected */
	for (list = priv->selected_tags; list; list = list->next) {
		gint srow;
		GtkTextTag *tag = (GtkTextTag*) list->data;
		srow = GPOINTER_TO_INT (g_object_get_data ((GObject*) tag, "row")) - 1;
		if (srow == row) {
			priv->selected_tags = g_slist_remove (priv->selected_tags, tag);
			g_object_set ((GObject*) tag,
				      "background-set", FALSE,
				      NULL);
			g_object_unref ((GObject*) tag);

			sync_iter_with_selection (cloud);
			g_signal_emit_by_name (cloud, "selection-changed");
			break;
		}
	}
}

static void
cloud_selector_set_column_visible (G_GNUC_UNUSED GdauiDataSelector *iface, G_GNUC_UNUSED gint column,
				   G_GNUC_UNUSED gboolean visible)
{
	/* nothing to do */
}
