/* gdaui-entry.c
 *
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <string.h>

#include "gdaui-entry.h"

struct _GdauiEntryPrivate {
	gchar   *prefix;
	gint     prefix_len;
	gint     prefix_clen; /* UTF8 len */
	gchar   *suffix;
	gint     suffix_len;
	gint     suffix_clen; /* UTF8 len */
	gint     maxlen; /* UTF8 len */
	gboolean isnull;
	guchar   internal_changes;
};

#define ENTER_INTERNAL_CHANGES(entry) (entry)->priv->internal_changes ++
#define LEAVE_INTERNAL_CHANGES(entry) (entry)->priv->internal_changes --

static void gdaui_entry_class_init   (GdauiEntryClass *klass);
static void gdaui_entry_init         (GdauiEntry *entry);
static void gdaui_entry_finalize     (GObject *object);
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
	ENTER_INTERNAL_CHANGES(entry);
}

void
_gdaui_entry_unblock_changes (GdauiEntry *entry)
{
	LEAVE_INTERNAL_CHANGES(entry);
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


static GObjectClass *parent_class = NULL;

GType
gdaui_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GdauiEntryClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_entry_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiEntry),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gdaui_entry_init,
		};
		
		type = g_type_register_static (GTK_TYPE_ENTRY, "GdauiEntry", &type_info, 0);
	}

	return type;
}

static void
gdaui_entry_class_init (GdauiEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_entry_finalize;
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
	entry->priv = g_new0 (GdauiEntryPrivate, 1);
	entry->priv->prefix = NULL;
	entry->priv->suffix = NULL;
	entry->priv->maxlen = 65535; /* eg. unlimited for GtkEntry */
	entry->priv->isnull = TRUE;
	entry->priv->internal_changes = 0;

	g_signal_connect (G_OBJECT (entry), "delete-text",
			  G_CALLBACK (delete_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "insert-text",
			  G_CALLBACK (insert_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (changed_cb), NULL);
}

static void 
gdaui_entry_finalize (GObject *object)
{
	GdauiEntry *entry;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDAUI_IS_ENTRY (object));

        entry = GDAUI_ENTRY (object);
        if (entry->priv) {
		g_free (entry->priv->prefix);
		g_free (entry->priv->suffix);
                g_free (entry->priv);
                entry->priv = NULL;
        }

        /* parent class */
        parent_class->finalize (object);
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
        if (entry->priv) {
                switch (param_id) {
                case PROP_PREFIX:
			otext = gdaui_entry_get_text (entry);
			g_free (entry->priv->prefix);
			entry->priv->prefix = NULL;
			entry->priv->prefix_len = 0;

			str = g_value_get_string (value);
			if (str) {
				if (! g_utf8_validate (str, -1, NULL))
					g_warning (_("Invalid UTF-8 format!"));
				else {
					entry->priv->prefix = g_strdup (str);
					entry->priv->prefix_len = strlen (str);
					entry->priv->prefix_clen = g_utf8_strlen (str, -1);
				}
			}
			adjust_display (entry, otext);
			g_free (otext);
                        break;
                case PROP_SUFFIX:
			otext = gdaui_entry_get_text (entry);
			g_free (entry->priv->suffix);
			entry->priv->suffix = NULL;
			entry->priv->suffix_len = 0;

			str = g_value_get_string (value);
			if (str) {
				if (! g_utf8_validate (str, -1, NULL))
					g_warning (_("Invalid UTF-8 format!"));
				else {
					entry->priv->suffix = g_strdup (str);
					entry->priv->suffix_len = strlen (str);
					entry->priv->suffix_clen = g_utf8_strlen (str, -1);
				}
			}
			adjust_display (entry, otext);
			g_free (otext);
                        break;
		case PROP_MAXLEN:
			entry->priv->maxlen = g_value_get_int (value);
			otext = gdaui_entry_get_text (entry);
			adjust_display (entry, otext);
			g_free (otext);
			break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
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
        if (entry->priv) {
                switch (param_id) {
                case PROP_PREFIX:
			g_value_set_string (value, entry->priv->prefix);
                        break;
                case PROP_SUFFIX:
			g_value_set_string (value, entry->priv->suffix);
                        break;
		case PROP_MAXLEN:
			g_value_set_int (value, entry->priv->maxlen);
			break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}

static void
signal_handlers_block (GdauiEntry *entry)
{
	ENTER_INTERNAL_CHANGES (entry);
	g_signal_handlers_block_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_block_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
}

static void
signal_handlers_unblock (GdauiEntry *entry)
{
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
	LEAVE_INTERNAL_CHANGES (entry);
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

	if (!entry->priv->isnull) {
		signal_handlers_block (entry);
		if (g_utf8_strlen (existing_text, -1) > entry->priv->maxlen)
			truncate_utf8_string (existing_text, entry->priv->maxlen);
		tmp = g_strdup_printf ("%s%s%s",
				       entry->priv->prefix ? entry->priv->prefix : "",
				       existing_text ? existing_text : "",
				       entry->priv->suffix ? entry->priv->suffix : "");
		
		gtk_entry_set_text (GTK_ENTRY (entry), tmp); /* emits a "changed" signal */
		g_free (tmp);
		signal_handlers_unblock (entry);
	}
}

/**
 * gdaui_entry_new:
 * @prefix: a prefix (not modifiable) string, or %NULL
 * @suffix: a suffix (not modifiable) string, or %NULL
 *
 * Creates a new #GdauiEntry widget.
 *
 * Returns: the newly created #GdauiEntry widget.
 */
GtkWidget*
gdaui_entry_new (const gchar *prefix, const gchar *suffix)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_ENTRY, "prefix", prefix, "suffix", suffix, NULL);
	return GTK_WIDGET (obj);
}

/**
 * gdaui_entry_set_max_length
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

	if (entry->priv->isnull)
		text = NULL;
	else {
		const gchar *ctext;
		gint len;
		ctext = gtk_entry_get_text (GTK_ENTRY (entry));
		if (ctext) {
			len = strlen (ctext);
			text = g_strdup (ctext);
			if (entry->priv->prefix) {
				len -= entry->priv->prefix_len;
				g_memmove (text, text + entry->priv->prefix_len, len+1);
			}
			if (entry->priv->suffix) {
				len -= entry->priv->suffix_len;
				text [len] = 0;
			}
		}
		else
			text = g_strdup ("");
	}

	return text;
}

/**
 * gdaui_entry_set_text
 * @entry: a #GdauiEntry widget
 * @text: the text to set into @entry, or %NULL
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

	if (text) {
		entry->priv->isnull = TRUE;
		signal_handlers_block (entry);
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		signal_handlers_unblock (entry);
		ENTER_INTERNAL_CHANGES(entry);
		gtk_entry_set_text (GTK_ENTRY (entry), text); /* emits the "insert-text" signal which is treated */
		entry->priv->isnull = FALSE; /* in case it has not been set */
		LEAVE_INTERNAL_CHANGES(entry);
		g_signal_emit_by_name (entry, "changed");
	}
	else {
		entry->priv->isnull = TRUE;
		signal_handlers_block (entry);
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		signal_handlers_unblock (entry);
		g_signal_emit_by_name (entry, "changed");
	}
}

/**
 * gdaui_entry_set_prefix
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
 * gdaui_entry_set_suffix
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
 * gdaui_entry_set_width_chars
 * @entry: a #GdauiEntry widget
 * @max_width: maximum width, or -1
 *
 * Sets @entry's maximum width in characters, without taking into account
 * any prefix or suffix (which will automatically be handled). If you want to take
 * a prefix or suffix into account direclty, then use gtk_entry_set_width_chars()
 */
void
gdaui_entry_set_width_chars (GdauiEntry *entry, gint max_width)
{
	g_return_if_fail (GDAUI_IS_ENTRY (entry));
	if (max_width < 0)
		gtk_entry_set_width_chars (GTK_ENTRY (entry), -1);
	else {
		max_width += entry->priv->prefix_clen;
		max_width += entry->priv->suffix_clen;
		gtk_entry_set_width_chars (GTK_ENTRY (entry), max_width);
	}
}

/*
 * callbacks
 */

static void
changed_cb (GtkEditable *editable, gpointer data)
{
        GdauiEntry *entry = (GdauiEntry*) editable;
        if (entry->priv->internal_changes > 0)
                g_signal_stop_emission_by_name (editable, "changed");
}

static void
delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, gpointer data)
{
	const gchar *otext = NULL;
	gint len;
	gint nstart = start_pos, nend = end_pos;
	GdauiEntry *entry = GDAUI_ENTRY (editable);

	signal_handlers_block (entry);
	if (entry->priv->prefix) {
		if (nstart < entry->priv->prefix_clen)
			nstart = entry->priv->prefix_clen;
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

	if (entry->priv->suffix) {
		if (!otext) {
			otext = gtk_entry_get_text ((GtkEntry*) entry);
			len = g_utf8_strlen (otext, -1);
		}
		if (nend - nstart == 1) {
			if ((nstart >= len - entry->priv->suffix_clen)) {
				nstart = len - entry->priv->suffix_clen - 1;
				nend = nstart + 1;
				g_signal_stop_emission_by_name (editable, "delete-text");
				signal_handlers_unblock (entry);
				gtk_editable_set_position (editable, nend);
				gtk_editable_delete_text (editable, nstart, nend);
				return;
			}
		}
		if (nend > len - entry->priv->suffix_clen)
			nend = len - entry->priv->suffix_clen;
	}

	if (GDAUI_ENTRY_GET_CLASS (editable)->assume_delete) {
		g_signal_stop_emission_by_name (editable, "delete-text");
		GDAUI_ENTRY_GET_CLASS (editable)->assume_delete (entry, nstart - entry->priv->prefix_clen,
								 nend - entry->priv->prefix_clen,
								 entry->priv->prefix_clen);
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
insert_text_cb (GtkEditable *editable, const gchar *text, gint text_length, gint *position, gpointer data)
{
	const gchar *otext;
	gint clen;
	GdauiEntry *entry = GDAUI_ENTRY (editable);
	gint text_clen;

	signal_handlers_block (entry);

 	if (entry->priv->isnull) {
		gchar *etext = NULL;
		entry->priv->isnull = FALSE;
		if (GDAUI_ENTRY_GET_CLASS (editable)->get_empty_text)
			etext = GDAUI_ENTRY_GET_CLASS (editable)->get_empty_text (entry);
		adjust_display (entry, etext ? etext : "");
		g_free (etext);
	}

	otext = gtk_entry_get_text ((GtkEntry*) entry);
	clen = g_utf8_strlen (otext, -1);

	/* adjust insert position */
	if (entry->priv->prefix) {
		if (*position < entry->priv->prefix_clen)
			*position = entry->priv->prefix_clen;
	}
	if (entry->priv->suffix) {
		if (*position > clen - entry->priv->suffix_clen)
			*position = clen - entry->priv->suffix_clen;
	}

	/* test if the whole insertion is Ok */
	text_clen = g_utf8_strlen (text, text_length);
	if (clen - entry->priv->prefix_clen - entry->priv->suffix_clen + text_clen > entry->priv->maxlen) {
		gchar *itext;
		gint nallowed;
		nallowed = entry->priv->maxlen - (clen - entry->priv->prefix_clen - entry->priv->suffix_clen);
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
	else if (GDAUI_ENTRY_GET_CLASS (editable)->assume_insert) {
		g_signal_stop_emission_by_name (editable, "insert-text");
		//g_print ("Subclass assumes text insert\n");
		gint pos = *position - entry->priv->prefix_clen;
		GDAUI_ENTRY_GET_CLASS (editable)->assume_insert (entry, text, text_length,
								 &pos, entry->priv->prefix_clen);
		*position = pos + entry->priv->prefix_clen;

		signal_handlers_unblock (entry);
		g_signal_emit_by_name (entry, "changed");
	}
	else
		signal_handlers_unblock (entry);
}
