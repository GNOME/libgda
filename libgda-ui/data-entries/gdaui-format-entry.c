/* gdaui-format-entry.c
 *
 * Copyright (C) 2007 Vivien Malerba <malerba@gnome-db.org>
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

#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <glib/gi18n-lib.h>
#include <ctype.h>
#include <string.h>

#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkentry.h>

#include <libgda/gda-value.h>

#include "gdaui-format-entry.h"

typedef enum {
	GDAUI_FORMAT_ENTRY_LEFT   = -1,
	GDAUI_FORMAT_ENTRY_RIGHT  = 1
} GdauiFormatEntryDirection;

/*
 * NOTE:
 * - by default @edited_type is G_TYPE_STRING
 * - if @user_format is set to something different than NULL then @edited_type is set to G_TYPE_STRING
 * - if @edited_type is set to something different than G_TYPE_STRING, then @user_format is set to NULL
 *
 * - @user_format and @user_mask are set only if the user has set an editing format (and mask)
 * - @comp_format is used only to store format and mask computed as the end user enters text 
 *   (format and mask are the same string composed of '*' chars)
 * - @comp_format and @user_format cannot be not NULL at the same time (they can be both NULL at the same time tough)
 *
 * - @thousands_sep is the thousands separator, or 0 if no separator
 * - @decimal_sep is the decimal separator, or 0 for the default separator
 */
struct _GdauiFormatEntryPrivate {
	gboolean is_null; /* TRUE => entry represents NULL */

	guchar   internal_changes;
	guchar   disable_completions;

	gint     max_length;
	gint     n_decimals;
	guchar   decimal_sep;
	guchar   thousands_sep;
	gchar   *prefix;      /* may contain UTF-8 chars */
	gchar   *suffix;      /* may contain UTF-8 chars */

	GType    edited_type;

	gchar   *user_format; /* may contain UTF-8 chars, only use appropriate g_utf8*() functions */
	gchar   *user_mask;   /* may contain UTF-8 chars, only use appropriate g_utf8*() functions */
	gchar   *user_compl;  /* may contain UTF-8 chars, only use appropriate g_utf8*() functions */

	gchar   *comp_format; /* does not contain any UTF-8 chars, it is possible to use the [] access notation */

	gchar   *i_format;    /* may contain UTF-8 chars, only use appropriate g_utf8*() functions */
	gchar   *i_mask;      /* does not contain any UTF-8 chars, it is possible to use the [] access notation */
	gint     i_chars_length;
	gchar   *i_compl;     /* may contain UTF-8 chars, only use appropriate g_utf8*() functions */
};

#define ENTER_INTERNAL_CHANGES(entry) (entry)->priv->internal_changes ++
#define LEAVE_INTERNAL_CHANGES(entry) (entry)->priv->internal_changes --

#define DISABLE_COMPLETIONS(entry) (entry)->priv->disable_completions ++
#define ENABLE_COMPLETIONS(entry) (entry)->priv->disable_completions --

static void gdaui_format_entry_class_init   (GdauiFormatEntryClass *klass);
static void gdaui_format_entry_init         (GdauiFormatEntry *entry);
static void gdaui_format_entry_finalize     (GObject *object);
static void gdaui_format_entry_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_format_entry_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

/* properties */
enum
{
        PROP_0,
	PROP_MAX_LENGTH,
        PROP_FORMAT,
        PROP_MASK,
	PROP_COMPL,
        PROP_EDITED_TYPE,
	PROP_PREFIX,
	PROP_SUFFIX,
	PROP_N_DECIMALS,
	PROP_DECIMAL_SEP,
	PROP_THOUSANDS_SEP
};

static void changed_cb (GtkEditable *editable, gpointer data);
static void delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, gpointer data);
static void insert_text_cb (GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer data);

static void signal_handlers_block (GdauiFormatEntry *entry);
static void signal_handlers_unblock (GdauiFormatEntry *entry);

static gboolean adjust_numeric_display (GdauiFormatEntry *entry);
static gboolean test_text_validity (GdauiFormatEntry *entry, const gchar *text);
static gboolean char_is_writable (GdauiFormatEntry *entry, gint format_index);
static gint     get_first_writable_index (GdauiFormatEntry *entry, GdauiFormatEntryDirection direction);
static gchar*   get_raw_text (GdauiFormatEntry *entry);

static GObjectClass *parent_class = NULL;

GType
gdaui_format_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GdauiFormatEntryClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gdaui_format_entry_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GdauiFormatEntry),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gdaui_format_entry_init,
		};
		
		type = g_type_register_static (GTK_TYPE_ENTRY, "GdauiFormatEntry",
					       &type_info, 0);
	}

	return type;
}

static void
gdaui_format_entry_class_init (GdauiFormatEntryClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = gdaui_format_entry_finalize;

	/* Properties */
        object_class->set_property = gdaui_format_entry_set_property;
        object_class->get_property = gdaui_format_entry_get_property;

	g_object_class_install_property (object_class, PROP_MAX_LENGTH,
					 g_param_spec_int ("max_length", NULL, NULL, 
							   0, G_MAXINT, 0,
							   G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_FORMAT,
                                         g_param_spec_string ("format", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_MASK,
                                         g_param_spec_string ("mask", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_COMPL,
                                         g_param_spec_string ("completion", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_EDITED_TYPE,
                                         g_param_spec_uint ("edited_type", NULL, NULL, 
							    0, G_MAXUINT, G_TYPE_STRING,
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_PREFIX,
                                         g_param_spec_string ("prefix", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_SUFFIX,
                                         g_param_spec_string ("suffix", NULL, NULL, NULL, 
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_N_DECIMALS,
					 g_param_spec_int ("n_decimals", NULL, NULL, 
							   -1, G_MAXINT, -1,
							   G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_DECIMAL_SEP,
                                         g_param_spec_uchar ("decimal_sep", NULL, NULL, 
							    0, 255, '.',
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_THOUSANDS_SEP,
                                         g_param_spec_uchar ("thousands_sep", NULL, NULL, 
							    0, 255, ',',
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static guchar
get_default_decimal_sep ()
{
	static guchar value = 0;

	if (value == 0) {
		gchar text[20];
		sprintf (text, "%f", 1.23);
		value = text[1];
	}
	return value;
}

static guchar
get_default_thousands_sep ()
{
	static guchar value = 255;

	if (value == 255) {
		gchar text[20];
		sprintf (text, "%f", 1234.);
		if (text[1] == '2')
			value = 0;
		else
			value = text[1];	
	}
	return value;
}

static void
gdaui_format_entry_init (GdauiFormatEntry *entry)
{
	entry->priv = g_new0 (GdauiFormatEntryPrivate, 1);
	entry->priv->is_null = TRUE;
	entry->priv->internal_changes = 0;
	entry->priv->disable_completions = 0;
	entry->priv->max_length = 0; /* no specific limitation */
	entry->priv->prefix = NULL;
	entry->priv->suffix = NULL;
	entry->priv->i_format = NULL;
	entry->priv->i_mask = NULL;
	entry->priv->i_compl = NULL;
	entry->priv->user_format = NULL;
	entry->priv->user_mask = NULL;
	entry->priv->user_compl = NULL;
	entry->priv->comp_format = NULL;
	entry->priv->edited_type = G_TYPE_STRING;
	entry->priv->n_decimals = -1;
	entry->priv->decimal_sep = get_default_decimal_sep ();
	entry->priv->thousands_sep = get_default_thousands_sep ();
#ifdef GDA_DEBUG_NO
	if (entry->priv->thousands_sep)
		g_print ("DEFAULT decimal separator is '%c' and thousands separator is '%c'\n",
			 entry->priv->decimal_sep, entry->priv->thousands_sep);
	else
		g_print ("DEFAULT decimal separator is '%c' and thousands separator is empty\n",
			 entry->priv->decimal_sep);
#endif

	g_signal_connect (G_OBJECT (entry), "delete-text",
			  G_CALLBACK (delete_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "insert-text",
			  G_CALLBACK (insert_text_cb), NULL);

	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (changed_cb), NULL);
}

static void 
gdaui_format_entry_finalize (GObject *object)
{
	GdauiFormatEntry *entry;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (object));

        entry = GDAUI_FORMAT_ENTRY (object);
        if (entry->priv) {
                g_free (entry->priv->prefix);
                g_free (entry->priv->suffix);
                g_free (entry->priv->i_format);
                g_free (entry->priv->i_mask);
                g_free (entry->priv->i_compl);
                g_free (entry->priv->user_format);
                g_free (entry->priv->user_mask);
                g_free (entry->priv->user_compl);
                g_free (entry->priv->comp_format);
                g_free (entry->priv);
                entry->priv = NULL;
        }

        /* parent class */
        parent_class->finalize (object);
}

static void adjust_internal_format (GdauiFormatEntry *entry, gboolean update_entry_text, const gchar *existing_text);

static void 
gdaui_format_entry_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiFormatEntry *entry;
	const gchar *str;
	gchar *otext;

        entry = GDAUI_FORMAT_ENTRY (object);
	otext = gdaui_format_entry_get_text (entry);
        if (entry->priv) {
                switch (param_id) {
		case PROP_MAX_LENGTH:
			entry->priv->max_length = g_value_get_int (value);
			adjust_internal_format (entry, TRUE, otext);
			break;
                case PROP_FORMAT:
			str = g_value_get_string (value);
			if (str) {
				g_free (entry->priv->user_format);
				entry->priv->user_format = NULL;
				if (*str)
					entry->priv->user_format = g_strdup (str);
			}
			entry->priv->edited_type = G_TYPE_STRING;
			g_free (entry->priv->comp_format);
			entry->priv->comp_format = NULL;
			adjust_internal_format (entry, TRUE, otext);
                        break;
                case PROP_MASK:
			g_free (entry->priv->user_mask);
			entry->priv->user_mask = NULL;
			str = g_value_get_string (value);
			if (str)
				entry->priv->user_mask = g_strdup (str);
			entry->priv->edited_type = G_TYPE_STRING;
			adjust_internal_format (entry, TRUE, otext);
                        break;
		case PROP_COMPL:
			g_free (entry->priv->user_compl);
			entry->priv->user_compl = NULL;
			str = g_value_get_string (value);
			if (str) 
				entry->priv->user_compl = g_strdup (str);
			adjust_internal_format (entry, TRUE, otext);
			break;
                case PROP_EDITED_TYPE:
			entry->priv->edited_type = g_value_get_uint (value);
			if (entry->priv->edited_type != G_TYPE_STRING) {
				g_free (entry->priv->user_format);
				entry->priv->user_format = NULL;
				g_free (entry->priv->user_mask);
				entry->priv->user_mask = NULL;
			}
			adjust_internal_format (entry, TRUE, otext);
                        break;
                case PROP_PREFIX:
			g_free (entry->priv->prefix);
			entry->priv->prefix = NULL;
			str = g_value_get_string (value);
			if (str)
				entry->priv->prefix = g_strdup (str);
			adjust_internal_format (entry, TRUE, otext);
                        break;
                case PROP_SUFFIX:
			g_free (entry->priv->suffix);
			entry->priv->suffix = NULL;
			str = g_value_get_string (value);
			if (str)
				entry->priv->suffix = g_strdup (str);
			adjust_internal_format (entry, TRUE, otext);
                        break;
		case PROP_N_DECIMALS:
			entry->priv->n_decimals = g_value_get_int (value);
			adjust_internal_format (entry, TRUE, otext);
			break;
		case PROP_DECIMAL_SEP: {
			guchar sep = g_value_get_uchar (value);
			if ((sep == 0) || (sep == '+') || (sep == '-'))
				g_warning (_("Decimal separator cannot be the '%c' character"), sep ? sep : '0');
			else {
				entry->priv->decimal_sep = g_value_get_uchar (value);
				adjust_internal_format (entry, TRUE, otext);
			}
			break;
		}
		case PROP_THOUSANDS_SEP: {
			guchar sep = g_value_get_uchar (value);
			if ((sep == '+') || (sep == '-') || (sep == '_'))
				g_warning (_("Decimal thousands cannot be the '%c' character"), sep);
			else {
				entry->priv->thousands_sep = g_value_get_uchar (value);
				adjust_internal_format (entry, TRUE, otext);
			}
			break;
		}
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
	
	if (!otext) 
		gdaui_format_entry_set_text (entry, NULL);
	else {
		gchar *ntext = gdaui_format_entry_get_text (entry);
		if ((!otext && ntext) ||
		    (ntext && !otext) ||
		    (otext && ntext && strcmp (otext, ntext)))
			g_signal_emit_by_name (entry, "changed");
		g_free (ntext);
	}
	g_free (otext);
}

static void
gdaui_format_entry_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdauiFormatEntry *entry;

        entry = GDAUI_FORMAT_ENTRY (object);
        if (entry->priv) {
                switch (param_id) {
		case PROP_MAX_LENGTH:
			g_value_set_int (value, entry->priv->max_length);
			break;
		case PROP_FORMAT:
			g_value_set_string (value, entry->priv->user_format);
                        break;
                case PROP_MASK:
			g_value_set_string (value, entry->priv->user_mask);
                        break;
                case PROP_EDITED_TYPE:
			g_value_set_uint (value, entry->priv->edited_type);
                        break;
                case PROP_PREFIX:
			g_value_set_string (value, entry->priv->prefix);
                        break;
                case PROP_SUFFIX:
			g_value_set_string (value, entry->priv->suffix);
                        break;
		case PROP_N_DECIMALS:
			g_value_set_int (value, entry->priv->n_decimals);
			break;
		case PROP_DECIMAL_SEP:
			g_value_set_uchar (value, entry->priv->decimal_sep);
			break;
		case PROP_THOUSANDS_SEP:
			g_value_set_uchar (value, entry->priv->thousands_sep);
			break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}

static void
signal_handlers_block (GdauiFormatEntry *entry)
{
	ENTER_INTERNAL_CHANGES (entry);
	g_signal_handlers_block_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_block_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
	LEAVE_INTERNAL_CHANGES (entry);
}

static void
signal_handlers_unblock (GdauiFormatEntry *entry)
{
	ENTER_INTERNAL_CHANGES (entry);
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (insert_text_cb), NULL);
	g_signal_handlers_unblock_by_func (entry, G_CALLBACK (delete_text_cb), NULL);
	LEAVE_INTERNAL_CHANGES (entry);
}

/*
 * Computes new format and mask strings from other internals changes
 *
 * if @existing_text is not NULL, then the contents of the entry is first erased and then
 * set to be @existing_text again (to achieve text "migration" after a format change)
 */
static void
adjust_internal_format (GdauiFormatEntry *entry, gboolean update_entry_text, const gchar *existing_text)
{
	GString *format, *mask, *compl = NULL;
	gint length;
	gchar *current_text = NULL;

	DISABLE_COMPLETIONS(entry);
	format = g_string_new ("");
	mask = g_string_new ("");

	if (entry->priv->user_compl)
		compl = g_string_new ("");

	if (update_entry_text) {
		current_text = existing_text ? g_strdup (existing_text) : NULL;
		signal_handlers_block (entry);
		gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
		signal_handlers_unblock (entry);
	}

	if (entry->priv->prefix) {
		gchar *str;
		gint len = g_utf8_strlen (entry->priv->prefix, -1);
		g_string_append (format, entry->priv->prefix);
		str = g_new (gchar, len + 1);
		str [len] = 0;
		memset (str, ' ', len);
		g_string_append (mask, str);
		if (compl)
			g_string_append (compl, str);
		g_free (str);
	}

	if (entry->priv->user_format) {
		gint i, flen;
		gchar *str, *ptr;

		g_assert (entry->priv->comp_format == NULL);
		g_string_append (format, entry->priv->user_format);

		flen = g_utf8_strlen (entry->priv->user_format, -1);
		ptr = entry->priv->user_format;
		if (entry->priv->user_mask) {
			gint len;
			len = g_utf8_strlen (entry->priv->user_mask, -1);
			if (len != flen)
				g_warning (_("Format and mask strings must have the same length, ignoring mask"));
			else
				ptr = entry->priv->user_mask;
		}

		str = g_new (gchar, flen + 1);
		for (i = 0; *ptr; ptr = g_utf8_next_char (ptr), i++) {
			gunichar f = g_utf8_get_char (ptr);
			if ((f == '0') || (f == '9') || (f == '@') || (f == '^') || (f == '#') || (f == '*'))
				str [i] = '*';
			else
				str [i] = ' ';
		}
		str [flen] = 0;
		g_string_append (mask, str);
		g_free (str);

		if (compl) {
			if (g_utf8_strlen (entry->priv->user_compl, -1) != flen) {
				g_warning (_("Format and completion strings must have the same length, ignoring completion"));
				g_string_free (compl, TRUE);
				compl = NULL;
			}
			else 
				g_string_append (compl, entry->priv->user_compl);
		}
	}

	if (entry->priv->comp_format) {
		if (update_entry_text) {
			g_free (entry->priv->comp_format);
			entry->priv->comp_format = NULL;
		}
		else {
			gchar *str, *ptr;
			g_string_append (format, entry->priv->comp_format);
			
			str = g_strdup (entry->priv->comp_format);
			for (ptr = str; *ptr; ptr++) {
				if ((*ptr != '0') && (*ptr != '9') && (*ptr != '@') &&
				    (*ptr != '^') && (*ptr != '#') && (*ptr != '*'))
					    *ptr = ' ';
			}
			g_string_append (mask, str);
			g_free (str);
		}
	}

	if (entry->priv->suffix) {
		gchar *str;
		gint len = g_utf8_strlen (entry->priv->suffix, -1);
		g_string_append (format, entry->priv->suffix);
		str = g_new (gchar, len + 1);
		str [len] = 0;
		memset (str, ' ', len);
		g_string_append (mask, str);
		if (compl)
			g_string_append (compl, str);
		g_free (str);
	}

	g_free (entry->priv->i_format);
	entry->priv->i_format = format->str;
	g_string_free (format, FALSE);
	g_free (entry->priv->i_mask);
	entry->priv->i_mask = mask->str;
	entry->priv->i_chars_length = strlen (mask->str);
	g_string_free (mask, FALSE);
	g_free (entry->priv->i_compl);
	entry->priv->i_compl = NULL;
	if (compl) {
		entry->priv->i_compl = compl->str;
		g_string_free (compl, FALSE);
	}

	length = g_utf8_strlen (entry->priv->i_format, -1);
	if (entry->priv->user_format)
		gtk_entry_set_max_length (GTK_ENTRY (entry), (length != 0) ? length : 1);
	else
		gtk_entry_set_max_length (GTK_ENTRY (entry), 0); /* unlimited length */

	if (update_entry_text && length != 0)
		g_signal_emit_by_name (G_OBJECT (entry), "delete-text", 0, length);

#ifdef GDA_DEBUG_NO
	g_print ("iformat=#%s#, imask=#%s#\n", entry->priv->i_format, entry->priv->i_mask);
#endif
	g_assert (g_utf8_strlen (entry->priv->i_format, -1) == entry->priv->i_chars_length);

	if (current_text) {
		gtk_entry_set_text (GTK_ENTRY (entry), current_text);
		g_free (current_text);
	}

	ENABLE_COMPLETIONS(entry);
}

/**
 * gdaui_format_entry_new:
 *
 * Creates a new #GdauiFormatEntry widget.
 *
 * Returns: the newly created #GdauiFormatEntry widget.
 */
GtkWidget*
gdaui_format_entry_new ()
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_FORMAT_ENTRY, NULL);
	return GTK_WIDGET (obj);
}

/**
 * gdaui_format_entry_set_max_length
 * @entry: a #GdauiFormatEntry.
 * @max: the maximum length of the entry, or 0 for no maximum.
 *
 * Sets the maximum allowed length of the contents of the widget.
 * If the current contents are longer than the given length, then they will be truncated to fit.
 *
 * The difference with gtk_entry_set_max_length() is that the max length does not take into account
 * the prefix and/or suffix parts which may have been set.
 */
void
gdaui_format_entry_set_max_length (GdauiFormatEntry *entry, gint max)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "max_length", max, NULL);
}

/**
 * gdaui_format_entry_set_format:
 * @entry: a #GdauiFormatEntry.
 * @format: the format specification of the data to edit
 * @mask: a 'mask' string helping understand @format, or %NULL
 * @completion: a 'completions' string, or %NULL
 *
 * Set the edited string's format.
 * Characters in the format are of two types:
 *   writeable: writeable characters are characters that will be replaced with 
 *              and underscore and where you can enter text.
 *   fixed: every other characters are fixed characters, where text cant' be edited
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
 * if @mask is not %NULL, then it must contain the exact same number of characters as @format; 
 * it is then interpreted in the following way: for a character C in @format, if the character at the same
 * position in @mask is the space character (' '), then C will not interpreted as a writable format
 * character as defined above.
 *
 * if @completion is not %NULL, then it must contain the exact same number of characters as @format. A
 * When the entry has a cursor at a position P and the character to enter is not what's expected at position P in the
 * @format string, then the character at a position P in this string is used.
 */
void
gdaui_format_entry_set_format (GdauiFormatEntry *entry, const gchar *format, const gchar *mask, const gchar *completion)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "format", format, "mask", mask, "completion", completion, NULL);
}

/**
 * gdaui_format_entry_get_text:
 * @entry: a #GdauiFormatEntry.
 *
 * Get a new string containing the contents of the widget as a string without the
 * prefix and/or suffix and/or format if they have been specified. This method differs
 * from calling gtk_entry_get_text() since the latest will return the complete text
 * in @entry including prefix and/or suffix and/or format.
 *
 * Note: %NULL may be returned if this method is called while the widget is working on some
 * internal modifications, or if gdaui_format_entry_set_text() was called with a %NULL
 * as its @text argument.
 *
 * Returns: a new string, or %NULL
 */
gchar*
gdaui_format_entry_get_text (GdauiFormatEntry *entry)
{
	gchar *text;
	gchar *ptr;
	gint len, i;

	g_return_val_if_fail (GDAUI_IS_FORMAT_ENTRY (entry), NULL);
	g_return_val_if_fail (entry->priv, NULL);

	if (entry->priv->is_null)
		text = NULL;
	else {
		text = get_raw_text (entry);
		if (text) {
			len = strlen (text);
			for (ptr = text, i = 0; *ptr; ) {
				if (*ptr == entry->priv->thousands_sep) 
					memmove (ptr, ptr+1, len - i);
				else {
					if (*ptr == entry->priv->decimal_sep)
						break;
					ptr++;
					i++;
				}
			}
		}
	}

	return text;
}

static gchar*
get_raw_text (GdauiFormatEntry *entry)
{
	const gchar *text;
	gchar *ret, *ptr;
	gint index = 0, length;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	length = strlen (text);
	/* don't take the PREFIX, if any */
	if (entry->priv->prefix) {
		index = strlen (entry->priv->prefix);
		length -= index;
	}
	/* don't take the SUFFIX, if any */
	if (entry->priv->suffix) {
		g_assert (!strcmp (text + length - strlen (entry->priv->suffix), entry->priv->suffix));
		length -= strlen (entry->priv->suffix);
	}

	if (length < 0)
		return NULL;

	ret = g_new (gchar, length + 1);
	memcpy (ret, text + index, length);
	ret [length] = 0;

	if (entry->priv->user_format) {
		/* replace the '_' with ' ' */
		for (ptr = ret; *ptr; ptr = g_utf8_next_char (ptr), index++) {
			if ((*ptr == '_') && char_is_writable (entry, index))
				*ptr = ' ';
		}
	}

	return ret;
}

/**
 * gdaui_format_entry_set_text
 * @entry: a #GdauiFormatEntry widget
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
gdaui_format_entry_set_text (GdauiFormatEntry *entry, const gchar *text)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	gtk_entry_set_text (GTK_ENTRY (entry), text ? text : "");

	if (!text) {
		entry->priv->is_null = TRUE;
		signal_handlers_block (entry);
		gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
		signal_handlers_unblock (entry);
	}
}

/**
 * gdaui_format_entry_set_prefix
 * @entry: a #GdauiFormatEntry widget
 * @prefix: a prefix string
 *
 * Sets @prefix as a prefix string of @entry: that string will always be displayed in the
 * text entry, will not be modifiable, and won't be part of the returned text
 */
void
gdaui_format_entry_set_prefix (GdauiFormatEntry *entry, const gchar *prefix)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "prefix", prefix, NULL);
}

/**
 * gdaui_format_entry_set_suffix
 * @entry: a #GdauiFormatEntry widget
 * @suffix: a suffix string
 *
 * Sets @suffix as a suffix string of @entry: that string will always be displayed in the
 * text entry, will not be modifiable, and won't be part of the returned text
 */
void
gdaui_format_entry_set_suffix (GdauiFormatEntry *entry, const gchar *suffix)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "suffix", suffix, NULL);
}

/**
 * gdaui_format_entry_set_decimal_places
 * @entry: a #GdauiFormatEntry widget
 * @nb_decimals: the number of decimals which must be displayed in @entry
 *
 * Sets the number of decimals which must be displayed in @entry when entering
 * a non integer numerical value
 */
void
gdaui_format_entry_set_decimal_places (GdauiFormatEntry *entry, gint nb_decimals)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "n_decimals", nb_decimals, NULL);
}

/**
 * gdaui_format_entry_set_separators
 * @entry: a #GdauiFormatEntry widget
 * @decimal: the character to use as a decimal separator, or 0 for default (locale specific) separator
 * @thousands: the character to use as a thousands separator, or 0 for no separator at all
 *
 * Sets the decimal and thousands separators to use within @entry when it contains a numerical value
 */
void
gdaui_format_entry_set_separators (GdauiFormatEntry *entry, guchar decimal, guchar thousands)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	if (decimal)
		g_object_set (G_OBJECT (entry), "decimal_sep", decimal, NULL);

	if (thousands)
		g_object_set (G_OBJECT (entry), "thousands_sep", thousands, NULL);
}

/**
 * gdaui_format_entry_set_edited_type
 * @entry: a #GdauiFormatEntry widget
 * @type: the type of data expected to be edited @entry itself returns a string,
 *        @type specifies into what type that string will be converted)
 *
 * Specifies what type of data is edited in @entry
 */
void
gdaui_format_entry_set_edited_type (GdauiFormatEntry *entry, GType type)
{
	g_return_if_fail (GDAUI_IS_FORMAT_ENTRY (entry));
	g_return_if_fail (entry->priv);

	g_object_set (G_OBJECT (entry), "edited_type", type, NULL);
}

/*
 * callbacks
 */

static void
changed_cb (GtkEditable *editable, gpointer data)
{
	GdauiFormatEntry *entry = (GdauiFormatEntry*) editable;
	if (entry->priv->internal_changes > 0) 
		g_signal_stop_emission_by_name (editable, "changed");
}

typedef struct {
	gboolean  is_numerical;
	gint64    imin;
	gint64    imax;
	guint64   uimax;
	gdouble   fmax;
	gboolean  is_int;
	gboolean  is_signed;
} NumAttr;

static void
compute_numeric_attributes (GType type, NumAttr *attr)
{
	attr->imin = 0;
	attr->imax = 0;
	attr->uimax = 0;
	attr->fmax = 0.;
	attr->is_int = FALSE;
	attr->is_signed = TRUE;
	attr->is_numerical = TRUE;

	if (type == G_TYPE_INT64) {
		attr->imax = G_MAXINT64;
		attr->imin = G_MININT64;
		attr->is_int = TRUE;
	}
	else if (type == G_TYPE_UINT64) {
		attr->uimax = G_MAXUINT64;
		attr->is_int = TRUE;
		attr->is_signed = FALSE;
	}
	else if (type == G_TYPE_LONG) {
		attr->imax = G_MAXLONG;
		attr->imin = G_MINLONG;
		attr->is_int = TRUE;
	}
	else if (type == G_TYPE_ULONG) {
		attr->uimax = G_MAXULONG;
		attr->is_int = TRUE;
		attr->is_signed = FALSE;
	}
	else if (type == G_TYPE_INT) {
		attr->imax = G_MAXINT;
		attr->imin = G_MININT;
		attr->is_int = TRUE;
	}
	else if (type == G_TYPE_UINT) {
		attr->uimax = G_MAXUINT;
		attr->is_int = TRUE;
		attr->is_signed = FALSE;
	}
	else if (type == G_TYPE_CHAR) {
		attr->imax = 127;
		attr->imin = -128;
		attr->is_int = TRUE;
	}
	else if (type == G_TYPE_UCHAR) {
		attr->uimax = 255;
		attr->is_int = TRUE;
		attr->is_signed = FALSE;
	}
	else if (type == G_TYPE_FLOAT) {
		attr->fmax = G_MAXFLOAT;
	}
	else if (type == G_TYPE_DOUBLE) {
		attr->fmax = G_MAXDOUBLE;
	}
	else if (type == GDA_TYPE_NUMERIC) {
	}
	else if (type == GDA_TYPE_SHORT) {
		attr->imax = G_MAXSHORT;
		attr->imin = G_MINSHORT;
		attr->is_int = TRUE;
	}
	else if (type == GDA_TYPE_USHORT) {
		attr->uimax = G_MAXUSHORT;
		attr->is_int = TRUE;
		attr->is_signed = FALSE;
	}
	else {
		attr->is_numerical = FALSE;
	}
}

/*
 * Add thousands separator if necessary.
 * always return FALSE because it's a timeout call and we want to remove it.
 */
static gboolean
adjust_numeric_display (GdauiFormatEntry *entry)
{
	gchar *raw_text, *number_text, *new_text;
	gchar *ptr;
	gint raw_length, number_length;
	gint cursor_pos, cursor_offset, insert_pos = 0;
	gint i, index;

	gchar *format;
	NumAttr attr;

	if (!entry->priv)
		return FALSE;

	compute_numeric_attributes (entry->priv->edited_type, &attr);
	
	if (!attr.is_numerical)
		return FALSE;

	if (entry->priv->is_null)
		return FALSE;

	ENTER_INTERNAL_CHANGES (entry);

	raw_text = get_raw_text (entry);
	if (! raw_text)
		return FALSE;

	raw_length = strlen (raw_text);
	number_text = gdaui_format_entry_get_text (entry);
	if (!number_text)
		return FALSE;
	number_length = strlen (number_text);

	/* make a copy of current number representation in a tmp buffer */
	new_text = g_new (gchar, 
			  number_length * 2 + ((entry->priv->n_decimals >= 0) ? entry->priv->n_decimals : 0) + 1);
	memcpy (new_text, number_text, number_length + 1);

	/* try to set ptr to the 1st decimal separator */
	for (ptr = new_text, i = 0; *ptr && (*ptr != entry->priv->decimal_sep); ptr++, i++);

	/* handle decimals if necessary */
	if ((entry->priv->n_decimals >= 0) && !attr.is_int) {
		if ((entry->priv->n_decimals == 0) && (*ptr == entry->priv->decimal_sep)) {
			*ptr = 0;
			number_length = i;
		}
		else if (entry->priv->n_decimals > 0) {
			gint n = 0;
			if (*ptr != entry->priv->decimal_sep) {
				g_assert (*ptr == 0);
				*ptr = entry->priv->decimal_sep;
				ptr++;
			}
			else {
				for (ptr++; *ptr && (n < entry->priv->n_decimals); n++, ptr++)
					g_assert (isdigit (*ptr));
				
				if (*ptr)
					*ptr = 0;
			}

			for (; n < entry->priv->n_decimals; n++, ptr++) 
				*ptr = '0';
			*ptr = 0;
		}
		number_length = strlen (new_text);
	}

	/* add thousands separator if necessary */
	if (entry->priv->thousands_sep) {
		index = i;
		for (i--; i > 0; i--) {
			if (isdigit (new_text [i-1]) && (index - i) % 3 == 0) {
				memmove (new_text + i + 1, new_text + i, number_length - i + 1);
				number_length ++;
				new_text [i] = entry->priv->thousands_sep;
			}
		}
	}
	
	/* make format from new text */
	format = g_strdup (new_text);
	for (ptr = format; *ptr; ptr++) {
		if ((*ptr != entry->priv->thousands_sep) && 
		    (*ptr != entry->priv->decimal_sep))
			*ptr = '0';
		else if ((*ptr == entry->priv->decimal_sep) && (entry->priv->n_decimals < 0))
			*ptr = '0';
	}
	g_free (entry->priv->comp_format);
	entry->priv->comp_format = format;

	/* actual changes to the entry widget */
	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (entry));
	if (entry->priv->prefix)
		insert_pos = g_utf8_strlen (entry->priv->prefix, -1);

	/* determine cursor offset to apply */
	for (cursor_offset = 0, i = 0; (raw_text [i] && new_text [i]) && (i <= (cursor_pos - insert_pos)); i++) {
		if (raw_text [i] == entry->priv->thousands_sep)
			cursor_offset --;
		if (new_text [i] == entry->priv->thousands_sep)
			cursor_offset ++;
	}

	signal_handlers_block (entry);
	gtk_editable_delete_text (GTK_EDITABLE (entry), insert_pos, insert_pos + raw_length);
	adjust_internal_format (entry, FALSE, NULL);
	gtk_editable_insert_text (GTK_EDITABLE (entry), new_text, number_length, &insert_pos);
	signal_handlers_unblock (entry);

	gtk_editable_set_position (GTK_EDITABLE (entry), cursor_pos + cursor_offset);

	g_free (number_text);
	g_free (raw_text);
	g_free (new_text);

	LEAVE_INTERNAL_CHANGES (entry);
	//g_signal_emit_by_name (entry, "changed");

	return FALSE; /* remove the timeout */
}

static void
delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, gpointer data)
{
	GdauiFormatEntry *entry = GDAUI_FORMAT_ENTRY (editable);

	gchar *newtext, *ptr;
	gint i;

	ENTER_INTERNAL_CHANGES (entry);
	i = gtk_entry_get_max_length (GTK_ENTRY (entry));
	if (start_pos < 0 || 
	    ((i > 0) && (end_pos > i))) {
		g_signal_stop_emission_by_name (editable, "delete-text");
		return;
	}
	if (end_pos < 0) {
		g_warning ("Not yet implemented");
		return;
	}

	/* determine boundaries for entry->priv->comp_format modifications */
	gint fs;
	if (entry->priv->comp_format) {
		gint fl;
		fs = start_pos;
		if (entry->priv->prefix) {
			fs -= g_utf8_strlen (entry->priv->prefix, -1);
			if (fs < 0) fs = 0;
		}
		fl = strlen (entry->priv->comp_format);
		if (fs > fl) fs = fl;
	}

	/* re-write the removed part */	
	i = strlen (entry->priv->i_format);
	newtext = g_new0 (gchar, i + 1);
	for (ptr = newtext, i = start_pos; i < end_pos; i++) {
		if (char_is_writable (entry, i)) {
			if (entry->priv->user_format)
				*ptr = '_';
			else {
				if (entry->priv->comp_format) {
					gint fl;
					fl = strlen (entry->priv->comp_format);
					if (fs > fl) {
						g_free (entry->priv->comp_format);
						entry->priv->comp_format = NULL;
					}
					else
						memmove (entry->priv->comp_format + fs,
							 entry->priv->comp_format + fs + 1,
							 fl - fs);
				}
			}
		}
		else {
			fs++;
			g_utf8_strncpy (ptr, g_utf8_offset_to_pointer (entry->priv->i_format, i), 1);
		}
		ptr = g_utf8_next_char (ptr);
	}
	
	i = start_pos;
	signal_handlers_block (entry);
	gtk_editable_delete_text (editable, start_pos, end_pos);
	adjust_internal_format (entry, FALSE, NULL);
	gtk_editable_insert_text (editable, newtext, strlen (newtext), &i);
	signal_handlers_unblock (entry);

	adjust_numeric_display (entry);

	g_signal_stop_emission_by_name (editable, "delete-text");
	g_free (newtext);
	LEAVE_INTERNAL_CHANGES (entry);
	g_signal_emit_by_name (entry, "changed");
}

/******************* Function copied from gstrfuncs.c until bug #416062 is corrected ******************/
static guint64
copied_g_parse_long_long (const gchar *nptr,
			  gchar      **endptr,
			  guint        base,
			  gboolean    *negative)
{
  /* this code is based on on the strtol(3) code from GNU libc released under
   * the GNU Lesser General Public License.
   *
   * Copyright (C) 1991,92,94,95,96,97,98,99,2000,01,02
   *        Free Software Foundation, Inc.
   */
#define ISSPACE(c)		((c) == ' ' || (c) == '\f' || (c) == '\n' || \
				 (c) == '\r' || (c) == '\t' || (c) == '\v')
#define ISUPPER(c)		((c) >= 'A' && (c) <= 'Z')
#define ISLOWER(c)		((c) >= 'a' && (c) <= 'z')
#define ISALPHA(c)		(ISUPPER (c) || ISLOWER (c))
#define	TOUPPER(c)		(ISLOWER (c) ? (c) - 'a' + 'A' : (c))
#define	TOLOWER(c)		(ISUPPER (c) ? (c) - 'A' + 'a' : (c))
  gboolean overflow;
  guint64 cutoff;
  guint64 cutlim;
  guint64 ui64;
  const gchar *s, *save;
  guchar c;
  
  g_return_val_if_fail (nptr != NULL, 0);
  
  if (base == 1 || base > 36)
    {
      errno = EINVAL;
      return 0;
    }
  
  save = s = nptr;
  
  /* Skip white space.  */
  while (ISSPACE (*s))
    ++s;

  if (G_UNLIKELY (!*s))
    goto noconv;
  
  /* Check for a sign.  */
  *negative = FALSE;
  if (*s == '-')
    {
      *negative = TRUE;
      ++s;
    }
  else if (*s == '+')
    ++s;
  
  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && TOUPPER (s[1]) == 'X')
	{
	  s += 2;
	  base = 16;
	}
      else if (base == 0)
	base = 8;
    }
  else if (base == 0)
    base = 10;
  
  /* Save the pointer so we can check later if anything happened.  */
  save = s;
  cutoff = G_MAXUINT64 / base;
  cutlim = G_MAXUINT64 % base;
  
  overflow = FALSE;
  ui64 = 0;
  c = *s;
  for (; c; c = *++s)
    {
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (ISALPHA (c))
	c = TOUPPER (c) - 'A' + 10;
      else
	break;
      if (c >= base)
	break;
      /* Check for overflow.  */
      if (ui64 > cutoff || (ui64 == cutoff && c > cutlim))
	overflow = TRUE;
      else
	{
	  ui64 *= base;
	  ui64 += c;
	}
    }
  
  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;
  
  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr)
    *endptr = (gchar*) s;
  
  if (G_UNLIKELY (overflow))
    {
      errno = ERANGE;
      return G_MAXUINT64;
    }

  return ui64;
  
 noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are no
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the `x`.  */
  if (endptr)
    {
      if (save - nptr >= 2 && TOUPPER (save[-1]) == 'X'
	  && save[-2] == '0')
	*endptr = (gchar*) &save[-1];
      else
	/*  There was no number to convert.  */
	*endptr = (gchar*) nptr;
    }
  return 0;
}

static gint64
copied_g_ascii_strtoll (const gchar *nptr,
			gchar      **endptr,
			guint        base)
{
  gboolean negative;
  guint64 result;

  result = copied_g_parse_long_long (nptr, endptr, base, &negative);

  if (negative && result > (guint64) G_MININT64)
    {
      errno = ERANGE;
      return G_MININT64;
    }
  else if (!negative && result > (guint64) G_MAXINT64)
    {
      errno = ERANGE;
      return G_MAXINT64;
    }
  else if (negative)
      return - ((gint64) result);
  else
      return (gint64) result;
}
/******************************************************************************************************/

static gboolean
test_text_validity (GdauiFormatEntry *entry, const gchar *text)
{
	gboolean retval = TRUE;
	NumAttr attr;
	gchar *endptr [1];

	compute_numeric_attributes (entry->priv->edited_type, &attr);

#ifdef GDA_DEBUG_NO
	g_print ("Validity text: #%s#", text);
#endif
	
	if (text && ((*text == '-') || (*text == '+')) && (text[1] == 0))
		;
	else {
		gchar *tmp = g_strdup (text);
		if (attr.is_int) {
			gchar *ptr;
			
			for (ptr = tmp; *ptr; ) {
				if (*ptr == entry->priv->thousands_sep) 
					/* remove that char */
					memmove (ptr, ptr + 1, strlen (ptr));
				else
					ptr++;
			}
			if (attr.is_signed) {
				gint64 value;
				errno = 0;
				value = copied_g_ascii_strtoll (tmp, endptr, 10);
				if (((value == G_MININT64) || (value == G_MAXINT64)) &&
				    (errno == ERANGE))
					retval = FALSE;
				if ((**endptr != 0) || (value < attr.imin) || (value > attr.imax))
					retval = FALSE;
			}
			else {
				guint64 value;
				errno = 0;
				value = g_ascii_strtoull (tmp, endptr, 10);
				if ((value == G_MAXUINT64) && (errno == ERANGE))
					retval = FALSE;
				if ((**endptr != 0) || (value > attr.uimax))
					retval = FALSE;
			}
		}
		else {
			gchar *ptr;
			gdouble value;
			
			for (ptr = tmp; *ptr; ) {
				if (*ptr == entry->priv->decimal_sep) {
					*ptr = get_default_decimal_sep ();
					ptr++;
				}
				else if (*ptr == entry->priv->thousands_sep)
					memmove (ptr, ptr + 1, strlen (ptr));
				else
					ptr++;
			}
			errno = 0;
			value = g_strtod (tmp, endptr);
			if (((value == HUGE_VAL) || (value == -HUGE_VAL)) && 
			    (errno == ERANGE))
				retval = FALSE;
			if ((**endptr != 0) || 
			    ((attr.fmax > 0) && (value > attr.fmax)))
				retval  = FALSE;
		}
		g_free (tmp);
	}
#ifdef GDA_DEBUG_NO
	g_print ("retval=%d\n", retval);
#endif
	return retval;
}

/*
 * Re-computes the internal format of @entry to be able to add the @text text
 * Returns: TRUE if the internal format has been adjusted to take into account @inschar
 */
static gboolean
adjust_computed_format_at_insert (GdauiFormatEntry *entry, const gunichar inschar, gint *position, 
				  gboolean *out_ignore_this_char)
{
	gint insert_pos;
	NumAttr attr;
	gboolean retval = FALSE;

	*out_ignore_this_char = FALSE;
	insert_pos = *position;
	if (entry->priv->prefix)
		insert_pos -= g_utf8_strlen (entry->priv->prefix, -1);
	if (insert_pos < 0)
		insert_pos = 0;

	compute_numeric_attributes (entry->priv->edited_type, &attr);

	if (! attr.is_numerical) {
		/* Text: any char is accepted */
		if (!entry->priv->comp_format) {
			entry->priv->comp_format = g_new (gchar, 2);
			entry->priv->comp_format [0] = '*';
			entry->priv->comp_format [1] = 0;
		}
		else {
			gint exist_length = strlen (entry->priv->comp_format);
			entry->priv->comp_format = g_renew (gchar, entry->priv->comp_format, 
							    exist_length + 2);
			entry->priv->comp_format [exist_length] = '*';
			entry->priv->comp_format [exist_length + 1] = 0;
		}
		if ((entry->priv->max_length > 0) && 
		    (strlen (entry->priv->comp_format) > entry->priv->max_length))
			entry->priv->comp_format [entry->priv->max_length] = 0;
		adjust_internal_format (entry, FALSE, NULL);
		retval = TRUE;
	}
	else {
		/* numerical */
		NumAttr attr;

		gchar *test_text;
		gint test_length = 0;

		compute_numeric_attributes (entry->priv->edited_type, &attr);
		
		/* initialize string which will be tested */
		test_text = get_raw_text (entry);
		if (test_text) {
			test_length = g_utf8_strlen (test_text, -1);
			test_text = g_renew (gchar, test_text, test_length + 7);
		}
		else
			test_text = g_new0 (gchar, 7);

		/* update the tested string with *ptr */
		gchar format_ext = 0;
		if (insert_pos >= test_length) {
			gint bsize;
			if (insert_pos > test_length)
				goto num_out;
			
			bsize = g_unichar_to_utf8 (inschar, test_text + test_length);
			g_assert (bsize <= 6);
			test_text [test_length + bsize] = 0;
		}
		else {
			/* test if char to insert is the decimal separator */
			if (inschar == entry->priv->decimal_sep) {
				if (g_utf8_get_char (g_utf8_offset_to_pointer (test_text, insert_pos)) == 
				    entry->priv->decimal_sep) {
					retval = TRUE;
					*out_ignore_this_char = TRUE;
					goto num_out;
				}
			}
			
			gchar buffer [6];
			gint bsize;

			bsize = g_unichar_to_utf8 (inschar, buffer);
			g_assert (bsize <= 6);
			memmove (test_text + insert_pos + bsize, test_text + insert_pos,
				 test_length - insert_pos + 1);
			memcpy (test_text + insert_pos, buffer, bsize);
		}
		test_length ++;

		/* update format_ext if tested string is valid, or get out of the loop if not */
		if (inschar == entry->priv->decimal_sep) {
			if (!attr.is_int) {
				if (test_text_validity (entry, test_text))
					format_ext = '*';
			}
		}
		else if (g_unichar_isdigit (inschar)) {
			if (test_text_validity (entry, test_text))
				format_ext = '0';
		}
		else if ((insert_pos == 0) && ((inschar == '-') || (inschar == '+'))) {
			if (attr.is_signed && test_text_validity (entry, test_text))
				format_ext = '*';
		}

		/* alter entry->priv->comp_format with the addition of @format_ext */
		if (format_ext != 0) {
			if (!entry->priv->comp_format) {
				entry->priv->comp_format = g_new (gchar, 2);
				entry->priv->comp_format [0] = format_ext;
				entry->priv->comp_format [1] = 0;
			}
			else {
				gint flen;
				
				flen = strlen (entry->priv->comp_format);
				entry->priv->comp_format = g_renew (gchar, entry->priv->comp_format,
								    flen + 2);	    
				
				if (insert_pos >= flen) {
					entry->priv->comp_format [flen] = format_ext;
					entry->priv->comp_format [flen + 1] = 0;
				}
				else {
					memmove (entry->priv->comp_format + insert_pos + 1, 
						 entry->priv->comp_format + insert_pos,
						 flen - insert_pos + 1);
					entry->priv->comp_format [insert_pos] = format_ext;
				}
			}
			adjust_internal_format (entry, FALSE, NULL);
			retval = TRUE;
		}

	num_out:
		g_free (test_text);
	}

	return retval;
}

static void
insert_text_cb (GtkEditable *editable, const gchar *text, gint text_length, gint *position, gpointer data)
{
	gchar *format;
	gchar *newtext;
	gint format_length;
	gint ti, ci, fi, write_pos;
	GdauiFormatEntry *entry = GDAUI_FORMAT_ENTRY (editable);

	ENTER_INTERNAL_CHANGES (entry);
	if (text == NULL || text_length == 0) {
		gtk_editable_delete_text (editable, 0, gtk_entry_get_max_length (GTK_ENTRY (entry)));
		goto exit;
	}

	if (entry->priv->is_null) {
		gint pos = 0;
		/* restore a correct display before treating @text */
		signal_handlers_block (entry);
		gtk_editable_insert_text (editable, entry->priv->i_format, 
					  g_utf8_strlen (entry->priv->i_format, -1), &pos);
		signal_handlers_unblock (entry);
		entry->priv->is_null = FALSE;
		gtk_editable_delete_text (editable, 0, gtk_entry_get_max_length (GTK_ENTRY (entry)));
	}

	text_length = g_utf8_strlen (text, -1);

	/* get first available position that it is a writeable format char */
	if (entry->priv->user_format) {
		if (!entry->priv->i_format)
			goto exit;

		format = entry->priv->i_format;	
		format_length = entry->priv->i_chars_length;
		if (format_length == 0 || *position >= format_length) 
			goto exit;
	}

	gboolean compl_done = FALSE;
	write_pos = -1;
	if (entry->priv->user_format) {
		write_pos = get_first_writable_index (entry, GDAUI_FORMAT_ENTRY_RIGHT);
		if (write_pos < 0) 
			goto exit;
		if (*position != write_pos)
			compl_done = TRUE;
		*position = write_pos;
		fi = *position;
	}

	/* treat @text char after char */
	const gchar *tptr;
	ci = 0;
#define NEWTEXT_SIZE 7
	newtext = g_new0 (gchar, NEWTEXT_SIZE);
	for (ti = 0, tptr = text; ti < text_length; ) {
		gunichar tchar, fchar;
		gboolean consume_char = TRUE;
		gboolean noinsert_char = FALSE;
		tchar = g_utf8_get_char (tptr);
		if (!entry->priv->user_format) {
			if (!adjust_computed_format_at_insert (entry, tchar, position, &noinsert_char))
				break;
			if (noinsert_char) {
				*position += 1;
				gtk_editable_set_position (editable, *position);
				goto chars_next;
			}

			if (write_pos < 0) {
				write_pos = get_first_writable_index (entry, GDAUI_FORMAT_ENTRY_RIGHT);
				if (write_pos < 0) 
					goto exit;
				else {
					*position = write_pos;
					fi = write_pos;
				}
			}
		}
		format = entry->priv->i_format;	
		format_length = entry->priv->i_chars_length;
		if (fi >= format_length)
			break;
		
		fchar = g_utf8_get_char (g_utf8_offset_to_pointer (format, fi));
		newtext[0] = 0;
		if (!char_is_writable (entry, fi)) {
			/* do nothing */
		}
		else {
			if (g_unichar_isdigit (tchar) &&
			    ((fchar == '0') || 
			     ((fchar == '9') && (tchar != '0')) || 
			     (fchar == '#'))) {
				g_unichar_to_utf8 (tchar, newtext);
			}
			else if (g_unichar_isalpha (tchar) &&
				 ((fchar == '@') || 
				  (fchar == '^') || 
				  (fchar == '#'))) {
				if (fchar == '^')
					g_unichar_to_utf8 (g_unichar_toupper (tchar), newtext);
				else
					g_unichar_to_utf8 (tchar, newtext);
			}
			else if (fchar == '*') {
				g_unichar_to_utf8 (tchar, newtext);
			}
			
			if (!newtext[0]) {
				/* use specified completions if provided */
				if (!entry->priv->disable_completions && !compl_done && entry->priv->i_compl) {
					fchar = g_utf8_get_char (g_utf8_offset_to_pointer (entry->priv->i_compl, fi));
					g_unichar_to_utf8 (fchar, newtext);
					consume_char = FALSE;
				}
				else
					break;
			}
		}
		
		if (newtext[0]) {
			signal_handlers_block (entry);
			if (entry->priv->user_format)
				gtk_editable_delete_text (editable, *position, *position + 1);
			gtk_editable_insert_text (editable, newtext, strlen (newtext), position);
			signal_handlers_unblock (entry);
			ci++;
			memset (newtext, 0, NEWTEXT_SIZE);
		}
		else {
			*position += 1;
			gtk_editable_set_position (editable, *position);
		}

	chars_next:
		fi = *position;
		if (consume_char) {
			ti++;
			tptr = g_utf8_next_char (tptr);
		}
	}
	g_free (newtext);

	if ((ci > 0) && !entry->priv->user_format)
		g_timeout_add_full (G_PRIORITY_HIGH, 1, (GSourceFunc) adjust_numeric_display, 
				    entry, NULL);	

 exit:
	g_signal_stop_emission_by_name (editable, "insert-text");
	LEAVE_INTERNAL_CHANGES (entry);
	g_signal_emit_by_name (entry, "changed");
}

/*
 * private functions
 */
static gboolean
char_is_writable (GdauiFormatEntry *entry, gint format_index)
{
	if ((format_index < 0) || (format_index >= entry->priv->i_chars_length))
		return FALSE;

	return (entry->priv->i_mask [format_index] == ' ') ? FALSE : TRUE;
}

/* returns -1 if there is no writable char */
static gint
get_first_writable_index (GdauiFormatEntry *entry, GdauiFormatEntryDirection direction)
{
	gint i, pos;

	pos = gtk_editable_get_position (GTK_EDITABLE (entry));

	if (!entry->priv->i_format)
		return -1;

	for (i = pos; (direction == GDAUI_FORMAT_ENTRY_LEFT ? i > 0 : i < entry->priv->i_chars_length);
	     (direction == GDAUI_FORMAT_ENTRY_LEFT ? i-- : i++)) {
		if (char_is_writable (entry, i))
			break;
	}
	
	if (i < entry->priv->i_chars_length)
		return i;
	else
		return -1;
}
