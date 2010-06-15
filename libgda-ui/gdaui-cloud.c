/* gdaui-cloud.c
 *
 * Copyright (C) 2009 - 2010 Vivien Malerba
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
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-cloud.h"
#include <gdk/gdkkeysyms.h>
#include "internal/popup-container.h"
#include "gdaui-data-selector.h"

static void gdaui_cloud_class_init (GdauiCloudClass * class);
static void gdaui_cloud_init (GdauiCloud *wid);
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
static void              gdaui_cloud_selector_init (GdauiDataSelectorIface *iface);
static GdaDataModel     *cloud_selector_get_model (GdauiDataSelector *iface);
static void              cloud_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *cloud_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *cloud_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          cloud_selector_select_row (GdauiDataSelector *iface, gint row);
static void              cloud_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              cloud_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

struct _GdauiCloudPriv
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
};

enum {
	ACTIVATE,
        LAST_SIGNAL
};

static guint objects_cloud_signals[LAST_SIGNAL] = { 0 };

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

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

GType
gdaui_cloud_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiCloudClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_cloud_class_init,
			NULL,
			NULL,
			sizeof (GdauiCloud),
			0,
			(GInstanceInitFunc) gdaui_cloud_init
		};

		static const GInterfaceInfo selector_info = {
                        (GInterfaceInitFunc) gdaui_cloud_selector_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiCloud", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_SELECTOR, &selector_info);
	}

	return type;
}

static void
cloud_map (GtkWidget *widget)
{
        GTK_WIDGET_CLASS (parent_class)->map (widget);
	GtkStyle *style;
	GdkColor color;
	style = gtk_widget_get_style (widget);
	color = style->bg[GTK_STATE_NORMAL];
	gtk_widget_modify_base (GDAUI_CLOUD (widget)->priv->tview, GTK_STATE_NORMAL, &color);
}

static void
gdaui_cloud_class_init (GdauiCloudClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
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
gdaui_cloud_selector_init (GdauiDataSelectorIface *iface)
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

	if (! cloud->priv->iter)
		return;

	/* locate selected row */
	for (list = cloud->priv->selected_tags; list; list = list->next) {
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
	if ((selrow == -1) || !gda_data_model_iter_move_to_row (cloud->priv->iter, selrow)) {
		gda_data_model_iter_invalidate_contents (cloud->priv->iter);
		g_object_set (G_OBJECT (cloud->priv->iter), "current-row", -1, NULL);
	}
}

static void
update_display (GdauiCloud *cloud)
{
	GtkTextBuffer *tbuffer;
        GtkTextIter start, end;

        /* clean all */
        tbuffer = cloud->priv->tbuffer;
        gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);
	if (cloud->priv->selected_tags) {
		g_slist_foreach (cloud->priv->selected_tags, (GFunc) g_object_unref, NULL);
		g_slist_free (cloud->priv->selected_tags);
		cloud->priv->selected_tags = NULL;
		sync_iter_with_selection (cloud);
		g_signal_emit_by_name (cloud, "selection-changed");
	}

	if (!cloud->priv->model)
		return;
	if (cloud->priv->label_column < 0)
		return;
	/* check for the data model's column type */
	GdaColumn *column;
	column = gda_data_model_describe_column (cloud->priv->model, cloud->priv->label_column);
	if (!column || (gda_column_get_g_type (column) != G_TYPE_STRING)) {
		g_warning (_("Wrong column type for label: expecting a string and got a %s"),
			   gda_g_type_to_string (gda_column_get_g_type (column)));
		return;
	}

	gint nrows, i;
	nrows = gda_data_model_get_n_rows (cloud->priv->model);

	/* compute scale range */
	gdouble min_weight = G_MAXDOUBLE, max_weight = G_MINDOUBLE, wrange;
	if ((cloud->priv->weight_column >= 0) || cloud->priv->weight_func) {
		for (i = 0; i < nrows; i++) {
			const GValue *cvalue;
			gdouble weight = 1.;
			if (cloud->priv->weight_func) {
				weight = cloud->priv->weight_func (cloud->priv->model, i,
								   cloud->priv->weight_func_data);
				min_weight = MIN (min_weight, weight);
				max_weight = MAX (max_weight, weight);
			}
			else {
				cvalue = gda_data_model_get_value_at (cloud->priv->model,
								      cloud->priv->weight_column, i, NULL);
				if (cvalue) {
					weight = atof (gda_value_stringify (cvalue));
					min_weight = MIN (min_weight, weight);
					max_weight = MAX (max_weight, weight);
				}
			}
		}
	}

	if (max_weight > min_weight)
		wrange = (cloud->priv->max_scale - cloud->priv->min_scale) / (max_weight - min_weight);
	else
		wrange = 0.;
	
	gtk_text_buffer_get_start_iter (tbuffer, &start);
	for (i = 0; i < nrows; i++) {
		const GValue *cvalue;
		gdouble weight = 1.;
		const gchar *ptr;
		GString *string;
		cvalue = gda_data_model_get_value_at (cloud->priv->model, cloud->priv->label_column, i, NULL);
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

		if ((cloud->priv->weight_column >= 0) || cloud->priv->weight_func) {
			if (cloud->priv->weight_func) {
				weight = cloud->priv->weight_func (cloud->priv->model, i,
								   cloud->priv->weight_func_data);
				weight = cloud->priv->min_scale + wrange * (weight - min_weight);
			}
			else {
				cvalue = gda_data_model_get_value_at (cloud->priv->model,
								      cloud->priv->weight_column, i, NULL);
				if (cvalue) {
					weight = atof (gda_value_stringify (cvalue));
					weight = cloud->priv->min_scale + wrange * (weight - min_weight);
				}
			}
		}

		GtkTextTag *tag;
		tag = gtk_text_buffer_create_tag (cloud->priv->tbuffer, NULL, 
					  "foreground", "#6161F2", 
					  "scale", weight,
					  NULL);
		g_object_set_data ((GObject*) tag, "row", GINT_TO_POINTER (i) + 1);
		gtk_text_buffer_insert_with_tags (cloud->priv->tbuffer, &start, string->str, -1,
						  tag, NULL);
		g_string_free (string, TRUE);
		gtk_text_buffer_insert (cloud->priv->tbuffer, &start, "   ", -1);
	}
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, GdauiCloud *cloud);
static void event_after (GtkWidget *text_view, GdkEvent *ev, GdauiCloud *cloud);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, GdauiCloud *cloud);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, GdauiCloud *cloud);

static void
gdaui_cloud_init (GdauiCloud *cloud)
{
	cloud->priv = g_new0 (GdauiCloudPriv, 1);
	cloud->priv->min_scale = .8;
	cloud->priv->max_scale = 2.;
	cloud->priv->selected_tags = NULL;
	cloud->priv->selection_mode = GTK_SELECTION_SINGLE;

	/* text buffer */
        cloud->priv->tbuffer = gtk_text_buffer_new (NULL);
        gtk_text_buffer_create_tag (cloud->priv->tbuffer, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

	/* text view */
	GtkWidget *sw, *vbox, *vp;
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (cloud), sw, TRUE, TRUE, 0);

        vbox = gtk_vbox_new (FALSE, 0);
	vp = gtk_viewport_new (NULL, NULL);
        gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);

        gtk_container_add (GTK_CONTAINER (sw), vp);
	gtk_container_add (GTK_CONTAINER (vp), vbox);

        cloud->priv->tview = gtk_text_view_new_with_buffer (cloud->priv->tbuffer);
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (cloud->priv->tview), GTK_WRAP_WORD);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (cloud->priv->tview), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (cloud->priv->tview), FALSE);
        gtk_box_pack_start (GTK_BOX (vbox), cloud->priv->tview, TRUE, TRUE, 0);
        gtk_widget_show_all (sw);

        g_signal_connect (cloud->priv->tview, "key-press-event",
                          G_CALLBACK (key_press_event), cloud);
        g_signal_connect (cloud->priv->tview, "event-after",
                          G_CALLBACK (event_after), cloud);
        g_signal_connect (cloud->priv->tview, "motion-notify-event",
                          G_CALLBACK (motion_notify_event), cloud);
        g_signal_connect (cloud->priv->tview, "visibility-notify-event",
                          G_CALLBACK (visibility_notify_event), cloud);
}

/**
 * gdaui_cloud_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiCloud widget suitable to display the data in @model
 *
 * Returns: the new widget
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

        if (cloud->priv) {
		if (cloud->priv->selected_tags) {
			g_slist_foreach (cloud->priv->selected_tags, (GFunc) g_object_unref, NULL);
			g_slist_free (cloud->priv->selected_tags);
		}
                if (cloud->priv->iter)
                        g_object_unref (cloud->priv->iter);
                if (cloud->priv->model)
                        g_object_unref (cloud->priv->model);
		if (cloud->priv->tbuffer)
			g_object_unref (cloud->priv->tbuffer);

                g_free (cloud->priv);
                cloud->priv = NULL;
        }

        /* for the parent class */
        parent_class->dispose (object);
}

static void
model_reset_cb (GdaDataModel *model, GdauiCloud *cloud)
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
	
	switch (param_id) {
	case PROP_MODEL:
		model = (GdaDataModel*) g_value_get_object (value);
		if (cloud->priv->model != model) {
			if (cloud->priv->iter) {
				g_object_unref (cloud->priv->iter);
				cloud->priv->iter = NULL;
			}
			if (cloud->priv->model) {
				g_signal_handlers_disconnect_by_func (cloud->priv->model,
								      G_CALLBACK (model_reset_cb), cloud);
				g_object_unref (cloud->priv->model);
			}
			cloud->priv->model = model;
			if (model) {
				g_signal_connect (model, "reset",
						  G_CALLBACK (model_reset_cb), cloud);
				g_object_ref (G_OBJECT (model));
			}
			update_display (cloud);
		}
		break;
	case PROP_LABEL_COLUMN:
		if (cloud->priv->label_column !=  g_value_get_int (value)) {
			cloud->priv->label_column = g_value_get_int (value);
			update_display (cloud);
		}
		break;
	case PROP_WEIGHT_COLUMN:
		if (cloud->priv->weight_column !=  g_value_get_int (value)) {
			cloud->priv->weight_column = g_value_get_int (value);
			update_display (cloud);
		}
		break;
	case PROP_MIN_SCALE:
		if (cloud->priv->min_scale !=  g_value_get_double (value)) {
			cloud->priv->min_scale = g_value_get_double (value);
			update_display (cloud);
		}
		break;
	case PROP_MAX_SCALE:
		if (cloud->priv->max_scale !=  g_value_get_double (value)) {
			cloud->priv->max_scale = g_value_get_double (value);
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
	
	switch (param_id) {
	case PROP_MODEL:
		g_value_set_object (value, cloud->priv->model);
		break;
	case PROP_LABEL_COLUMN:
		g_value_set_int (value, cloud->priv->label_column);
		break;
	case PROP_WEIGHT_COLUMN:
		g_value_set_int (value, cloud->priv->weight_column);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}	
}

/**
 * gdaui_cloud_set_selection_mode
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
	if (mode == cloud->priv->selection_mode)
		return;
	switch (mode) {
	case GTK_SELECTION_NONE:
		if (cloud->priv->selected_tags) {
			/* remove any selection */
			GSList *list;
			for (list = cloud->priv->selected_tags; list; list = list->next) {
				g_object_unref ((GObject*) list->data);
				g_object_set ((GObject*) list->data,
					      "background-set", FALSE,
					      NULL);
			}

			g_slist_free (cloud->priv->selected_tags);
			cloud->priv->selected_tags = NULL;
			sync_iter_with_selection (cloud);
			g_signal_emit_by_name (cloud, "selection-changed");
		}
		break;
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_BROWSE:
		if (cloud->priv->selected_tags && cloud->priv->selected_tags->next) {
			/* cut down to 1 selected node */
			GSList *newsel;
			newsel = cloud->priv->selected_tags;
			cloud->priv->selected_tags = g_slist_remove_link (cloud->priv->selected_tags,
									  cloud->priv->selected_tags);
			GSList *list;
			for (list = cloud->priv->selected_tags; list; list = list->next) {
				g_object_unref ((GObject*) list->data);
				g_object_set ((GObject*) list->data,
					      "background-set", FALSE,
					      NULL);
			}
			g_slist_free (cloud->priv->selected_tags);
			cloud->priv->selected_tags = newsel;
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
	cloud->priv->selection_mode = mode;
}

static void
row_clicked (GdauiCloud *cloud, gint row, GtkTextTag *tag)
{
	if (cloud->priv->selection_mode == GTK_SELECTION_NONE) {
		/* emit "activate" signal */
		g_signal_emit (cloud, objects_cloud_signals [ACTIVATE], 0, row);
		return;
	}

	/* toggle @rows's selection */
	if (g_slist_find (cloud->priv->selected_tags, tag)) {
		cloud->priv->selected_tags = g_slist_remove (cloud->priv->selected_tags, tag);
		g_object_set ((GObject*) tag,
			      "background-set", FALSE,
			      NULL);
		g_object_unref ((GObject*) tag);
	}
	else {
		cloud->priv->selected_tags = g_slist_prepend (cloud->priv->selected_tags, tag);
		g_object_ref ((GObject*) tag);
		g_object_set ((GObject*) tag,
			      "background", "yellow",
			      "background-set", TRUE,
			      NULL);

		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_mark (cloud->priv->tbuffer, &iter,
                                                  gtk_text_buffer_get_insert (cloud->priv->tbuffer));
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
		gtk_text_buffer_place_cursor (cloud->priv->tbuffer, &iter);
	}

	if ((cloud->priv->selection_mode == GTK_SELECTION_SINGLE) ||
	    (cloud->priv->selection_mode == GTK_SELECTION_BROWSE)) {
		/* no more than 1 element can be selected */
		if (cloud->priv->selected_tags && cloud->priv->selected_tags->next) {
			GtkTextTag *tag2;
			tag2 = GTK_TEXT_TAG (cloud->priv->selected_tags->next->data);
			cloud->priv->selected_tags = g_slist_remove (cloud->priv->selected_tags, tag2);
			g_object_set ((GObject*) tag2,
				      "background-set", FALSE,
				      NULL);
			g_object_unref ((GObject*) tag2);
		}
	}
	if (cloud->priv->selection_mode == GTK_SELECTION_BROWSE) {
		/* one element is always selected */
		if (! cloud->priv->selected_tags) {
			cloud->priv->selected_tags = g_slist_prepend (cloud->priv->selected_tags, tag);
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
	
	if (hovering != cloud->priv->hovering_over_link) {
		cloud->priv->hovering_over_link = hovering;
		
		if (cloud->priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new (GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new (GDK_XTERM);
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
visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, GdauiCloud *cloud)
{
	gint wx, wy, bx, by;

	gdk_window_get_pointer (gtk_widget_get_window (text_view), &wx, &wy, NULL);
	
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
	
	gdk_window_get_pointer (gtk_widget_get_window (text_view), NULL, NULL, NULL);
	return FALSE;
}

/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (GtkWidget *text_view, GtkTextIter *iter, GdauiCloud *cloud)
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

        switch (event->keyval) {
        case GDK_Return:
        case GDK_space:
        case GDK_KP_Enter:
                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
                gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                                  gtk_text_buffer_get_insert (buffer));
                follow_if_link (text_view, &iter, cloud);
		return TRUE;
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
		if ((cloud->priv->selection_mode == GTK_SELECTION_SINGLE) ||
		    (cloud->priv->selection_mode == GTK_SELECTION_BROWSE)) {
			GtkTextIter iter;
			if (cloud->priv->selected_tags) {
				GtkTextMark *mark;
				mark = gtk_text_buffer_get_insert (cloud->priv->tbuffer);
				gtk_text_buffer_get_iter_at_mark (cloud->priv->tbuffer, &iter, mark);
			}
			else if ((event->keyval == GDK_Right) || (event->keyval == GDK_Down))
				gtk_text_buffer_get_start_iter (cloud->priv->tbuffer, &iter);
			else
				gtk_text_buffer_get_end_iter (cloud->priv->tbuffer, &iter);
			
			gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (cloud->priv->tview), TRUE);
			while (1) { /* loop to move the cursor enough positions to change the selected item */
				gboolean done = FALSE;
				GtkMovementStep mvt_type;
				gint mvt_amount;
				switch (event->keyval) {
				case GDK_Up:
					done = ! gtk_text_view_backward_display_line ((GtkTextView*)cloud->priv->tview, &iter);
					mvt_type = GTK_MOVEMENT_DISPLAY_LINES;
					mvt_amount = -1;
					break;
				case GDK_Down:
					done = ! gtk_text_view_forward_display_line ((GtkTextView*)cloud->priv->tview, &iter);
					mvt_type = GTK_MOVEMENT_DISPLAY_LINES;
					mvt_amount = 1;
					break;
				case GDK_Left:
					done = ! gtk_text_iter_backward_char (&iter);
					mvt_type = GTK_MOVEMENT_VISUAL_POSITIONS;
					mvt_amount = -1;
					break;
				case GDK_Right:
					done = ! gtk_text_iter_forward_char (&iter);
					mvt_type = GTK_MOVEMENT_VISUAL_POSITIONS;
					mvt_amount = 1;
					break;
				}
				if (done)
					break; /* end of treatment as no movement possible */
				g_signal_emit_by_name (cloud->priv->tview, "move-cursor",
						       mvt_type, mvt_amount, FALSE);

				GtkTextMark *mark;
				mark = gtk_text_buffer_get_insert (cloud->priv->tbuffer);
				gtk_text_buffer_get_iter_at_mark (cloud->priv->tbuffer, &iter, mark);

				GSList *tags, *tagp;
				done = FALSE;
				tags = gtk_text_iter_get_tags (&iter);
				for (tagp = tags;  tagp;  tagp = tagp->next) {
					GtkTextTag *tag = (GtkTextTag*) tagp->data;
					gint row;
					row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
					if (row >= 0) {
						if ((cloud->priv->selected_tags &&
						     (tag != cloud->priv->selected_tags->data)) ||
						    !cloud->priv->selected_tags) {
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
					    
					mark = gtk_text_buffer_get_insert (cloud->priv->tbuffer);
					gtk_text_view_scroll_mark_onscreen ((GtkTextView*)cloud->priv->tview, mark);
					break;
				}
			}
			gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (cloud->priv->tview), FALSE);
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

	if (! fdata->cloud->priv->model)
		return;
	if (fdata->cloud->priv->label_column < 0)
		return;

	/* check for the data model's column type */
	GdaColumn *column;
	column = gda_data_model_describe_column (fdata->cloud->priv->model,
						 fdata->cloud->priv->label_column);
	if (!column || (gda_column_get_g_type (column) != G_TYPE_STRING)) {
		g_warning (_("Wrong column type for label: expecting a string and got a %s"),
			   gda_g_type_to_string (gda_column_get_g_type (column)));
		return;
	}

	row = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "row")) - 1;
	if (row < 0)
		return;

	cvalue = gda_data_model_get_value_at (fdata->cloud->priv->model,
					      fdata->cloud->priv->label_column, row, NULL);
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
 * gdaui_cloud_filter
 * @cloud: a #GdauiCloud widget
 * @filter: the filter to use, or %NULL to remove any filter
 *
 * Filters the elements displayed in @cloud, by altering their color.
 *
 * Since: 4.2
 */
void
gdaui_cloud_filter (GdauiCloud *cloud, const gchar *filter)
{
	g_return_if_fail (GDAUI_IS_CLOUD (cloud));

	GtkTextTagTable *tags_table = gtk_text_buffer_get_tag_table (cloud->priv->tbuffer);

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
 * gdaui_cloud_create_filter_widget
 * @cloud: a #GdauiCloud widget
 *
 * Creates a search widget linked directly to modify @cloud's appearance.
 *
 * Returns: a new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_cloud_create_filter_widget (GdauiCloud *cloud)
{
	GtkWidget *hbox, *label, *wid;
	g_return_val_if_fail (GDAUI_IS_CLOUD (cloud), NULL);
	
	hbox = gtk_hbox_new (FALSE, 0);

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
 * gdaui_cloud_set_weight_func
 * @cloud: a #GdauiCloud widget
 * @func: a #GdauiCloudWeightFunc function which computes weights, or %NULL to unset
 * @data: a pointer to pass as last argument of @func each time it is called
 *
 * Specifies a function called by @cloud to compute each row's respective weight.
 *
 * Since: 4.2
 */
void
gdaui_cloud_set_weight_func (GdauiCloud *cloud, GdauiCloudWeightFunc func, gpointer data)
{
	g_return_if_fail (GDAUI_IS_CLOUD (cloud));
	if ((cloud->priv->weight_func != func) || (cloud->priv->weight_func_data != data)) {
		cloud->priv->weight_func = func;
		cloud->priv->weight_func_data = data;
		update_display (cloud);
	}
}

/* GdauiDataSelector interface */
static GdaDataModel *
cloud_selector_get_model (GdauiDataSelector *iface)
{
	GdauiCloud *cloud;
	cloud = GDAUI_CLOUD (iface);
	return cloud->priv->model;
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
	for (list = cloud->priv->selected_tags; list; list = list->next) {
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
	if (! cloud->priv->iter && cloud->priv->model) {
		cloud->priv->iter = gda_data_model_create_iter (cloud->priv->model);
		sync_iter_with_selection (cloud);
	}

	return cloud->priv->iter;
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
	if (cloud->priv->selection_mode == GTK_SELECTION_NONE)
		return FALSE;

	/* test if row already selected */
	for (list = cloud->priv->selected_tags; list; list = list->next) {
		gint srow;
		srow = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "row")) - 1;
		if (srow == row)
			return TRUE;
	}

	/* try to select row */
	RowLookup rl;
	rl.row_to_find = row;
	rl.tag = NULL;
	gtk_text_tag_table_foreach (gtk_text_buffer_get_tag_table (cloud->priv->tbuffer),
				    (GtkTextTagTableForeach) text_tag_table_foreach_cb2,
				    (gpointer) &rl);
	if (rl.tag) {
		row_clicked (cloud, row, rl.tag);
		for (list = cloud->priv->selected_tags; list; list = list->next) {
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
	if (cloud->priv->selection_mode == GTK_SELECTION_NONE)
		return;

	/* test if row already selected */
	for (list = cloud->priv->selected_tags; list; list = list->next) {
		gint srow;
		GtkTextTag *tag = (GtkTextTag*) list->data;
		srow = GPOINTER_TO_INT (g_object_get_data ((GObject*) tag, "row")) - 1;
		if (srow == row) {
			cloud->priv->selected_tags = g_slist_remove (cloud->priv->selected_tags, tag);
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
cloud_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	/* nothing to do */
}
