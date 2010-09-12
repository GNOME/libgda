/* gdaui-entry-common-time.c
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
#include "gdaui-entry-common-time.h"
#include <libgda/gda-data-handler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include "gdaui-formatted-entry.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_common_time_class_init (GdauiEntryCommonTimeClass * class);
static void gdaui_entry_common_time_init (GdauiEntryCommonTime * srv);
static void gdaui_entry_common_time_dispose (GObject *object);
static void gdaui_entry_common_time_finalize (GObject *object);

static void gdaui_entry_common_time_set_property (GObject *object,
						  guint param_id,
						  const GValue *value,
						  GParamSpec *pspec);
static void gdaui_entry_common_time_get_property (GObject *object,
						  guint param_id,
						  GValue *value,
						  GParamSpec *pspec);

/* properties */
enum
{
	PROP_0,
	PROP_EDITING_CANCELED,
	PROP_TYPE
};

/* GtkCellEditable interface */
static void gdaui_entry_common_time_cell_editable_init (GtkCellEditableIface *iface);
static void gdaui_entry_common_time_start_editing (GtkCellEditable *iface, GdkEvent *event);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


/* private structure */
struct _GdauiEntryCommonTimePrivate
{
	/* for date */
	GtkWidget *entry_date;
	GtkWidget *date;
        GtkWidget *window;
        GtkWidget *date_button;
	gboolean   editing_canceled;

	/* for time */
	GtkWidget *entry_time;

	/* for timestamp */
	GtkWidget *hbox;

	/* Last value set */
	GValue    *last_value_set;
};

static void
gdaui_entry_common_time_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_common_time_start_editing;
}

GType
gdaui_entry_common_time_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryCommonTimeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_common_time_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryCommonTime),
			0,
			(GInstanceInitFunc) gdaui_entry_common_time_init
		};

		static const GInterfaceInfo cell_editable_info = {
			(GInterfaceInitFunc) gdaui_entry_common_time_cell_editable_init,    /* interface_init */
			NULL,                                                 /* interface_finalize */
			NULL                                                  /* interface_data */
		};
	
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryCommonTime", &info, 0);
		g_type_add_interface_static (type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	return type;
}

static void
gdaui_entry_common_time_class_init (GdauiEntryCommonTimeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_common_time_dispose;
	object_class->finalize = gdaui_entry_common_time_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->can_expand = can_expand;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_common_time_set_property;
	object_class->get_property = gdaui_entry_common_time_get_property;

	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_uint ("type", NULL, NULL, 0, G_MAXUINT, GDA_TYPE_TIME, 
							    G_PARAM_WRITABLE | G_PARAM_READABLE));
}

static gboolean
key_press_event_cb (GdauiEntryCommonTime *mgtim, GdkEventKey *key_event, gpointer data)
{
	if (key_event->keyval == GDK_KEY_Escape)
		mgtim->priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_common_time_init (GdauiEntryCommonTime *gdaui_entry_common_time)
{
	gdaui_entry_common_time->priv = g_new0 (GdauiEntryCommonTimePrivate, 1);
	gdaui_entry_common_time->priv->entry_date = NULL;
	gdaui_entry_common_time->priv->entry_time = NULL;
	gdaui_entry_common_time->priv->date = NULL;
	gdaui_entry_common_time->priv->window = NULL;
	gdaui_entry_common_time->priv->date_button = NULL;
	gdaui_entry_common_time->priv->hbox = NULL;
	gdaui_entry_common_time->priv->last_value_set = NULL;
	gdaui_entry_common_time->priv->editing_canceled = FALSE;
	g_signal_connect (gdaui_entry_common_time, "key-press-event",
			  G_CALLBACK (key_press_event_cb), NULL);
}

/**
 * gdaui_entry_common_time_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_common_time_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryCommonTime *mgtim;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_COMMON_TIME, "handler", dh, NULL);
	mgtim = GDAUI_ENTRY_COMMON_TIME (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtim), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_common_time_dispose (GObject   * object)
{
	GdauiEntryCommonTime *gdaui_entry_common_time;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (object));

	gdaui_entry_common_time = GDAUI_ENTRY_COMMON_TIME (object);
	if (gdaui_entry_common_time->priv) {
		if (gdaui_entry_common_time->priv->window) {
			gtk_widget_destroy (gdaui_entry_common_time->priv->window);
			gdaui_entry_common_time->priv->window = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_common_time_finalize (GObject   * object)
{
	GdauiEntryCommonTime *gdaui_entry_common_time;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (object));

	gdaui_entry_common_time = GDAUI_ENTRY_COMMON_TIME (object);
	if (gdaui_entry_common_time->priv) {
		if (gdaui_entry_common_time->priv->last_value_set) 
			gda_value_free (gdaui_entry_common_time->priv->last_value_set);

		g_free (gdaui_entry_common_time->priv);
		gdaui_entry_common_time->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
gdaui_entry_common_time_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	GdauiEntryCommonTime *mgtim;

	mgtim = GDAUI_ENTRY_COMMON_TIME (object);
	if (mgtim->priv) {
		switch (param_id) {
		case PROP_TYPE:
			gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (object), g_value_get_uint (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_entry_common_time_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec)
{
	GdauiEntryCommonTime *mgtim;

	mgtim = GDAUI_ENTRY_COMMON_TIME (object);
	if (mgtim->priv) {
		switch (param_id) {
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, mgtim->priv->editing_canceled);
			break;
		case PROP_TYPE:
			g_value_set_uint (value, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (object)));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static GtkWidget *create_entry_date (GdauiEntryCommonTime *mgtim);
static GtkWidget *create_entry_time (GdauiEntryCommonTime *mgtim);
static GtkWidget *create_entry_ts (GdauiEntryCommonTime *mgtim);
static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryCommonTime *mgtim;
	GtkWidget *entry = NULL;
	GType type;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap), NULL);
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_val_if_fail (mgtim->priv, NULL);

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
	if (type == G_TYPE_DATE)
		entry = create_entry_date (mgtim);
	else if (type == GDA_TYPE_TIME)
		entry = create_entry_time (mgtim);
	else if (type == GDA_TYPE_TIMESTAMP)
		entry = create_entry_ts (mgtim);
	else
		g_assert_not_reached ();
		
	return entry;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryCommonTime *mgtim;
	GType type;
	GdaDataHandler *dh;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_if_fail (mgtim->priv);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));

	if (type == G_TYPE_DATE) {
		if (value) {
			if (gda_value_is_null ((GValue *) value))
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), NULL);
			else {
				gchar *str;
				
				str = gda_data_handler_get_str_from_value (dh, value);
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), str);
				g_free (str);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), NULL);
	}
	else if (type == GDA_TYPE_TIME) {
		if (value) {
			if (gda_value_is_null ((GValue *) value))
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), NULL);
			else {
				gchar *str;
				
				str = gda_data_handler_get_str_from_value (dh, value);
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), str);
				g_free (str);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), NULL);
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		if (value) {
			if (gda_value_is_null ((GValue *) value)) {
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), NULL);
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), NULL);
			}
			else {
				gchar *str, *ptr;
				
				str = gda_data_handler_get_str_from_value (dh, value);
				ptr = strtok (str, " ");
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), ptr);
				ptr = strtok (NULL, " ");
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), ptr);
				g_free (str);
			}
		}
		else {
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), NULL);
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), NULL);
		}
	}
	else
		g_assert_not_reached ();

	/* keep track of the last value set */
	if (mgtim->priv->last_value_set) {
		gda_value_free (mgtim->priv->last_value_set);
		mgtim->priv->last_value_set = NULL;
	}
	if (value)
		mgtim->priv->last_value_set = gda_value_copy ((GValue *) value);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value = NULL;
	GdauiEntryCommonTime *mgtim;
	GdaDataHandler *dh;
	gchar *str2;
	GType type;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap), NULL);
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_val_if_fail (mgtim->priv, NULL);

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	if (type == G_TYPE_DATE) {
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry_date));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry_time));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}

		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL) &&
		    mgtim->priv->last_value_set && 
		    gda_value_isa (mgtim->priv->last_value_set, GDA_TYPE_TIME)) {
			/* copy the 'timezone' part, we we have not modified */
			const GdaTime *dgatime_set = gda_value_get_time (mgtim->priv->last_value_set);
			GdaTime *gdatime = g_new (GdaTime, 1);
			*gdatime = *(gda_value_get_time (value));
			gdatime->timezone = dgatime_set->timezone;
			gda_value_set_time (value, gdatime);
			g_free (gdatime);
		}
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		gchar *tmpstr, *tmpstr2;

		tmpstr = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry_time));
		tmpstr2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry_date));
		if (tmpstr && tmpstr2) {
			str2 = g_strdup_printf ("%s %s", tmpstr2, tmpstr);
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}
		g_free (tmpstr);
		g_free (tmpstr2);
		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL) &&
		    mgtim->priv->last_value_set && 
		    gda_value_isa (mgtim->priv->last_value_set, GDA_TYPE_TIMESTAMP)) {
			/* copy the 'fraction' and 'timezone' parts, we have not modified */
			const GdaTimestamp *dgatime_set = gda_value_get_timestamp (mgtim->priv->last_value_set);
			GdaTimestamp *gdatime = g_new (GdaTimestamp, 1);
			*gdatime = *(gda_value_get_timestamp (value));
			gdatime->fraction = dgatime_set->fraction;
			gdatime->timezone = dgatime_set->timezone;
			gda_value_set_timestamp (value, gdatime);
			g_free (gdatime);
		}
	}
	else
		g_assert_not_reached ();
	
	if (!value) {
		/* in case the gda_data_handler_get_value_from_str() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

typedef void (*Callback2) (gpointer, gpointer);
static gboolean
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryCommonTime *mgtim)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgtim);

	return FALSE;
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryCommonTime *mgtim;
	GType type;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_if_fail (mgtim->priv);

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));

	if ((type == G_TYPE_DATE) || (type == GDA_TYPE_TIMESTAMP)) {
		g_object_set_data (G_OBJECT (mgtim->priv->entry_date), "_activate_cb", activate_cb);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_date), "changed",
				  modify_cb, mgwrap);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_date), "activate",
				  activate_cb, mgwrap);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_date), "focus-out-event",
				  G_CALLBACK (focus_out_cb), mgtim);
	}

	if ((type == GDA_TYPE_TIME) || (type == GDA_TYPE_TIMESTAMP)) {
		g_object_set_data (G_OBJECT (mgtim->priv->entry_time), "_activate_cb", activate_cb);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_time), "changed",
				  modify_cb, mgwrap);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_time), "activate",
				  activate_cb, mgwrap);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_time), "focus-out-event",
				  G_CALLBACK (focus_out_cb), mgtim);
	}
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	return FALSE;
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_if_fail (mgtim->priv);

	if (mgtim->priv->entry_date)
		gtk_editable_set_editable (GTK_EDITABLE (mgtim->priv->entry_date), editable);
	if (mgtim->priv->entry_time)
		gtk_editable_set_editable (GTK_EDITABLE (mgtim->priv->entry_time), editable);
	if (mgtim->priv->date_button)
		gtk_widget_set_sensitive (mgtim->priv->date_button, editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_if_fail (mgtim->priv);

	if (mgtim->priv->entry_date)
		gtk_widget_grab_focus (mgtim->priv->entry_date);
	if (mgtim->priv->entry_time)
		gtk_widget_grab_focus (mgtim->priv->entry_time);
}



/*
 * callbacks for the date 
 */
static void internal_set_time (GtkWidget *widget, GdauiEntryCommonTime *mgtim);
static gint date_delete_popup (GtkWidget *widget, GdauiEntryCommonTime *mgtim);
static gint date_key_press_popup (GtkWidget *widget, GdkEventKey *event, GdauiEntryCommonTime *mgtim);
static gint date_button_press_popup (GtkWidget *widget, GdkEventButton *event, GdauiEntryCommonTime *mgtim);
static void date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);
static void date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);
static void date_calendar_choose_cb (GtkWidget *button, GdauiEntryCommonTime *mgtim);

static void entry_date_insert_func (GdauiFormattedEntry *fentry, gunichar insert_char, gint virt_pos, gpointer data);

static GtkWidget *
create_entry_date (GdauiEntryCommonTime *mgtim)
{
	GtkWidget *wid, *hb, *window, *arrow;
	GdaDataHandler *dh;

	/* top widget */
	hb = gtk_hbox_new (FALSE, 3);

	/* text entry */
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
	if (GDA_IS_HANDLER_TIME (dh)) {
		gchar *str, *mask, *ptr;
		str = gda_handler_time_get_format (GDA_HANDLER_TIME (dh), G_TYPE_DATE);
		mask = g_strdup (str);
		for (ptr = mask; *ptr; ptr++) {
			if (*ptr == '0')
				*ptr = '-';
		}
		wid = gdaui_formatted_entry_new (str, mask);
		g_free (str);
		g_free (mask);

		gdaui_formatted_entry_set_insert_func (GDAUI_FORMATTED_ENTRY (wid), entry_date_insert_func,
						       mgtim);
	}
	else
		wid = gdaui_entry_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (hb), wid, FALSE, FALSE, 0);
	gtk_widget_show (wid);
	mgtim->priv->entry_date = wid;
	g_signal_connect (G_OBJECT (wid), "changed",
			  G_CALLBACK (internal_set_time), mgtim);
	
	/* window to hold the calendar popup */
	window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_events (window, gtk_widget_get_events (window) | GDK_KEY_PRESS_MASK);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	g_signal_connect (G_OBJECT (window), "delete-event",
			  G_CALLBACK (date_delete_popup), mgtim);
	g_signal_connect (G_OBJECT (window), "key-press-event",
			  G_CALLBACK (date_key_press_popup), mgtim);
	g_signal_connect (G_OBJECT (window), "button-press-event",
			  G_CALLBACK (date_button_press_popup), mgtim);
	mgtim->priv->window = window;
	
	/* calendar */
	wid = gtk_calendar_new ();
	mgtim->priv->date = wid;
	gtk_container_add (GTK_CONTAINER (window), wid);
	gtk_widget_show (wid);
	g_signal_connect (G_OBJECT (wid), "day-selected",
			  G_CALLBACK (date_day_selected), mgtim);
	g_signal_connect (G_OBJECT (wid), "day-selected-double-click",
			  G_CALLBACK (date_day_selected_double_click), mgtim);
	
	/* button to pop up the calendar */
	wid = gtk_button_new ();
	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (wid), arrow);
	gtk_box_pack_start (GTK_BOX (hb), wid, FALSE, FALSE, 0);
	gtk_widget_show_all (wid);
	g_signal_connect (G_OBJECT (wid), "clicked",
			  G_CALLBACK (date_calendar_choose_cb), mgtim);
	mgtim->priv->date_button = wid;

	/* padding */
	wid = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hb), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);

	return hb;
}

static void
entry_date_insert_func (GdauiFormattedEntry *fentry, gunichar insert_char, gint virt_pos, gpointer data)
{
	GValue *value;
	GType type;

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (data));
	value = real_get_value (GDAUI_ENTRY_WRAPPER (data));
	if (!value)
		return;

	if (G_VALUE_TYPE (value) == GDA_TYPE_NULL) {
		if (type == G_TYPE_DATE) {
			/* set current date, whatever @insert_char is */
			GDate *ndate;
			ndate = g_new0 (GDate, 1);
			g_date_set_time_t (ndate, time (NULL));

			gda_value_reset_with_type (value, type);
			g_value_take_boxed (value, ndate);
			real_set_value (GDAUI_ENTRY_WRAPPER (data), value);
		}
		else if (type == GDA_TYPE_TIMESTAMP) {
			GValue *tsvalue;
			tsvalue = gda_value_new_timestamp_from_timet (time (NULL));
			real_set_value (GDAUI_ENTRY_WRAPPER (data), tsvalue);
			gda_value_free (tsvalue);
		}
	}
	else {
		GDate *date = NULL;
		if (type == G_TYPE_DATE) {
			date = (GDate*) g_value_get_boxed (value);
		}
		else if (type == GDA_TYPE_TIMESTAMP) {
			const GdaTimestamp *ts;
			ts = gda_value_get_timestamp (value);
			date = g_date_new_dmy (ts->day, ts->month, ts->year);
		}

		if (date) {
			GDate *ndate;
			gboolean changed = FALSE;

			ndate = g_new (GDate, 1);
			*ndate = *date;
			if ((insert_char == g_utf8_get_char ("+")) ||
			    (insert_char == g_utf8_get_char ("="))) {
				g_date_add_days (ndate, 1);
				changed = TRUE;
			}
			else if ((insert_char == g_utf8_get_char ("-")) ||
				 (insert_char == g_utf8_get_char ("6"))) {
				g_date_subtract_days (ndate, 1);
				changed = TRUE;
			}

			if (changed) {
				if (type == G_TYPE_DATE) {
					g_value_take_boxed (value, ndate);
				}
				else if (type == GDA_TYPE_TIMESTAMP) {
					GdaTimestamp *ts;
					ts = (GdaTimestamp*) gda_timestamp_copy ((gpointer) gda_value_get_timestamp (value));
					ts->day = g_date_get_day (ndate);
					ts->month = g_date_get_month (ndate);
					ts->year = g_date_get_year (ndate);
					g_date_free (date);
					g_date_free (ndate);
					gda_value_set_timestamp (value, ts);
					gda_timestamp_free (ts);
				}
				real_set_value (GDAUI_ENTRY_WRAPPER (data), value);
			}
		}
	}
	gda_value_free (value);
}

static void
internal_set_time (GtkWidget *widget, GdauiEntryCommonTime *mgtim)
{
	/* the aim is that when the mode is TIMESTAMP, when the user sets a date,
	 * then the time automatically sets to 00:00:00
	 */

	GType type;

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
	if (type == GDA_TYPE_TIMESTAMP) {
		GdaDataHandler *dh;
		gchar *str, *str2;
		GValue *value;

		dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
		str = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry_time));
		str2 = g_strdup_printf ("01.01.2000 %s", str);
		g_free (str);
		value = gda_data_handler_get_value_from_str (dh, str2, type);
		if (!value || gda_value_is_null (value)) 
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_time), "00:00:00");
		if (value)
			gda_value_free (value);
		g_free (str2);
	}
}

static void
hide_popup (GdauiEntryCommonTime *mgtim)
{
        gtk_widget_hide (mgtim->priv->window);
        gtk_grab_remove (mgtim->priv->window);
}

static gint
date_delete_popup (GtkWidget *widget, GdauiEntryCommonTime *mgtim)
{
	hide_popup (mgtim);
	return TRUE;
}

static gint
date_key_press_popup (GtkWidget *widget, GdkEventKey *event, GdauiEntryCommonTime *mgtim)
{
	if (event->keyval != GDK_KEY_Escape)
                return FALSE;

        g_signal_stop_emission_by_name (widget, "key-press-event");
        hide_popup (mgtim);

        return TRUE;
}

static gint
date_button_press_popup (GtkWidget *widget, GdkEventButton *event, GdauiEntryCommonTime *mgtim)
{
	GtkWidget *child;

        child = gtk_get_event_widget ((GdkEvent *) event);

        /* We don't ask for button press events on the grab widget, so
         *  if an event is reported directly to the grab widget, it must
         *  be on a window outside the application (and thus we remove
         *  the popup window). Otherwise, we check if the widget is a child
         *  of the grab widget, and only remove the popup window if it
         *  is not.
         */
        if (child != widget) {
                while (child) {
                        if (child == widget)
                                return FALSE;
                        child = gtk_widget_get_parent (child);
                }
        }

        hide_popup (mgtim);

        return TRUE;
}

static void
date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
	char buffer [256];
        guint year, month, day;
        struct tm mtm = {0};
        char *str_utf8;

        gtk_calendar_get_date (calendar, &year, &month, &day);

        mtm.tm_mday = day;
        mtm.tm_mon = month;
        if (year > 1900)
                mtm.tm_year = year - 1900;
        else
                mtm.tm_year = year;

        if (strftime (buffer, sizeof (buffer), "%x", &mtm) == 0)
                strcpy (buffer, "???");
        buffer[sizeof(buffer)-1] = '\0';

        str_utf8 = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
        gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry_date), str_utf8);
        g_free (str_utf8);
}

static void
date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
	hide_popup (mgtim);
}


static gboolean popup_grab_on_window (GdkWindow *window, guint32 activate_time);
static void position_popup (GdauiEntryCommonTime *mgtim);
static void
date_calendar_choose_cb (GtkWidget *button, GdauiEntryCommonTime *mgtim)
{
	GValue *value;
        guint year=0, month=0, day=0;
	gboolean unset = TRUE;
	GdaDataHandler *dh;

        /* setting the calendar to the latest displayed date */
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (mgtim));
	
        if (value && !gda_value_is_null (value)) {
		const GDate *date;
		const GdaTimestamp *ts;
		GType type;

		type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
		if (type == G_TYPE_DATE) {
			date = (GDate*) g_value_get_boxed (value);
			if (date) {
				month = g_date_get_month (date);
				year = g_date_get_year (date);
				day = g_date_get_day (date);
				if ((month != G_DATE_BAD_MONTH) && 
				    (day != G_DATE_BAD_DAY) &&
				    (year != G_DATE_BAD_YEAR)) {
					month -= 1;
					unset = FALSE;
				}
			}
		}
		else if (type == GDA_TYPE_TIMESTAMP) {
			ts = gda_value_get_timestamp (value);
			if (ts) {
				year = ts->year;
				month = ts->month - 1;
				day = ts->day;
				unset = FALSE;
			}
		}
		else
			g_assert_not_reached ();
        }

	if (unset) {
                time_t now;
                struct tm *stm;

                now = time (NULL);
                stm = localtime (&now);
                year = stm->tm_year + 1900;
                month = stm->tm_mon;
                day = stm->tm_mday;
        }

        gtk_calendar_select_month (GTK_CALENDAR (mgtim->priv->date), month, year);
        gtk_calendar_select_day (GTK_CALENDAR (mgtim->priv->date), day);

        /* popup window */
        /* Temporarily grab pointer and keyboard, copied from GnomeDateEdit */
        if (!popup_grab_on_window (gtk_widget_get_window (button), gtk_get_current_event_time ()))
                return;

        position_popup (mgtim);
        gtk_grab_add (mgtim->priv->window);
        gtk_widget_show (mgtim->priv->window);

	GdkScreen *screen;
	gint swidth, sheight;
	gint root_x, root_y;
	gint wwidth, wheight;
	gboolean do_move = FALSE;
	screen = gtk_window_get_screen (GTK_WINDOW (mgtim->priv->window));
	if (screen) {
		swidth = gdk_screen_get_width (screen);
		sheight = gdk_screen_get_height (screen);
	}
	else {
		swidth = gdk_screen_width ();
		sheight = gdk_screen_height ();
	}
	gtk_window_get_position (GTK_WINDOW (mgtim->priv->window), &root_x, &root_y);
	gtk_window_get_size (GTK_WINDOW (mgtim->priv->window), &wwidth, &wheight);
	if (root_x + wwidth > swidth) {
		do_move = TRUE;
		root_x = swidth - wwidth;
	}
	else if (root_x < 0) {
		do_move = TRUE;
		root_x = 0;
	}
	if (root_y + wheight > sheight) {
		do_move = TRUE;
		root_y = sheight - wheight;
	}
	else if (root_y < 0) {
		do_move = TRUE;
		root_y = 0;
	}
	if (do_move)
		gtk_window_move (GTK_WINDOW (mgtim->priv->window), root_x, root_y);

        gtk_widget_grab_focus (mgtim->priv->date);
        popup_grab_on_window (gtk_widget_get_window (mgtim->priv->window),
                              gtk_get_current_event_time ());
}

static gboolean
popup_grab_on_window (GdkWindow *window, guint32 activate_time)
{
        if ((gdk_pointer_grab (window, TRUE,
                               GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                               GDK_POINTER_MOTION_MASK,
                               NULL, NULL, activate_time) == 0)) {
                if (gdk_keyboard_grab (window, TRUE,
                                       activate_time) == 0)
                        return TRUE;
                else {
                        gdk_pointer_ungrab (activate_time);
                        return FALSE;
                }
        }
        return FALSE;
}

static void
position_popup (GdauiEntryCommonTime *mgtim)
{
        gint x, y;
        gint bwidth, bheight;
        GtkRequisition req;

        gtk_widget_size_request (mgtim->priv->window, &req);

        gdk_window_get_origin (gtk_widget_get_window (mgtim->priv->date_button), &x, &y);
	GtkAllocation alloc;
	gtk_widget_get_allocation (mgtim->priv->date_button, &alloc);
        x += alloc.x;
        y += alloc.y;
        bwidth = alloc.width;
        bheight = alloc.height;

        x += bwidth - req.width;
        y += bheight;

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

        gtk_window_move (GTK_WINDOW (mgtim->priv->window), x, y);
}




/*
 * callbacks for the time
 */
static void entry_time_insert_func (GdauiFormattedEntry *fentry, gunichar insert_char, gint virt_pos, gpointer data);

static GtkWidget *
create_entry_time (GdauiEntryCommonTime *mgtim)
{
	GtkWidget *wid;
	GdaDataHandler *dh;

	/* text entry */
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
	if (GDA_IS_HANDLER_TIME (dh)) {
		gchar *str, *mask, *ptr;
		str = gda_handler_time_get_format (GDA_HANDLER_TIME (dh), GDA_TYPE_TIME);
		mask = g_strdup (str);
		for (ptr = mask; *ptr; ptr++) {
			if (*ptr == '0')
				*ptr = '-';
		}
		wid = gdaui_formatted_entry_new (str, mask);
		g_free (str);
		g_free (mask);

		gdaui_formatted_entry_set_insert_func (GDAUI_FORMATTED_ENTRY (wid), entry_time_insert_func,
						       mgtim);
	}
	else
		wid = gdaui_entry_new (NULL, NULL);
        mgtim->priv->entry_time = wid;

        /* format tooltip */
	gtk_widget_set_tooltip_text (wid, _("Format is hh:mm:ss"));

        return wid;
}

static void
entry_time_insert_func (GdauiFormattedEntry *fentry, gunichar insert_char, gint virt_pos, gpointer data)
{
	GValue *value;
	GType type;

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (data));
	value = real_get_value (GDAUI_ENTRY_WRAPPER (data));
	if (!value)
		return;

	if ((type == GDA_TYPE_TIME) && 
	    (insert_char == g_utf8_get_char (" "))) {
		/* set current time */
		gda_value_reset_with_type (value, type);
		struct tm *ltm;
		time_t val;
		
		val = time (NULL);
		ltm = localtime ((const time_t *) &val);
		if (ltm) {
			GdaTime tim;
			memset (&tim, 0, sizeof (GdaTime));
			tim.hour = ltm->tm_hour;
			tim.minute = ltm->tm_min;
			tim.second = ltm->tm_sec;
			tim.fraction = 0;
			tim.timezone = GDA_TIMEZONE_INVALID;
			gda_value_set_time (value, (const GdaTime *) &tim);
			real_set_value (GDAUI_ENTRY_WRAPPER (data), value);
		}
	}
	
	gda_value_free (value);
}

/*
 * callbacks for the timestamp
 */
static GtkWidget *
create_entry_ts (GdauiEntryCommonTime *mgtim)
{
	GtkWidget *hb, *wid;

	hb = gtk_hbox_new (FALSE, 0);
	
	/* date part */
	wid = create_entry_date (mgtim);
	gtk_box_pack_start (GTK_BOX (hb), wid, FALSE, FALSE, 0);
	gtk_widget_show (wid);

	/* time part */
	wid = create_entry_time (mgtim);
	gtk_box_pack_start (GTK_BOX (hb), wid, FALSE, FALSE, 0);
	gtk_widget_show (wid);

	mgtim->priv->hbox = hb;
	
	return hb;
}



/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (GtkEntry *entry, GdauiEntryCommonTime *mgtim) 
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgtim));
}

static void
gtk_cell_editable_entry_remove_widget_cb (GtkEntry *entry, GdauiEntryCommonTime *mgtim) 
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgtim));
}

static void
gdaui_entry_common_time_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (iface));
	mgtim = GDAUI_ENTRY_COMMON_TIME (iface);
	g_return_if_fail (mgtim->priv);

	mgtim->priv->editing_canceled = FALSE;
	if (mgtim->priv->date_button) {
		gtk_widget_destroy (mgtim->priv->date_button);
		mgtim->priv->date_button = NULL;
	}

	if (mgtim->priv->hbox) {
		gtk_box_set_spacing (GTK_BOX (mgtim->priv->hbox), 0);
		gtk_container_set_border_width (GTK_CONTAINER (mgtim->priv->hbox), 0);
	}

	if (mgtim->priv->entry_date) {
		g_object_set (G_OBJECT (mgtim->priv->entry_date), "has-frame", FALSE, NULL);
		gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (mgtim->priv->entry_date), event);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_date), "editing-done",
				  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgtim);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_date), "remove-widget",
				  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgtim);
	}

	if (mgtim->priv->entry_time) {
		g_object_set (G_OBJECT (mgtim->priv->entry_time), "has-frame", FALSE, NULL);
		gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (mgtim->priv->entry_time), event);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_time), "editing-done",
				  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgtim);
		g_signal_connect (G_OBJECT (mgtim->priv->entry_time), "remove-widget",
				  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgtim);
	}

	gdaui_entry_shell_refresh (GDAUI_ENTRY_SHELL (mgtim));

	if (mgtim->priv->entry_date)
		gtk_widget_grab_focus (mgtim->priv->entry_date);
	else
		gtk_widget_grab_focus (mgtim->priv->entry_time);
	gtk_widget_queue_draw (GTK_WIDGET (mgtim));
}
