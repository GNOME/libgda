/* gdaui-entry-number.c
 *
 * Copyright (C) 2009 Vivien Malerba
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
#include "gdaui-entry-number.h"
#include "gdaui-numeric-entry.h"
#include <libgda/gda-data-handler.h>
#include "gdk/gdkkeysyms.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_number_class_init (GdauiEntryNumberClass *klass);
static void gdaui_entry_number_init (GdauiEntryNumber *srv);
static void gdaui_entry_number_dispose (GObject *object);
static void gdaui_entry_number_finalize (GObject *object);

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
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* options */
static void set_entry_options (GdauiEntryNumber *mgstr, const gchar *options);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryNumberPrivate
{
	GtkWidget     *entry;
	gboolean       editing_canceled;

	guchar         thousand_sep;
	guint16        nb_decimals;
	gchar         *currency;

	gulong         entry_change_sig;
};

static void
gdaui_entry_number_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_number_start_editing;
}

GType
gdaui_entry_number_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryNumberClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_number_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryNumber),
			0,
			(GInstanceInitFunc) gdaui_entry_number_init
		};
		
		static const GInterfaceInfo cell_editable_info = {
			(GInterfaceInitFunc) gdaui_entry_number_cell_editable_init,    /* interface_init */
			NULL,                                                 /* interface_finalize */
			NULL                                                  /* interface_data */
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryNumber", &info, 0);
		g_type_add_interface_static (type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	return type;
}

static void
gdaui_entry_number_class_init (GdauiEntryNumberClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gdaui_entry_number_dispose;
	object_class->finalize = gdaui_entry_number_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (klass)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->can_expand = can_expand;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (klass)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_number_set_property;
	object_class->get_property = gdaui_entry_number_get_property;

	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_OPTIONS,
					 g_param_spec_string ("options", NULL, NULL, NULL, G_PARAM_WRITABLE));
}

static gboolean
key_press_event_cb (GdauiEntryNumber *mgstr, GdkEventKey *key_event, gpointer data)
{
	if (key_event->keyval == GDK_Escape)
		mgstr->priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_number_init (GdauiEntryNumber *mgstr)
{
	mgstr->priv = g_new0 (GdauiEntryNumberPrivate, 1);
	mgstr->priv->entry = NULL;

	mgstr->priv->thousand_sep = 0;
	mgstr->priv->nb_decimals = G_MAXUINT16; /* unlimited number of decimals */
	mgstr->priv->currency = NULL;

	mgstr->priv->entry_change_sig = 0;

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
 * gdaui_entry_number_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_number_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);
	g_return_val_if_fail (gdaui_entry_number_is_type_numeric (type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_NUMBER, "handler", dh, NULL);
	mgstr = GDAUI_ENTRY_NUMBER (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgstr), type);

	g_object_set (obj, "options", options, NULL);

	return GTK_WIDGET (obj);
}

static gboolean focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryNumber *mgstr);
static void
gdaui_entry_number_dispose (GObject   * object)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (object));

	mgstr = GDAUI_ENTRY_NUMBER (object);
	if (mgstr->priv) {
		if (mgstr->priv->entry) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (mgstr->priv->entry),
							      G_CALLBACK (focus_out_cb), mgstr);
			mgstr->priv->entry = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_number_finalize (GObject   * object)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (object));

	mgstr = GDAUI_ENTRY_NUMBER (object);
	if (mgstr->priv) {
		g_free (mgstr->priv->currency);
		g_free (mgstr->priv);
		mgstr->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
gdaui_entry_number_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiEntryNumber *mgstr;

	mgstr = GDAUI_ENTRY_NUMBER (object);
	if (mgstr->priv) {
		switch (param_id) {
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
gdaui_entry_number_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdauiEntryNumber *mgstr;

	mgstr = GDAUI_ENTRY_NUMBER (object);
	if (mgstr->priv) {
		switch (param_id) {
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, mgstr->priv->editing_canceled);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);

	mgstr->priv->entry = gdaui_numeric_entry_new (gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
	sync_entry_options (mgstr);

	return mgstr->priv->entry;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryNumber *mgstr;
	GdaDataHandler *dh;
	gchar *text;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	text = gda_data_handler_get_str_from_value (dh, value);
	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), NULL);
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), text);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (mgstr->priv->entry), NULL);

	g_free (text);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryNumber *mgstr;

	g_return_val_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap), NULL);
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	g_return_val_if_fail (mgstr->priv, NULL);

	value = gdaui_numeric_entry_get_value (GDAUI_NUMERIC_ENTRY (mgstr->priv->entry));

	if (!value) {
		/* in case the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

typedef void (*Callback2) (gpointer, gpointer);
static gboolean
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryNumber *mgstr)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgstr);

	return FALSE;
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);
	g_return_if_fail (mgstr->priv);
	g_object_set_data (G_OBJECT (mgstr->priv->entry), "_activate_cb", activate_cb);

	mgstr->priv->entry_change_sig = g_signal_connect (G_OBJECT (mgstr->priv->entry), "changed",
							  modify_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "activate",
			  activate_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "focus-out-event",
			  G_CALLBACK (focus_out_cb), mgstr);
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	return FALSE;
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);

	gtk_editable_set_editable (GTK_EDITABLE (mgstr->priv->entry), editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (mgwrap));
	mgstr = GDAUI_ENTRY_NUMBER (mgwrap);

	gtk_widget_grab_focus (mgstr->priv->entry);
}

/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (GtkEntry *entry, GdauiEntryNumber *mgstr) 
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgstr));
}

static void
gtk_cell_editable_entry_remove_widget_cb (GtkEntry *entry, GdauiEntryNumber *mgstr) 
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgstr));
}

static void
gdaui_entry_number_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryNumber *mgstr;

	g_return_if_fail (GDAUI_IS_ENTRY_NUMBER (iface));
	mgstr = GDAUI_ENTRY_NUMBER (iface);

	mgstr->priv->editing_canceled = FALSE;
	g_object_set (G_OBJECT (mgstr->priv->entry), "has-frame", FALSE, "xalign", 0., NULL);

	gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (mgstr->priv->entry), event);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "editing-done",
			  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgstr);
	g_signal_connect (G_OBJECT (mgstr->priv->entry), "remove-widget",
			  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgstr);
	gdaui_entry_shell_refresh (GDAUI_ENTRY_SHELL (mgstr));

	gtk_widget_grab_focus (mgstr->priv->entry);
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
	g_assert (mgstr->priv);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);

		str = gda_quark_list_find (params, "THOUSAND_SEP");
		if (str) {
			if ((*str == 't') || (*str == 'T'))
				mgstr->priv->thousand_sep = get_default_thousands_sep ();
			else
				mgstr->priv->thousand_sep = 0;
		}
		str = gda_quark_list_find (params, "NB_DECIMALS");
		if (str) {
			if (*str)
				mgstr->priv->nb_decimals = atoi (str);
			else
				mgstr->priv->nb_decimals = 0;
		}
		str = gda_quark_list_find (params, "CURRENCY");
		if (str && *str) {
			g_free (mgstr->priv->currency);
			mgstr->priv->currency = g_strdup_printf ("%s ", str);
		}
                gda_quark_list_free (params);
		sync_entry_options (mgstr);
        }
}

/* sets the correct options for mgstr->priv->entry if it exists */
static void
sync_entry_options (GdauiEntryNumber *mgstr)
{
	if (!mgstr->priv->entry)
		return;

	g_object_set (G_OBJECT (mgstr->priv->entry), 
		      "type", gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgstr)),
		      "n-decimals", mgstr->priv->nb_decimals,
		      "thousands-sep", mgstr->priv->thousand_sep,
		      "prefix", mgstr->priv->currency,
		      NULL);
	g_signal_emit_by_name (mgstr->priv->entry, "changed");
}
