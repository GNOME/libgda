/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <string.h>

#include "gdaui-entry.h"

typedef struct {
	gchar   *prefix;
	gint     prefix_len;
	gint     prefix_clen; /* UTF8 len */
	gchar   *suffix;
	gint     suffix_len;
	gint     suffix_clen; /* UTF8 len */
	gint     maxlen; /* UTF8 len */
	gboolean isnull;
	guchar   internal_changes;
	gint     max_width;
} GdauiEntryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiEntry, gdaui_entry, GTK_TYPE_ENTRY)

static void gdaui_entry_dispose     (GObject *object);
static void gdaui_entry_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec);
static void gdaui_entry_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);

void
_gdaui_entry_block_changes (GdauiEntry *entry)
{
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	priv->internal_changes++;
}

void
_gdaui_entry_unblock_changes (GdauiEntry *entry)
{
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	priv->internal_changes--;
}


static gchar *truncate_utf8_string (gchar *text, gint pos);
static void adjust_display (GdauiEntry *entry, gchar *existing_text);

/* properties */
enum
{
        PROP_0,
	PROP_PREFIX,
	PROP_SUFFIX,
	PROP_MAXLEN
};

static void signal_handlers_block (GdauiEntry *entry);
static void signal_handlers_unblock (GdauiEntry *entry);

static void changed_cb (GtkEditable *editable, gpointer data);
static void delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, gpointer data);
static void insert_text_cb (GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer data);
static void internal_insert_text (GtkEditable *editable, const gchar *text, gint text_length, gint *position,
				  gboolean handle_default_insert);

static void
gdaui_entry_class_init (GdauiEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gdaui_entry_dispose;
	klass->assume_insert = NULL;
	klass->assume_delete = NULL;
	klass->get_empty_text = NULL;

	/* Properties */
        object_class->set_property = gdaui_entry_set_property;
        object_class->get_property = gdaui_entry_get_property;

        g_object_class_install_property (object_class, PROP_PREFIX,
                                         g_param_spec_string ("prefix", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_SUFFIX,
                                         g_param_spec_string ("suffix", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_override_property (object_class, PROP_MAXLEN, "max-length");
}

static void
gdaui_entry_init (GdauiEntry *entry)
{
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	priv->prefix = NULL;
	priv->suffix = NULL;
	priv->maxlen = 65535; /* eg. unlimited for GtkEntry */
	priv->isnull = TRUE;
	priv->internal_changes = 0;
	priv->max_width = -1;

	g_signal_connect (G_OBJECT (entry), "delete-text",
			  G_CALLBACK (delete_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "insert-text",
			  G_CALLBACK (insert_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (changed_cb), NULL);
}

static void 
gdaui_entry_dispose (GObject *object)
{
        GdauiEntry *entry;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDAUI_IS_ENTRY (object));

        entry = GDAUI_ENTRY (object);
        GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
        g_free (priv->prefix);
        g_free (priv->suffix);

        /* parent class */
        G_OBJECT_CLASS (gdaui_entry_parent_class)->dispose (object);
}

static void 
gdaui_entry_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiEntry *entry;
	const gchar *str;
	gchar *otext;

	entry = GDAUI_ENTRY (object);
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	switch (param_id) {
		case PROP_PREFIX:
			otext = gdaui_entry_get_text (entry);
			g_free (priv->prefix);
			priv->prefix = NULL;
			priv->prefix_len = 0;

			str = g_value_get_string (value);
			if (str) {
				if (! g_utf8_validate (str, -1, NULL))
					g_warning (_("Invalid UTF-8 format!"));
				else {
					priv->prefix = g_strdup (str);
					priv->prefix_len = strlen (str);
					priv->prefix_clen = g_utf8_strlen (str, -1);
				}
			}
			adjust_display (entry, otext);
			g_free (otext);
			gdaui_entry_set_width_chars (entry, priv->max_width);
			break;
		case PROP_SUFFIX:
			otext = gdaui_entry_get_text (entry);
			g_free (priv->suffix);
			priv->suffix = NULL;
			priv->suffix_len = 0;

			str = g_value_get_string (value);
			if (str) {
				if (! g_utf8_validate (str, -1, NULL))
					g_warning (_("Invalid UTF-8 format!"));
				else {
					priv->suffix = g_strdup (str);
					priv->suffix_len = strlen (str);
					priv->suffix_clen = g_utf8_strlen (str, -1);
				}
			}
			adjust_display (entry, otext);
			g_free (otext);
			gdaui_entry_set_width_chars (entry, priv->max_width);
			break;
		case PROP_MAXLEN:
			priv->maxlen = g_value_get_int (value);
			otext = gdaui_entry_get_text (entry);
			adjust_display (entry, otext);
			g_free (otext);
			gdaui_entry_set_width_chars (entry, priv->max_width);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gdaui_entry_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdauiEntry *entry;

	entry = GDAUI_ENTRY (object);
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	switch (param_id) {
		case PROP_PREFIX:
			g_value_set_string (value, priv->prefix);
			break;
		case PROP_SUFFIX:
			g_value_set_string (value, priv->suffix);
			break;
		case PROP_MAXLEN:
			g_value_set_int (value, priv->maxlen);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
signal_handlers_block (GdauiEntry *entry)
{
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	priv->internal_changes++;
	g_signal_handlers_block_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_block_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
}

static void
signal_handlers_unblock (GdauiEntry *entry)
{
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
	priv->internal_changes--;
}

/*
 * truncate_utf8_string
 * @text: a string, not %NULL
 * @pos: the position where the string wil be truncated <=> text[pos]=0 for ASCII
 *
 * Returns: @text
 */
static gchar *
truncate_utf8_string (gchar *text, gint pos)
{
	gchar *ptr;
	gint i;
	for (ptr = text, i = 0; (i < pos) && ptr && *ptr; ptr = g_utf8_next_char (ptr), i++);
	if (i == pos)
		*ptr = 0;
	return text;
}

/*
 * Computes new new display
 *
 * WARNING: @existing_text may be modified!!!
 */
static void
adjust_display (GdauiEntry *entry, gchar *existing_text)
{
	gchar *tmp;
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);

	if (!priv->isnull) {
		signal_handlers_block (entry);
		if (g_utf8_strlen (existing_text, -1) > priv->maxlen)
			truncate_utf8_string (existing_text, priv->maxlen);
		tmp = g_strdup_printf ("%s%s%s",
				       priv->prefix ? priv->prefix : "",
				       existing_text ? existing_text : "",
				       priv->suffix ? priv->suffix : "");
		
		gtk_entry_set_text (GTK_ENTRY (entry), tmp); /* emits a "changed" signal */
		g_free (tmp);
		signal_handlers_unblock (entry);
	}
}

/**
 * gdaui_entry_new:
 * @prefix: (nullable): a prefix (not modifiable) string, or %NULL
 * @suffix: (nullable): a suffix (not modifiable) string, or %NULL
 *
 * Creates a new #GdauiEntry widget.
 *
 * Returns: (transfer full): the newly created #GdauiEntry widget.
 */
GtkWidget*
gdaui_entry_new (const gchar *prefix, const gchar *suffix)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_ENTRY, "prefix", prefix, "suffix", suffix, NULL);
	return GTK_WIDGET (obj);
}

/**
 * gdaui_entry_set_max_length:
 * @entry: a #GdauiEntry.
 * @max: the maximum length of the entry, or 0 for no maximum.
 *
 * Sets the maximum allowed length of the contents of the widget.
 * If the current contents are longer than the given length, then they will be truncated to fit.
 *
 * The difference with gtk_entry_set_max_length() is that the max length does not take into account
 * the prefix and/or suffix parts which may have been set.
 */
void
gdaui_entry_set_max_length (GdauiEntry *entry, gint max)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));

	g_object_set (G_OBJECT (entry), "max-length", max, NULL);
}

/**
 * gdaui_entry_get_text:
 * @entry: a #GdauiEntry.
 *
 * Get a new string containing the contents of the widget as a string without the
 * prefix and/or suffix and/or format if they have been specified. This method differs
 * from calling gtk_entry_get_text() since the latest will return the complete text
 * in @entry including prefix and/or suffix and/or format.
 *
 * Note: %NULL may be returned if this method is called while the widget is working on some
 * internal modifications, or if gdaui_entry_set_text() was called with a %NULL
 * as its @text argument.
 *
 * Returns: a new string, or %NULL
 */
gchar *
gdaui_entry_get_text (GdauiEntry *entry)
{
	gchar *text;

	g_return_val_if_fail (GDAUI_IS_ENTRY (entry), NULL);
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);

	if (priv->isnull)
		text = NULL;
	else {
		const gchar *ctext;
		gint len;
		ctext = gtk_entry_get_text (GTK_ENTRY (entry));
		if (ctext) {
			len = strlen (ctext);
			text = g_strdup (ctext);
			if (priv->prefix) {
				len -= priv->prefix_len;
				memmove (text, text + priv->prefix_len, len+1);
			}
			if (priv->suffix) {
				len -= priv->suffix_len;
				text [len] = 0;
			}
		}
		else
			text = g_strdup ("");
	}

	return text;
}

/**
 * gdaui_entry_set_text:
 * @entry: a #GdauiEntry widget
 * @text: (nullable): the text to set into @entry, or %NULL
 *
 * Sets @text into @entry. 
 *
 * As a side effect, if @text is %NULL, then the entry will
 * be completely empty, whereas if @text is the empty string (""), then
 * @entry will display the prefix and/or suffix and/or format string if they have
 * been set. Except this case, calling this method is similar to calling
 * gtk_entry_set_text()
 */
void
gdaui_entry_set_text (GdauiEntry *entry, const gchar *text)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);

	if (text) {
		priv->isnull = TRUE;
		signal_handlers_block (entry);
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		signal_handlers_unblock (entry);
		priv->internal_changes++;

		if (priv->internal_changes ==  1) {
			/* function has been called by "external" programmer,
			 * emits the "insert-text" signal which is treated */
			gtk_entry_set_text (GTK_ENTRY (entry), text);
		}
		else {
			/* function has been called by a subsequent call of one of
			 * the descendant's implementation */
			gint pos = 0;
			internal_insert_text (GTK_EDITABLE (entry), text, g_utf8_strlen (text, -1), &pos, TRUE);
		}
		priv->isnull = FALSE; /* in case it has not been set */
		priv->internal_changes--;
	}
	else {
		priv->isnull = TRUE;
		signal_handlers_block (entry);
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		signal_handlers_unblock (entry);
	}
	g_signal_emit_by_name (entry, "changed");
}

/**
 * gdaui_entry_set_prefix:
 * @entry: a #GdauiEntry widget
 * @prefix: a prefix string
 *
 * Sets @prefix as a prefix string of @entry: that string will always be displayed in the
 * text entry, will not be modifiable, and won't be part of the returned text
 */
void
gdaui_entry_set_prefix (GdauiEntry *entry, const gchar *prefix)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));

	g_object_set (G_OBJECT (entry), "prefix", prefix, NULL);
}

/**
 * gdaui_entry_set_suffix:
 * @entry: a #GdauiEntry widget
 * @suffix: a suffix string
 *
 * Sets @suffix as a suffix string of @entry: that string will always be displayed in the
 * text entry, will not be modifiable, and won't be part of the returned text
 */
void
gdaui_entry_set_suffix (GdauiEntry *entry, const gchar *suffix)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));

	g_object_set (G_OBJECT (entry), "suffix", suffix, NULL);
}


/**
 * gdaui_entry_set_width_chars:
 * @entry: a #GdauiEntry widget
 * @max_width: maximum width, or -1
 *
 * Sets @entry's width in characters, without taking into account
 * any prefix or suffix (which will automatically be handled). If you want to take
 * a prefix or suffix into account direclty, then use gtk_entry_set_width_chars()
 */
void
gdaui_entry_set_width_chars (GdauiEntry *entry, gint max_width)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
	priv->max_width = max_width;
	if (max_width < 0) {
		gtk_entry_set_width_chars (GTK_ENTRY (entry), -1);
		gtk_widget_set_hexpand (GTK_WIDGET (entry), TRUE);
	}
	else {
		max_width += (priv->prefix_clen > 0 ? priv->prefix_clen + 1 : 0);
		max_width += (priv->suffix_clen > 0 ? priv->suffix_clen + 1 : 0);
		gtk_entry_set_width_chars (GTK_ENTRY (entry), max_width);
		gtk_entry_set_max_width_chars (GTK_ENTRY (entry), max_width);
		gtk_widget_set_hexpand (GTK_WIDGET (entry), FALSE);
	}
}

/*
 * callbacks
 */

static void
changed_cb (GtkEditable *editable, G_GNUC_UNUSED gpointer data)
{
        GdauiEntry *entry = (GdauiEntry*) editable;
        GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);
        if (priv->internal_changes > 0)
                g_signal_stop_emission_by_name (editable, "changed");
}

static void
delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, G_GNUC_UNUSED gpointer data)
{
	const gchar *otext = NULL;
	gint len = 0;
	gint nstart = start_pos, nend = end_pos;
	GdauiEntry *entry = GDAUI_ENTRY (editable);
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);

	signal_handlers_block (entry);
	if (priv->prefix) {
		if (nstart < priv->prefix_clen)
			nstart = priv->prefix_clen;
	}
	if (nend < 0) {
		otext = gtk_entry_get_text ((GtkEntry*) entry);
		len = g_utf8_strlen (otext, -1);
		nend = len;
	}

	if (nend - nstart < 1) {
		g_signal_stop_emission_by_name (editable, "delete-text");
		signal_handlers_unblock (entry);
		return;
	}

	if (priv->suffix) {
		if (!otext) {
			otext = gtk_entry_get_text ((GtkEntry*) entry);
			len = g_utf8_strlen (otext, -1);
		}
		if (nend - nstart == 1) {
			if ((nstart >= len - priv->suffix_clen)) {
				nstart = len - priv->suffix_clen - 1;
				nend = nstart + 1;
				g_signal_stop_emission_by_name (editable, "delete-text");
				signal_handlers_unblock (entry);
				gtk_editable_set_position (editable, nend);
				gtk_editable_delete_text (editable, nstart, nend);
				return;
			}
		}
		if (nend > len - priv->suffix_clen)
			nend = len - priv->suffix_clen;
	}

	if (GDAUI_ENTRY_GET_CLASS (editable)->assume_delete) {
		g_signal_stop_emission_by_name (editable, "delete-text");
		GDAUI_ENTRY_GET_CLASS (editable)->assume_delete (entry, nstart - priv->prefix_clen,
								 nend - priv->prefix_clen,
								 priv->prefix_clen);
		//g_print ("Subclass assumes text delete\n");
	}
	else if ((nstart != start_pos) || (nend != end_pos)) {
		g_signal_stop_emission_by_name (editable, "delete-text");
		if (nstart != nend)
			gtk_editable_delete_text (editable, nstart, nend);
	}
	
	signal_handlers_unblock (entry);
	g_signal_emit_by_name (entry, "changed");
}

static void
internal_insert_text (GtkEditable *editable, const gchar *text, gint text_length, gint *position,
		      gboolean handle_default_insert)
{
	/*g_print ("%s ([%s] @ %d)\n", __FUNCTION__, text, *position);*/
	const gchar *otext;
	gint clen;
	GdauiEntry *entry = GDAUI_ENTRY (editable);
	gint text_clen;
	gint start;
	GdauiEntryPrivate *priv = gdaui_entry_get_instance_private (entry);

	if (gtk_editable_get_selection_bounds (editable, &start, NULL))
		*position = start;

	signal_handlers_block (entry);

 	if (priv->isnull) {
		gchar *etext = NULL;
		priv->isnull = FALSE;
		if (GDAUI_ENTRY_GET_CLASS (editable)->get_empty_text)
			etext = GDAUI_ENTRY_GET_CLASS (editable)->get_empty_text (entry);
		adjust_display (entry, etext ? etext : "");
		g_free (etext);
	}

	otext = gtk_entry_get_text ((GtkEntry*) entry);
	clen = g_utf8_strlen (otext, -1);

	/* adjust insert position */
	if (priv->prefix) {
		if (*position < priv->prefix_clen)
			*position = priv->prefix_clen;
	}
	if (priv->suffix) {
		if (*position > clen - priv->suffix_clen)
			*position = clen - priv->suffix_clen;
	}

	/* test if the whole insertion is Ok */
	text_clen = g_utf8_strlen (text, text_length);
	if (GDAUI_ENTRY_GET_CLASS (editable)->assume_insert) {
		/* Subclass assumes text insert */
		gint pos = *position - priv->prefix_clen;
		GDAUI_ENTRY_GET_CLASS (editable)->assume_insert (entry, text, text_length,
								 &pos, priv->prefix_clen);
		*position = pos + priv->prefix_clen;

		g_signal_stop_emission_by_name (editable, "insert-text");
		signal_handlers_unblock (entry);
		g_signal_emit_by_name (entry, "changed");
	}
	else if (clen - priv->prefix_clen - priv->suffix_clen + text_clen > priv->maxlen) {
		/* text to insert is too long and needs to be truncated */
		gchar *itext;
		gint nallowed;
		nallowed = priv->maxlen - (clen - priv->prefix_clen - priv->suffix_clen);
		g_signal_stop_emission_by_name (editable, "insert-text");
		itext = g_strdup (text);
		itext [nallowed] = 0; /* FIXME: convert nallowed to gchar */
		/*g_print ("Corrected by length insert text: [%s]\n", itext);*/
		if (*itext)
			gtk_editable_insert_text (editable, itext, nallowed, position);
		g_free (itext);

		signal_handlers_unblock (entry);
		g_signal_emit_by_name (entry, "changed");
	}
	else {
		if (handle_default_insert) {
			priv->internal_changes++;
			gtk_editable_insert_text (editable, text, text_length, position);
			priv->internal_changes--;
			signal_handlers_unblock (entry);
			g_signal_emit_by_name (entry, "changed");
		}
		else
			signal_handlers_unblock (entry);
	}
}

static void
insert_text_cb (GtkEditable *editable, const gchar *text, gint text_length, gint *position,
		G_GNUC_UNUSED gpointer data)
{
	internal_insert_text (editable, text, text_length, position, FALSE);
}
