/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

static void gdaui_data_filter_class_init (GdauiDataFilterClass * class);
static void gdaui_data_filter_init (GdauiDataFilter *wid);
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

struct _GdauiDataFilterPriv
{
	GdauiDataProxy   *data_widget;
	GdaDataProxy      *proxy;

	GtkWidget         *filter_entry;
	GtkWidget         *notice;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
	PROP_0,
	PROP_DATA_WIDGET
};

GType
gdaui_data_filter_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo filter = {
			sizeof (GdauiDataFilterClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_data_filter_class_init,
			NULL,
			NULL,
			sizeof (GdauiDataFilter),
			0,
			(GInstanceInitFunc) gdaui_data_filter_init,
			0
		};		

		type = g_type_register_static (GTK_TYPE_BOX, "GdauiDataFilter", &filter, 0);
	}

	return type;
}

static void
gdaui_data_filter_class_init (GdauiDataFilterClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

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
		GdkCursor* cursor;
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gtk_widget_get_window (parent), cursor);
		gdk_cursor_unref (cursor);
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
	const gchar *expr;
	gchar *err = NULL;

	expr = gtk_entry_get_text (GTK_ENTRY (filter->priv->filter_entry));
	if (expr && !*expr)
		expr = NULL;

	gtk_widget_hide (filter->priv->notice);
	if (filter->priv->proxy) {
		GError *error = NULL;

		g_signal_handlers_block_by_func (G_OBJECT (filter->priv->proxy),
						 G_CALLBACK (proxy_filter_changed_cb), filter);
		set_wait_cursor ((GtkWidget*) filter);
		while (g_main_context_pending (NULL))
			g_main_context_iteration (NULL, FALSE);

		if (!gda_data_proxy_set_filter_expr (filter->priv->proxy, expr, &error)) {
			if (error && error->message)
				err = g_strdup (error->message);
			else
				err = g_strdup (_("No detail"));
			if (error)
				g_error_free (error);
		}

		unset_wait_cursor ((GtkWidget*) filter);
		g_signal_handlers_unblock_by_func (G_OBJECT (filter->priv->proxy),
						   G_CALLBACK (proxy_filter_changed_cb), filter);
	}

	if (err) {
		gchar *esc, *markup;

		esc = g_markup_escape_text (err, -1);
		markup = g_strdup_printf ("<small><span foreground=\"#FF0000\"><b>%s</b>: %s</span></small>", 
					  _("Filter failed:"), esc);
		g_free (esc);
		gtk_label_set_line_wrap (GTK_LABEL (filter->priv->notice), TRUE);
		gtk_label_set_line_wrap_mode (GTK_LABEL (filter->priv->notice), PANGO_WRAP_WORD);
		gtk_label_set_selectable (GTK_LABEL (filter->priv->notice), TRUE);
		gtk_label_set_markup (GTK_LABEL (filter->priv->notice), markup);
		g_free (markup);
		gtk_widget_show (filter->priv->notice);
	}
}

static void
clear_filter_cb (GtkButton *button, GdauiDataFilter *filter)
{
	gtk_entry_set_text (GTK_ENTRY (filter->priv->filter_entry), "");
	apply_filter_cb (button, filter);
}

static void
gdaui_data_filter_init (GdauiDataFilter * wid)
{
	GtkWidget *table, *label, *entry, *button, *bbox;
	gchar *str;

	wid->priv = g_new0 (GdauiDataFilterPriv, 1);
	wid->priv->data_widget = NULL;
	wid->priv->proxy = NULL;

	gtk_box_set_orientation (GTK_BOX (wid), GTK_ORIENTATION_VERTICAL);

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacing (GTK_TABLE (table), 1, 10);
	gtk_table_set_col_spacing (GTK_TABLE (table), 0, 5);
	gtk_box_pack_start (GTK_BOX (wid), table, TRUE, TRUE, 0);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>\n(<small>%s</small>):", _("Filter"), _("any valid SQL expression"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_SHRINK, 0, 0, 0);
	entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);
	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (apply_filter_cb), wid);

	label = gtk_label_new ("");
	wid->priv->notice = label;
	gtk_table_attach (GTK_TABLE (table), label, 0, 2, 1, 2, GTK_FILL, 0, GTK_SHRINK, 5);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_table_attach (GTK_TABLE (table), bbox, 0, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, GTK_SHRINK, 5);
	button = gtk_button_new_with_label (_("Set filter"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (apply_filter_cb), wid);

	button = gtk_button_new_with_label (_("Clear filter"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (clear_filter_cb), wid);

	gtk_widget_show_all (table);
	gtk_widget_hide (wid->priv->notice);

	wid->priv->filter_entry = entry;
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
	g_assert (wid == filter->priv->data_widget);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_widget_destroyed_cb), filter);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_widget_proxy_changed_cb), filter);

	filter->priv->data_widget = NULL;
}

static void
proxy_filter_changed_cb (GdaDataProxy *proxy, GdauiDataFilter *filter)
{
	const gchar *expr;

	g_assert (proxy == filter->priv->proxy);
	expr = gda_data_proxy_get_filter_expr (proxy);
	gtk_entry_set_text (GTK_ENTRY (filter->priv->filter_entry), expr ? expr : "");
}

static void
release_proxy (GdauiDataFilter *filter)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (filter->priv->proxy),
					      G_CALLBACK (proxy_filter_changed_cb), filter);
	g_object_unref (filter->priv->proxy);
	filter->priv->proxy = NULL;
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

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_FILTER (object));
	filter = GDAUI_DATA_FILTER (object);

	if (filter->priv) {
		if (filter->priv->proxy)
			release_proxy (filter);
		if (filter->priv->data_widget)
			data_widget_destroyed_cb (filter->priv->data_widget, filter);

		/* the private area itself */
		g_free (filter->priv);
		filter->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
gdaui_data_filter_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
	GdauiDataFilter *filter;

        filter = GDAUI_DATA_FILTER (object);
        if (filter->priv) {
                switch (param_id) {
                case PROP_DATA_WIDGET:
			if (filter->priv->data_widget)
				data_widget_destroyed_cb (filter->priv->data_widget, filter);
			if (filter->priv->proxy)
				release_proxy (filter);

			filter->priv->data_widget = GDAUI_DATA_PROXY (g_value_get_object (value));
			if (filter->priv->data_widget) {
				GdaDataProxy *proxy;

				/* data widget */
				g_signal_connect (filter->priv->data_widget, "destroy",
						  G_CALLBACK (data_widget_destroyed_cb), filter);
				g_signal_connect (filter->priv->data_widget, "proxy-changed",
						  G_CALLBACK (data_widget_proxy_changed_cb), filter);

				/* proxy */
				proxy = gdaui_data_proxy_get_proxy (filter->priv->data_widget);
				if (proxy) {
					filter->priv->proxy = proxy;
					g_object_ref (filter->priv->proxy);
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
}

static void
gdaui_data_filter_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
	GdauiDataFilter *filter;

        filter = GDAUI_DATA_FILTER (object);
        if (filter->priv) {
                switch (param_id) {
		case PROP_DATA_WIDGET:
			g_value_set_pointer (value, filter->priv->data_widget);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }	
}
