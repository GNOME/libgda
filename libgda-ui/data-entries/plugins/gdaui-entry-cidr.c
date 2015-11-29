/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "gdaui-entry-cidr.h"
#include <libgda/gda-data-handler.h>
#include <string.h>
#include "gdaui-formatted-entry.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_cidr_class_init (GdauiEntryCidrClass * class);
static void gdaui_entry_cidr_init (GdauiEntryCidr * srv);
static void gdaui_entry_cidr_dispose (GObject   * object);
static void gdaui_entry_cidr_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* useful static functions */
static gint     get_ip_nb_bits (GdauiEntryCidr *mgcidr);
static gint     get_mask_nb_bits (GdauiEntryCidr *mgcidr);
static gboolean get_complete_value (GdauiEntryCidr *mgcidr, gboolean target_mask, gulong *result);
static void     truncate_entries_to_mask_length (GdauiEntryCidr *mgcidr, gboolean target_mask,
						 guint mask_nb_bits);
typedef struct {
	gchar **ip_array;
	gchar **mask_array;
} SplitValues;

static SplitValues *split_values_get  (GdauiEntryCidr *mgcidr);
static SplitValues *split_values_new  ();
static void         split_values_set  (GdauiEntryCidr *mgcidr, SplitValues *svalues);
static void         split_values_free (SplitValues *values);

/* private structure */
struct _GdauiEntryCidrPrivate
{
	GtkWidget *entry;
};


GType
gdaui_entry_cidr_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryCidrClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_cidr_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryCidr),
			0,
			(GInstanceInitFunc) gdaui_entry_cidr_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryCidr", &info, 0);
	}
	return type;
}

static void
gdaui_entry_cidr_class_init (GdauiEntryCidrClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_cidr_dispose;
	object_class->finalize = gdaui_entry_cidr_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
}

static void
gdaui_entry_cidr_init (GdauiEntryCidr * gdaui_entry_cidr)
{
	gdaui_entry_cidr->priv = g_new0 (GdauiEntryCidrPrivate, 1);
	gdaui_entry_cidr->priv->entry = NULL;
}

/**
 * gdaui_entry_cidr_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_cidr_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryCidr *mgcidr;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_CIDR, "handler", dh, NULL);
	mgcidr = GDAUI_ENTRY_CIDR (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgcidr), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_cidr_dispose (GObject   * object)
{
	GdauiEntryCidr *gdaui_entry_cidr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_CIDR (object));

	gdaui_entry_cidr = GDAUI_ENTRY_CIDR (object);
	if (gdaui_entry_cidr->priv) {

	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_cidr_finalize (GObject   * object)
{
	GdauiEntryCidr *gdaui_entry_cidr;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_CIDR (object));

	gdaui_entry_cidr = GDAUI_ENTRY_CIDR (object);
	if (gdaui_entry_cidr->priv) {

		g_free (gdaui_entry_cidr->priv);
		gdaui_entry_cidr->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}


static gboolean ip_focus_out_event_cb (GtkEntry *entry, GdkEventFocus *event, GdauiEntryCidr *mgcidr);
static gboolean mask_focus_out_event_cb (GtkEntry *entry, GdkEventFocus *event, GdauiEntryCidr *mgcidr);
static void mask_popup (GtkEntry *entry, GtkMenu *arg1, GdauiEntryCidr *mgcidr);

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *entry;
	GdauiEntryCidr *mgcidr;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_CIDR (mgwrap), NULL);
	mgcidr = GDAUI_ENTRY_CIDR (mgwrap);
	g_return_val_if_fail (mgcidr->priv, NULL);

	entry = gdaui_formatted_entry_new ("000.000.000.000/000.000.000.000", "---.---.---.---/---.---.---.---");
	mgcidr->priv->entry = entry;
	gtk_entry_set_width_chars (GTK_ENTRY (entry), 19);

	g_signal_connect (G_OBJECT (entry), "focus-out-event",
			  G_CALLBACK (ip_focus_out_event_cb), mgcidr);		
	g_signal_connect (G_OBJECT (entry), "populate-popup",
			  G_CALLBACK (mask_popup), mgcidr);

	return entry;
}

/* makes sure the mask part of the widget is compatible with the ip part */
static gboolean
ip_focus_out_event_cb (G_GNUC_UNUSED GtkEntry *entry, GdkEventFocus *event, GdauiEntryCidr *mgcidr)
{
	gint ip;

	ip = get_ip_nb_bits (mgcidr);
	if (ip >= 0) {
		gint mask;

		mask = get_mask_nb_bits (mgcidr);
		if (ip > mask) {
			int i;
			SplitValues *svalues;
			
			svalues = split_values_get (mgcidr);
			if (svalues) {
				for (i = 0; i < 4; i++) {
					g_free (svalues->mask_array [i]);
					svalues->mask_array [i] = g_strdup ("255");
				}
				split_values_set (mgcidr, svalues);
				split_values_free (svalues);
				truncate_entries_to_mask_length (mgcidr, TRUE, ip);
			}
		}
	}

	return gtk_widget_event (GTK_WIDGET (mgcidr), (GdkEvent*) event);
}

/* makes sure the ip part of the widget is truncated to the right number of bits corresponding to
 * the mask part */
static gboolean
mask_focus_out_event_cb (G_GNUC_UNUSED GtkEntry *entry, G_GNUC_UNUSED GdkEventFocus *event, GdauiEntryCidr *mgcidr)
{
	gint mask;

	mask = get_mask_nb_bits (mgcidr);
	if (mask >= 0) 
		truncate_entries_to_mask_length (mgcidr, FALSE, mask);

	return FALSE;
}

static void popup_menu_item_activate_cb (GtkMenuItem *item, GdauiEntryCidr *mgcidr);
static void
mask_popup (G_GNUC_UNUSED GtkEntry *entry, GtkMenu *arg1, GdauiEntryCidr *mgcidr)
{
	GtkWidget *item;
	gint net;

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (arg1), item);
	gtk_widget_show (item);
	
	item = gtk_menu_item_new_with_label (_("Set to host mask"));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (arg1), item);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (popup_menu_item_activate_cb), mgcidr);
	g_object_set_data (G_OBJECT (item), "mask", GINT_TO_POINTER ('D'));
	gtk_widget_show (item);

	for (net='C'; net >= 'A'; net--) {
		gchar *str;
		str = g_strdup_printf (_("Set to class %c network"), net);
		item = gtk_menu_item_new_with_label (str);
		g_free (str);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (arg1), item);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (popup_menu_item_activate_cb), mgcidr);
		g_object_set_data (G_OBJECT (item), "mask", GINT_TO_POINTER (net));
		gtk_widget_show (item);
	}
}

static void
popup_menu_item_activate_cb (GtkMenuItem *item, GdauiEntryCidr *mgcidr)
{
	gint i, mask, limit;
	SplitValues *svalues;

	mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "mask"));
	switch (mask) {
	default:
	case 'D':
		limit = 4;
		break;
	case 'C':
		limit = 3;
		break;
	case 'B':
		limit = 2;
		break;
	case 'A':
		limit = 1;
		break;
	}

	svalues = split_values_get (mgcidr);
	if (!svalues)
		svalues = split_values_new ();

	for (i = 0; i < limit; i++) {
		g_free (svalues->mask_array [i]);
		svalues->mask_array [i] = g_strdup ("255");
	}
	for (i = limit; i < 4; i++) {
		g_free (svalues->mask_array [i]);
		svalues->mask_array [i] = g_strdup ("000");
	}
	split_values_set (mgcidr, svalues);
	split_values_free (svalues);
	
	/* force the ip part to be updated */
	mask_focus_out_event_cb (NULL, NULL, mgcidr);
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryCidr *mgcidr;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_CIDR (mgwrap));
	mgcidr = GDAUI_ENTRY_CIDR (mgwrap);
	g_return_if_fail (mgcidr->priv);

	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (mgcidr->priv->entry), NULL);
		else {
			SplitValues *svalues;
			gchar *str, *ptr, *tok = NULL;
			gint i=1;
			str = g_strdup (g_value_get_string ((GValue *) value));

			svalues = split_values_new ();
			ptr = strtok_r (str, ".", &tok);
			svalues->ip_array[0] = g_strdup (ptr);
			for (i = 1; (i < 4) && ptr; i++) {
				if (i < 3)
					ptr = strtok_r (NULL, ".", &tok);
				else
					ptr = strtok_r (NULL, "/", &tok);
				svalues->ip_array[i] = g_strdup (ptr);
			}
			
			if (ptr) {
				for (i = 0; i < 4; i++) 
					svalues->mask_array[i] = g_strdup ("255");
				
				ptr = strtok_r (NULL, "./", &tok);
				if (ptr) {
					gint net;
					
					net = atoi (ptr);
					if (net >= 0)
						truncate_entries_to_mask_length (mgcidr, TRUE, net);
				}
			}
			g_free (str);			
			split_values_set (mgcidr, svalues);
			split_values_free (svalues);
		}
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (mgcidr->priv->entry), NULL);
}

static void truncate_entries_to_mask_length (GdauiEntryCidr *mgcidr, gboolean target_mask, guint mask_nb_bits)
{
	guint i, j;
	gchar *val;
	guint mask, maskiter;
	gint oldval, newval;
	
	SplitValues *svalues;
	svalues = split_values_get (mgcidr);
	if (!svalues)
		return;

	for (j = 0; j < 4; j++) {
		mask = 0;
		maskiter = 1 << 7;
		i = 0;
		while ((i < 8) && (8*j + i < mask_nb_bits)) {
			mask += maskiter;
			maskiter >>= 1;
			i++;
		}

		if (target_mask)
			oldval = atoi (svalues->mask_array[j]);
		else
			oldval = atoi (svalues->ip_array[j]);
		
		newval = oldval & mask;
		val = g_strdup_printf ("%03d", newval);
		if (target_mask) {
			g_free (svalues->mask_array[j]);
			svalues->mask_array[j] = val;
		}
		else {
			g_free (svalues->ip_array[j]);
			svalues->ip_array[j] = val;
		}
	}
	split_values_set (mgcidr, svalues);
	split_values_free (svalues);
}

/*
 * returns an error if empty string
 */
static gboolean
get_complete_value (GdauiEntryCidr *mgcidr, gboolean target_mask, gulong *result)
{
	gboolean error = FALSE;
	
	SplitValues *svalues;

	svalues = split_values_get (mgcidr);
	if (!svalues) {
		*result = 0;
		error = TRUE;
	}
	else {
		gulong retval = 0;
		gint i;
		gchar **array;
		if (target_mask)
			array = svalues->mask_array;
		else
			array = svalues->ip_array;
		
		for (i = 0; i < 4; i++) {
			gint part;
			guint part2;
			part = atoi (array [i]);
			if ((part < 0) || (part > 255))
				error = TRUE;
			else {
				part2 = part;
				retval += part2 << 8*(3-i);
			}
		}
		split_values_free (svalues);
		
		*result = retval;
	}

	return !error;
}

static gint
get_ip_nb_bits (GdauiEntryCidr *mgcidr)
{
	gulong ipval;
	if (get_complete_value (mgcidr, FALSE, &ipval)) {
		/* bits counting */
		gboolean ipend = FALSE;
		gint i, ip;
		gulong ipiter;

		ip = 32;
		i = 0;
		ipiter = 1;
		while (!ipend && (i<=31)) {
			if (ipval & ipiter)
				ipend = TRUE;
			else
				ip--;
			ipiter <<= 1;
			i++;
		}

		return ip;
	}
	else
		return -1;
}

/* returns -1 if error */
static gint
get_mask_nb_bits (GdauiEntryCidr *mgcidr)
{
	gulong maskval;
	if (get_complete_value (mgcidr, TRUE, &maskval)) {
		/* bits counting */
		gboolean maskend = FALSE;
		gint i, mask;
		gulong maskiter;
		gboolean error = FALSE;

		mask = 0;
		i = 31;
		maskiter = 1 << i;
		while (!error && (i>=0)) {
			if (maskval & maskiter) {
				mask ++;
				if (maskend)
					error = TRUE;
			}
			else
				maskend = TRUE;
			maskiter >>= 1;
			i--;
		}

		if (error) 
			return -1;
		else
			return mask;
	}
	else 
		return -1;
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value = NULL;
	GdauiEntryCidr *mgcidr;
	gint i;
	gboolean error = FALSE;
	gint iplen, masklen;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_CIDR (mgwrap), NULL);
	mgcidr = GDAUI_ENTRY_CIDR (mgwrap);
	g_return_val_if_fail (mgcidr->priv, NULL);

	iplen = get_ip_nb_bits (mgcidr);
	masklen = get_mask_nb_bits (mgcidr);
	if (iplen > masklen)
		error = TRUE;
	else {
		SplitValues *svalues;

		svalues = split_values_get (mgcidr);
		if (svalues) {
			GString *string;
			string = g_string_new ("");
			/* ip part */
			for (i = 0; i < 4; i++) {
				gint ippart;
				
				if (i > 0)
					g_string_append_c (string, '.');
				ippart = atoi (svalues->ip_array [i]);
				if ((ippart < 0) || (ippart > 255))
					error = TRUE;
				g_string_append_printf (string, "%03d", ippart);
			}
			split_values_free (svalues);
			
			/* mask part */
			if (masklen < 0)
				error = TRUE;
			
			if (!error) {
				g_string_append_printf (string, "/%d", masklen);
				g_value_set_string (value = gda_value_new (G_TYPE_STRING), string->str);
			}
			g_string_free (string, TRUE);
		}
	}

	if (!value) {
		/* in case the gda_data_handler_get_value_from_sql() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryCidr *mgcidr;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_CIDR (mgwrap));
	mgcidr = GDAUI_ENTRY_CIDR (mgwrap);
	g_return_if_fail (mgcidr->priv);

	g_signal_connect_swapped (G_OBJECT (mgcidr->priv->entry), "changed", modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (mgcidr->priv->entry), "activate", activate_cb, mgwrap);
}

static SplitValues *
split_values_get (GdauiEntryCidr *mgcidr)
{
	SplitValues *values;
	gchar **tmp_array;
	gchar *str;

	str = gdaui_entry_get_text (GDAUI_ENTRY (mgcidr->priv->entry));
	if (!str)
		return NULL;

	values = g_new0 (SplitValues, 1);

	tmp_array = g_strsplit (str, "/", -1);
	if (!tmp_array[0] || !tmp_array[1])
		goto split_exit;

	values->ip_array = g_strsplit (tmp_array[0], ".", -1);
	if (g_strv_length (values->ip_array) != 4)
		goto split_exit;
	values->mask_array = g_strsplit (tmp_array[1], ".", -1);
	if (g_strv_length (values->mask_array) != 4)
		goto split_exit;

	g_strfreev (tmp_array);
	g_free (str);
	return values;

 split_exit:
	g_free (str);
	split_values_free (values);
	g_strfreev (tmp_array);
	return NULL;
}

static SplitValues *
split_values_new ()
{
	SplitValues *svalues;
	svalues = g_new (SplitValues, 1);
	svalues->ip_array = g_new0 (gchar *, 5);
	svalues->mask_array = g_new0 (gchar *, 5);
	return svalues;
}

static void
split_values_set (GdauiEntryCidr *mgcidr, SplitValues *svalues)
{
	gchar *ip_str, *mask_str, *str;
	gint i;

	for (i = 0; i < 4; i++) {
		guchar val;

		if (svalues->ip_array [i])
			val = atoi (svalues->ip_array [i]);
		else
			val = 0;
		g_free (svalues->ip_array [i]);
		svalues->ip_array [i] = g_strdup_printf ("%03d", val);

		if (svalues->mask_array [i])
			val = atoi (svalues->mask_array [i]);
		else
			val = 0;
		g_free (svalues->mask_array [i]);
		svalues->mask_array [i] = g_strdup_printf ("%03d", val);
	}
	ip_str = g_strjoinv (".", svalues->ip_array);
	mask_str = g_strjoinv (".", svalues->mask_array);
	str = g_strdup_printf ("%s/%s", ip_str, mask_str);
	gdaui_entry_set_text (GDAUI_ENTRY (mgcidr->priv->entry), str);
	g_free (str);
}

static void
split_values_free (SplitValues *values)
{
	g_strfreev (values->ip_array);
	g_strfreev (values->mask_array);
	g_free (values);
}
