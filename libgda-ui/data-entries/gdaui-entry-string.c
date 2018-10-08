/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include "gdaui-entry-string.h"
#include "gdaui-entry.h"
#include <libgda/gda-data-handler.h>
#include "gdk/gdkkeysyms.h"
#include <libgda/gda-debug-macros.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_string_dispose (GObject *object);

static void gdaui_entry_string_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gdaui_entry_string_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

/* properties */
enum
{
	PROP_0,
	PROP_MULTILINE,
	PROP_EDITING_CANCELED,
	PROP_OPTIONS
};

/* GtkCellEditable interface */
static void gdaui_entry_string_cell_editable_init (GtkCellEditableIface *iface);
static void gdaui_entry_string_start_editing (GtkCellEditable *iface, GdkEvent *event);
static void sync_entry_options (GdauiEntryString *mgstr);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* options */
static void set_entry_options (GdauiEntryString *mgstr, const gchar *options);

/* private structure */
typedef struct
{
	gboolean       multiline;
	gboolean       hidden;
	GtkWidget     *vbox;

	GtkWidget     *entry;
	gboolean       editing_canceled;

	GtkTextBuffer *buffer;
	GtkWidget     *sw;
	GtkWidget     *view;

	gint           maxsize;
} GdauiEntryStringPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiEntryString, gdaui_entry_string, GDAUI_TYPE_ENTRY_WRAPPER,
                         G_ADD_PRIVATE (GdauiEntryString)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, gdaui_entry_string_cell_editable_init))

static void
gdaui_entry_string_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_string_start_editing;
}


static void
gdaui_entry_string_class_init (GdauiEntryStringClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gdaui_entry_string_dispose;

	GDAUI_ENTRY_WRAPPER_CLASS (klass)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_string_set_property;
	object_class->get_property = gdaui_entry_string_get_property;

	g_object_class_install_property (object_class, PROP_MULTILINE,
					 g_param_spec_boolean ("multiline", NULL, NULL, FALSE, 
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_OPTIONS,
					 g_param_spec_string ("options", NULL, NULL, NULL, G_PARAM_WRITABLE));
}

static gboolean
key_press_event_cb (GdauiEntryString *mgstr, GdkEventKey *key_event, G_GNUC_UNUSED gpointer data)
{
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	if (key_event->keyval == GDK_KEY_Escape)
		priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_string_init (GdauiEntryString *mgstr)
{
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	priv->multiline = FALSE;
	priv->hidden = FALSE;
	priv->vbox = NULL;
	priv->entry = NULL;
	priv->editing_canceled = FALSE;
	priv->buffer = NULL;
	priv->view = NULL;
	priv->sw = NULL;

	priv->maxsize = 65535; /* eg. unlimited for GtkEntry */

	g_signal_connect (mgstr, "key-press-event",
			  G_CALLBACK (key_press_event_cb), NULL);
}

/**
 * gdaui_entry_string_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: (nullable): some options formatting the new entry, or %NULL
 *
 * Creates a new data entry widget. Known options are: MAX_SIZE, MULTILINE, and HIDDEN
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_string_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryString *mgstr;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);
	g_return_val_if_fail (type == G_TYPE_STRING, NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_STRING, "handler", dh, NULL);
	mgstr = GDAUI_ENTRY_STRING (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgstr), type);

	g_object_set (obj, "options", options, NULL);

	return GTK_WIDGET (obj);
}

static void
gdaui_entry_string_dispose (GObject   * object)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_STRING (object));

	mgstr = GDAUI_ENTRY_STRING (object);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	if (priv->entry)
		priv->entry = NULL;

	if (priv->view)
		priv->view = NULL;

	/* parent class */
	G_OBJECT_CLASS (gdaui_entry_string_parent_class)->dispose (object);
}

static void
gdaui_entry_string_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	GdauiEntryString *mgstr;

	mgstr = GDAUI_ENTRY_STRING (object);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	switch (param_id) {
		case PROP_MULTILINE:
			if (g_value_get_boolean (value) != priv->multiline) {
				priv->multiline = g_value_get_boolean (value);
				if (priv->multiline) {
					gtk_widget_hide (priv->entry);
					gtk_widget_show (priv->sw);
					gtk_widget_set_vexpand (GTK_WIDGET (mgstr), TRUE);
				}
				else {
					gtk_widget_show (priv->entry);
					gtk_widget_hide (priv->sw);
					gtk_widget_set_vexpand (GTK_WIDGET (mgstr), FALSE);
				}
				g_signal_emit_by_name (object, "expand-changed");
			}
			break;
		case PROP_OPTIONS:
			set_entry_options (mgstr, g_value_get_string (value));
			break;
		case PROP_EDITING_CANCELED:
			TO_IMPLEMENT;
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gdaui_entry_string_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdauiEntryString *mgstr;

	mgstr = GDAUI_ENTRY_STRING (object);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	switch (param_id) {
		case PROP_MULTILINE:
			g_value_set_boolean (value, priv->multiline);
			break;
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, priv->editing_canceled);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void widget_shown_cb (GtkWidget *wid, GdauiEntryString *mgstr);
static void
ev_cb (GtkWidget *wid, GdkEvent *event, GObject *obj)
{
	/* see /usr/include/gtk-3.0/gdk/gdkevents.h */
  if ((event->type == GDK_FOCUS_CHANGE) && !((GdkEventFocus*)event)->in)
		g_signal_emit_by_name (obj, "event-after", event);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *vbox;
	GdauiEntryString *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand (GTK_WIDGET (mgwrap), TRUE);
	priv->vbox = vbox;
	gtk_widget_add_events (vbox, GDK_FOCUS_CHANGE_MASK);

	/* one line entry */
	priv->entry = gdaui_entry_new (NULL, NULL);
	sync_entry_options (mgstr);
	gtk_box_pack_start (GTK_BOX (vbox), priv->entry, FALSE, TRUE, 0);
	g_signal_connect_after (G_OBJECT (priv->entry), "show",
				G_CALLBACK (widget_shown_cb), mgstr);

	/* multiline entry */
	priv->view = gtk_text_view_new ();
	gtk_widget_set_vexpand (priv->view, TRUE);
	priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->view));
	priv->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (priv->sw), priv->view);
	gtk_widget_show (priv->view);
	gtk_box_pack_start (GTK_BOX (vbox), priv->sw, TRUE, TRUE, 0);
	g_signal_connect_after (G_OBJECT (priv->sw), "show",
				G_CALLBACK (widget_shown_cb), mgstr);

	/* mark both widgets to be shown */
	gtk_widget_show (priv->entry);
	gtk_widget_show (priv->sw);

	g_signal_connect (priv->entry, "event-after",
                          G_CALLBACK (ev_cb), vbox);
	g_signal_connect (priv->view, "event-after",
                          G_CALLBACK (ev_cb), vbox);
	return vbox;
}

static void
widget_shown_cb (GtkWidget *wid, GdauiEntryString *mgstr)
{
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	if ((wid == priv->entry) && priv->multiline)
		gtk_widget_hide (priv->entry);

	if ((wid == priv->sw) && !priv->multiline)
		gtk_widget_hide (priv->sw);
}


static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryString *mgstr;
	GdaDataHandler *dh;

	PangoLayout *layout;
	gchar *text;
	
	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	/* do we need to go into multi line mode ? */
	text = gda_data_handler_get_str_from_value (dh, value);
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (mgwrap), text);
	if (pango_layout_get_line_count (layout) > 1) 
		g_object_set (G_OBJECT (mgwrap), "multiline", TRUE, NULL);
	g_object_unref (G_OBJECT (layout));
	
	/* fill the single line widget */
	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), text);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);

	/* fill the multiline widget */
	if (value) {
		if (gda_value_is_null ((GValue *) value) || !text)
                        gtk_text_buffer_set_text (priv->buffer, "", -1);
		else 
			gtk_text_buffer_set_text (priv->buffer, text, -1);
	}
	else 
		gtk_text_buffer_set_text (priv->buffer, "", -1);

	g_free (text);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryString *mgstr;
	GdaDataHandler *dh;

	g_return_val_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	if (! priv->multiline) {
		gchar *cstr;
		cstr = gdaui_entry_get_text (GDAUI_ENTRY (priv->entry));
		value = gda_data_handler_get_value_from_str (dh, cstr, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
		g_free (cstr);
	}
	else {
		GtkTextIter start, end;
		gchar *str;
		gtk_text_buffer_get_start_iter (priv->buffer, &start);
		gtk_text_buffer_get_end_iter (priv->buffer, &end);
		str = gtk_text_buffer_get_text (priv->buffer, &start, &end, FALSE);
		value = gda_data_handler_get_value_from_str (dh, str, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
		g_free (str);
	}

	if (!value) {
		/* in case the gda_data_handler_get_value_from_str() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	g_signal_connect_swapped (G_OBJECT (priv->entry), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (priv->entry), "activate",
				  activate_cb, mgwrap);

	g_signal_connect_swapped (G_OBJECT (priv->buffer), "changed",
				  modify_cb, mgwrap);
	/* FIXME: how does the user "activates" the GtkTextView widget ? */
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	gtk_editable_set_editable (GTK_EDITABLE (priv->entry), editable);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->view), editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	if (priv->multiline)
		gtk_widget_grab_focus (priv->view);
	else
		gtk_widget_grab_focus (priv->entry);
}

/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryString *mgstr)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgstr));
}

static void
gtk_cell_editable_entry_remove_widget_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryString *mgstr)
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgstr));
}

static void
gdaui_entry_string_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (iface));
	mgstr = GDAUI_ENTRY_STRING (iface);
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	priv->editing_canceled = FALSE;
	g_object_set (G_OBJECT (priv->entry), "has-frame", FALSE, "xalign", 0., NULL);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->view), GTK_TEXT_WINDOW_LEFT, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->view), GTK_TEXT_WINDOW_RIGHT, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->view), GTK_TEXT_WINDOW_TOP, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (priv->view), GTK_TEXT_WINDOW_BOTTOM, 0);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (priv->sw), 0);

	gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (priv->entry), event);
	g_signal_connect (priv->entry, "editing-done",
			  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgstr);
	g_signal_connect (priv->entry, "remove-widget",
			  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgstr);
	
	gtk_widget_grab_focus (priv->entry);
	gtk_widget_queue_draw (GTK_WIDGET (mgstr));
}

/*
 * Options handling
 */
static void
set_entry_options (GdauiEntryString *mgstr, const gchar *options)
{
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;
		
                params = gda_quark_list_new_from_string (options);

		str = gda_quark_list_find (params, "MAX_SIZE");
		if (str) 
			priv->maxsize = atoi (str);
		
		str = gda_quark_list_find (params, "MULTILINE");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				priv->multiline = TRUE;
			else
				priv->multiline = FALSE;
			
		}
		
		str = gda_quark_list_find (params, "HIDDEN");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				priv->hidden = TRUE;
			else
				priv->hidden = FALSE;
		}
		
		if (priv->entry) {
			if (priv->multiline) {
				gtk_widget_hide (priv->entry);
				gtk_widget_show (priv->sw);
			}
			else {
				gtk_widget_show (priv->entry);
				gtk_widget_hide (priv->sw);
				gtk_entry_set_visibility (GTK_ENTRY (priv->entry),
							  !priv->hidden);
			}
		}
                gda_quark_list_free (params);
		sync_entry_options (mgstr);
        }
}

/* sets the correct options for priv->entry if it exists */
static void
sync_entry_options (GdauiEntryString *mgstr)
{
	GdauiEntryStringPrivate *priv = gdaui_entry_string_get_instance_private (mgstr);
	if (!priv->entry)
		return;

	g_object_set (G_OBJECT (priv->entry),
		      "max-length", priv->maxsize,
		      NULL);
	g_signal_emit_by_name (priv->entry, "changed");
}
