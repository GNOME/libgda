/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <string.h>

#include "gdaui-formatted-entry.h"

struct _GdauiFormattedEntryPrivate {
	gchar   *format; /* UTF-8! */
	gint     format_clen; /* in characters, not gchar */
	gchar   *mask; /* ASCII! */
	gint     mask_len; /* in gchar */

	GdauiFormattedEntryInsertFunc insert_func;
	gpointer                      insert_func_data;
};

static void gdaui_formatted_entry_class_init   (GdauiFormattedEntryClass *klass);
static void gdaui_formatted_entry_init         (GdauiFormattedEntry *entry);
static void gdaui_formatted_entry_finalize     (GObject *object);
static void gdaui_formatted_entry_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_formatted_entry_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);
static gchar *gdaui_formatted_entry_get_empty_text (GdauiEntry *entry);
static void gdaui_formatted_entry_assume_insert (GdauiEntry *entry, const gchar *text, gint text_length, gint *virt_pos, gint offset);
static void gdaui_formatted_entry_assume_delete (GdauiEntry *entry, gint virt_start_pos, gint virt_end_pos, gint offset);

/* properties */
enum
{
        PROP_0,
	PROP_FORMAT,
	PROP_MASK
};

static GObjectClass *parent_class = NULL;

GType
gdaui_formatted_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GdauiFormattedEntryClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_formatted_entry_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiFormattedEntry),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gdaui_formatted_entry_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY, "GdauiFormattedEntry", &type_info, 0);
	}

	return type;
}

static void
gdaui_formatted_entry_class_init (GdauiFormattedEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gdaui_formatted_entry_finalize;
	GDAUI_ENTRY_CLASS (klass)->assume_insert = gdaui_formatted_entry_assume_insert;
	GDAUI_ENTRY_CLASS (klass)->assume_delete = gdaui_formatted_entry_assume_delete;
	GDAUI_ENTRY_CLASS (klass)->get_empty_text = gdaui_formatted_entry_get_empty_text;

	/* Properties */
        object_class->set_property = gdaui_formatted_entry_set_property;
        object_class->get_property = gdaui_formatted_entry_get_property;

        g_object_class_install_property (object_class, PROP_FORMAT,
                                         g_param_spec_string ("format", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_MASK,
                                         g_param_spec_string ("mask", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gdaui_formatted_entry_init (GdauiFormattedEntry *entry)
{
	entry->priv = g_new0 (GdauiFormattedEntryPrivate, 1);
	entry->priv->format = NULL;
	entry->priv->mask = NULL;
	entry->priv->insert_func = NULL;
	entry->priv->insert_func_data = NULL;
}

static void 
gdaui_formatted_entry_finalize (GObject *object)
{
	GdauiFormattedEntry *entry;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDAUI_IS_ENTRY (object));

        entry = GDAUI_FORMATTED_ENTRY (object);
        if (entry->priv) {
		g_free (entry->priv->format);
		g_free (entry->priv->mask);
                g_free (entry->priv);
                entry->priv = NULL;
        }

        /* parent class */
        parent_class->finalize (object);
}

static void 
gdaui_formatted_entry_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiFormattedEntry *entry;
	const gchar *str;
	gchar *otext;

        entry = GDAUI_FORMATTED_ENTRY (object);
	otext = gdaui_entry_get_text (GDAUI_ENTRY (entry));
        if (entry->priv) {
                switch (param_id) {
                case PROP_FORMAT:
			g_free (entry->priv->format);
			entry->priv->format = NULL;
			entry->priv->format_clen = 0;

			str = g_value_get_string (value);
			if (str) {
				if (! g_utf8_validate (str, -1, NULL))
					g_warning (_("Invalid UTF-8 format!"));
				else {
					entry->priv->format = g_strdup (str);
					entry->priv->format_clen = g_utf8_strlen (str, -1);
					gdaui_entry_set_width_chars (GDAUI_ENTRY (entry),
								     entry->priv->format_clen);
				}
			}
                        break;
                case PROP_MASK:
			g_free (entry->priv->mask);
			entry->priv->mask = NULL;
			entry->priv->mask_len = 0;

			str = g_value_get_string (value);
			if (str) {
				entry->priv->mask = g_strdup (str);
				entry->priv->mask_len = strlen (str);
			}
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
	gdaui_entry_set_text (GDAUI_ENTRY (entry), otext);
	g_free (otext);
}

static void
gdaui_formatted_entry_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdauiFormattedEntry *entry;

        entry = GDAUI_FORMATTED_ENTRY (object);
        if (entry->priv) {
                switch (param_id) {
                case PROP_FORMAT:
			g_value_set_string (value, entry->priv->format);
                        break;
                case PROP_MASK:
			g_value_set_string (value, entry->priv->mask);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}

/*
 * is_writable
 * @fentry:
 * @pos: the position (in characters) in @fentry->priv->format
 * @ptr: the character (in @fentry->priv->format)
 *
 * Returns: %TRUE if it is a writable loaction
 */
static gboolean
is_writable (GdauiFormattedEntry *fentry, gint pos, const gchar *ptr)
{
	if (((*ptr == '0') ||
	     (*ptr == '9') ||
	     (*ptr == '@') ||
	     (*ptr == '^') ||
	     (*ptr == '#') ||
	     (*ptr == '*')) &&
	    (!fentry->priv->mask ||
	     (fentry->priv->mask &&
	      (pos < fentry->priv->mask_len) &&
	      (fentry->priv->mask [pos] != ' '))))
		return TRUE;
	else
		return FALSE;
}

/*
 * is_allowed
 * @fentry:
 * @ptr: the character (in @fentry->priv->format)
 * @wc: the character to be inserted
 *
 * Returns: %TRUE if @wc can be used to replace @ptr
 */
static gboolean
is_allowed (G_GNUC_UNUSED GdauiFormattedEntry *fentry, const gchar *ptr, const gunichar wc, gunichar *out_wc)
{
/* TODO: Use this?
	gunichar fwc;

	fwc = g_utf8_get_char (ptr);
*/
	*out_wc = wc;
	if (*ptr == '0')
		return g_unichar_isdigit (wc);
	else if (*ptr == '9')
		return g_unichar_isdigit (wc) && (wc != g_utf8_get_char ("0"));
	else if (*ptr == '@')
		return g_unichar_isalpha (wc);
	else if (*ptr == '^') {
		gboolean isa = g_unichar_isalpha (wc);
		if (isa)
			*out_wc = g_unichar_toupper (wc);
		return isa;
	}
	else if (*ptr == '#')
		return g_unichar_isalnum (wc);
	else if (*ptr == '*')
		return g_unichar_isprint (wc);
	else {
		g_warning (_("Unknown format character starting at %s"), ptr);
		return FALSE;
	}
}

static gchar *
gdaui_formatted_entry_get_empty_text (GdauiEntry *entry)
{
	GdauiFormattedEntry *fentry;

	fentry = (GdauiFormattedEntry*) entry;
	if (fentry->priv->format) {
		GString *string;

		string = g_string_new ("");
		gchar *ptr;
		gint i;
		for (ptr = fentry->priv->format, i = 0;
		     ptr && *ptr;
		     ptr = g_utf8_next_char (ptr), i++) {
			if (is_writable (fentry, i, ptr))
				g_string_append_c (string, '_');
			else {
				gunichar wc;
				wc = g_utf8_get_char (ptr);
				g_string_append_unichar (string, wc);
			}
		}
		return g_string_free (string, FALSE);
	}
	else
		return NULL;
}

static void
gdaui_formatted_entry_assume_insert (GdauiEntry *entry, const gchar *text, G_GNUC_UNUSED gint text_length,
				     gint *virt_pos, gint offset)
{
	GdauiFormattedEntry *fentry;
	gint i, pos;

	fentry = (GdauiFormattedEntry*) entry;

	const gchar *ptr, *fptr;
	pos = *virt_pos;
	for (fptr = fentry->priv->format, i = 0;
	     (i < pos) && fptr && *fptr;
	     fptr = g_utf8_next_char (fptr), i++);
	
	if (i != pos)
		return;

	_gdaui_entry_block_changes (entry);
	gboolean inserted = FALSE;
	gunichar wc;
	for (ptr = text, i = 0; ptr && *ptr && *fptr; ptr = g_utf8_next_char (ptr)) {
		while ((pos < fentry->priv->format_clen) &&
		       !is_writable (fentry, pos, fptr)) {
			fptr = g_utf8_next_char (fptr);
			if (!fptr || !*fptr)
				return;
			pos++;
		}
		
		wc = g_utf8_get_char (ptr);
		if ((pos < fentry->priv->format_clen) &&
		    is_allowed (fentry, fptr, wc, &wc)){
			/* Ok, insert *ptr (<=> text[i] if it was ASCII) */
			gint rpos = pos + offset;
			gint usize;
			gchar buf [6];

			usize = g_unichar_to_utf8 (wc, buf);
			gtk_editable_delete_text ((GtkEditable*) entry, rpos, rpos + 1);
			gtk_editable_insert_text ((GtkEditable*) entry, buf, usize, &rpos);
			inserted = TRUE;
			pos++;
			fptr = g_utf8_next_char (fptr);
		}
	}
	_gdaui_entry_unblock_changes (entry);
	*virt_pos = pos;

	if (!inserted && fentry->priv->insert_func) {
		ptr = g_utf8_next_char (text);
		if (!*ptr) {
			wc = g_utf8_get_char (text);
			fentry->priv->insert_func (fentry, wc, *virt_pos, fentry->priv->insert_func_data);
		}
	}
}

static void
gdaui_formatted_entry_assume_delete (GdauiEntry *entry, gint virt_start_pos, gint virt_end_pos, gint offset)
{
	GdauiFormattedEntry *fentry;
	gchar *fptr;
	gint i;

	fentry = (GdauiFormattedEntry*) entry;

#ifdef GDA_DEBUG
	gint clen;
	gchar *otext;
	otext = gdaui_entry_get_text (entry);
	if (otext) {
		clen = g_utf8_strlen (otext, -1);
		g_assert (clen == fentry->priv->format_clen);
		g_free (otext);
	}
#endif

	g_assert (virt_end_pos <= fentry->priv->format_clen);
	
	/* move fptr to the @virt_start_pos in fentry->priv->format */
	for (fptr = fentry->priv->format, i = 0;
	     (i < virt_start_pos) && *fptr;
	     fptr = g_utf8_next_char (fptr), i++);
	if (i != virt_start_pos)
		return;

	_gdaui_entry_block_changes (entry);
	for (;
	     (i < virt_end_pos) && fptr && *fptr;
	     fptr = g_utf8_next_char (fptr), i++) {
		if (!is_writable (fentry, i, fptr)) {
			if (virt_end_pos - virt_start_pos == 1) {
				gint npos;
				npos = gtk_editable_get_position ((GtkEditable*) entry);
				while ((i >= 0) && !is_writable (fentry, i, fptr)) {
					virt_start_pos --;
					virt_end_pos --;
					i--;
					fptr = g_utf8_find_prev_char (fentry->priv->format, fptr);
					npos --;
				}
				if (i < 0)
					return;
				else
					gtk_editable_set_position ((GtkEditable*) entry, npos);
			}
			else
				continue;
		}
		gint rpos = i + offset;
		gtk_editable_delete_text ((GtkEditable*) entry, rpos, rpos + 1);
		gtk_editable_insert_text ((GtkEditable*) entry, "_", 1, &rpos);
	}
	_gdaui_entry_unblock_changes (entry);
}

/**
 * gdaui_formatted_entry_new:
 * @format: a format string
 * @mask: (allow-none): a mask string, or %NULL
 *
 * Creates a new #GdauiFormattedEntry widget.
 *
 * Characters in @format are of two types:
 *   writeable: writeable characters which will be replaced with and underscore and where text will be entered
 *   fixed: every other characters are fixed characters, where text cant' be edited, and will be displayed AS IS
 *
 * Possible values for writeable characters are:
 * <itemizedlist>
 *   <listitem><para>'0': digits</para></listitem>
 *   <listitem><para>'9': digits excluded 0</para></listitem>
 *   <listitem><para>'@': alpha</para></listitem>
 *   <listitem><para>'^': alpha converted to upper case</para></listitem>
 *   <listitem><para>'#': alphanumeric</para></listitem>
 *   <listitem><para>'*': any char</para></listitem>
 * </itemizedlist>
 *
 * if @mask is not %NULL, then it should only contains the follogin characters, which are used side by side with
 * @format's characters:
 * <itemizedlist>
 *   <listitem><para>'_': the corresponding character in @format is actually used as a writable character</para></listitem>
 *   <listitem><para>'-': the corresponding character in @format is actually used as a writable character, but
 *              the character will be removed from gdaui_formatted_entry_get_text()'s result if it was not
 *              filled by the user</para></listitem>
 *   <listitem><para>' ': the corresponding character in @format will not be considered as a writable character
 *              but as a non writable character</para></listitem>
 * </itemizedlist>
 * it is then interpreted in the following way: for a character C in @format, if the character at the same
 * position in @mask is the space character (' '), then C will not interpreted as a writable format
 * character as defined above. @mask does not be to have the same length as @format.
 *
 * Returns: (transfer full): the newly created #GdauiFormattedEntry widget.
 */
GtkWidget*
gdaui_formatted_entry_new (const gchar *format, const gchar *mask)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_FORMATTED_ENTRY, "format", format, "mask", mask, NULL);
	return GTK_WIDGET (obj);
}

/**
 * gdaui_formatted_entry_get_text:
 * @entry: a #GdauiFormattedEntry widget
 *
 * Get @entry's contents. This function is similar to gdaui_get_text() except
 * that it optionnally uses the information contained in @mask when gdaui_formatted_entry_new()
 * was called to format the output differently.
 *
 * Returns: (transfer full): a new string, or %NULL
 */
gchar *
gdaui_formatted_entry_get_text (GdauiFormattedEntry *entry)
{
	gchar *text;
	g_return_val_if_fail (GDAUI_IS_FORMATTED_ENTRY (entry), NULL);
	
	text = gdaui_entry_get_text ((GdauiEntry*) entry);
	if (text && entry->priv->mask) {
		gchar *tptr, *mptr;
		gint len;
		len = strlen (text);
		for (tptr = text, mptr = entry->priv->mask;
		     *tptr && *mptr;
		     mptr++) {
			if ((*mptr == '-') && (*tptr == '_')) {
				/* remove that char */
				g_memmove (tptr, tptr+1, len - (tptr - text));
			}
			else
				tptr = g_utf8_next_char (tptr);
		}
	}

	return text;
}

/**
 * gdaui_formatted_entry_set_insert_func:
 * @entry: a #GdauiFormattedEntry widget
 * @insert_func: (allow-none): a #GdauiFormattedEntryInsertFunc, or %NULL
 * @data: (allow-none): a pointer which will be passed to @insert_func
 *
 * Specifies that @entry should call @insert_func when the user wants to insert a char
 * which is anot allowed, to perform other actions
 */
void
gdaui_formatted_entry_set_insert_func (GdauiFormattedEntry *entry, GdauiFormattedEntryInsertFunc insert_func,
				       gpointer data)
{
	g_return_if_fail (GDAUI_IS_FORMATTED_ENTRY (entry));

	entry->priv->insert_func = insert_func;
	entry->priv->insert_func_data = data;
}
