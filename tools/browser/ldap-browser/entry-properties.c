/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2016 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "entry-properties.h"
#include "ldap-marshal.h"
#include <time.h>
#include <libgda-ui/libgda-ui.h>
#include "../ui-support.h"
#include "../text-search.h"

struct _EntryPropertiesPrivate {
	TConnection *tcnc;

	GtkTextView *view;
	GtkTextBuffer *text;
	gboolean hovering_over_link;

	GtkWidget *text_search;

	/* coordinates of mouse */
	gint bx;
	gint by;
};

static void entry_properties_class_init (EntryPropertiesClass *klass);
static void entry_properties_init       (EntryProperties *eprop, EntryPropertiesClass *klass);
static void entry_properties_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;

/* signals */
enum {
        OPEN_DN,
	OPEN_CLASS,
        LAST_SIGNAL
};

gint entry_properties_signals [LAST_SIGNAL] = { 0, 0 };

/*
 * EntryProperties class implementation
 */

static void
entry_properties_class_init (EntryPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	entry_properties_signals [OPEN_DN] =
		g_signal_new ("open-dn",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EntryPropertiesClass, open_dn),
                              NULL, NULL,
                              _ldap_marshal_VOID__STRING, G_TYPE_NONE,
                              1, G_TYPE_STRING);
	entry_properties_signals [OPEN_CLASS] =
		g_signal_new ("open-class",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (EntryPropertiesClass, open_class),
                              NULL, NULL,
                              _ldap_marshal_VOID__STRING, G_TYPE_NONE,
                              1, G_TYPE_STRING);
	klass->open_dn = NULL;
	klass->open_class = NULL;

	object_class->dispose = entry_properties_dispose;
}


static void
entry_properties_init (EntryProperties *eprop, G_GNUC_UNUSED EntryPropertiesClass *klass)
{
	eprop->priv = g_new0 (EntryPropertiesPrivate, 1);
	eprop->priv->hovering_over_link = FALSE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (eprop), GTK_ORIENTATION_VERTICAL);
}

static void
entry_properties_dispose (GObject *object)
{
	EntryProperties *eprop = (EntryProperties *) object;

	/* free memory */
	if (eprop->priv) {
		if (eprop->priv->tcnc) {
			g_object_unref (eprop->priv->tcnc);
		}

		g_free (eprop->priv);
		eprop->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
entry_properties_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo columns = {
			sizeof (EntryPropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) entry_properties_class_init,
			NULL,
			NULL,
			sizeof (EntryProperties),
			0,
			(GInstanceInitFunc) entry_properties_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "EntryProperties", &columns, 0);
	}
	return type;
}

static gboolean key_press_event (GtkWidget *text_view, GdkEventKey *event, EntryProperties *eprop);
static gboolean event_after (GtkWidget *text_view, GdkEvent *ev, EntryProperties *eprop);
static gboolean motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, EntryProperties *eprop);
static gboolean visibility_notify_event (GtkWidget *text_view, GdkEventVisibility *event, EntryProperties *eprop);
static void populate_popup_cb (GtkWidget *text_view, GtkMenu *menu, EntryProperties *eprop);

static void show_search_bar (EntryProperties *eprop);

/**
 * entry_properties_new:
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
entry_properties_new (TConnection *tcnc)
{
	EntryProperties *eprop;
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	eprop = ENTRY_PROPERTIES (g_object_new (ENTRY_PROPERTIES_TYPE, NULL));
	eprop->priv->tcnc = g_object_ref (tcnc);
	
	GtkWidget *sw;
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (eprop), sw, TRUE, TRUE, 0);

	GtkWidget *textview;
	textview = gtk_text_view_new ();
        gtk_container_add (GTK_CONTAINER (sw), textview);
        gtk_text_view_set_left_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_right_margin (GTK_TEXT_VIEW (textview), 5);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
        eprop->priv->text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	eprop->priv->view = GTK_TEXT_VIEW (textview);
        gtk_widget_show_all (sw);

        gtk_text_buffer_create_tag (eprop->priv->text, "section",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

        gtk_text_buffer_create_tag (eprop->priv->text, "error",
                                    "foreground", "red", NULL);

        gtk_text_buffer_create_tag (eprop->priv->text, "data",
				    "left-margin", 20, NULL);

        gtk_text_buffer_create_tag (eprop->priv->text, "convdata",
				    "style", PANGO_STYLE_ITALIC,
				    "background", "lightgray",
                                    "left-margin", 20, NULL);

        gtk_text_buffer_create_tag (eprop->priv->text, "starter",
                                    "indent", -10,
                                    "left-margin", 20, NULL);

	g_signal_connect (textview, "key-press-event", 
			  G_CALLBACK (key_press_event), eprop);
	g_signal_connect (textview, "event-after", 
			  G_CALLBACK (event_after), eprop);
	g_signal_connect (textview, "motion-notify-event", 
			  G_CALLBACK (motion_notify_event), eprop);
	g_signal_connect (textview, "visibility-notify-event", 
			  G_CALLBACK (visibility_notify_event), eprop);
	g_signal_connect (textview, "populate-popup",
			  G_CALLBACK (populate_popup_cb), eprop);

	entry_properties_set_dn (eprop, NULL);

	return (GtkWidget*) eprop;
}

static void
data_save_cb (GtkWidget *mitem, EntryProperties *eprop)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new (_("Select the file to save data to"),
					      (GtkWindow*) gtk_widget_get_toplevel (GTK_WIDGET (eprop)),
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      _("_Cancel"), GTK_RESPONSE_CANCEL,
					      _("_Save"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
					     gdaui_get_default_path ());
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		GValue *binvalue;
		GError *lerror = NULL;
		GdaBinary *bin = NULL;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		binvalue = g_object_get_data (G_OBJECT (mitem), "binvalue");
		if (binvalue)
			bin = gda_value_get_binary (binvalue);
		if (!bin || !g_file_set_contents (filename, (gchar*) gda_binary_get_data (bin),
						  gda_binary_get_size (bin), &lerror)) {
			ui_show_error ((GtkWindow*) gtk_widget_get_toplevel (GTK_WIDGET (eprop)),
					    _("Could not save data: %s"),
					    lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
		}
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog)));
		g_free (filename);
	}
	gtk_widget_destroy (dialog);
}

static void
populate_popup_cb (G_GNUC_UNUSED GtkWidget *text_view, GtkMenu *menu, EntryProperties *eprop)
{
	GtkTextIter iter;
	gtk_text_view_get_iter_at_position (eprop->priv->view, &iter, NULL,
					    eprop->priv->bx, eprop->priv->by);

	GSList *tags = NULL, *tagp = NULL;
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		GValue *binvalue;
		
		binvalue = g_object_get_data (G_OBJECT (tag), "binvalue");
		if (binvalue) {
			GtkWidget *item;

			item = gtk_separator_menu_item_new ();
			gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
			
			item = gtk_menu_item_new_with_label (_("Save"));
			gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
			g_signal_connect (G_OBJECT (item), "activate",
					  G_CALLBACK (data_save_cb), eprop);
			g_object_set_data (G_OBJECT (item), "binvalue", binvalue);
			gtk_widget_show (item);

			break;
		}
        }

	if (tags) 
		g_slist_free (tags);
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;

/* Looks at all tags covering the position (x, y) in the text view, 
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView *text_view, gint x, gint y, EntryProperties *eprop)
{
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;
	gboolean hovering = FALSE;
	
	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	
	tags = gtk_text_iter_get_tags (&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;

		if (g_object_get_data (G_OBJECT (tag), "dn") ||
		    g_object_get_data (G_OBJECT (tag), "class")) {
			hovering = TRUE;
			break;
		}
	}
	
	if (hovering != eprop->priv->hovering_over_link) {
		eprop->priv->hovering_over_link = hovering;
		
		if (eprop->priv->hovering_over_link) {
			if (! hand_cursor)
				hand_cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (text_view)), GDK_HAND2);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       hand_cursor);
		}
		else {
			if (!regular_cursor)
				regular_cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (text_view)), GDK_XTERM);
			gdk_window_set_cursor (gtk_text_view_get_window (text_view,
									 GTK_TEXT_WINDOW_TEXT),
					       regular_cursor);
		}
	}
	
	if (tags) 
		g_slist_free (tags);
}

/* 
 * Also update the cursor image if the window becomes visible
 * (e.g. when a window covering it got iconified).
 */
static gboolean
visibility_notify_event (GtkWidget *text_view, G_GNUC_UNUSED GdkEventVisibility *event,
			 EntryProperties *eprop)
{
	gint wx, wy, bx, by;
	GdkSeat *seat;
        GdkDevice *pointer;
	
	seat = gdk_display_get_default_seat (gtk_widget_get_display (text_view));
	pointer = gdk_seat_get_pointer (seat);
	gdk_window_get_device_position (gtk_widget_get_window (text_view), pointer, &wx, &wy, NULL);
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       wx, wy, &bx, &by);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), bx, by, eprop);
	
	return FALSE;
}

/*
 * Update the cursor image if the pointer moved. 
 */
static gboolean
motion_notify_event (GtkWidget *text_view, GdkEventMotion *event, EntryProperties *eprop)
{
	gint x, y;

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	set_cursor_if_appropriate (GTK_TEXT_VIEW (text_view), x, y, eprop);

	/* store coordinates */
	eprop->priv->bx = x;
	eprop->priv->by = y;

	return FALSE;
}

/* Looks at all tags covering the position of iter in the text view, 
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (G_GNUC_UNUSED GtkWidget *text_view, GtkTextIter *iter, EntryProperties *eprop)
{
	GSList *tags = NULL, *tagp = NULL;
	
	tags = gtk_text_iter_get_tags (iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next) {
		GtkTextTag *tag = tagp->data;
		const gchar *dn;
		
		dn = g_object_get_data (G_OBJECT (tag), "dn");
		if (dn)
			g_signal_emit (eprop, entry_properties_signals [OPEN_DN], 0, dn);
		dn = g_object_get_data (G_OBJECT (tag), "class");
		if (dn)
			g_signal_emit (eprop, entry_properties_signals [OPEN_CLASS], 0, dn);
        }

	if (tags) 
		g_slist_free (tags);
}


/*
 * Links can also be activated by clicking.
 */
static gboolean
event_after (GtkWidget *text_view, GdkEvent *ev, EntryProperties *eprop)
{
	GtkTextIter start, end, iter;
	GtkTextBuffer *buffer;
	GdkEventButton *event;
	gint x, y;
	
	if (ev->type != GDK_BUTTON_RELEASE)
		return FALSE;
	
	event = (GdkEventButton *)ev;
	
	if (event->button != 1)
		return FALSE;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	
	/* we shouldn't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
		return FALSE;
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       event->x, event->y, &x, &y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);
	
	follow_if_link (text_view, &iter, eprop);
	
	return FALSE;
}

/* 
 * Links can be activated by pressing Enter.
 */
static gboolean
key_press_event (GtkWidget *text_view, GdkEventKey *event, EntryProperties *eprop)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	
	switch (event->keyval) {
	case GDK_KEY_Return: 
	case GDK_KEY_KP_Enter:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, 
						  gtk_text_buffer_get_insert (buffer));
		follow_if_link (text_view, &iter, eprop);
		break;
	case GDK_KEY_F:
	case GDK_KEY_f:
		if (event->state & GDK_CONTROL_MASK) {
			show_search_bar (eprop);
			return TRUE;
		}
		break;
	case GDK_KEY_slash:
		show_search_bar (eprop);
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

static GdkPixbuf *
data_to_pixbuf (const GValue *cvalue)
{
	GdkPixbuf *retpixbuf = NULL;

	if (G_VALUE_TYPE (cvalue) == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		GdkPixbufLoader *loader;
		
		bin = gda_value_get_binary ((GValue*) cvalue);
		if (!gda_binary_get_data (bin))
			goto out;

		loader = gdk_pixbuf_loader_new ();
		if (gdk_pixbuf_loader_write (loader, gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
			if (gdk_pixbuf_loader_close (loader, NULL)) {
				retpixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				g_object_ref (retpixbuf);
			}
			else
				gdk_pixbuf_loader_close (loader, NULL);
		}
		else
			gdk_pixbuf_loader_close (loader, NULL);
		g_object_unref (loader);
	}

 out:
	return retpixbuf;
}

static gchar *
unix_shadow_to_string (const gchar *value, const gchar *attname)
{
	/* value is the number of days since 1970-01-01 */
	gint64 i64;
	gchar *endptr [1];

	if (!value || !*value)
		return NULL;

	i64 = g_ascii_strtoll (value, endptr, 10);
	if (**endptr != '\0')
		return NULL;

	if ((i64 == -1) &&
	    (!strcmp (attname, "shadowInactive") ||
	     !strcmp (attname, "shadowMin") ||
	     !strcmp (attname, "shadowExpire")))
		return g_strdup (_("Non activated"));
	else if ((i64 == 99999) && !strcmp (attname, "shadowMax"))
		return g_strdup ("Always valid");
	if ((i64 >= G_MAXUINT) || (i64 < 0))
		return NULL;

	if (!strcmp (attname, "shadowMax") ||
	    !strcmp (attname, "shadowMin") ||
	    !strcmp (attname, "shadowInactive"))
		return NULL;

	GDate *date;
	date = g_date_new_dmy (1, 1, 1970);
	g_date_add_days (date, (guint) i64);
	if (! g_date_valid (date)) {
		g_date_free (date);
		return NULL;
	}

	GdaDataHandler *dh;
	GValue tvalue;
	gchar *str;
	 
	memset (&tvalue, 0, sizeof (GValue));
	g_value_init (&tvalue, G_TYPE_DATE);
	g_value_take_boxed (&tvalue, date);
	dh = gda_data_handler_get_default (G_TYPE_DATE);
	str = gda_data_handler_get_str_from_value (dh, &tvalue);
	g_value_reset (&tvalue);

	return str;
}

static gchar *
ad_1601_timestamp_to_string (const gchar *value, const gchar *attname)
{
	/* value is the number of 100 nanoseconds since 1601-01-01 UTC */
	gint64 i64;
	gchar *endptr [1];

	if (!value || !*value)
		return NULL;

	i64 = g_ascii_strtoll (value, endptr, 10);
	if (**endptr != '\0')
		return NULL;

	if (i64 == 0x7FFFFFFFFFFFFFFF)
		return g_strdup (_("Never"));

	if (i64 == 0 && attname) {
		if (!strcmp (attname, "accountExpires"))
			return g_strdup (_("Never"));
		else
			return g_strdup (_("Unknown"));
	}

	i64 = (i64 / (guint64) 10000000);
	if (i64 < (gint64) 11644473600)
		return NULL;
	i64 = i64 - (guint64) 11644473600;
	if (i64 >= G_MAXINT)
		return NULL;

	GdaDataHandler *dh;
	struct tm *stm;
	GValue tvalue;
	GDateTime *ts;
	time_t nsec = (time_t) i64;
	gchar *str;
#ifdef HAVE_LOCALTIME_R
	struct tm tmpstm;
	stm = localtime_r (&nsec, &tmpstm);
#elif HAVE_LOCALTIME_S
	struct tm tmpstm;
	if (localtime_s (&tmpstm, &nsec) == 0)
		stm = &tmpstm;
	else
		stm = NULL;
#else
	stm = localtime (&nsec);
#endif

	if (!stm)
		return NULL;

	GTimeZone *tz = g_time_zone_new ("Z");
	ts = g_date_time_new (tz,
												stm->tm_year + 1900,
												stm->tm_mon + 1,
												stm->tm_mday,
												stm->tm_hour,
												stm->tm_min,
												stm->tm_sec);
	memset (&tvalue, 0, sizeof (GValue));
	g_value_set_boxed (&tvalue, ts);
	dh = gda_data_handler_get_default (G_TYPE_DATE_TIME);
	str = gda_data_handler_get_str_from_value (dh, &tvalue);
	g_value_reset (&tvalue);
	g_date_time_unref (ts);
	return str;
}

typedef struct {
	guint mask;
	gchar *descr;
} ADUACData;

ADUACData uac_data[] = {
	{0x00000001, "Logon script is executed"},
	{0x00000002, "Account disabled"},
	{0x00000008, "Home directory required"},
	{0x00000010, "Account locked out"},
	{0x00000020, "No password required"},
	{0x00000040, "User cannot change password"},
	{0x00000080, "User can send an encrypted password"},
	{0x00000100, "Duplicate account (local user account)"},
	{0x00000200, "Default account type"},
	{0x00000800, "Permit to trust account for a system domain that trusts other domains"},
	{0x00001000, "Account for a computer"},
	{0x00002000, "Account for a system backup domain controller"},
	{0x00010000, "Account never expires"},
	{0x00020000, "Majority Node Set (MNS) logon account"},
	{0x00040000, "User must log on using a smart card"},
	{0x00080000, "Service account for trusted for Kerberos delegation"},
	{0x00100000, "Security context not delegated"},
	{0x00200000, "Only Data Encryption Standard (DES) encryption for keys"},
	{0x00400000, "Kerberos pre-authentication not required"},
	{0x00800000, "User password expired"},
	{0x01000000, "Account enabled for delegation"}
};

static gchar *
ad_1601_uac_to_string (const gchar *value)
{
	gint64 i64;
	gchar *endptr [1];

	if (!value || !*value)
		return NULL;

	i64 = g_ascii_strtoll (value, endptr, 10);
	if (**endptr != '\0')
		return NULL;
	if (i64 < 0)
		return NULL;
	if (i64 > G_MAXUINT32)
		return NULL;

	GString *string = NULL;
	guint i;
	guint32 v;
	v = (guint32) i64;
	for (i = 0; i < sizeof (uac_data) / sizeof (ADUACData); i++) {
		ADUACData *d;
		d = & (uac_data [i]);
		if (v & d->mask) {
			if (string)
				g_string_append (string, " / ");
			else
				string = g_string_new ("");
			g_string_append (string, d->descr);
		}
	}
	if (string)
		return g_string_free (string, FALSE);	
	else
		return NULL;
}

static gchar *
ad_sam_account_type_to_string (const gchar *value)
{
	gint64 i64;
	gchar *endptr [1];

	if (!value || !*value)
		return NULL;

	i64 = g_ascii_strtoll (value, endptr, 10);
	if (**endptr != '\0')
		return NULL;
	if (i64 < 0)
		return NULL;

	switch (i64) {
	case 0x0:
		return g_strdup ("SAM_DOMAIN_OBJECT");
	case 0x10000000:
		return g_strdup ("SAM_GROUP_OBJECT");
	case 0x10000001:
		return g_strdup ("SAM_NON_SECURITY_GROUP_OBJECT");
	case 0x20000000:
		return g_strdup ("SAM_ALIAS_OBJECT");
	case 0x20000001:
		return g_strdup ("SAM_NON_SECURITY_ALIAS_OBJECT");
	case 0x30000000:
		return g_strdup ("SAM_NORMAL_USER_ACCOUNT");
	case 0x30000001:
		return g_strdup ("SAM_MACHINE_ACCOUNT");
	case 0x30000002:
		return g_strdup ("SAM_TRUST_ACCOUNT");
	case 0x40000000:
		return g_strdup ("SAM_APP_BASIC_GROUP");
	case 0x40000001:
		return g_strdup ("SAM_APP_QUERY_GROUP");
	case 0x7fffffff:
		return g_strdup ("SAM_ACCOUNT_TYPE_MAX");
	default:
		return NULL;
	}
}

static void
entry_info_fetched_done (EntryProperties *eprop, GdaLdapEntry *entry)
{
	GtkTextBuffer *tbuffer;
	GtkTextIter start, end;
	TConnection *tcnc = eprop->priv->tcnc;
	
	tbuffer = eprop->priv->text;
	gtk_text_buffer_get_start_iter (tbuffer, &start);
	gtk_text_buffer_get_end_iter (tbuffer, &end);
	gtk_text_buffer_delete (tbuffer, &start, &end);

	guint i;
	GtkTextIter current;

	gtk_text_buffer_get_start_iter (tbuffer, &current);

	/* DN */
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, _("Distinguished Name:"), -1,
						  "section", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1, "starter", NULL);
	gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, entry->dn, -1,
						  "data", NULL);
	gtk_text_buffer_insert (tbuffer, &current, "\n", -1);

	/* other attributes */
	const gchar *basedn;
	GdaDataHandler *ts_dh = NULL;
	basedn = t_connection_ldap_get_base_dn (tcnc);

	for (i = 0; i < entry->nb_attributes; i++) {
		GdaLdapAttribute *attr;
		gchar *tmp;
		attr = entry->attributes [i];
		tmp = g_strdup_printf ("%s:", attr->attr_name);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, tmp, -1, "section", NULL);
		g_free (tmp);
		gtk_text_buffer_insert (tbuffer, &current, "\n", -1);

		guint j;
		for (j = 0; j < attr->nb_values; j++) {
			const GValue *cvalue;
			cvalue = attr->values [j];

			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, " ", -1,
								  "starter", NULL);

			if (G_VALUE_TYPE (cvalue) == GDA_TYPE_BINARY) {
				GValue *copyvalue;
				GtkTextTagTable *table;
				GtkTextTag *tag;
				GtkTextMark *mark;

				copyvalue = gda_value_copy (cvalue);
				table = gtk_text_buffer_get_tag_table (tbuffer);
				tag = gtk_text_tag_new (NULL);
				gtk_text_tag_table_add (table, tag);
				g_object_set_data_full ((GObject*) tag, "binvalue",
							copyvalue, (GDestroyNotify) gda_value_free);
				g_object_unref ((GObject*) tag);

				mark = gtk_text_buffer_create_mark (tbuffer, NULL, &current, TRUE);

				GdkPixbuf *pixbuf;
				pixbuf = data_to_pixbuf (cvalue);
				if (pixbuf) {
					gtk_text_buffer_insert_pixbuf (tbuffer, &current, pixbuf); 
					g_object_unref (pixbuf);
				}
				else {
					GdaDataHandler *dh;
					dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
					if (dh)
						tmp = gda_data_handler_get_str_from_value (dh, cvalue);
					else
						tmp = gda_value_stringify (cvalue);
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
										  tmp, -1,
										  "data", NULL);
					g_free (tmp);
				}
				GtkTextIter before;
				gtk_text_buffer_get_iter_at_mark (tbuffer, &before, mark);
				gtk_text_buffer_apply_tag (tbuffer, tag, &before, &current);
				gtk_text_buffer_delete_mark (tbuffer, mark);
					
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  "\n", 1,
									  "data", NULL);
			}
			else {
				GdaDataHandler *dh;
				dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
				if (dh)
					tmp = gda_data_handler_get_str_from_value (dh, cvalue);
				else
					tmp = gda_value_stringify (cvalue);
				if (tmp) {
					if (*tmp &&
					    ((basedn && g_str_has_suffix (tmp, basedn)) || !basedn) &&
					    gda_ldap_is_dn (tmp)) {
						/* we have a DN */
						GtkTextTag *tag;
						tag = gtk_text_buffer_create_tag (tbuffer, NULL,
										  "foreground", "blue",
										  "weight", PANGO_WEIGHT_NORMAL,
										  "underline", PANGO_UNDERLINE_SINGLE,
										  NULL);
						g_object_set_data_full (G_OBJECT (tag), "dn",
									g_strdup (tmp), g_free);
						gtk_text_buffer_insert_with_tags (tbuffer, &current,
										  tmp, -1,
										  tag, NULL);
					}
					else if (attr->attr_name &&
						 !g_ascii_strcasecmp (attr->attr_name, "objectClass")) {
						GtkTextTag *tag;
						tag = gtk_text_buffer_create_tag (tbuffer, NULL,
										  "foreground", "blue",
										  "weight", PANGO_WEIGHT_NORMAL,
										  "underline", PANGO_UNDERLINE_SINGLE,
										  NULL);
						g_object_set_data_full (G_OBJECT (tag), "class",
									g_strdup (tmp), g_free);
						gtk_text_buffer_insert_with_tags (tbuffer, &current,
										  tmp, -1,
										  tag, NULL);
					}
					else
						gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, tmp, -1,
											  "data", NULL);

					gchar *extrainfo = NULL;
					if (!strncmp (attr->attr_name, "shadow", 6) &&
					    (!strcmp (attr->attr_name, "shadowLastChange") ||
					     !strcmp (attr->attr_name, "shadowMax") ||
					     !strcmp (attr->attr_name, "shadowMin") ||
					     !strcmp (attr->attr_name, "shadowInactive") ||
					     !strcmp (attr->attr_name, "shadowExpire")))
						extrainfo = unix_shadow_to_string (tmp, attr->attr_name);
					else if (!strcmp (attr->attr_name, "badPasswordTime") ||
						 !strcmp (attr->attr_name, "lastLogon") ||
						 !strcmp (attr->attr_name, "pwdLastSet") ||
						 !strcmp (attr->attr_name, "accountExpires") ||
						 !strcmp (attr->attr_name, "lockoutTime") ||
						 !strcmp (attr->attr_name, "lastLogonTimestamp"))
						extrainfo = ad_1601_timestamp_to_string (tmp, attr->attr_name);
					else if (!strcmp (attr->attr_name, "userAccountControl"))
						extrainfo = ad_1601_uac_to_string (tmp);
					else if (!strcmp (attr->attr_name, "sAMAccountType"))
						extrainfo = ad_sam_account_type_to_string (tmp);

					if (extrainfo) {
						gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
											  " ", 1,
											  "data", NULL);
						gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
											  extrainfo, -1,
											  "convdata", NULL);
						g_free (extrainfo);
					}
					g_free (tmp);
				}
				else {
					gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current, _("Can't display attribute value"), -1,
										  "error", NULL);
				}
				gtk_text_buffer_insert_with_tags_by_name (tbuffer, &current,
									  "\n", 1,
									  "data", NULL);
			}
		}
	}
	if (ts_dh)
		g_object_unref (ts_dh);
	gda_ldap_entry_free (entry);


	if (eprop->priv->text_search && gtk_widget_get_visible (eprop->priv->text_search))
		text_search_rerun (TEXT_SEARCH (eprop->priv->text_search));
}

/**
 * entry_properties_set_dn:
 * @eprop: a #EntryProperties widget
 * @dn: a DN to display information for
 *
 * Adjusts the display to show @dn's properties
 */
void
entry_properties_set_dn (EntryProperties *eprop, const gchar *dn)
{
	g_return_if_fail (IS_ENTRY_PROPERTIES (eprop));

	GtkTextBuffer *tbuffer;
	GtkTextIter start, end;

	tbuffer = eprop->priv->text;
	gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);

	if (dn && *dn) {
		GdaLdapEntry *entry;
		entry = t_connection_ldap_describe_entry (eprop->priv->tcnc, dn, NULL);
		if (entry)
			entry_info_fetched_done (eprop, entry);
		else 
			ui_show_message (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) eprop)),
					      "%s", _("Could not get information about LDAP entry"));
	}
}

static void
show_search_bar (EntryProperties *eprop)
{
	if (! eprop->priv->text_search) {
		eprop->priv->text_search = text_search_new (eprop->priv->view);
		gtk_box_pack_start (GTK_BOX (eprop), eprop->priv->text_search, FALSE, FALSE, 0);
		gtk_widget_show (eprop->priv->text_search);
	}
	else {
		gtk_widget_show (eprop->priv->text_search);
		text_search_rerun (TEXT_SEARCH (eprop->priv->text_search));
	}

	gtk_widget_grab_focus (eprop->priv->text_search);
}
