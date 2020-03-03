/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2017, 2018 Daniel Espinosa <esodan@gmail.com>
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
#include "gdaui-entry-common-time.h"
#include <libgda/gda-data-handler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include "gdaui-formatted-entry.h"
#include "gdaui-numeric-entry.h"
#include <libgda/gda-debug-macros.h>

/*
 * REM:
 * Times are displayed relative to localtime
 */

/*
 * Main static functions 
 */
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
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* private structure */
typedef struct
{
	GtkWidget *entry;
	GtkWidget *cal_popover;
	GtkWidget *calendar;
	glong     displayed_tz;
	glong     value_tz;
	gulong     value_fraction;

	gboolean   editing_canceled;
} GdauiEntryCommonTimePrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiEntryCommonTime, gdaui_entry_common_time, GDAUI_TYPE_ENTRY_WRAPPER,
                         G_ADD_PRIVATE (GdauiEntryCommonTime)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, gdaui_entry_common_time_cell_editable_init))

static void
gdaui_entry_common_time_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = gdaui_entry_common_time_start_editing;
}

static void
gdaui_entry_common_time_class_init (GdauiEntryCommonTimeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_common_time_set_property;
	object_class->get_property = gdaui_entry_common_time_get_property;

	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_gtype ("type", "GType", "Entry GType handled", G_TYPE_NONE,
							    G_PARAM_WRITABLE | G_PARAM_READABLE));
}

static gboolean
key_press_event_cb (GdauiEntryCommonTime *mgtim, GdkEventKey *key_event, G_GNUC_UNUSED gpointer data)
{
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	if (key_event->keyval == GDK_KEY_Escape)
		priv->editing_canceled = TRUE;
	return FALSE;
}

static void
gdaui_entry_common_time_init (GdauiEntryCommonTime *mgtim)
{
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	priv->entry = NULL;
	priv->calendar = NULL;
	priv->editing_canceled = FALSE;
	priv->value_tz = 0; /* safe init value */
	priv->value_fraction = 0; /* safe init value */
	GDateTime *dt = g_date_time_new_now_local ();
	priv->displayed_tz = (glong) g_date_time_get_utc_offset (dt);
	g_date_time_unref (dt);
	g_signal_connect (mgtim, "key-press-event",
			  G_CALLBACK (key_press_event_cb), NULL);
}

/**
 * gdaui_entry_common_time_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_common_time_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryCommonTime *mgtim;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_COMMON_TIME, "handler", dh, NULL);
	mgtim = GDAUI_ENTRY_COMMON_TIME (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtim), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_common_time_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	/* GdauiEntryCommonTime *mgtim; */

	/* mgtim = GDAUI_ENTRY_COMMON_TIME (object); */
	/* GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim); */
	switch (param_id) {
		case PROP_TYPE:
			gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (object), g_value_get_gtype (value));
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
gdaui_entry_common_time_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec)
{
	GdauiEntryCommonTime *mgtim;

	mgtim = GDAUI_ENTRY_COMMON_TIME (object);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	switch (param_id) {
		case PROP_EDITING_CANCELED:
			g_value_set_boolean (value, priv->editing_canceled);
			break;
		case PROP_TYPE:
			g_value_set_gtype (value, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (object)));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);
static void date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);

static void
icon_press_cb (GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, GdauiEntryCommonTime *mgtim)
{
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	if (icon_pos == GTK_ENTRY_ICON_PRIMARY) {
		if (! priv->cal_popover) {
			/* calendar */
			GtkWidget *wid;
			wid = gtk_calendar_new ();
			priv->calendar = wid;
			gtk_widget_show (wid);
			g_signal_connect (G_OBJECT (wid), "day-selected",
					  G_CALLBACK (date_day_selected), mgtim);
			g_signal_connect (G_OBJECT (wid), "day-selected-double-click",
					  G_CALLBACK (date_day_selected_double_click), mgtim);

			/* popover */
			GtkWidget *popover;
			popover = gtk_popover_new (priv->entry);
			gtk_container_add (GTK_CONTAINER (popover), priv->calendar);
			priv->cal_popover = popover;
		}

		/* set calendar to current value */
		GValue *value;
		guint year=0, month=0, day=0;
		gboolean unset = TRUE;

		/* setting the calendar to the displayed date */
		value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (mgtim));

		if (value && !gda_value_is_null (value)) {
			GType type;
			type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
			if (type == G_TYPE_DATE) {
				const GDate *date;
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
			else if (type == G_TYPE_DATE_TIME) {
				GDateTime *ts;
				ts = g_value_get_boxed (value);
				if (ts) {
					year = g_date_time_get_year (ts);
					month = g_date_time_get_month (ts) - 1;
					day = g_date_time_get_day_of_month (ts);
					unset = FALSE;
				}
			}
			else
				g_assert_not_reached ();
		}

		if (unset) {
			GDateTime *dt = g_date_time_new_now_local ();
			year = g_date_time_get_year (dt);
			month = g_date_time_get_month (dt);
			day = g_date_time_get_day_of_month (dt);
			g_date_time_unref (dt);
		}

		if (! unset) {
			g_signal_handlers_block_by_func (G_OBJECT (priv->calendar),
							 G_CALLBACK (date_day_selected), mgtim);
			g_signal_handlers_block_by_func (G_OBJECT (priv->calendar),
							 G_CALLBACK (date_day_selected_double_click), mgtim);
		}
		gtk_calendar_select_month (GTK_CALENDAR (priv->calendar), month, year);
		gtk_calendar_select_day (GTK_CALENDAR (priv->calendar), day);
		if (! unset) {
			g_signal_handlers_unblock_by_func (G_OBJECT (priv->calendar),
							   G_CALLBACK (date_day_selected), mgtim);
			g_signal_handlers_unblock_by_func (G_OBJECT (priv->calendar),
							   G_CALLBACK (date_day_selected_double_click), mgtim);
		}

		gtk_widget_show (priv->cal_popover);
	}
}

static void
date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	char buffer [256];
        guint year, month, day;
        struct tm mtm = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	GType type;
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));

        gtk_calendar_get_date (calendar, &year, &month, &day);

        mtm.tm_mday = day;
        mtm.tm_mon = month;
        if (year > 1900)
                mtm.tm_year = year - 1900;
        else
                mtm.tm_year = year;

	if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		/* get the time part back from current value */
		GValue *value = NULL;
		gchar *tmpstr;
		GdaDataHandler *dh;
		dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
		tmpstr = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (priv->entry));
		if (tmpstr) {
			value = gda_data_handler_get_value_from_str (dh, tmpstr, type);
			g_free (tmpstr);
		}

		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* copy the 'fraction' and 'timezone' parts, we have not modified */
			GDateTime *ets = g_value_get_boxed (value);
			mtm.tm_hour = g_date_time_get_hour (ets);
			mtm.tm_min = g_date_time_get_minute (ets);
			mtm.tm_sec = g_date_time_get_second (ets);
		}
		gda_value_free (value);
	}

	guint bufsize;
	bufsize = sizeof (buffer) / sizeof (char);
	if (strftime (buffer, bufsize, "%x", &mtm) == 0)
		buffer [0] = 0;
	else if (type == G_TYPE_DATE_TIME) {
		char buffer2 [128];
		if (strftime (buffer2, bufsize, "%X", &mtm) == 0)
			buffer [0] = 0;
		else {
			gchar *tmp;
			tmp = g_strdup_printf ("%s %s", buffer, buffer2);
			if (strlen (tmp) < bufsize)
				strcpy (buffer, tmp);
			else
				buffer [0] = 0;
			g_free (tmp);
		}
	}

	g_print ("BUFFER:[%s]\n", buffer);

	if (buffer [0]) {
		char *str_utf8;
		str_utf8 = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), str_utf8);
		g_free (str_utf8);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), "");
}

static void
date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
	gtk_widget_hide (priv->cal_popover);
}

static void entry_insert_func (GdauiFormattedEntry *fentry, gunichar insert_char, gint virt_pos, gpointer data);

static void
event_after_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_signal_emit_by_name (widget, "event-after", event);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryCommonTime *mgtim;
	g_return_val_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap), NULL);
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	GtkWidget *wid, *hb;
	GdaDataHandler *dh;
	GType type;
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
	
	/* top widget */
	hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);

	/* text entry */
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgtim));
	if (GDA_IS_HANDLER_TIME (dh)) {
		gchar *str, *mask, *ptr;
		str = gda_handler_time_get_format (GDA_HANDLER_TIME (dh), type);
		mask = g_strdup (str);
		for (ptr = mask; *ptr; ptr++) {
			if (*ptr == '0')
				*ptr = '-';
		}
		wid = gdaui_formatted_entry_new (str, mask);
		gdaui_entry_set_suffix (GDAUI_ENTRY (wid), "");
		g_free (str);
		g_free (mask);

		gchar *tmp;
		gulong tz;
		tz = priv->displayed_tz;
		if (tz == 0)
			tmp = g_strdup ("GMT");
		else if ((tz % 3600) == 0)
			tmp = g_strdup_printf ("GMT %+03d", (gint) tz / 3600);
		else if ((tz % 60) == 0)
			tmp = g_strdup_printf ("GMT %+02d min", (gint) tz / 60);
		else
			tmp = g_strdup_printf ("GMT %+02d sec", (gint) tz);

		gchar *hint;
		gdaui_formatted_entry_set_insert_func (GDAUI_FORMATTED_ENTRY (wid), entry_insert_func, mgtim);
		str = gda_handler_time_get_hint (GDA_HANDLER_TIME (dh), type);
		hint = g_strdup_printf (_("Expected format is %s\nTime is relative to local time (%s)"), str, tmp);
		g_free (str);
		g_free (tmp);
		gtk_widget_set_tooltip_text (wid, hint);
		g_free (hint);
	}
	else
		wid = gdaui_entry_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (hb), wid, FALSE, FALSE, 0);
	gtk_widget_show (wid);
	priv->entry = wid;
	g_signal_connect_swapped (wid, "event-after",
				  G_CALLBACK (event_after_cb), hb);

	if ((type == G_TYPE_DATE) || (type == G_TYPE_DATE_TIME))
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY (wid),
						   GTK_ENTRY_ICON_PRIMARY, "x-office-calendar-symbolic");

	g_signal_connect (wid, "icon-press",
			  G_CALLBACK (icon_press_cb), mgtim);

	return hb;
}

/*
 * NB: the displayed value is relative to priv->displayed_tz
 */
static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryCommonTime *mgtim;
	GType type;
	GdaDataHandler *dh;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));

	if (type == G_TYPE_DATE) {
		if (value) {
			if (gda_value_is_null ((GValue *) value))
				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
			else {
				gchar *str;
				
				str = gda_data_handler_get_str_from_value (dh, value);
				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), str);
				g_free (str);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
	}
	else if (type == GDA_TYPE_TIME) {
		if (value) {
			if (gda_value_is_null ((GValue *) value)) {
				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
				priv->value_tz = priv->displayed_tz;
				priv->value_fraction = 0;
			}
			else {
				const GdaTime *gtim;
				GdaTime* copy;
				GTimeZone *tz;

                gtim = gda_value_get_time (value);
				priv->value_tz = gda_time_get_timezone (gtim);
				priv->value_fraction = gda_time_get_fraction (gtim);

                tz = g_time_zone_new_offset (priv->displayed_tz);

                GdaTime *gtime_copy = gda_time_copy (gtim);
				copy = gda_time_to_timezone (gtime_copy, tz);
                gda_time_free (gtime_copy);

				GValue *copy_value;
				copy_value = g_new0 (GValue, 1);
				gda_value_set_time (copy_value, copy);

				gchar *str;
				str = gda_data_handler_get_str_from_value (dh, copy_value);
				gda_value_free (copy_value);

				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), str);
				g_free (str);
				gda_time_free (copy);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
	}
	else if (type == G_TYPE_DATE_TIME) {
		if (value) {
			if (gda_value_is_null ((GValue *) value)) {
				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
				priv->value_tz = priv->displayed_tz;
				priv->value_fraction = 0;
			}
			else {
				GDateTime *gts;
				GDateTime *copy;
				gts = g_value_get_boxed (value);
				priv->value_tz = g_date_time_get_utc_offset (gts);
				priv->value_fraction = (glong) ((g_date_time_get_seconds (gts) - g_date_time_get_second (gts)) * 1000000);

				gint itz = priv->displayed_tz;
				if (itz < 0)
					itz *= -1;
				gint h = itz/60/60;
				gint m = itz/60 - h*60;
				gint s = itz - h*60*60 - m*60;
				gchar *stz = g_strdup_printf ("%s%02d:%02d:%02d",
																			priv->displayed_tz < 0 ? "-" : "+",
																			h, m, s);
				GTimeZone *tz = g_time_zone_new (stz);
				g_free (stz);
				copy = g_date_time_to_timezone (gts, tz);

				GValue *copy_value;
				copy_value = gda_value_new (G_TYPE_DATE_TIME);
				g_value_set_boxed (copy_value, copy);

				gchar *str;
				str = gda_data_handler_get_str_from_value (dh, copy_value);
				gda_value_free (copy_value);

				gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), str);
				g_free (str);
				g_date_time_unref (copy);
			}
		}
	} else {
		g_warning (_("Can't set value to entry: Invalid Data Type for Entry Wrapper Time"));
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
	}
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
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));
	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));

	if (type == G_TYPE_DATE) {
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (priv->entry));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (priv->entry));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}

        if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
            const GdaTime *gdatime;
            GTimeZone *tz;
            gdatime = gda_value_get_time (value);
            tz = g_time_zone_new_offset (priv->displayed_tz);

            GdaTime *gdatime_copy = gda_time_copy (gdatime);
            GdaTime *time_copy = gda_time_to_timezone (gdatime_copy, tz);
            gda_time_free (gdatime_copy);
            g_time_zone_unref (tz);

            tz = g_time_zone_new_offset (priv->value_tz);
            GdaTime *tnz = gda_time_to_timezone (time_copy, tz);
            g_time_zone_unref (tz);
            gda_value_set_time (value, tnz);
            gda_time_free (time_copy);
            gda_time_free (tnz);
        }
	}
	else if (type == G_TYPE_DATE_TIME) {
		gchar *tmpstr;

		tmpstr = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (priv->entry));
		if (tmpstr) {
			value = gda_data_handler_get_value_from_str (dh, tmpstr, type);
			g_free (tmpstr);
		}

		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* copy the 'fraction' part, we have not modified */
			GDateTime *gdatime = NULL;
			GDateTime *copy;
			g_value_take_boxed (value, gdatime);
			gint tzi = priv->displayed_tz; // FIXME: This is always positive
			if (tzi < 0)
				tzi *= -1;
			gint h = tzi/60/60;
			gint m = tzi/60 - h*60;
			gint s = tzi - h*60*60 - m*60;
			gchar *stz = g_strdup_printf ("%s%02d:%02d:%02d",
																		priv->displayed_tz < 0 ? "-" : "+",
																		h, m, s);
			GTimeZone *tz = g_time_zone_new (stz);
			g_free (stz);
			gdouble seconds = g_date_time_get_second (gdatime) + priv->value_fraction / 1000000.0;
			copy = g_date_time_new (tz,
															g_date_time_get_year (gdatime),
															g_date_time_get_month (gdatime),
															g_date_time_get_day_of_month (gdatime),
															g_date_time_get_hour (gdatime),
															g_date_time_get_minute (gdatime),
															seconds);
			g_time_zone_unref (tz);
			g_date_time_unref (gdatime);
			g_value_set_boxed (value, copy);
			/* FIXME: Original code change timezone, but before set a displayed one, so inconsistency
			gda_timestamp_change_timezone (gdatime, priv->value_tz);
			 */
		}
	}
	else {
		g_warning (_("Can't get value from entry: Invalid Data Type for Entry Wrapper Time"));
		gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), NULL);
		value = gda_value_new (G_TYPE_DATE);
	}
	
	if (!value) {
		/* in case the gda_data_handler_get_value_from_str() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryCommonTime *mgtim;
	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	g_signal_connect_swapped (G_OBJECT (priv->entry), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (priv->entry), "activate",
				  activate_cb, mgwrap);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	if (priv->entry)
		gtk_editable_set_editable (GTK_EDITABLE (priv->entry), editable);

	gtk_entry_set_icon_sensitive (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_PRIMARY, editable);
	gtk_entry_set_icon_sensitive (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_SECONDARY, editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	gtk_widget_grab_focus (priv->entry);
}

static void
entry_insert_func (G_GNUC_UNUSED GdauiFormattedEntry *fentry, gunichar insert_char,
		   G_GNUC_UNUSED gint virt_pos, gpointer data)
{
	GValue *value;
	GType type;

	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (data));
	value = real_get_value (GDAUI_ENTRY_WRAPPER (data));
	if (!value)
		return;

	 /* current value is NULL and we are not entering a digit */
	if ((G_VALUE_TYPE (value) == GDA_TYPE_NULL) && (insert_char == g_utf8_get_char (" "))) {
		gda_value_reset_with_type (value, type);
		if (type == G_TYPE_DATE) {
			/* set current date */
			GDate *ndate;
			ndate = g_new0 (GDate, 1);
			g_date_set_time_t (ndate, time (NULL));

			g_value_take_boxed (value, ndate);
			real_set_value (GDAUI_ENTRY_WRAPPER (data), value);
		}
		else if (type == GDA_TYPE_TIME) {
			/* set current time */
			GValue *timvalue;
			timvalue = gda_value_new_time_from_timet (time (NULL));
			real_set_value (GDAUI_ENTRY_WRAPPER (data), timvalue);
			gda_value_free (timvalue);
		}
		else if (type == G_TYPE_DATE_TIME) {
			/* set current date and time */
			GValue *tsvalue;
			//gchar *str;
			//GdauiEntryCommonTime *mgtim = GDAUI_ENTRY_COMMON_TIME (data);
			// GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);
			//str = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (priv->entry));
			tsvalue = gda_value_new_date_time_from_timet (time (NULL));
			real_set_value (GDAUI_ENTRY_WRAPPER (data), tsvalue);
			gda_value_free (tsvalue);
			//if (str && g_ascii_isdigit (*str))
			//	gdaui_entry_set_text (GDAUI_ENTRY (priv->entry), str);
			//g_free (str);
		}
	}
	else if ((G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
		/* REM: if (type == GDA_TYPE_TIME) we have nothing to do  */
		GDate *date = NULL;
		if (type == G_TYPE_DATE) {
			date = (GDate*) g_value_get_boxed (value);
		}
		else if (type == G_TYPE_DATE_TIME) {
			GDateTime *ts;
			ts = g_value_get_boxed (value);
			date = g_date_new_dmy (g_date_time_get_day_of_month (ts),
														 g_date_time_get_month (ts),
														 g_date_time_get_year (ts));
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
				else if (type == G_TYPE_DATE_TIME) {
					GDateTime *dt = g_value_get_boxed (value);
					GDateTime *ts;
					GTimeZone *tz = g_time_zone_new (g_date_time_get_timezone_abbreviation (dt));
					ts = g_date_time_new (tz,
																g_date_get_year (ndate),
																g_date_get_month (ndate),
																g_date_get_day (ndate),
																g_date_time_get_hour (dt),
																g_date_time_get_minute (dt),
																g_date_time_get_seconds (dt));

					g_date_free (date);
					g_date_free (ndate);
					g_value_set_boxed (value, ts);
					g_date_time_unref (ts);
				}
				real_set_value (GDAUI_ENTRY_WRAPPER (data), value);
			}
		}
	}
	gda_value_free (value);
}

/*
 * GtkCellEditable interface
 */
static void
gtk_cell_editable_entry_editing_done_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryCommonTime *mgtim)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (mgtim));
}

static void
gtk_cell_editable_entry_remove_widget_cb (G_GNUC_UNUSED GtkEntry *entry, GdauiEntryCommonTime *mgtim)
{
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (mgtim));
}

static void
gdaui_entry_common_time_start_editing (GtkCellEditable *iface, GdkEvent *event)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (iface));
	mgtim = GDAUI_ENTRY_COMMON_TIME (iface);
	GdauiEntryCommonTimePrivate *priv = gdaui_entry_common_time_get_instance_private (mgtim);

	priv->editing_canceled = FALSE;

	if (priv->entry) {
		g_object_set (G_OBJECT (priv->entry), "has-frame", FALSE, NULL);
		gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (priv->entry), event);
		g_signal_connect (G_OBJECT (priv->entry), "editing-done",
				  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgtim);
		g_signal_connect (G_OBJECT (priv->entry), "remove-widget",
				  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgtim);
	}

	gtk_widget_grab_focus (priv->entry);
	//gtk_widget_queue_draw (GTK_WIDGET (mgtim)); useful ?
}
