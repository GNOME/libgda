/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include "gdaui-entry-number.h"
#include "gdaui-numeric-entry.h"
#include <libgda/gda-data-handler.h>
#include "gdk/gdkkeysyms.h"
#include <libgda/gda-debug-macros.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_number_dispose (GObject *object);

static void gdaui_entry_number_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gdaui_entry_number_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

/* properties */
enum
{
	PROP_0,
	PROP_EDITING_CANCELED,
	PROP_OPTIONS
};

/* GtkCellEditable interface */
static void gdaui_entry_number_cell_editable_init (GtkCellEditableIface *iface);
static void gdaui_entry_number_start_editing (GtkCellEditable *iface, GdkEvent *event);
static void sync_entry_options (GdauiEntryNumber *mgstr);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* options */
static void set_entry_options (GdauiEntryNumber *mgstr, const gchar *options);

/* private structure */
typedef struct
{
	GtkWidget     *entry;
	gboolean       editing_canceled;

	guchar         thousand_sep;
	guint16        nb_decimals;
	gchar         *currency;
} GdauiEntryNumberPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiEntryNumber, gdaui_entry_number, GDAUI_TYPE_ENTRY_WRAPPER,
                         G_ADD_PRIVATE (GdauiEntryNumber)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, gdaui_entry_number_cell_editable_init))

static void
gdaui_entry_number_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_number_start_editing;
}

static void
gdaui_entry_number_class_init (GdauiEntryNumberClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gdaui_entry_number_dispose;

	GDAUI_ENTRY_WRAPPER_CLASS (klass)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_number_set_property;
	object_class->get_property = gdaui_entry_number_get_property;

	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_OPTIONS,
					 g_param_spec_string ("options", NULL, NULL, NULL, G_PARAM_WRITABLE));
}

static gboolean
key_press_event_cb (GdauiEntryNumber *mgstr, GdkEventKey *key_event, G_GNUC_UNUSED gpointer data)
{
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);
	if (key_event->keyval == GDK_KEY_Escape)
		priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_number_init (GdauiEntryNumber *mgstr)
{
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);
	priv->entry = NULL;

	priv->thousand_sep = 0;
	priv->nb_decimals = G_MAXUINT16; /* unlimited number of decimals */
	priv->currency = NULL;

	g_signal_connect (mgstr, "key-press-event",
			  G_CALLBACK (key_press_event_cb), NULL);
}

gboolean
gdaui_entry_number_is_type_numeric (GType type)
{
	if ((type == G_TYPE_INT64) || (type == G_TYPE_UINT64) || (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) || (type == GDA_TYPE_NUMERIC) || (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) || (type == GDA_TYPE_USHORT) || (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) || (type == G_TYPE_LONG) || (type ==  G_TYPE_ULONG) || (type == G_TYPE_UINT))
		return TRUE;
	else
		return FALSE;
}

/**
 * gdaui_entry_number_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: (nullable): some options formatting the new entry, or %NULL
 *
 * Creates a new data entry widget. Known options are: THOUSAND_SEP, NB_DECIMALS and CURRENCY
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_number_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);
	g_return_val_if_fail (gdaui_entry_number_is_type_numeric (type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_NUMBER, "handler", dh, NULL);
	mgstr = GDAUI_ENTRY_NUMBER (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgstr), type);

	g_object_set (obj, "options", options, NULL);

	return GTK_WIDGET (obj);
}

static void
gdaui_entry_number_dispose (GObject   * object)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (object));

	mgstr = GDAUI_ENTRY_NUMBER (object);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);
	if (priv->entry)
		priv->entry = NULL;
	if (priv->currency) {
		g_free (priv->currency);
		priv->currency = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_entry_number_parent_class)->dispose (object);
}

static void
gdaui_entry_number_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	GdauiEntryNumber *mgstr;

	mgstr = GDAUI_ENTRY_NUMBER (object);
	switch (param_id) {
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
gdaui_entry_number_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	GdauiEntryNumber *mgstr;

	mgstr = GDAUI_ENTRY_NUMBER (object);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);
	switch (param_id) {
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, priv->editing_canceled);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
event_after_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
        g_signal_emit_by_name (widget, "event-after", event);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *hb;
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	priv->entry = gdaui_numeric_entry_new (gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
	sync_entry_options (mgstr);

	gtk_container_add (GTK_CONTAINER (hb), priv->entry);
	gtk_widget_show (priv->entry);
	g_signal_connect_swapped (priv->entry, "event-after",
                                  G_CALLBACK (event_after_cb), hb);
	return hb;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryNumber *mgstr;
	GdaDataHandler *dh;
	gchar *text;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	text = gda_data_handler_get_str_from_value (dh, value);
	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), text);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);

	g_free (text);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	value = gdaui_numeric_entry_get_value (GDAUI_NUMERIC_ENTRY (priv->entry));

	if (!value) {
		/* in case the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	g_signal_connect_swapped (G_OBJECT (priv->entry), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (priv->entry), "activate",
				  activate_cb, mgwrap);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	gtk_editable_set_editable (GTK_EDITABLE (priv->entry), editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	gtk_widget_grab_focus (priv->entry);
}

/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryNumber *mgstr)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgstr));
}

static void
gtk_cell_editable_entry_remove_widget_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryNumber *mgstr)
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgstr));
}

static void
gdaui_entry_number_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (iface));
	mgstr = GDAUI_ENTRY_NUMBER (iface);
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	priv->editing_canceled = FALSE;
	g_object_set (G_OBJECT (priv->entry), "has-frame", FALSE, "xalign", 0., NULL);

	gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (priv->entry), event);
	g_signal_connect (G_OBJECT (priv->entry), "editing-done",
			  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgstr);
	g_signal_connect (G_OBJECT (priv->entry), "remove-widget",
			  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgstr);

	gtk_widget_grab_focus (priv->entry);
	gtk_widget_queue_draw (GTK_WIDGET (mgstr));
}

/*
 * Options handling
 */

static guchar
get_default_thousands_sep ()
{
	static guchar value = 255;

	if (value == 255) {
		gchar text[20];
		sprintf (text, "%'f", 1234.);
		if (text[1] == '2')
			value = ' ';
		else
			value = text[1];	
	}
	return value;
}

static void
set_entry_options (GdauiEntryNumber *mgstr, const gchar *options)
{
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);

		str = gda_quark_list_find (params, "THOUSAND_SEP");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				priv->thousand_sep = get_default_thousands_sep ();
			else
				priv->thousand_sep = 0;
		}
		str = gda_quark_list_find (params, "NB_DECIMALS");
		if (str) {
			if (*str)
				priv->nb_decimals = atoi (str);
			else
				priv->nb_decimals = 0;
		}
		str = gda_quark_list_find (params, "CURRENCY");
		if (str && *str) {
			g_free (priv->currency);
			priv->currency = g_strdup_printf ("%s ", str);
		}
                gda_quark_list_free (params);
		sync_entry_options (mgstr);
        }
}

/* sets the correct options for priv->entry if it exists */
static void
sync_entry_options (GdauiEntryNumber *mgstr)
{
	GdauiEntryNumberPrivate *priv = gdaui_entry_number_get_instance_private (mgstr);
	if (!priv->entry)
		return;

	g_object_set (G_OBJECT (priv->entry),
		      "type", gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgstr)),
		      "n-decimals", priv->nb_decimals,
		      "thousands-sep", priv->thousand_sep,
		      "prefix", priv->currency,
		      NULL);
	g_signal_emit_by_name (priv->entry, "changed");
}
