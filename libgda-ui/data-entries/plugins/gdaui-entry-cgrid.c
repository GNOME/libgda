/* gdaui-entry-cgrid.c
 *
 * Copyright (C) 2007 - 2007 Carlos Savoretti
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

#include <gdk/gdkkeysyms.h>

#include <gtk/gtkhbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>

#include <libgda/gda-data-model.h>
#include <libgda-ui/gdaui-raw-grid.h>
#include <libgda-ui/gdaui-data-widget.h>

#include "gdaui-entry-cgrid.h"

struct _GdauiEntryCGridPrivate {
	gint                   text_column;            /* text column. */
	gint                   grid_height;            /* grid height. */
	gboolean               headers_visible;

	GtkTreeModel          *model;
	GtkTreeSelection      *selection;
	/* Widgets contained within the GdauiEntryWrapper. */
	GtkWidget             *hbox;

	GtkWidget             *entry;
	GtkWidget             *toggle_button;
	GtkWidget             *window_popup;
	GtkWidget             *scrolled_window;
	GtkWidget             *tree_view;
};

enum {
	PROP_0,
	PROP_TEXT_COLUMN,
	PROP_GRID_HEIGHT,
	PROP_HEADERS_VISIBLE
};

enum {
	SIGNAL_CGRID_CHANGED,
	SIGNAL_LAST
};

static guint cgrid_signals[SIGNAL_LAST];

G_DEFINE_TYPE (GdauiEntryCGrid, gdaui_entry_cgrid, GDAUI_TYPE_ENTRY_WRAPPER)

static GdauiEntryWrapperClass *parent_class;


static guint
get_header_height (GdauiEntryCGrid  *cgrid)
{
	guint header_height = 0;

	GList *columns = gtk_tree_view_get_columns (GTK_TREE_VIEW(cgrid->priv->tree_view));

	GList *current = columns;
	while (current) {
		GtkTreeViewColumn *column = (GtkTreeViewColumn *) current->data;
		const gchar *title = gtk_tree_view_column_get_title (column);

		gchar **strs;
		strs = g_strsplit (title, "__", 0);

		gchar *str;
		str = g_strjoinv ("_", strs);

		g_strfreev (strs);

		// GtkWidget *label = (GtkWidget *) gtk_label_new (title);
		GtkWidget *label = (GtkWidget *) gtk_label_new (str);

		g_free (str);

		gtk_tree_view_column_set_widget (column, label);

		GtkRequisition requisition;
		gtk_widget_size_request (label, &requisition);

		if (requisition.height > header_height)
			header_height = requisition.height;

		gtk_widget_show (label);

		current = g_list_next (current);
	}
	g_list_free (columns);

	header_height += 18;  /* ? */

	return header_height;
}

static guint
get_row_height (GdauiEntryCGrid  *cgrid)
{
	guint row_height = 0;

	GList *columns = gtk_tree_view_get_columns (GTK_TREE_VIEW(cgrid->priv->tree_view));

	GList *current = columns;
	while (current) {
		GList *renderers = gtk_tree_view_column_get_cell_renderers
			((GtkTreeViewColumn *) current->data);

		guint cell_height = 0;

		GList *current1 = renderers;
		while (current1) {
			GtkCellRenderer *renderer = (GtkCellRenderer *) current1->data;
			guint height;

			gtk_cell_renderer_get_size (renderer, cgrid->priv->tree_view,
						    NULL, NULL, NULL, NULL, &height);

			if (height > cell_height)
				cell_height = height;

			current1 = g_list_next (current1);
		}

		g_list_free (renderers);

		if (cell_height > row_height)
			row_height = cell_height;

		current = g_list_next (current);
	}
	g_list_free (columns);

	row_height += 4;  /* horizontal-separator ? */

	return row_height;
}

/**
 * gdaui_entry_cgrid_new:
 *
 * Creates a new #GdauiEntryCGrid.
 *
 * Returns: the newly created #GdauiEntryCGrid.
 */
GdauiEntryCGrid *
gdaui_entry_cgrid_new (GdaDataHandler  *data_handler,
			  GType            gtype,
			  const gchar     *options)
{
	g_return_val_if_fail (GDA_IS_DATA_HANDLER (data_handler), NULL);
	g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (data_handler, gtype), NULL);

	GdauiEntryCGrid *cgrid = (GdauiEntryCGrid *) g_object_new (GDAUI_TYPE_ENTRY_CGRID,
								       "handler", data_handler,
								       NULL);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (cgrid), gtype);

	return cgrid;
}

static void
gdaui_entry_cgrid_changed (GdauiEntryCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	g_signal_emit (cgrid, cgrid_signals [SIGNAL_CGRID_CHANGED], 0);
}

static gboolean
popup_grab_on_window (GdkWindow  *window,
		      guint32     activate_time)
{

	if (gdk_pointer_grab (window, TRUE,
			      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
			      NULL, NULL,
			      activate_time) == 0) {

		if (gdk_keyboard_grab (window, TRUE, activate_time) == 0) {
			return TRUE;
		} else {
			gdk_pointer_ungrab (activate_time);
			return FALSE;
		}
	}

	return FALSE;
}

static void
toggle_button_on_toggled (GtkToggleButton  *toggle_button,
			  gpointer          data)
{
	g_return_if_fail (GTK_TOGGLE_BUTTON(toggle_button));

	if (gtk_toggle_button_get_active (toggle_button) == TRUE) {

		GdauiEntryCGrid  *cgrid = (GdauiEntryCGrid  *) data;

		GtkWidget *window_popup = GDAUI_ENTRY_CGRID(cgrid)->priv->window_popup;
		GtkRequisition requisition;

		/* show dropdown */
		gtk_widget_size_request (window_popup, &requisition);

		gint x, y, width, height;

		gdk_window_get_origin (GDK_WINDOW
				       (GTK_WIDGET(cgrid)->window),
				       &x, &y);

		x += GTK_WIDGET(cgrid)->allocation.x;
/* 		y += cgrid->priv->entry->allocation.y; */
		width = GTK_WIDGET(cgrid)->allocation.width;
		height = GTK_WIDGET(cgrid)->allocation.height;

/* 		x += width - requisition.width; */
		y += height;
		if (x < 0) x = 0;
		if (y < 0) y = 0;

		gtk_grab_add (window_popup);
		gtk_window_move (GTK_WINDOW(window_popup), x, y);
		gtk_widget_set_size_request (window_popup,
					     GTK_WIDGET(cgrid)->allocation.width,
					     ((y + cgrid->priv->grid_height) > gdk_screen_height ()) ? gdk_screen_height () - y : cgrid->priv->grid_height);
		gtk_widget_show (window_popup);
		gtk_widget_grab_focus (cgrid->priv->tree_view);

		popup_grab_on_window (window_popup->window,
				      gtk_get_current_event_time ());
	}

}

static void
set_text_from_grid (GdauiEntryCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_ENTRY_CGRID(cgrid));

	GList *selected_rows = gtk_tree_selection_get_selected_rows (cgrid->priv->selection, &cgrid->priv->model);

	GList *current = selected_rows;
	while (current) {
		GtkTreePath *path = (GtkTreePath *) (current->data);
		GtkTreeIter iter;
		gtk_tree_model_get_iter (cgrid->priv->model, &iter, path);

		GValue *gvalue;

		gtk_tree_model_get (GTK_TREE_MODEL(cgrid->priv->model), &iter,
				    cgrid->priv->text_column, &gvalue,
				    -1);

		gtk_entry_set_text (GTK_ENTRY
				    (GDAUI_ENTRY_CGRID(cgrid)->priv->entry),
				    gda_value_stringify (gvalue));

		current = g_list_next (current);
	}

	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

}

static void
hide_window_popup (GtkWidget  *cgrid)
{
	g_return_if_fail (GDAUI_ENTRY_CGRID(cgrid));

	gtk_widget_hide (GDAUI_ENTRY_CGRID(cgrid)->priv->window_popup);
	gtk_grab_remove (GDAUI_ENTRY_CGRID(cgrid)->priv->window_popup);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (GDAUI_ENTRY_CGRID(cgrid)->priv->toggle_button), FALSE);
	gtk_widget_grab_focus (GDAUI_ENTRY_CGRID(cgrid)->priv->entry);
}

static gint
window_popup_on_delete_event (GtkToggleButton  *window_popup,
			      gpointer          data)
{
	g_return_val_if_fail (GTK_WINDOW(window_popup), TRUE);

	hide_window_popup ((GtkWidget *) data);
	gtk_widget_grab_focus (GDAUI_ENTRY_CGRID(data)->priv->entry);

	return TRUE;
}

static gint
window_popup_on_key_press_event (GtkToggleButton  *window_popup,
				 GdkEventKey      *event,
				 gpointer          data)
{
	g_return_val_if_fail (GTK_WINDOW(window_popup), TRUE);

	switch (event->keyval) {
	case GDK_Escape:
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		set_text_from_grid ((GdauiEntryCGrid *) data);
		break;
	default:
		return FALSE;
	}

	g_signal_stop_emission_by_name (G_OBJECT(window_popup), "key_press_event");

	hide_window_popup ((GtkWidget *) data);
	gtk_widget_grab_focus (GDAUI_ENTRY_CGRID(data)->priv->entry);

	return TRUE;
}

static gint
window_popup_on_button_press_event (GtkWidget       *window_popup,
				    GdkEventButton  *event,
				    gpointer         data)
{
	g_return_val_if_fail (GTK_WINDOW(window_popup), TRUE);

	GtkWidget *event_widget = gtk_get_event_widget ((GdkEvent *) event);

	/* We don't ask for button press events on the grab widget, so
	 * if an event is reported directly to the grab widget, it must
	 * be on a window outside the application (and thus we remove
	 * the popup window).  Otherwise, we check if the widget is a child
	 * of the grab widget, and only remove the popup window if it is not.
	 */
	if (event_widget != window_popup) {

		while (event_widget) {
			if (event_widget == window_popup)
				return FALSE;
			event_widget = event_widget->parent;
		}

	}

	hide_window_popup ((GtkWidget *) data);
	gtk_widget_grab_focus (GDAUI_ENTRY_CGRID(data)->priv->entry);

	return TRUE;
}

static void
tree_view_on_row_activated (GtkTreeView        *tree_view,
			    GtkTreePath        *tree_path,
			    GtkTreeViewColumn  *view_column,
			    gpointer            data)
{
	g_return_if_fail (GTK_TREE_VIEW(tree_view));

	set_text_from_grid ((GdauiEntryCGrid *) data);

	hide_window_popup ((GtkWidget *) data);
	gtk_widget_grab_focus (GDAUI_ENTRY_CGRID(data)->priv->entry);
}

static void
gdaui_entry_cgrid_init (GdauiEntryCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	cgrid->priv = g_new0 (GdauiEntryCGridPrivate, 1);

	cgrid->priv->hbox = gtk_hbox_new (FALSE, 0);
	cgrid->priv->text_column = 0;
	cgrid->priv->grid_height = 0;
	cgrid->priv->headers_visible = FALSE;

	cgrid->priv->model = NULL;
}

static void
gdaui_entry_cgrid_finalize (GdauiEntryCGrid  *cgrid)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	GObjectClass *object_class = G_OBJECT_CLASS(parent_class);

	if (cgrid->priv) {
		cgrid->priv->text_column = 0;
		cgrid->priv->grid_height = 0;
		cgrid->priv->headers_visible = FALSE;
		g_free (cgrid->priv);
		cgrid->priv = NULL;
	}

	if (object_class->finalize)
		object_class->finalize (G_OBJECT(cgrid));
}


/**
 * gdaui_entry_cgrid_get_text_column
 * @cgrid: a #GdauiEntryCGrid.
 *
 * Get the text column for this cgrid.
 */
gint
gdaui_entry_cgrid_get_text_column (GdauiEntryCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid), 0);

	return cgrid->priv->text_column;
}

/**
 * gdaui_entry_cgrid_set_text_column:
 * @cgrid: a #GdauiEntryCGrid.
 * @text_column: the cgrid text column.
 *
 * Set the text column for this cgrid.
 */
void
gdaui_entry_cgrid_set_text_column (GdauiEntryCGrid  *cgrid,
				      gint          text_column)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	cgrid->priv->text_column = text_column;

	g_object_notify (G_OBJECT(cgrid), "text-column");
}

/**
 * gdaui_entry_cgrid_get_grid_height
 * @cgrid: a #GdauiEntryCGrid.
 *
 * Get the grid height for this cgrid.
 */
gint
gdaui_entry_cgrid_get_grid_height (GdauiEntryCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid), 0);

	return cgrid->priv->grid_height;
}

/**
 * gdaui_entry_cgrid_set_grid_height:
 * @cgrid: a #GdauiEntryCGrid.
 * @grid_height: the cgrid height.
 *
 * Set the grid height for this cgrid.
 */
void
gdaui_entry_cgrid_set_grid_height (GdauiEntryCGrid  *cgrid,
				      gint          grid_height)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	cgrid->priv->grid_height = grid_height;

	g_object_notify (G_OBJECT(cgrid), "grid-height");
}

/**
 * gdaui_entry_cgrid_get_headers_visible
 * @cgrid: a #GdauiEntryCGrid.
 *
 * TRUE if the cgrid has itself its headers visible.
 */
gboolean
gdaui_entry_cgrid_get_headers_visible (GdauiEntryCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid), FALSE);
	return cgrid->priv->headers_visible;
}

/**
 * gdaui_entry_cgrid_set_headers_visible:
 * @cgrid: a #GdauiEntryCGrid.
 * @headers_visible: the cgrid headers is visible.
 *
 * Set to TRUE if this cgrid has its headers visible.
 */
void
gdaui_entry_cgrid_set_headers_visible (GdauiEntryCGrid  *cgrid,
					  gboolean      headers_visible)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));
	cgrid->priv->headers_visible = headers_visible;
	g_object_notify (G_OBJECT(cgrid), "headers-visible");
}

/**
 * gdaui_entry_cgrid_get_model
 * @cgrid: a #GdauiEntryCGrid.
 *
 * Returns the data model the cgrid is based on.
 */
GdaDataModel *
gdaui_entry_cgrid_get_model (GdauiEntryCGrid  *cgrid)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid), FALSE);

	GdaDataModel *model;

	g_object_get (G_OBJECT(cgrid->priv->tree_view),
		      "model", &model,
		      NULL);
	/* don't keep reference on object */
	g_object_unref (model);

	return model;
}

/**
 * gdaui_entry_cgrid_set_model:
 * @cgrid: a #GdauiEntryCGrid.
 * @model: the cgrid data model.
 *
 * Sets the data model for this #GdauiEntryCGrid. If the @cgrid already has a data model set,
 * it will remove it before setting the new data model. If data model is NULL, then it will
 * unset the old data model.
 */
void
gdaui_entry_cgrid_set_model (GdauiEntryCGrid   *cgrid,
				GdaDataModel  *model)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	g_object_set (G_OBJECT(cgrid->priv->tree_view),
		      "model", model,
		      NULL);

	cgrid->priv->model = gtk_tree_view_get_model (GTK_TREE_VIEW (cgrid->priv->tree_view));

	GList *columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (cgrid->priv->tree_view));
	guint list_length = g_list_length (columns);
	g_list_free (columns);

	guint i;
	for (i = 0; i < list_length; ++i) 
		gdaui_data_widget_column_set_editable (GDAUI_DATA_WIDGET (cgrid->priv->tree_view),
							  i, FALSE);

	gint grid_height = gda_data_model_get_n_rows ((GdaDataModel  *) model) * get_row_height (cgrid)
		+ get_header_height (cgrid);

	gdaui_entry_cgrid_set_grid_height (cgrid, grid_height);

	gdaui_entry_cgrid_changed (cgrid);
}

/**
 * gdaui_entry_cgrid_append_column:
 * @cgrid: a #GdauiEntryCGrid.
 * @column: a #GtkTreeViewColumn object.
 *
 * Append column to this cgrid.
 */
void
gdaui_entry_cgrid_append_column (GdauiEntryCGrid        *cgrid,
				    GtkTreeViewColumn  *column)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cgrid->priv->tree_view), (GtkTreeViewColumn  *) column);
}

/**
 * gdaui_entry_cgrid_get_active_iter.
 * @cgrid: a #GdauiEntryCGrid.
 * @iter: the unitialized #GtkTreeIter.
 *
 * Return TRUE if iter was set.
 */
gboolean
gdaui_entry_cgrid_get_active_iter (GdauiEntryCGrid  *cgrid,
				      GtkTreeIter  *iter)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID (cgrid), FALSE);

	return gtk_tree_selection_get_selected (cgrid->priv->selection, NULL, (GtkTreeIter  *) iter);
}

static void
gdaui_entry_cgrid_set_property (GObject       *object,
				   guint          param_id,
				   const GValue  *value,
				   GParamSpec    *pspec)
{
	GdauiEntryCGrid *cgrid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID (object));

	cgrid = GDAUI_ENTRY_CGRID(object);

	switch (param_id) {
	case PROP_TEXT_COLUMN:
		gdaui_entry_cgrid_set_text_column (cgrid, g_value_get_int (value));
		break;
	case PROP_GRID_HEIGHT:
		gdaui_entry_cgrid_set_grid_height (cgrid, g_value_get_int (value));
		break;
	case PROP_HEADERS_VISIBLE:
		gdaui_entry_cgrid_set_headers_visible (cgrid, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

}

static void
gdaui_entry_cgrid_get_property (GObject     *object,
				   guint        param_id,
				   GValue      *value,
				   GParamSpec  *pspec)
{
	GdauiEntryCGrid *cgrid;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID(object));

	cgrid = GDAUI_ENTRY_CGRID(object);

	switch (param_id) {
	case PROP_TEXT_COLUMN:
		g_value_set_int (value, cgrid->priv->text_column);
		break;
	case PROP_GRID_HEIGHT:
		g_value_set_int (value, cgrid->priv->grid_height);
		break;
	case PROP_HEADERS_VISIBLE:
		g_value_set_boolean (value, cgrid->priv->headers_visible);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

}

static GtkWidget *
create_entry (GdauiEntryWrapper  *entry_wrapper)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper), NULL);

	GdauiEntryCGrid *cgrid = GDAUI_ENTRY_CGRID(entry_wrapper);

	cgrid->priv->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX(cgrid->priv->hbox), cgrid->priv->entry, TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET(cgrid->priv->entry));

	cgrid->priv->toggle_button = gtk_toggle_button_new ();
	gtk_box_pack_start (GTK_BOX(cgrid->priv->hbox), cgrid->priv->toggle_button, FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET(cgrid->priv->toggle_button));

	GtkWidget *arrow = GTK_WIDGET(gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE));
	gtk_container_add (GTK_CONTAINER(cgrid->priv->toggle_button), arrow);
	gtk_widget_show (arrow);

	cgrid->priv->window_popup = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_resizable (GTK_WINDOW(cgrid->priv->window_popup), FALSE);

	g_object_set (G_OBJECT(cgrid->priv->window_popup),
		      "border-width", 3,
		      NULL);

	gtk_widget_set_events (GTK_WIDGET(cgrid->priv->window_popup),
			       gtk_widget_get_events (GTK_WIDGET(cgrid->priv->window_popup)) | GDK_KEY_PRESS_MASK);

	cgrid->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(cgrid->priv->scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER(cgrid->priv->window_popup), cgrid->priv->scrolled_window);
	gtk_widget_show (GTK_WIDGET(cgrid->priv->scrolled_window));

	cgrid->priv->tree_view = gdaui_raw_grid_new (NULL);
	gtk_container_add (GTK_CONTAINER(cgrid->priv->scrolled_window), cgrid->priv->tree_view);

	cgrid->priv->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cgrid->priv->tree_view));
	gtk_tree_selection_set_mode (cgrid->priv->selection, GTK_SELECTION_SINGLE);

	gtk_widget_show (GTK_WIDGET(cgrid->priv->tree_view));

	return cgrid->priv->hbox;
}

static void
real_set_value (GdauiEntryWrapper  *entry_wrapper,
		const GValue         *gvalue)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper));

	GdauiEntryCGrid *cgrid = GDAUI_ENTRY_CGRID(entry_wrapper);

	GdaDataHandler *data_handler = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY(entry_wrapper));

	if (gvalue) {

		if (gda_value_is_null ((GValue *) gvalue)) {
			gtk_entry_set_text (GTK_ENTRY(cgrid->priv->entry), "");
		} else {
			gchar *str;
			str = gda_data_handler_get_str_from_value (data_handler, gvalue);
			if (str) {
				gtk_entry_set_text (GTK_ENTRY(cgrid->priv->entry), str);
				g_free (str);
			}
		}

	}

}

static GValue *
real_get_value (GdauiEntryWrapper  *entry_wrapper)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper), NULL);

	GdauiEntryCGrid *cgrid = GDAUI_ENTRY_CGRID(entry_wrapper);

	GdaDataHandler *data_handler = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY(entry_wrapper));
	GType gtype = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY(entry_wrapper));

	GValue *gvalue = NULL;

	const gchar *str;
	str = gtk_entry_get_text (GTK_ENTRY(cgrid->priv->entry));

	if (*str) 
		gvalue = gda_data_handler_get_value_from_str (data_handler, str, gtype);

	gvalue = (gvalue != NULL) ? gvalue : gda_value_new_null ();

	return gvalue;
}

static void
connect_signals (GdauiEntryWrapper  *entry_wrapper,
		 GCallback             modify_callback,
		 GCallback             activate_callback)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper));

	GdauiEntryCGrid *cgrid = GDAUI_ENTRY_CGRID(entry_wrapper);

	g_signal_connect (G_OBJECT(cgrid->priv->toggle_button), "toggled",
			  G_CALLBACK(toggle_button_on_toggled), (gpointer) cgrid);

	g_signal_connect (G_OBJECT(cgrid->priv->window_popup), "delete_event",
			  G_CALLBACK(window_popup_on_delete_event), (gpointer) cgrid);

	g_signal_connect (G_OBJECT(cgrid->priv->window_popup), "key_press_event",
			  G_CALLBACK(window_popup_on_key_press_event), (gpointer) cgrid);

	g_signal_connect (G_OBJECT(cgrid->priv->window_popup), "button_press_event",
			  G_CALLBACK(window_popup_on_button_press_event), (gpointer) cgrid);


	g_signal_connect (G_OBJECT(cgrid->priv->tree_view), "row-activated",
			  G_CALLBACK(tree_view_on_row_activated), (gpointer) cgrid);
}

static gboolean
expand_in_layout (GdauiEntryWrapper  *entry_wrapper)
{
	g_return_val_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper), FALSE);
	return FALSE;
}

static void
set_editable (GdauiEntryWrapper  *entry_wrapper,
	      gboolean        editable)
{
	g_return_if_fail (GDAUI_IS_ENTRY_CGRID(entry_wrapper));

	GdauiEntryCGrid *cgrid = GDAUI_ENTRY_CGRID(entry_wrapper);

	gtk_entry_set_editable (GTK_ENTRY(cgrid->priv->entry), editable);
}

static void
gdaui_entry_cgrid_class_init (GdauiEntryCGridClass  *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	GdauiEntryShellClass *shell_class = GDAUI_ENTRY_SHELL_CLASS(klass);
	GdauiEntryWrapperClass *wrapper_class = GDAUI_ENTRY_WRAPPER_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	wrapper_class->create_entry = create_entry;
	wrapper_class->real_set_value = real_set_value;
	wrapper_class->real_get_value = real_get_value;
	wrapper_class->connect_signals = connect_signals;
	wrapper_class->expand_in_layout = expand_in_layout;

	wrapper_class->set_editable = set_editable;

	/* Override the virtual finalize method in the GObject
	   class vtable (which is contained in GdauiEntryCGridClass). */
	gobject_class->finalize = (GObjectFinalizeFunc) gdaui_entry_cgrid_finalize;
	gobject_class->set_property = (GObjectSetPropertyFunc) gdaui_entry_cgrid_set_property;
	gobject_class->get_property = (GObjectGetPropertyFunc) gdaui_entry_cgrid_get_property;

	klass->cgrid_changed = NULL;

	g_object_class_install_property
		(gobject_class,
		 PROP_TEXT_COLUMN,
		 g_param_spec_int ("text-column", _("Cgrid text column"),
				   _("A column in the data source model to get the string from."),
				   0, G_MAXINT, 0,
				   (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_GRID_HEIGHT,
		 g_param_spec_int ("grid-height", _("Cgrid grid height"),
				   _("Cgrid height's."),
				   0, G_MAXINT, 100,
				   (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	g_object_class_install_property
		(gobject_class,
		 PROP_HEADERS_VISIBLE,
		 g_param_spec_boolean ("headers-visible", _("Cgrid has its headers visible"),
				      _("Cgrid headers visible"),
				      TRUE,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

	cgrid_signals[SIGNAL_CGRID_CHANGED] =
		g_signal_new ("cgrid-changed",
			      G_OBJECT_CLASS_TYPE(klass),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GdauiEntryCGridClass, cgrid_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}
