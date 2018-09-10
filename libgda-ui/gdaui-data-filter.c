/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2012 Daniel Mustieles <daniel.mustieles@gmail.com>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-data-proxy.h"
#include "gdaui-data-filter.h"

static void gdaui_data_filter_dispose (GObject *object);

static void gdaui_data_filter_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gdaui_data_filter_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);


/* callbacks */
static void proxy_filter_changed_cb (GdaDataProxy *proxy, GdauiDataFilter *filter);
static void release_proxy (GdauiDataFilter *filter);
static void data_widget_destroyed_cb (GdauiDataProxy *wid, GdauiDataFilter *filter);
static void data_widget_proxy_changed_cb (GdauiDataProxy *data_widget, 
					  GdaDataProxy *proxy, GdauiDataFilter *filter);

static void clear_filter_cb (GtkButton *button, GdauiDataFilter *filter);
static void apply_filter_cb (GtkButton *button, GdauiDataFilter *filter);

typedef struct
{
	GdauiDataProxy    *data_widget;
	GdaDataProxy      *proxy;

	GtkWidget         *filter_entry;
	GtkWidget         *notice;
} GdauiDataFilterPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataFilter, gdaui_data_filter, GTK_TYPE_BOX)


/* properties */
enum {
	PROP_0,
	PROP_DATA_WIDGET
};

static void
gdaui_data_filter_class_init (GdauiDataFilterClass * class)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gdaui_data_filter_dispose;

  /* Properties */
  object_class->set_property = gdaui_data_filter_set_property;
  object_class->get_property = gdaui_data_filter_get_property;
	g_object_class_install_property (object_class, PROP_DATA_WIDGET,
                                   g_param_spec_object ("data-widget", NULL, NULL, GDAUI_TYPE_DATA_PROXY,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
set_wait_cursor (GtkWidget *w)
{
	GtkWidget *parent;
	
	parent = gtk_widget_get_toplevel (w);
	if (parent) {
    gdk_cursor_new_for_display (gdk_display_get_default (), GDK_WATCH);
	}
}

static void
unset_wait_cursor (GtkWidget *w)
{
	GtkWidget *parent;

	parent = gtk_widget_get_toplevel (w);
	if (parent)
		gdk_window_set_cursor (gtk_widget_get_window (parent), NULL);
}

static void
apply_filter_cb (G_GNUC_UNUSED GtkButton *button, GdauiDataFilter *filter)
{
	g_return_if_fail (GDAUI_IS_DATA_FILTER (filter));
	const gchar *expr;
	gchar *err = NULL;
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);

	expr = gtk_entry_get_text (GTK_ENTRY (priv->filter_entry));
	if (expr && !*expr)
		expr = NULL;

	gtk_widget_hide (priv->notice);
	if (priv->proxy) {
		GError *error = NULL;

		g_signal_handlers_block_by_func (G_OBJECT (priv->proxy),
						 G_CALLBACK (proxy_filter_changed_cb), filter);
		set_wait_cursor ((GtkWidget*) filter);
		while (g_main_context_pending (NULL))
			g_main_context_iteration (NULL, FALSE);

		if (!gda_data_proxy_set_filter_expr (priv->proxy, expr, &error)) {
			if (error && error->message)
				err = g_strdup (error->message);
			else
				err = g_strdup (_("No detail"));
			if (error)
				g_error_free (error);
		}

		unset_wait_cursor ((GtkWidget*) filter);
		g_signal_handlers_unblock_by_func (G_OBJECT (priv->proxy),
						   G_CALLBACK (proxy_filter_changed_cb), filter);
	}

	if (err) {
		gchar *esc, *markup;

		esc = g_markup_escape_text (err, -1);
		markup = g_strdup_printf ("<small><span foreground=\"#FF0000\"><b>%s</b>: %s</span></small>", 
					  _("Filter failed:"), esc);
		g_free (esc);
		gtk_label_set_line_wrap (GTK_LABEL (priv->notice), TRUE);
		gtk_label_set_line_wrap_mode (GTK_LABEL (priv->notice), PANGO_WRAP_WORD);
		gtk_label_set_selectable (GTK_LABEL (priv->notice), TRUE);
		gtk_label_set_markup (GTK_LABEL (priv->notice), markup);
		g_free (markup);
		gtk_widget_show (priv->notice);
	}
}

static void
clear_filter_cb (GtkButton *button, GdauiDataFilter *filter)
{
	g_return_if_fail (GDAUI_IS_DATA_FILTER (filter));
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);
	gtk_entry_set_text (GTK_ENTRY (priv->filter_entry), "");
	apply_filter_cb (button, filter);
}

static void
gdaui_data_filter_init (GdauiDataFilter * wid)
{
	GtkWidget *grid, *label, *entry, *button, *bbox;
	gchar *str;

	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (wid);
	priv->data_widget = NULL;
	priv->proxy = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (wid), GTK_ORIENTATION_VERTICAL);

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (wid), grid, TRUE, TRUE, 0);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>\n(<small>%s</small>):", _("Filter"), _("any valid SQL expression"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);

	gtk_widget_set_tooltip_markup (label, _("Columns can be referenced by their name or more easily "
						"using <b><tt>_&lt;column number&gt;</tt></b>. For example a valid "
						"expression can be: <b><tt>_2 like 'doe%'</tt></b> "
						"to filter rows where the 2nd column starts with <tt>doe</tt>."));

	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	entry = gtk_entry_new ();
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (apply_filter_cb), wid);

	label = gtk_label_new ("");
	priv->notice = label;
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 2, 1);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach (GTK_GRID (grid), bbox, 0, 2, 2, 1);
	button = gtk_button_new_with_label (_("Set filter"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (apply_filter_cb), wid);

	button = gtk_button_new_with_label (_("Clear filter"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (clear_filter_cb), wid);

	gtk_widget_show_all (grid);
	gtk_widget_hide (priv->notice);

	priv->filter_entry = entry;
}

/**
 * gdaui_data_filter_new:
 * @data_widget: a widget implementing the #GdauiDataProxy interface
 *
 * Creates a new #GdauiDataFilter widget suitable to change the filter expression
 * for @data_widget's displayed rows
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_data_filter_new (GdauiDataProxy *data_widget)
{
	GtkWidget *filter;

	g_return_val_if_fail (!data_widget || GDAUI_IS_DATA_PROXY (data_widget), NULL);

	filter = (GtkWidget *) g_object_new (GDAUI_TYPE_DATA_FILTER, 
					     "data-widget", data_widget, NULL);

	return filter;
}

static void
data_widget_destroyed_cb (GdauiDataProxy *wid, GdauiDataFilter *filter)
{
	g_return_if_fail(GDAUI_DATA_PROXY (wid));
	g_return_if_fail(GDAUI_IS_DATA_FILTER (filter));
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);
	g_assert (wid == priv->data_widget);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_widget_destroyed_cb), filter);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_widget_proxy_changed_cb), filter);

	priv->data_widget = NULL;
}

static void
proxy_filter_changed_cb (GdaDataProxy *proxy, GdauiDataFilter *filter)
{
	g_return_if_fail (GDAUI_IS_DATA_FILTER (filter));
	g_return_if_fail (GDAUI_IS_DATA_PROXY (proxy));
	const gchar *expr;
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);

	g_assert (proxy == priv->proxy);
	expr = gda_data_proxy_get_filter_expr (proxy);
	gtk_entry_set_text (GTK_ENTRY (priv->filter_entry), expr ? expr : "");
}

static void
release_proxy (GdauiDataFilter *filter)
{
	g_return_if_fail (GDAUI_IS_DATA_FILTER (filter));
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);
	g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
					      G_CALLBACK (proxy_filter_changed_cb), filter);
	g_object_unref (priv->proxy);
	priv->proxy = NULL;
}

static void
data_widget_proxy_changed_cb (GdauiDataProxy *data_widget, G_GNUC_UNUSED GdaDataProxy *proxy, GdauiDataFilter *filter)
{
	g_object_set (G_OBJECT (filter), "data-widget", data_widget, NULL);
}

static void
gdaui_data_filter_dispose (GObject *object)
{
	GdauiDataFilter *filter;

	filter = GDAUI_DATA_FILTER (object);
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);

	if (priv->proxy)
		release_proxy (filter);
	if (priv->data_widget)
		data_widget_destroyed_cb (priv->data_widget, filter);

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_data_filter_parent_class)->dispose (object);
}

static void
gdaui_data_filter_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiDataFilter *filter;

	filter = GDAUI_DATA_FILTER (object);
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);
	switch (param_id) {
	case PROP_DATA_WIDGET:
		if (priv->data_widget)
			data_widget_destroyed_cb (priv->data_widget, filter);
		if (priv->proxy)
			release_proxy (filter);

		priv->data_widget = GDAUI_DATA_PROXY (g_value_get_object (value));
		if (priv->data_widget) {
			GdaDataProxy *proxy;

			/* data widget */
			g_signal_connect (priv->data_widget, "destroy",
					  G_CALLBACK (data_widget_destroyed_cb), filter);
			g_signal_connect (priv->data_widget, "proxy-changed",
					  G_CALLBACK (data_widget_proxy_changed_cb), filter);

			/* proxy */
			proxy = gdaui_data_proxy_get_proxy (priv->data_widget);
			if (proxy) {
				priv->proxy = proxy;
				g_object_ref (priv->proxy);
				g_signal_connect (G_OBJECT (proxy), "filter_changed",
						  G_CALLBACK (proxy_filter_changed_cb), filter);
				proxy_filter_changed_cb (proxy, filter);
			}
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_data_filter_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdauiDataFilter *filter;

	filter = GDAUI_DATA_FILTER (object);
	GdauiDataFilterPrivate *priv = gdaui_data_filter_get_instance_private (filter);
	switch (param_id) {
	case PROP_DATA_WIDGET:
		g_value_set_pointer (value, priv->data_widget);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
