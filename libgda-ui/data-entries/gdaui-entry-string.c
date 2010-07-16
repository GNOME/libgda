/* gdaui-entry-string.c
 *
 * Copyright (C) 2003 - 2010 Vivien Malerba
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
#include <string.h>
#include "gdaui-entry-string.h"
#include "gdaui-entry.h"
#include <libgda/gda-data-handler.h>
#include "gdk/gdkkeysyms.h"

#define MAX_ACCEPTED_STRING_LENGTH 500

/* 
 * Main static functions 
 */
static void gdaui_entry_string_class_init (GdauiEntryStringClass *klass);
static void gdaui_entry_string_init (GdauiEntryString *srv);
static void gdaui_entry_string_dispose (GObject *object);
static void gdaui_entry_string_finalize (GObject *object);

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
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* options */
static void set_entry_options (GdauiEntryString *mgstr, const gchar *options);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryStringPrivate
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

	gulong         entry_change_sig;
};

static void
gdaui_entry_string_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_string_start_editing;
}

GType
gdaui_entry_string_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryStringClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_string_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryString),
			0,
			(GInstanceInitFunc) gdaui_entry_string_init
		};
		
		static const GInterfaceInfo cell_editable_info = {
			(GInterfaceInitFunc) gdaui_entry_string_cell_editable_init,    /* interface_init */
			NULL,                                                 /* interface_finalize */
			NULL                                                  /* interface_data */
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryString", &info, 0);
		g_type_add_interface_static (type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	return type;
}

static void
gdaui_entry_string_class_init (GdauiEntryStringClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gdaui_entry_string_dispose;
	object_class->finalize = gdaui_entry_string_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (klass)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->can_expand = can_expand;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_string_set_property;
	object_class->get_property = gdaui_entry_string_get_property;

	g_object_class_install_property (object_class, PROP_MULTILINE,
					 g_param_spec_boolean ("multiline", NULL, NULL, FALSE, 
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_OPTIONS,
					 g_param_spec_string ("options", NULL, NULL, NULL, G_PARAM_WRITABLE));
}

static gboolean
key_press_event_cb (GdauiEntryString *mgstr, GdkEventKey *key_event, gpointer data)
{
	if (key_event->keyval == GDK_Escape)
		mgstr->priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_string_init (GdauiEntryString *mgstr)
{
	mgstr->priv = g_new0 (GdauiEntryStringPrivate, 1);
	mgstr->priv->multiline = FALSE;
	mgstr->priv->hidden = FALSE;
	mgstr->priv->vbox = NULL;
	mgstr->priv->entry = NULL;
	mgstr->priv->editing_canceled = FALSE;
	mgstr->priv->buffer = NULL;
	mgstr->priv->view = NULL;
	mgstr->priv->sw = NULL;

	mgstr->priv->maxsize = 65535; /* eg. unlimited for GtkEntry */

	mgstr->priv->entry_change_sig = 0;

	g_signal_connect (mgstr, "key-press-event",
			  G_CALLBACK (key_press_event_cb), NULL);
}

/**
 * gdaui_entry_string_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_string_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryString *mgstr;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);
	g_return_val_if_fail (type == G_TYPE_STRING, NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_STRING, "handler", dh, NULL);
	mgstr = GDAUI_ENTRY_STRING (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgstr), type);

	g_object_set (obj, "options", options, NULL);

	return GTK_WIDGET (obj);
}

static gboolean focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryString *mgstr);
static void
gdaui_entry_string_dispose (GObject   * object)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_STRING (object));

	mgstr = GDAUI_ENTRY_STRING (object);
	if (mgstr->priv) {
		if (mgstr->priv->entry) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (mgstr->priv->entry),
							      G_CALLBACK (focus_out_cb), mgstr);
			mgstr->priv->entry = NULL;
		}
		
		if (mgstr->priv->view) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (mgstr->priv->view),
							      G_CALLBACK (focus_out_cb), mgstr);
			mgstr->priv->view = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_string_finalize (GObject   * object)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_STRING (object));

	mgstr = GDAUI_ENTRY_STRING (object);
	if (mgstr->priv) {
		g_free (mgstr->priv);
		mgstr->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
gdaui_entry_string_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	GdauiEntryString *mgstr;

	mgstr = GDAUI_ENTRY_STRING (object);
	if (mgstr->priv) {
		switch (param_id) {
		case PROP_MULTILINE:
			if (g_value_get_boolean (value) != mgstr->priv->multiline) {
				mgstr->priv->multiline = g_value_get_boolean (value);
				if (mgstr->priv->multiline) {
					gtk_widget_hide (mgstr->priv->entry);
					gtk_widget_show (mgstr->priv->sw);
				}
				else {
					gtk_widget_show (mgstr->priv->entry);
					gtk_widget_hide (mgstr->priv->sw);
				}
				g_signal_emit_by_name (object, "expand-changed");
			}
			break;
		case PROP_OPTIONS:
			set_entry_options (mgstr, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
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
	if (mgstr->priv) {
		switch (param_id) {
		case PROP_MULTILINE:
			g_value_set_boolean (value, mgstr->priv->multiline);
			break;
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, mgstr->priv->editing_canceled);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void widget_shown_cb (GtkWidget *wid, GdauiEntryString *mgstr);

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *vbox;
	GdauiEntryString *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_val_if_fail (mgstr->priv, NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	mgstr->priv->vbox = vbox;

	/* one line entry */
	mgstr->priv->entry = gdaui_entry_new (NULL, NULL);
	sync_entry_options (mgstr);
	gtk_box_pack_start (GTK_BOX (vbox), mgstr->priv->entry, FALSE, TRUE, 0);
	g_signal_connect_after (G_OBJECT (mgstr->priv->entry), "show", 
				G_CALLBACK (widget_shown_cb), mgstr);

	/* multiline entry */
	mgstr->priv->view = gtk_text_view_new ();
	mgstr->priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mgstr->priv->view));
	mgstr->priv->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (mgstr->priv->sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mgstr->priv->sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (mgstr->priv->sw), mgstr->priv->view);
	gtk_widget_show (mgstr->priv->view);
	gtk_box_pack_start (GTK_BOX (vbox), mgstr->priv->sw, TRUE, TRUE, 0);
	g_signal_connect_after (G_OBJECT (mgstr->priv->sw), "show", 
				G_CALLBACK (widget_shown_cb), mgstr);


	/* show widgets if they need to be shown */
	gtk_widget_show (mgstr->priv->entry);
	gtk_widget_show (mgstr->priv->sw);

	return vbox;
}

static void
widget_shown_cb (GtkWidget *wid, GdauiEntryString *mgstr)
{
	if ((wid == mgstr->priv->entry) && mgstr->priv->multiline)
		gtk_widget_hide (mgstr->priv->entry);

	if ((wid == mgstr->priv->sw) && !mgstr->priv->multiline)
		gtk_widget_hide (mgstr->priv->sw);
}


static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryString *mgstr;
	GdaDataHandler *dh;

	PangoLayout *layout;
	gchar *text;
	static gchar *too_long_msg = NULL;
	static gint too_long_msg_len;
	
	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_if_fail (mgstr->priv);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	/* do we need to go into multi line mode ? */
	if (!too_long_msg) {
		too_long_msg = _("<string cut because too long>");
		too_long_msg_len = strlen (too_long_msg);
	}
	text = gda_data_handler_get_str_from_value (dh, value);
	if (text && (strlen (text) > MAX_ACCEPTED_STRING_LENGTH + too_long_msg_len)) {
		memmove (text + too_long_msg_len, text, MAX_ACCEPTED_STRING_LENGTH);
		memcpy (text, too_long_msg, too_long_msg_len);
		text [MAX_ACCEPTED_STRING_LENGTH + too_long_msg_len + 1] = 0;
	}

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (mgwrap), text);
	if (pango_layout_get_line_count (layout) > 1) 
		g_object_set (G_OBJECT (mgwrap), "multiline", TRUE, NULL);
	g_object_unref (G_OBJECT (layout));
	
	/* fill the single line widget */
	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), NULL);
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), text);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), NULL);

	/* fill the multiline widget */
	if (value) {
		if (gda_value_is_null ((GValue *) value) || !text)
                        gtk_text_buffer_set_text (mgstr->priv->buffer, "", -1);
		else 
			gtk_text_buffer_set_text (mgstr->priv->buffer, text, -1);
	}
	else 
		gtk_text_buffer_set_text (mgstr->priv->buffer, "", -1);

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
	g_return_val_if_fail (mgstr->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	if (! mgstr->priv->multiline) {
		gchar *cstr;
		cstr = gdaui_entry_get_text (GDAUI_ENTRY (mgstr->priv->entry));
		value = gda_data_handler_get_value_from_str (dh, cstr, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
		g_free (cstr);
	}
	else {
		GtkTextIter start, end;
		gchar *str;
		gtk_text_buffer_get_start_iter (mgstr->priv->buffer, &start);
		gtk_text_buffer_get_end_iter (mgstr->priv->buffer, &end);
		str = gtk_text_buffer_get_text (mgstr->priv->buffer, &start, &end, FALSE);
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

typedef void (*Callback2) (gpointer, gpointer);
static gboolean
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryString *mgstr)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgstr);

	return FALSE;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_if_fail (mgstr->priv);
	g_object_set_data (G_OBJECT (mgstr->priv->entry), "_activate_cb", activate_cb);
	g_object_set_data (G_OBJECT (mgstr->priv->view), "_activate_cb", activate_cb);

	mgstr->priv->entry_change_sig = g_signal_connect (G_OBJECT (mgstr->priv->entry), "changed",
							  modify_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "activate",
			  activate_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "focus-out-event",
			  G_CALLBACK (focus_out_cb), mgstr);

	g_signal_connect (G_OBJECT (mgstr->priv->buffer), "changed",
			  modify_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgstr->priv->view), "focus-out-event",
			  G_CALLBACK (focus_out_cb), mgstr);
	/* FIXME: how does the user "activates" the GtkTextView widget ? */
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	GdauiEntryString *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap), FALSE);
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_val_if_fail (mgstr->priv, FALSE);

	return mgstr->priv->multiline;
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_if_fail (mgstr->priv);

	gtk_entry_set_editable (GTK_ENTRY (mgstr->priv->entry), editable);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (mgstr->priv->view), editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (mgwrap));
	mgstr = GDAUI_ENTRY_STRING (mgwrap);
	g_return_if_fail (mgstr->priv);

	if (mgstr->priv->multiline)
		gtk_widget_grab_focus (mgstr->priv->view);
	else
		gtk_widget_grab_focus (mgstr->priv->entry);
}

/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (GtkEntry *entry, GdauiEntryString *mgstr) 
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgstr));
}

static void
gtk_cell_editable_entry_remove_widget_cb (GtkEntry *entry, GdauiEntryString *mgstr) 
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgstr));
}

static void
gdaui_entry_string_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryString *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_STRING (iface));
	mgstr = GDAUI_ENTRY_STRING (iface);
	g_return_if_fail (mgstr->priv);

	mgstr->priv->editing_canceled = FALSE;
	g_object_set (G_OBJECT (mgstr->priv->entry), "has-frame", FALSE, "xalign", 0., NULL);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (mgstr->priv->view), GTK_TEXT_WINDOW_LEFT, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (mgstr->priv->view), GTK_TEXT_WINDOW_RIGHT, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (mgstr->priv->view), GTK_TEXT_WINDOW_TOP, 0);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (mgstr->priv->view), GTK_TEXT_WINDOW_BOTTOM, 0);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (mgstr->priv->sw), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (mgstr->priv->sw), 0);

	gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (mgstr->priv->entry), event);
	g_signal_connect (mgstr->priv->entry, "editing-done",
			  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgstr);
	g_signal_connect (mgstr->priv->entry, "remove-widget",
			  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgstr);
	gdaui_entry_shell_refresh (GDAUI_ENTRY_SHELL (mgstr));
	
	gtk_widget_grab_focus (mgstr->priv->entry);
	gtk_widget_queue_draw (GTK_WIDGET (mgstr));
}

/*
 * Options handling
 */
static void
set_entry_options (GdauiEntryString *mgstr, const gchar *options)
{
	g_assert (mgstr->priv);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;
		
                params = gda_quark_list_new_from_string (options);

		str = gda_quark_list_find (params, "MAX_SIZE");
		if (str) 
			mgstr->priv->maxsize = atoi (str);
		
		str = gda_quark_list_find (params, "MULTILINE");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				mgstr->priv->multiline = TRUE;
			else
				mgstr->priv->multiline = FALSE;
			
		}
		
		str = gda_quark_list_find (params, "HIDDEN");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				mgstr->priv->hidden = TRUE;
			else
				mgstr->priv->hidden = FALSE;
		}
		
		if (mgstr->priv->entry) {
			if (mgstr->priv->multiline) {
				gtk_widget_hide (mgstr->priv->entry);
				gtk_widget_show (mgstr->priv->sw);
			}
			else {
				gtk_widget_show (mgstr->priv->entry);
				gtk_widget_hide (mgstr->priv->sw);
				gtk_entry_set_visibility (GTK_ENTRY (mgstr->priv->entry), 
							  !mgstr->priv->hidden);
			}
		}
                gda_quark_list_free (params);
		sync_entry_options (mgstr);
        }
}

/* sets the correct options for mgstr->priv->entry if it exists */
static void
sync_entry_options (GdauiEntryString *mgstr)
{
	if (!mgstr->priv->entry)
		return;

	g_object_set (G_OBJECT (mgstr->priv->entry), 
		      "max-length", mgstr->priv->maxsize,
		      NULL);
	g_signal_emit_by_name (mgstr->priv->entry, "changed");
}
