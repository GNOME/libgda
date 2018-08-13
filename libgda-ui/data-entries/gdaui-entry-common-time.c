/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2017 Daniel Espinosa <esodan@gmail.com>
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
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;


/* private structure */
struct _GdauiEntryCommonTimePrivate
{
	GtkWidget *entry;
	GtkWidget *cal_popover;
	GtkWidget *calendar;
	glong     displayed_tz;
	glong     value_tz;
	gulong     value_fraction;

	gboolean   editing_canceled;
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
			(GInstanceInitFunc) gdaui_entry_common_time_init,
			0
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
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;

	/* Properties */
	object_class->set_property = gdaui_entry_common_time_set_property;
	object_class->get_property = gdaui_entry_common_time_get_property;

	g_object_class_install_property (object_class, PROP_EDITING_CANCELED,
					 g_param_spec_boolean ("editing-canceled", NULL, NULL, FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_uint ("type", NULL, NULL, 0, G_MAXUINT, GDA_TYPE_TIME, 
							    G_PARAM_WRITABLE | G_PARAM_READABLE));
}

static gboolean
key_press_event_cb (GdauiEntryCommonTime *mgtim, GdkEventKey *key_event, G_GNUC_UNUSED gpointer data)
{
	if (key_event->keyval == GDK_KEY_Escape)
		mgtim->priv->editing_canceled = TRUE;
	return FALSE;
}

static glong
compute_tz_offset (struct tm *gmttm, struct tm *loctm)
{
        if (! gmttm || !loctm)
                return G_MAXLONG;

        struct tm cgmttm, cloctm;
        cgmttm = *gmttm;
        cloctm = *loctm;

        time_t lt, gt;
        cgmttm.tm_isdst = 0;
        cloctm.tm_isdst = 0;

        lt = mktime (&cloctm);
        if (lt == -1)
                return G_MAXLONG;
        gt = mktime (&cgmttm);
        if (gt == -1)
                return G_MAXLONG;
        glong off;
        off = lt - gt;

        if ((off >= 24 * 3600) || (off <= - 24 * 3600))
                return G_MAXLONG;
        else
                return off;
}

static gulong
compute_localtime_tz (void)
{
	time_t val;
	val = time (NULL);
	glong tz = 0;

#ifdef HAVE_LOCALTIME_R
        struct tm gmttm, loctm;
        tzset ();
	localtime_r ((const time_t *) &val, &loctm);
        tz = compute_tz_offset (gmtime_r ((const time_t *) &val, &gmttm), &loctm);
#elif HAVE_LOCALTIME_S
        struct tm gmttm, loctm;
        if ((localtime_s (&loctm, (const time_t *) &val) == 0) &&
            (gmtime_s (&gmttm, (const time_t *) &val) == 0)) {
                tz = compute_tz_offset (&gmttm, &loctm);
        }
#else
        struct tm gmttm, loctm;
	struct tm *ltm;
        ltm = gmtime ((const time_t *) &val);
        if (ltm) {
                gmttm = *ltm;
                ltm = localtime ((const time_t *) &val);
                if (ltm) {
                        loctm = *ltm;
                        tz = compute_tz_offset (&gmttm, &loctm);
                }
        }
#endif
	if (tz == G_MAXLONG)
		tz = 0;
	return tz;
}

static void
gdaui_entry_common_time_init (GdauiEntryCommonTime *mgtim)
{
	mgtim->priv = g_new0 (GdauiEntryCommonTimePrivate, 1);
	mgtim->priv->entry = NULL;
	mgtim->priv->calendar = NULL;
	mgtim->priv->editing_canceled = FALSE;
	mgtim->priv->value_tz = 0; /* safe init value */
	mgtim->priv->value_fraction = 0; /* safe init value */
	mgtim->priv->displayed_tz = compute_localtime_tz ();
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
gdaui_entry_common_time_dispose (GObject   * object)
{
	GdauiEntryCommonTime *gdaui_entry_common_time;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (object));

	gdaui_entry_common_time = GDAUI_ENTRY_COMMON_TIME (object);
	if (gdaui_entry_common_time->priv) {
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
		case PROP_EDITING_CANCELED:
			TO_IMPLEMENT;
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

static void date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);
static void date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim);

static void
icon_press_cb (GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, GdauiEntryCommonTime *mgtim)
{
	if (icon_pos == GTK_ENTRY_ICON_PRIMARY) {
		if (! mgtim->priv->cal_popover) {
			/* calendar */
			GtkWidget *wid;
			wid = gtk_calendar_new ();
			mgtim->priv->calendar = wid;
			gtk_widget_show (wid);
			g_signal_connect (G_OBJECT (wid), "day-selected",
					  G_CALLBACK (date_day_selected), mgtim);
			g_signal_connect (G_OBJECT (wid), "day-selected-double-click",
					  G_CALLBACK (date_day_selected_double_click), mgtim);

			/* popover */
			GtkWidget *popover;
			popover = gtk_popover_new (mgtim->priv->entry);
			gtk_container_add (GTK_CONTAINER (popover), mgtim->priv->calendar);
			mgtim->priv->cal_popover = popover;
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
			time_t now;
			struct tm *stm;

			now = time (NULL);
#ifdef HAVE_LOCALTIME_R
			struct tm tmpstm;
			stm = localtime_r (&now, &tmpstm);
#elif HAVE_LOCALTIME_S
			struct tm tmpstm;
			g_assert (localtime_s (&tmpstm, &now) == 0);
			stm = &tmpstm;
#else
			stm = localtime (&now);
#endif
			year = stm->tm_year + 1900;
			month = stm->tm_mon;
			day = stm->tm_mday;
		}

		if (! unset) {
			g_signal_handlers_block_by_func (G_OBJECT (mgtim->priv->calendar),
							 G_CALLBACK (date_day_selected), mgtim);
			g_signal_handlers_block_by_func (G_OBJECT (mgtim->priv->calendar),
							 G_CALLBACK (date_day_selected_double_click), mgtim);
		}
		gtk_calendar_select_month (GTK_CALENDAR (mgtim->priv->calendar), month, year);
		gtk_calendar_select_day (GTK_CALENDAR (mgtim->priv->calendar), day);
		if (! unset) {
			g_signal_handlers_unblock_by_func (G_OBJECT (mgtim->priv->calendar),
							   G_CALLBACK (date_day_selected), mgtim);
			g_signal_handlers_unblock_by_func (G_OBJECT (mgtim->priv->calendar),
							   G_CALLBACK (date_day_selected_double_click), mgtim);
		}

		gtk_widget_show (mgtim->priv->cal_popover);
	}
}

static void
date_day_selected (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
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
		tmpstr = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry));
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
		gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), str_utf8);
		g_free (str_utf8);
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), "");
}

static void
date_day_selected_double_click (GtkCalendar *calendar, GdauiEntryCommonTime *mgtim)
{
	gtk_widget_hide (mgtim->priv->cal_popover);
}

static glong
fit_tz (glong tz)
{
	tz = tz % 86400;
	if (tz > 43200)
		tz -= 86400;
	else if (tz < -43200)
		tz += 86400;
	return tz;
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
		tz = mgtim->priv->displayed_tz;
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
	mgtim->priv->entry = wid;
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
 * NB: the displayed value is relative to mgtim->priv->displayed_tz
 */
static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryCommonTime *mgtim;
	GType type;
	GdaDataHandler *dh;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);
	g_return_if_fail (mgtim->priv);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgtim));

	if (type == G_TYPE_DATE) {
		if (value) {
			if (gda_value_is_null ((GValue *) value))
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
			else {
				gchar *str;
				
				str = gda_data_handler_get_str_from_value (dh, value);
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), str);
				g_free (str);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
	}
	else if (type == GDA_TYPE_TIME) {
		if (value) {
			if (gda_value_is_null ((GValue *) value)) {
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
				mgtim->priv->value_tz = mgtim->priv->displayed_tz;
				mgtim->priv->value_fraction = 0;
			}
			else {
				const GdaTime *gtim;
				GdaTime* copy;
				gtim = gda_value_get_time (value);
				mgtim->priv->value_tz = fit_tz (gda_time_get_timezone (gtim));
				mgtim->priv->value_fraction = gda_time_get_fraction (gtim);

				copy = gda_time_copy (gtim);
				gda_time_change_timezone (copy, mgtim->priv->displayed_tz);

				GValue *copy_value;
				copy_value = g_new0 (GValue, 1);
				gda_value_set_time (copy_value, copy);

				gchar *str;
				str = gda_data_handler_get_str_from_value (dh, copy_value);
				gda_value_free (copy_value);

				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), str);
				g_free (str);
				gda_time_free (copy);
			}
		}
		else 
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
	}
	else if (type == G_TYPE_DATE_TIME) {
		if (value) {
			if (gda_value_is_null ((GValue *) value)) {
				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
				mgtim->priv->value_tz = mgtim->priv->displayed_tz;
				mgtim->priv->value_fraction = 0;
			}
			else {
				GDateTime *gts;
				GDateTime *copy;
				gts = g_value_get_boxed (value);
				mgtim->priv->value_tz = fit_tz (g_date_time_get_utc_offset (gts) / 1000000);
				mgtim->priv->value_fraction = (glong) ((g_date_time_get_seconds (gts) - g_date_time_get_second (gts)) * 1000000);

				gint itz = mgtim->priv->displayed_tz;
				if (itz < 0)
					itz *= -1;
				gint h = itz/60/60;
				gint m = itz/60 - h*60;
				gint s = itz - h*60*60 - m*60;
				gchar *stz = g_strdup_printf ("%s%02d:%02d:%02d",
																			mgtim->priv->displayed_tz < 0 ? "-" : "+",
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

				gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), str);
				g_free (str);
				g_date_time_unref (copy);
			}
		}
		else
			gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), NULL);
	}
	else
		g_assert_not_reached ();
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
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		str2 = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry));
		if (str2) {
			value = gda_data_handler_get_value_from_str (dh, str2, type);
			g_free (str2);
		}

		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			const GdaTime *gdatime;
			gdatime = gda_value_get_time (value);
			GdaTime *time_copy = gda_time_copy (gdatime);
			gda_time_set_timezone (time_copy, mgtim->priv->displayed_tz);
			gda_time_change_timezone (time_copy, mgtim->priv->value_tz);
			gda_value_set_time (value, time_copy);
			gda_time_free (time_copy);
		}
	}
	else if (type == G_TYPE_DATE_TIME) {
		gchar *tmpstr;

		tmpstr = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry));
		if (tmpstr) {
			value = gda_data_handler_get_value_from_str (dh, tmpstr, type);
			g_free (tmpstr);
		}

		if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
			/* copy the 'fraction' part, we have not modified */
			GDateTime *gdatime = NULL;
			GDateTime *copy;
			g_value_take_boxed (value, gdatime);
			gint tzi = mgtim->priv->displayed_tz; // FIXME: This is always positive
			if (tzi < 0)
				tzi *= -1;
			gint h = tzi/60/60;
			gint m = tzi/60 - h*60;
			gint s = tzi - h*60*60 - m*60;
			gchar *stz = g_strdup_printf ("%s%02d:%02d:%02d",
																		mgtim->priv->displayed_tz < 0 ? "-" : "+",
																		h, m, s);
			GTimeZone *tz = g_time_zone_new (stz);
			g_free (stz);
			gdouble seconds = g_date_time_get_second (gdatime) + mgtim->priv->value_fraction / 1000000.0;
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
			gda_timestamp_change_timezone (gdatime, mgtim->priv->value_tz);
			 */
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

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryCommonTime *mgtim;
	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);

	g_signal_connect_swapped (G_OBJECT (mgtim->priv->entry), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (mgtim->priv->entry), "activate",
				  activate_cb, mgwrap);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);

	if (mgtim->priv->entry)
		gtk_editable_set_editable (GTK_EDITABLE (mgtim->priv->entry), editable);

	gtk_entry_set_icon_sensitive (GTK_ENTRY (mgtim->priv->entry), GTK_ENTRY_ICON_PRIMARY, editable);
	gtk_entry_set_icon_sensitive (GTK_ENTRY (mgtim->priv->entry), GTK_ENTRY_ICON_SECONDARY, editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryCommonTime *mgtim;

	g_return_if_fail (GDAUI_IS_ENTRY_COMMON_TIME (mgwrap));
	mgtim = GDAUI_ENTRY_COMMON_TIME (mgwrap);

	gtk_widget_grab_focus (mgtim->priv->entry);
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
			//str = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (mgtim->priv->entry));
			tsvalue = gda_value_new_date_time_from_timet (time (NULL));
			real_set_value (GDAUI_ENTRY_WRAPPER (data), tsvalue);
			gda_value_free (tsvalue);
			//if (str && g_ascii_isdigit (*str))
			//	gdaui_entry_set_text (GDAUI_ENTRY (mgtim->priv->entry), str);
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
	g_return_if_fail (mgtim->priv);

	mgtim->priv->editing_canceled = FALSE;

	if (mgtim->priv->entry) {
		g_object_set (G_OBJECT (mgtim->priv->entry), "has-frame", FALSE, NULL);
		gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (mgtim->priv->entry), event);
		g_signal_connect (G_OBJECT (mgtim->priv->entry), "editing-done",
				  G_CALLBACK (gtk_cell_editable_entry_editing_done_cb), mgtim);
		g_signal_connect (G_OBJECT (mgtim->priv->entry), "remove-widget",
				  G_CALLBACK (gtk_cell_editable_entry_remove_widget_cb), mgtim);
	}

	gtk_widget_grab_focus (mgtim->priv->entry);
	//gtk_widget_queue_draw (GTK_WIDGET (mgtim)); useful ?
}
