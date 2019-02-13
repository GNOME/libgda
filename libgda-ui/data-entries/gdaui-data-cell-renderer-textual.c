/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#define OPTIMIZE 0

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libgda/libgda.h>
#include "gdaui-data-cell-renderer-textual.h"
#include "gdaui-data-entry.h"
#include "gdaui-entry-string.h"
#include "gdaui-entry-number.h"
#include "gdaui-entry-date.h"
#include "gdaui-entry-time.h"
#include "gdaui-entry-timestamp.h"
#include <libgda/gda-enum-types.h>
#include "marshallers/gdaui-custom-marshal.h"
#include "gdaui-data-cell-renderer-util.h"

#define MAX_ACCEPTED_STRING_LENGTH 800U

static void gdaui_data_cell_renderer_textual_dispose    (GObject *object);
static void gdaui_data_cell_renderer_textual_finalize   (GObject *object);

static void gdaui_data_cell_renderer_textual_get_property  (GObject *object,
							    guint param_id,
							    GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_textual_set_property  (GObject *object,
							    guint param_id,
							    const GValue *value,
							    GParamSpec *pspec);
static void gdaui_data_cell_renderer_textual_get_size   (GtkCellRenderer          *cell,
							 GtkWidget                *widget,
							 const GdkRectangle       *cell_area,
							 gint                     *x_offset,
							 gint                     *y_offset,
							 gint                     *width,
							 gint                     *height);
static void gdaui_data_cell_renderer_textual_render     (GtkCellRenderer          *cell,
							 cairo_t                  *cr,
							 GtkWidget                *widget,
							 const GdkRectangle       *background_area,
							 const GdkRectangle       *cell_area,
							 GtkCellRendererState      flags);

static GtkCellEditable *gdaui_data_cell_renderer_textual_start_editing (GtkCellRenderer      *cell,
									GdkEvent             *event,
									GtkWidget            *widget,
									const gchar          *path,
									const GdkRectangle   *background_area,
									const GdkRectangle   *cell_area,
									GtkCellRendererState  flags);

enum {
	CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_VALUE,
	PROP_VALUE_ATTRIBUTES,
	PROP_TO_BE_DELETED,
	PROP_DATA_HANDLER,
	PROP_TYPE,
	PROP_OPTIONS
};

typedef struct
{
	GdaDataHandler *dh;
	GType           type;
	gboolean        type_forced; /* TRUE if ->type has been forced by a value and changed from what was specified */
	GValue         *value;
	gboolean        to_be_deleted;
	gboolean        invalid;

	gchar          *options;
	gchar          *currency;
	gint            max_length;
	gint            n_decimals;
	guchar          thousands_sep;

} GdauiDataCellRendererTextualPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataCellRendererTextual, gdaui_data_cell_renderer_textual, GTK_TYPE_CELL_RENDERER_TEXT)

typedef struct
{
	/* text renderer */
	gulong focus_out_id;
} GdauiDataCellRendererTextualInfo;
#define GDAUI_DATA_CELL_RENDERER_TEXTUAL_INFO_KEY "__info_key"

static guint text_cell_renderer_textual_signals [LAST_SIGNAL];

#define GDAUI_DATA_CELL_RENDERER_TEXTUAL_PATH "__path"

static void
gdaui_data_cell_renderer_textual_init (GdauiDataCellRendererTextual *datacell)
{
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);
	priv->dh = NULL;
	priv->type = GDA_TYPE_NULL;
	priv->type_forced = FALSE;
	priv->value = NULL;
	priv->options = NULL;

	priv->n_decimals = -1; /* unlimited */

	g_object_set ((GObject*) datacell, "xalign", 0.0, "yalign", 0.5,
		      "xpad", 2, "ypad", 2, NULL);
}

static void
gdaui_data_cell_renderer_textual_class_init (GdauiDataCellRendererTextualClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->dispose = gdaui_data_cell_renderer_textual_dispose;
	object_class->finalize = gdaui_data_cell_renderer_textual_finalize;

	object_class->get_property = gdaui_data_cell_renderer_textual_get_property;
	object_class->set_property = gdaui_data_cell_renderer_textual_set_property;

	cell_class->get_size = gdaui_data_cell_renderer_textual_get_size;
	cell_class->render = gdaui_data_cell_renderer_textual_render;
	cell_class->start_editing = gdaui_data_cell_renderer_textual_start_editing;

	g_object_class_install_property (object_class,
					 PROP_VALUE,
					 g_param_spec_pointer ("value",
							       _("Value"),
							       _("GValue to render"),
							       G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_VALUE_ATTRIBUTES,
					 g_param_spec_flags ("value-attributes", NULL, NULL, GDA_TYPE_VALUE_ATTRIBUTE,
							     GDA_VALUE_ATTR_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TO_BE_DELETED,
					 g_param_spec_boolean ("to-be-deleted", NULL, NULL, FALSE,
                                                               G_PARAM_WRITABLE));
	g_object_class_install_property(object_class,
					PROP_DATA_HANDLER,
					g_param_spec_object("data-handler", NULL, NULL, GDA_TYPE_DATA_HANDLER,
							    G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property(object_class,
					PROP_TYPE,
					g_param_spec_gtype("type", NULL, NULL, G_TYPE_NONE,
							   G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
	                                PROP_OPTIONS,
	                                g_param_spec_string("options", NULL, NULL, NULL,
	                                                    G_PARAM_WRITABLE));

	text_cell_renderer_textual_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdauiDataCellRendererTextualClass, changed),
			      NULL, NULL,
			      _gdaui_marshal_VOID__STRING_VALUE,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_VALUE);

}

static void
gdaui_data_cell_renderer_textual_dispose (GObject *object)
{
	GdauiDataCellRendererTextual *datacell = GDAUI_DATA_CELL_RENDERER_TEXTUAL (object);
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);

	if (priv->dh) {
		g_object_unref (G_OBJECT (priv->dh));
		priv->dh = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_textual_parent_class)->dispose (object);
}

static void
gdaui_data_cell_renderer_textual_finalize (GObject *object)
{
	GdauiDataCellRendererTextual *datacell = GDAUI_DATA_CELL_RENDERER_TEXTUAL (object);
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);

	g_free (priv->options);

	/* parent class */
	G_OBJECT_CLASS (gdaui_data_cell_renderer_textual_parent_class)->finalize (object);
}


static void
gdaui_data_cell_renderer_textual_get_property (GObject *object,
					       guint param_id,
					       G_GNUC_UNUSED GValue *value,
					       GParamSpec *pspec)
{
	switch (param_id) {
	case PROP_VALUE_ATTRIBUTES:
		/* nothing to do */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static gboolean
is_numerical (GType type, gboolean *is_int)
{
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == G_TYPE_LONG) ||
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_INT) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT)) {
		*is_int = TRUE;
		return TRUE;
	}
	else if ((type == G_TYPE_FLOAT) ||
		 (type == G_TYPE_DOUBLE) ||
		 (type == GDA_TYPE_NUMERIC)) {
		*is_int = FALSE;
		return TRUE;
	}
	else {
		*is_int = FALSE;
		return FALSE;
	}
}

/*
 * Add thousands separator and number of decimals if necessary
 * always return FALSE because it's a timeout call and we want to remove it.
 */
static gchar *
adjust_numeric_display (const gchar *number_text, gboolean is_int, gint n_decimals, guchar decimal_sep, guchar thousands_sep)
{
	gchar *new_text;
	gchar *ptr;
	gint number_length;
	gint i, index;

	number_length = strlen (number_text);

	/* make a copy of current number representation in a tmp buffer */
	new_text = g_new (gchar,
			  number_length * 2 + ((n_decimals >= 0) ? n_decimals : 0) + 1);
	memcpy (new_text, number_text, number_length + 1);

	/* try to set ptr to the 1st decimal separator */
	for (ptr = new_text, i = 0; *ptr && (*ptr != decimal_sep); ptr++, i++);

	/* handle decimals if necessary */
	if ((n_decimals >= 0) && !is_int) {
		if ((n_decimals == 0) && (*ptr == decimal_sep)) {
			*ptr = 0;
			number_length = i;
		}
		else if (n_decimals > 0) {
			gint n = 0;
			if (*ptr != decimal_sep) {
				g_assert (*ptr == 0);
				*ptr = decimal_sep;
				ptr++;
			}
			else {
				for (ptr++; *ptr && (n < n_decimals); n++, ptr++)
					g_assert (isdigit (*ptr));

				if (*ptr)
					*ptr = 0;
			}

			for (; n < n_decimals; n++, ptr++)
				*ptr = '0';
			*ptr = 0;
		}
		number_length = strlen (new_text);
	}

	/* add thousands separator if necessary */
	if (thousands_sep) {
		index = i;
		for (i--; i > 0; i--) {
			if (isdigit (new_text [i-1]) && (index - i) % 3 == 0) {
				memmove (new_text + i + 1, new_text + i, number_length - i + 1);
				number_length ++;
				new_text [i] = thousands_sep;
			}
		}
	}

	return new_text;
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
gdaui_data_cell_renderer_textual_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec)
{
	GdauiDataCellRendererTextual *datacell = GDAUI_DATA_CELL_RENDERER_TEXTUAL (object);
	gfloat xalign = 0.;
	const gchar* options;
	static gchar *too_long_msg = NULL;
	static gint too_long_msg_len;
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);

	if (!too_long_msg) {
		too_long_msg = g_strconcat ("<b><i>&lt;", _("string truncated because too long"),
					    "&gt;</i></b>", NULL);
		too_long_msg_len = strlen (too_long_msg);
	}

	switch (param_id) {
	case PROP_VALUE:
		/* FIXME: is it necessary to make a copy of value, couldn't we just use the
		 * value AS IS for performances reasons ? */
		if (! OPTIMIZE) {
			if (priv->value) {
				gda_value_free (priv->value);
				priv->value = NULL;
			}
		}

		if (value) {
			GValue *gval = g_value_get_pointer (value);
			if (gval && !gda_value_is_null (gval)) {
				gchar *str;
				gboolean is_int, is_num;

				if (G_VALUE_TYPE (gval) != priv->type) {
					if (!priv->type_forced) {
						priv->type_forced = TRUE;
						if (priv->type != GDA_TYPE_NULL)
							g_warning (_("Data cell renderer's specified type (%s) "
								     "differs from actual "
								     "value to display type (%s)"),
								   g_type_name (priv->type),
								   g_type_name (G_VALUE_TYPE (gval)));
					}
					else
						g_warning (_("Data cell renderer asked to display values of different "
							     "data types, at least %s and %s, which means the data model has "
							     "some incoherencies"),
							   g_type_name (priv->type),
							   g_type_name (G_VALUE_TYPE (gval)));
					priv->type = G_VALUE_TYPE (gval);
				}
				is_num = is_numerical (priv->type, &is_int);
				if (is_num)
					xalign = 1.;

				if (! OPTIMIZE)
					priv->value = gda_value_copy (gval);

				if (!priv->dh && (priv->type != GDA_TYPE_NULL))
					priv->dh = g_object_ref (gda_data_handler_get_default (priv->type));

				if (priv->dh) {
					str = gda_data_handler_get_str_from_value (priv->dh, gval);
					gboolean use_markup = FALSE;
					if (str) {
						guint length;
						length = g_utf8_strlen (str, -1);
						if (length > MAX_ACCEPTED_STRING_LENGTH + too_long_msg_len) {
							gchar *tmp;
							tmp = g_utf8_offset_to_pointer (str,
											MAX_ACCEPTED_STRING_LENGTH);
							*tmp = 0;
							tmp = g_markup_escape_text (str, -1);
							g_free (str);
							str = g_strconcat (tmp, too_long_msg, NULL);
							g_free (tmp);
							use_markup = TRUE;
						}
					}

					if (priv->options) {
						/* extra specific treatments to handle the options */
						gchar *tmp_str;

						if (!is_num && (priv->max_length > 0))
							str [priv->max_length] = 0;
						else if (is_num) {
							tmp_str = adjust_numeric_display (str, is_int,
											  priv->n_decimals,
											  get_default_decimal_sep (),
											  priv->thousands_sep);
							g_free (str);
							str = tmp_str;
						}
						if (priv->currency) {
							tmp_str = g_strdup_printf ("%s %s", str, priv->currency);
							g_free (str);
							str = tmp_str;
						}
					}

					if (use_markup)
						g_object_set (G_OBJECT (object), "markup",
							      str, "xalign", xalign, NULL);
					else
						g_object_set (G_OBJECT (object), "text",
							      str, "xalign", xalign, NULL);
					g_free (str);
				}
				else
					g_object_set (G_OBJECT (object), "text", _("<non-printable>"),
						      "xalign", xalign, NULL);
			}
			else if (gval)
				g_object_set (G_OBJECT (object), "text", "", NULL);
			else {
				priv->invalid = TRUE;
				g_object_set (G_OBJECT (object), "text", "", NULL);
			}
		}
		else {
			priv->invalid = TRUE;
			g_object_set (G_OBJECT (object), "text", "", NULL);
		}
		break;
	case PROP_VALUE_ATTRIBUTES:
		priv->invalid = g_value_get_flags (value) & GDA_VALUE_ATTR_DATA_NON_VALID ? TRUE : FALSE;
		break;
	case PROP_TO_BE_DELETED:
		priv->to_be_deleted = g_value_get_boolean (value);
		break;
	case PROP_DATA_HANDLER:
		if (priv->dh)
			g_object_unref (G_OBJECT (priv->dh));
		priv->dh = GDA_DATA_HANDLER (g_value_get_object (value));
		if (priv->dh)
			g_object_ref (G_OBJECT (priv->dh));
		break;
	case PROP_TYPE:
		priv->type = g_value_get_gtype (value);
		break;
	case PROP_OPTIONS:
		options = g_value_get_string(value);
		if (options) {
			GdaQuarkList *params;
			const gchar *str;

			priv->options = g_strdup (options);

			params = gda_quark_list_new_from_string (options);
			str = gda_quark_list_find (params, "MAX_SIZE");
			if (str)
				priv->max_length = atoi (str);
			str = gda_quark_list_find (params, "THOUSAND_SEP");
			if (str) {
				if ((*str == 't') || (*str == 'T'))
					priv->thousands_sep = get_default_thousands_sep ();
				else
					priv->thousands_sep = 0;
			}
			str = gda_quark_list_find (params, "NB_DECIMALS");
			if (str)
				priv->n_decimals = atoi (str);
			str = gda_quark_list_find (params, "CURRENCY");
			if (str)
				priv->currency = g_strdup_printf ("%s ", str);
			gda_quark_list_free (params);
		}

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_cell_renderer_textual_new:
 * @dh: (nullable): a #GdaDataHandler object, or %NULL
 * @type: the #GType being edited
 * @options: options as a string
 *
 * Creates a new #GdauiDataCellRendererTextual. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 *
 * Returns: (transfer full): the new cell renderer
 **/
GtkCellRenderer *
gdaui_data_cell_renderer_textual_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;

	g_return_val_if_fail (!dh || GDA_IS_DATA_HANDLER (dh), NULL);
	obj = g_object_new (GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL,
			    "type", type, "data-handler", dh,
	                    "options", options, NULL);

	return GTK_CELL_RENDERER (obj);
}


static void
gdaui_data_cell_renderer_textual_get_size (GtkCellRenderer *cell,
					   GtkWidget       *widget,
					   const GdkRectangle *cell_area,
					   gint            *x_offset,
					   gint            *y_offset,
					   gint            *width,
					   gint            *height)
{
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
}

static void
gdaui_data_cell_renderer_textual_render (GtkCellRenderer      *cell,
					 cairo_t              *cr,
					 GtkWidget            *widget,
					 const GdkRectangle   *background_area,
					 const GdkRectangle   *cell_area,
					 GtkCellRendererState  flags)

{
	GdauiDataCellRendererTextual *datacell = (GdauiDataCellRendererTextual*) cell;
	GtkCellRendererClass *text_class = g_type_class_peek (GTK_TYPE_CELL_RENDERER_TEXT);
	(text_class->render) (cell, cr, widget, background_area, cell_area, flags);
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);

	if (priv->to_be_deleted) {
		cairo_set_source_rgba (cr, 0., 0., 0., 1.);
		cairo_rectangle (cr, cell_area->x, cell_area->y + cell_area->height / 2. - .5,
				 cell_area->width, 1.);
		cairo_fill (cr);
	}
	if (priv->invalid)
		gdaui_data_cell_renderer_draw_invalid_area (cr, cell_area);
}

static void
gdaui_data_cell_renderer_textual_editing_done (GtkCellEditable *entry,
					       gpointer         data)
{
	const gchar *path;
	GdauiDataCellRendererTextualInfo *info;
	GValue *value;

	info = g_object_get_data (G_OBJECT (data),
				  GDAUI_DATA_CELL_RENDERER_TEXTUAL_INFO_KEY);

	if (info->focus_out_id > 0) {
		g_signal_handler_disconnect (entry, info->focus_out_id);
		info->focus_out_id = 0;
	}

	if (g_object_class_find_property (G_OBJECT_GET_CLASS (entry), "editing-canceled")) {
		gboolean editing_canceled;

		g_object_get (G_OBJECT (entry), "editing-canceled", &editing_canceled, NULL);
		if (editing_canceled)
			return;
	}

	path = g_object_get_data (G_OBJECT (entry), GDAUI_DATA_CELL_RENDERER_TEXTUAL_PATH);

	value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (entry));
	g_signal_emit (data, text_cell_renderer_textual_signals[CHANGED], 0, path, value);
	gda_value_free (value);
}

static gboolean
gdaui_data_cell_renderer_textual_focus_out_event (GtkWidget *entry,
						  G_GNUC_UNUSED GdkEvent  *event,
						  gpointer   data)
{
	gdaui_data_cell_renderer_textual_editing_done (GTK_CELL_EDITABLE (entry), data);

	/* entry needs focus-out-event */
	return FALSE;
}

static GtkCellEditable *
gdaui_data_cell_renderer_textual_start_editing (GtkCellRenderer      *cell,
						G_GNUC_UNUSED GdkEvent             *event,
						G_GNUC_UNUSED GtkWidget            *widget,
						const gchar          *path,
						G_GNUC_UNUSED const GdkRectangle *background_area,
						G_GNUC_UNUSED const GdkRectangle *cell_area,
						G_GNUC_UNUSED GtkCellRendererState  flags)
{
	GdauiDataCellRendererTextual *datacell;
	GtkWidget *entry;
	GdauiDataCellRendererTextualInfo *info;
	gboolean editable;

	datacell = GDAUI_DATA_CELL_RENDERER_TEXTUAL (cell);
	GdauiDataCellRendererTextualPrivate *priv = gdaui_data_cell_renderer_textual_get_instance_private (datacell);

	/* If the cell isn't editable we return NULL. */
	g_object_get (G_OBJECT (cell), "editable", &editable, NULL);
	if (!editable)
		return NULL;

	/* If there is no data handler then the cell also is not editable */
	if (!priv->dh)
		return NULL;

	if (priv->type == G_TYPE_DATE)
		entry = gdaui_entry_date_new (priv->dh);
	else if (priv->type == GDA_TYPE_TIME)
		entry = gdaui_entry_time_new (priv->dh);
	else if (priv->type == G_TYPE_DATE_TIME)
		entry = gdaui_entry_timestamp_new (priv->dh);
	else if (gdaui_entry_number_is_type_numeric (priv->type))
		entry = gdaui_entry_number_new (priv->dh, priv->type, priv->options);
	else
		entry = gdaui_entry_string_new (priv->dh, priv->type, priv->options);

	 g_object_set (G_OBJECT (entry), "is-cell-renderer", TRUE, NULL);

	if (OPTIMIZE) {
		GValue *orig;
		gchar *text;

		g_object_get (G_OBJECT (cell), "text", &text, NULL);
		orig = gda_data_handler_get_value_from_str (priv->dh, text, priv->type);
		g_free (text);
		gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), orig);
	}
	else
		gdaui_data_entry_set_reference_value (GDAUI_DATA_ENTRY (entry), priv->value);

	info = g_new0 (GdauiDataCellRendererTextualInfo, 1);
 	g_object_set_data_full (G_OBJECT (entry), GDAUI_DATA_CELL_RENDERER_TEXTUAL_PATH, g_strdup (path), g_free);
	g_object_set_data_full (G_OBJECT (cell), GDAUI_DATA_CELL_RENDERER_TEXTUAL_INFO_KEY, info, g_free);

	g_signal_connect (entry, "editing-done",
			  G_CALLBACK (gdaui_data_cell_renderer_textual_editing_done), datacell);
	info->focus_out_id = g_signal_connect (entry, "focus-out-event",
					       G_CALLBACK (gdaui_data_cell_renderer_textual_focus_out_event),
					       datacell);
	gtk_widget_show (entry);
	return GTK_CELL_EDITABLE (entry);
}
