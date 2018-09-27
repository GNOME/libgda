/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-value.h>
#include <errno.h>

#include <math.h>

#include "gdaui-numeric-entry.h"

typedef struct {
        gboolean  is_numerical;
        gint64    imin;
        gint64    imax;
        guint64   uimax;
        gdouble   fmax;
        gboolean  is_int;
        gboolean  is_signed;
	guint8    max_nchars;
} NumAttr;

typedef struct {
	GType   type;
	gchar   decimal_sep; /* default obtained automatically */
	gchar   thousands_sep; /* 0 for no separator */
	guint16 nb_decimals; /* G_MAXUINT16 for no limit */

	NumAttr num_attr;
} GdauiNumericEntryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiNumericEntry, gdaui_numeric_entry, GDAUI_TYPE_ENTRY)

static void gdaui_numeric_entry_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_numeric_entry_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);
static void gdaui_numeric_entry_assume_insert (GdauiEntry *entry, const gchar *text, gint text_length, gint *virt_pos, gint offset);
static void gdaui_numeric_entry_assume_delete (GdauiEntry *entry, gint virt_start_pos, gint virt_end_pos, gint offset);

/* properties */
enum
{
        PROP_0,
	PROP_TYPE,
	PROP_N_DECIMALS,
        PROP_DECIMAL_SEP,
        PROP_THOUSANDS_SEP
};

static void
gdaui_numeric_entry_class_init (GdauiNumericEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	GDAUI_ENTRY_CLASS (klass)->assume_insert = gdaui_numeric_entry_assume_insert;
	GDAUI_ENTRY_CLASS (klass)->assume_delete = gdaui_numeric_entry_assume_delete;
	GDAUI_ENTRY_CLASS (klass)->get_empty_text = NULL;

	/* Properties */
        object_class->set_property = gdaui_numeric_entry_set_property;
        object_class->get_property = gdaui_numeric_entry_get_property;

        g_object_class_install_property (object_class, PROP_TYPE,
                                         g_param_spec_gtype ("type", NULL, NULL, G_TYPE_NONE,
							     G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_N_DECIMALS,
                                         g_param_spec_uint ("n-decimals", NULL, NULL,
							    0, G_MAXUINT16, G_MAXUINT16,
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_DECIMAL_SEP,
                                         g_param_spec_char ("decimal-sep", NULL, NULL,
							    0, 127, '.',
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_THOUSANDS_SEP,
                                         g_param_spec_char ("thousands-sep", NULL, NULL,
							    0, 127, ',',
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
}

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
	attr->max_nchars = 0;

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
                attr->is_signed = FALSE;}
        else if (type == G_TYPE_FLOAT) {
                attr->fmax = G_MAXFLOAT;
        }
        else if (type == G_TYPE_DOUBLE) {
                attr->fmax = G_MAXDOUBLE;
        }
        else if (type == GDA_TYPE_NUMERIC) {
		attr->max_nchars = 30;
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

	if (attr->is_numerical && attr->is_int) {
		guint64 number;
		if (attr->is_signed) {
			number = (guint64) attr->imax;
			attr->max_nchars = 1; /* for the sign */
		}
		else
			number = attr->uimax;

		do {
			number /= 10;
			attr->max_nchars ++;
		} while (number != 0);
	}
}

static gchar
get_default_decimal_sep ()
{
        static gchar value = 0;

        if (value == 0) {
                gchar text[20];
                sprintf (text, "%f", 1.23);
                value = text[1];
        }
        return value;
}

static void
gdaui_numeric_entry_init (GdauiNumericEntry *entry)
{
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);
	priv->type = G_TYPE_INT64;
	compute_numeric_attributes (priv->type, &(priv->num_attr));
	priv->decimal_sep = get_default_decimal_sep ();
	priv->thousands_sep = 0;
	priv->nb_decimals = G_MAXUINT16;
}

static void 
gdaui_numeric_entry_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdauiNumericEntry *entry;
	gchar *otext;

        entry = GDAUI_NUMERIC_ENTRY (object);
        GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);
        otext = gdaui_entry_get_text (GDAUI_ENTRY (entry));
        if (priv) {
                switch (param_id) {
                case PROP_TYPE: {
			NumAttr num_attr;
			compute_numeric_attributes (g_value_get_gtype (value), &num_attr);
			if (num_attr.is_numerical == FALSE)
				g_warning (_("Type %s is not numerical"), g_type_name (g_value_get_gtype (value)));
			else {
				priv->type = g_value_get_gtype (value);
				priv->num_attr = num_attr;
			}
                        break;
		}
		case PROP_N_DECIMALS:
                        priv->nb_decimals = g_value_get_uint (value);
                        break;
                case PROP_DECIMAL_SEP: {
			gchar sep;
                        sep = (gchar) g_value_get_schar (value);
                        if ((sep == 0) || (sep == '+') || (sep == '-'))
                                g_warning (_("Decimal separator cannot be the '%c' character"), sep ? sep : '0');
                        else {
                                priv->decimal_sep = (gchar) g_value_get_schar (value);
                        }
                        break;
                }
                case PROP_THOUSANDS_SEP: {
			gchar sep;
                        sep = (gchar) g_value_get_schar (value);
                        if ((sep == '+') || (sep == '-') || (sep == '_'))
                                g_warning (_("Decimal thousands cannot be the '%c' character"), sep);
                        else {
                                priv->thousands_sep = (gchar) g_value_get_schar (value);
                        }
                        break;
                }
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
	gdaui_entry_set_text (GDAUI_ENTRY (entry), otext);
	g_free (otext);

	gint msize;
	if (priv->num_attr.max_nchars == 0)
		msize = -1;
	else {
		msize = (gint) priv->num_attr.max_nchars;
		if (priv->thousands_sep)
			msize += priv->num_attr.max_nchars / 3;
		if (! priv->num_attr.is_int)
			msize += 1;
		if (priv->nb_decimals != G_MAXUINT16)
			msize += priv->nb_decimals;
	}
	/*g_print ("GdauiNumericEntry: type %s => msize = %d\n", g_type_name (priv->type), msize);*/
	gdaui_entry_set_width_chars (GDAUI_ENTRY (entry), msize);
}

static void
gdaui_numeric_entry_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdauiNumericEntry *entry;

        entry = GDAUI_NUMERIC_ENTRY (object);
        GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);
        if (priv) {
                switch (param_id) {
                case PROP_TYPE:
			g_value_set_gtype (value, priv->type);
                        break;
		case PROP_N_DECIMALS:
                        g_value_set_uint (value, priv->nb_decimals);
                        break;
                case PROP_DECIMAL_SEP:
                        g_value_set_schar (value, (gint8) priv->decimal_sep);
                        break;
                case PROP_THOUSANDS_SEP:
                        g_value_set_schar (value, (gint8) priv->thousands_sep);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                        break;
                }
        }
}

/*
 * text_unformat
 * @pos: an inout parameter to keep track of a position
 *
 * Removes any thousand separator
 *
 * Returns: @str
 */
static gchar *
text_unformat (GdauiNumericEntry *entry, gchar *str, gint *pos)
{
	g_assert (str);
	gchar *ptr;
	gint i, len;
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);
	len = strlen (str);
	for (ptr = str, i = 0;
	     *ptr;
	     i++) {
		if (*ptr == priv->thousands_sep) {
			memmove (ptr, ptr+1, len - (ptr - str));
			len--;
			if (*pos >= i)
				*pos = *pos - 1;
		}
		else if ((*ptr == priv->decimal_sep) &&
			 (ptr[1] == priv->decimal_sep)) {
			memmove (ptr, ptr+1, len - (ptr - str));
			len--;
			/*if (*pos > i)
			 *pos = *pos - 1;*/
		}
		else
			ptr++;
	}

	return str;
}

/*
 * text_reformat
 * @pos: an inout parameter to keep track of a position
 *
 * Retunrs: a new string
 */
static gchar *
text_reformat (GdauiNumericEntry *entry, gchar *str, gint *pos, gboolean allow_sep_ending)
{
	g_assert (str);
	GString *string;
	gint len;
	gchar *ptr;
	gint i, last_th_sep_pos;
	gint dec_pos = -1; /* position of the decimal */
	gint cpos;
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);

	len = strlen (str);
	string = g_string_new ("");
	if (priv->num_attr.is_int)
		last_th_sep_pos = len - 1;
	else
		last_th_sep_pos = -1;

	cpos = *pos;
	for (i = len - 1, ptr = str + len - 1;
	     ptr >= str;
	     ptr --, i --) {
		if (*ptr == priv->decimal_sep) {
			last_th_sep_pos = i - 1;
			dec_pos = len - i - 1;
		}
		else if (i == last_th_sep_pos - 3) {
			last_th_sep_pos = i;
			if (priv->thousands_sep) {
				g_string_append_c (string, priv->thousands_sep);
				if (i < cpos)
					cpos ++;
			}
		}
			
		g_string_append_c (string, *ptr);
	}
	if (last_th_sep_pos == -1) {
		cpos = *pos;
		g_string_truncate (string, 0);
		last_th_sep_pos = len - 1;
		for (i = len - 1, ptr = str + len - 1;
		     ptr >= str;
		     ptr --, i --) {
			if (i == last_th_sep_pos - 3) {
				last_th_sep_pos = i;
				if (priv->thousands_sep) {
					g_string_append_c (string, priv->thousands_sep);
					if (i < cpos)
						cpos ++;
				}
			}
				
			g_string_append_c (string, *ptr);
		}
	}
	g_strreverse (string->str);

	/* fix the number of decimals if necessary */
	if ((! priv->num_attr.is_int) &&
	    priv->nb_decimals != G_MAXUINT16) {
		if (dec_pos == -1) {
			/* no decimal */
			if (priv->nb_decimals > 0) {
				g_string_append_c (string, priv->decimal_sep);
				for (i = 0; i < priv->nb_decimals; i++)
					g_string_append_c (string, '0');
			}
		}
		else {
			gint nb_dec;
			len = strlen (string->str);
			nb_dec = dec_pos; /* FIXME */
			if (nb_dec < priv->nb_decimals) {
				for (i = nb_dec; i < priv->nb_decimals; i++)
					g_string_append_c (string, '0');
			}
			else if (nb_dec > priv->nb_decimals) {
				g_string_truncate (string, len - (nb_dec - priv->nb_decimals));
				if (!allow_sep_ending &&
				    (string->str[string->len - 1] == priv->decimal_sep)) {
					/* we don't want numbers terminated by priv->decimal_sep */
					g_string_truncate (string, string->len - 1);
				}
			}
		}
	}
	else if ((! priv->num_attr.is_int) &&
		 dec_pos >= 0) {
		/* remove all the '0' which are useless */
		while ((string->str[string->len - 1] == priv->decimal_sep) ||
		       (string->str[string->len - 1] == '0')) {
			if (string->str[string->len - 1] == priv->decimal_sep) {
				if (!allow_sep_ending)
					g_string_truncate (string, string->len - 1);
				break;
			}
			else
				g_string_truncate (string, string->len - 1);
		}
	}

	*pos = cpos;

	return g_string_free (string, FALSE);
}

static gboolean
test_text_validity (GdauiNumericEntry *entry, const gchar *text)
{
	gboolean retval = TRUE;
	gchar *endptr [1];
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);

#ifdef GDA_DEBUG_NO
	g_print ("Validity text: #%s#", text);
#endif
	
	if (text && ((*text == '-') || (*text == '+')) && (text[1] == 0))
		;
	else {
		gchar *tmp = g_strdup (text);
		if (priv->num_attr.is_int) {
			gchar *ptr;
			
			for (ptr = tmp; *ptr; ) {
				if (*ptr == priv->thousands_sep)
					/* remove that char */
					memmove (ptr, ptr + 1, strlen (ptr));
				else
					ptr++;
			}
			if (priv->num_attr.is_signed) {
				gint64 value;
				errno = 0;
				value = g_ascii_strtoll (tmp, endptr, 10);
				if (((value == G_MININT64) || (value == G_MAXINT64)) &&
				    (errno == ERANGE))
					retval = FALSE;
				if ((**endptr != 0) || (value < priv->num_attr.imin) || (value > priv->num_attr.imax))
					retval = FALSE;
			}
			else {
				guint64 value;
				errno = 0;
				value = g_ascii_strtoull (tmp, endptr, 10);
				if ((value == G_MAXUINT64) && (errno == ERANGE))
					retval = FALSE;
				if ((**endptr != 0) || (value > priv->num_attr.uimax))
					retval = FALSE;
			}
		}
		else {
			gchar *ptr;
			gdouble value;
			
			for (ptr = tmp; *ptr; ) {
				if (*ptr == priv->decimal_sep) {
					*ptr = get_default_decimal_sep ();
					ptr++;
				}
				else if (*ptr == priv->thousands_sep)
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
			    ((priv->num_attr.fmax > 0) && (value > priv->num_attr.fmax)))
				retval  = FALSE;
		}
		g_free (tmp);
	}
#ifdef GDA_DEBUG_NO
	g_print ("retval=%d\n", retval);
#endif
	return retval;
}


static void
gdaui_numeric_entry_assume_insert (GdauiEntry *entry, const gchar *text, gint text_length,
				   gint *virt_pos, gint offset)
{
	GdauiNumericEntry *fentry;
	gchar *otext, *ptr, *ntext;
	gchar tmp;
	gint i;
	GString *string;
	gint olen, nlen;

	fentry = (GdauiNumericEntry*) entry;
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (fentry);
	otext = gdaui_entry_get_text (GDAUI_ENTRY (entry));
	olen = strlen (otext);
	for (ptr = otext, i = 0; (i < *virt_pos) && *ptr; ptr = g_utf8_next_char (ptr), i++);
	if (i != *virt_pos)
		return;
	tmp = *ptr;
	*ptr = 0;
	string = g_string_new ("");
	g_string_append (string, otext);
	*ptr = tmp;
	g_string_append (string, text);
	g_string_append (string, ptr);
	g_free (otext);

	/*g_print ("RAW: [%s]", string->str);*/
	*virt_pos += text_length;
	text_unformat (fentry, string->str, virt_pos);
	/*g_print ("SANITIZED: [%s]", string->str);*/

	if (!test_text_validity (fentry, string->str)) {
		g_string_free (string, TRUE);
		/*g_print ("ERROR!\n");*/
		return;
	}
	ntext = text_reformat (fentry, string->str, virt_pos, (text[text_length-1] == priv->decimal_sep));
	g_string_free (string, TRUE);
	/*g_print ("NEW: [%s]\n", ntext);*/

	i = offset;
	nlen = strlen (ntext);
	gtk_editable_delete_text ((GtkEditable*) entry, offset, olen + offset);
	gtk_editable_insert_text ((GtkEditable*) entry, ntext, nlen, &i);
	g_free (ntext);
}

static void
gdaui_numeric_entry_assume_delete (GdauiEntry *entry, gint virt_start_pos, gint virt_end_pos, gint offset)
{
	GdauiNumericEntry *fentry;
	GString *string;
	gchar *otext, *ntext = NULL;
	gint i, nlen, ndel, olen = 0;
	gint cursor_pos;

	fentry = (GdauiNumericEntry*) entry;
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (fentry);
	ndel = virt_end_pos - virt_start_pos;
	otext = gdaui_entry_get_text (GDAUI_ENTRY (entry));
	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (entry));

	if (otext) {
		if ((ndel == 1) && (otext[virt_start_pos] == priv->decimal_sep) &&
		    (priv->nb_decimals != G_MAXUINT16)) {
			gtk_editable_set_position (GTK_EDITABLE (entry), cursor_pos - 1);
			g_free (otext);
			return;
		}
		string = g_string_new (otext);
		olen = g_utf8_strlen (otext, -1);
		g_free (otext);
		g_string_erase (string, virt_start_pos, ndel);
	}
	else
		string = g_string_new (NULL);
	
	cursor_pos -= (virt_end_pos - virt_start_pos);
	/*g_print ("RAW: [%s]", string->str);*/
	text_unformat (fentry, string->str, &cursor_pos);
	/*g_print ("SANITIZED: [%s]", string->str);*/

	if (!test_text_validity (fentry, string->str)) {
		if ((string->str[0] == priv->decimal_sep) &&
		    string->str[1] == 0)
			ntext = g_strdup ("");
		g_string_free (string, TRUE);
		if (!ntext) {
			/*g_print ("ERROR!\n");*/
			return;
		}
	}
	else {
		ntext = text_reformat (fentry, string->str, &cursor_pos, FALSE);
		g_string_free (string, TRUE);
	}
	/*g_print ("NEW: [%s]\n", ntext);*/

	i = offset;
	nlen = strlen (ntext);
	gtk_editable_delete_text ((GtkEditable*) entry, offset, olen + offset);
	gtk_editable_insert_text ((GtkEditable*) entry, ntext, nlen, &i);
	g_free (ntext);	
	gtk_editable_set_position (GTK_EDITABLE (entry), cursor_pos);
}

/**
 * gdaui_numeric_entry_new:
 * @type: the numeric type
 *
 * Creates a new #GdauiNumericEntry widget.
 *
 * Returns: (transfer full): the newly created #GdauiNumericEntry widget.
 */
GtkWidget*
gdaui_numeric_entry_new (GType type)
{
	GObject *obj;

	obj = g_object_new (GDAUI_TYPE_NUMERIC_ENTRY, "type", type, NULL);
	return GTK_WIDGET (obj);
}

/**
 * gdaui_numeric_entry_get_text:
 * @entry: a #GdauiNumericEntry widget
 *
 * Get @entry's contents as a #GValue.
 *
 * Returns: (transfer full): a new #GValue, or %NULL
 */
GValue *
gdaui_numeric_entry_get_value (GdauiNumericEntry *entry)
{
	gchar *text;
	GValue *value = NULL;
	g_return_val_if_fail (GDAUI_IS_NUMERIC_ENTRY (entry), NULL);
	
	text = gdaui_entry_get_text ((GdauiEntry*) entry);
	GdauiNumericEntryPrivate *priv = gdaui_numeric_entry_get_instance_private (entry);
	if (text) {
		gchar *ptr;
		gint len;
		len = strlen (text);
		for (ptr = text; *ptr; ) {
			if (*ptr == priv->thousands_sep)
				memmove (ptr, ptr+1, len - (ptr - text));
			else {
				if (*ptr == priv->decimal_sep)
					*ptr = '.';
				ptr++;
			}
		}
		value = gda_value_new_from_string (text, priv->type);
		g_free (text);
	}

	return value;
}
