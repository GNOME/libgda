/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-document.h>

#define PARENT_TYPE GDA_TYPE_XML_DOCUMENT

#define OBJECT_REPORT         "report"
#define OBJECT_REPORT_ELEMENT "reportelement"

struct _GdaReportDocumentPrivate {
	GdaReportStyle        reportstyle;
	GdaReportPageSize     pagesize;
	GdaReportOrientation  orientation;
	GdaReportUnits        units;
	gfloat                topmargin;
	gfloat                bottommargin;
	gfloat                leftmargin;
	gfloat                rightmargin;
	GdaReportColor       *bgcolor;
	GdaReportColor       *fgcolor;
	GdaReportColor       *bordercolor;
	gfloat                borderwidth;
	GdaReportLineStyle    borderstyle;
	gchar                *fontfamily;
	gint                  fontsize;
	GdaReportFontWeight   fontweight;
	gboolean              fontitalic;
	GdaReportHAlignment   halignment;
	GdaReportVAlignment   valignment;
	gboolean              wordwrap;
	GdaReportColor       *negvaluecolor;
	gchar                *dateformat;
	gint8                 precision;
	gchar                *currency;
	gboolean              commaseparator;
	gfloat                linewidth;
	GdaReportColor       *linecolor;
	GdaReportLineStyle    linestyle;
};

static void gda_report_document_class_init (GdaReportDocumentClass *klass);
static void gda_report_document_init       (GdaReportDocument *document,
					    GdaReportDocumentClass *klass);
static void gda_report_document_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaReportDocument class implementation
 */

static void
gda_report_document_class_init (GdaReportDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_report_document_finalize;
}

static void
gda_report_document_init (GdaReportDocument *document, GdaReportDocumentClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	/* allocate private structure */
	document->priv = g_new0 (GdaReportDocumentPrivate, 1);
}

static void
gda_report_document_finalize (GObject *object)
{
	GdaReportDocument *document = (GdaReportDocument *) object;

	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	/* free memory */
	g_free (document->priv);
	document->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_report_document_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportDocumentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_document_class_init,
			NULL,
			NULL,
			sizeof (GdaReportDocument),
			0,
			(GInstanceInitFunc) gda_report_document_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaReportDocument", &info, 0);
	}
	return type;
}

/**
 * gda_report_document_new
 *
 * Create a new #GdaReportDocument object, which is a wrapper that lets
 * you easily manage the XML format used in the GDA report engine.
 *
 * Returns: the newly created object.
 */
GdaReportDocument *
gda_report_document_new (void)
{
	GdaReportDocument *document;

	document = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);
	gda_xml_document_construct (GDA_XML_DOCUMENT (document), OBJECT_REPORT);

	return document;
}

/**
 * gda_report_document_new_from_string
 */
GdaReportDocument *
gda_report_document_new_from_string (const gchar *xml)
{
	GdaReportDocument *document;

	g_return_val_if_fail (xml != NULL, NULL);

	/* parse the XML string */
	document = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);

	GDA_XML_DOCUMENT (document)->doc = xmlParseMemory (xml, strlen (xml));
	if (!GDA_XML_DOCUMENT (document)->doc) {
		gda_log_error (_("Could not parse XML document"));
		g_object_unref (G_OBJECT (document));
		return NULL;
	}

	GDA_XML_DOCUMENT (document)->root =
		xmlDocGetRootElement (GDA_XML_DOCUMENT (document)->doc);

	return document;
}

/**
 * gda_report_document_new_from_uri
 */
GdaReportDocument *
gda_report_document_new_from_uri (const gchar *uri)
{
	gchar *body;
	GdaReportDocument *document;

	g_return_val_if_fail (uri != NULL, NULL);

	/* get the file contents from the given URI */
	body = gda_file_load (uri);
	if (!body) {
		gda_log_error (_("Could not get file from %s"), uri);
		return NULL;
	}

	/* create the GdaReportDocument object */
	document = gda_report_document_new_from_string (body);
	g_free (body);

	return document;
}


/**
 * gda_report_document_get_report_style
 */
GdaReportStyle
gda_report_document_get_report_style (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->reportstyle;
}

/**
 * gda_report_document_set_report_style
 */
void
gda_report_document_set_report_style (GdaReportDocument *document,
				     GdaReportStyle reportstyle)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->reportstyle = reportstyle;
}

/**
 * gda_report_document_get_page_size
 */
GdaReportPageSize
gda_report_document_get_page_size (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->pagesize;
}

/**
 * gda_report_document_set_page_size
 */
void
gda_report_document_set_page_size (GdaReportDocument *document,
				  GdaReportPageSize pagesize)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->pagesize = pagesize;
}

/**
 * gda_report_document_get_orientation
 */
GdaReportOrientation
gda_report_document_get_orientation (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->orientation;
}

/**
 * gda_report_document_set_orientation
 */
void
gda_report_document_set_orientation (GdaReportDocument *document,
				     GdaReportOrientation orientation)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->orientation = orientation;
}

/**
 * gda_report_document_get_units
 */
GdaReportUnits
gda_report_document_get_units (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->units;
}

/**
 * gda_report_document_set_units
 */
void
gda_report_document_set_units (GdaReportDocument *document,
			       GdaReportUnits units)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->units = units;
}

/**
 * gda_report_document_get_top_margin
 */
gfloat
gda_report_document_get_top_margin (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->topmargin;
}

/**
 * gda_report_document_set_top_margin
 */
void
gda_report_document_set_top_margin (GdaReportDocument *document,
				   gfloat topmargin)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->topmargin = topmargin;
}

/**
 * gda_report_document_get_bottom_margin
 */
gfloat
gda_report_document_get_bottom_margin (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->bottommargin;
}

/**
 * gda_report_document_set_bottom_margin
 */
void
gda_report_document_set_bottom_margin (GdaReportDocument *document,
				      gfloat bottommargin)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->bottommargin = bottommargin;
}

/**
 * gda_report_document_get_left_margin
 */
gfloat
gda_report_document_get_left_margin (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->leftmargin;
}

/**
 * gda_report_document_set_left_margin
 */
void
gda_report_document_set_left_margin (GdaReportDocument *document,
				    gfloat leftmargin)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->leftmargin = leftmargin;
}

/**
 * gda_report_document_get_right_margin
 */
gfloat
gda_report_document_get_right_margin (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->rightmargin;
}

/**
 * gda_report_document_set_righ_tmargin
 */
void
gda_report_document_set_right_margin (GdaReportDocument *document,
				     gfloat rightmargin)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->rightmargin = rightmargin;
}

/**
 * gda_report_document_get_bg_color
 */
GdaReportColor *
gda_report_document_get_bg_color (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->bgcolor;
}

/**
 * gda_report_document_set_bg_color
 */
void
gda_report_document_set_bg_color (GdaReportDocument *document,
				 GdaReportColor *bgcolor)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->bgcolor = bgcolor;
}

/**
 * gda_report_document_get_fg_color
 */
GdaReportColor *
gda_report_document_get_fg_color (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->fgcolor;
}

/**
 * gda_report_document_set_fg_color
 */
void
gda_report_document_set_fg_color (GdaReportDocument *document,
				 GdaReportColor *fgcolor)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->fgcolor = fgcolor;
}

/**
 * gda_report_document_get_border_color
 */
GdaReportColor *
gda_report_document_get_border_color (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->bordercolor;
}

/**
 * gda_report_document_set_border_color
 */
void
gda_report_document_set_border_color (GdaReportDocument *document,
				     GdaReportColor *bordercolor)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->bordercolor = bordercolor;
}

/**
 * gda_report_document_get_border_width
 */
gfloat
gda_report_document_get_border_width (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->borderwidth;
}

/**
 * gda_report_document_set_border_width
 */
void
gda_report_document_set_border_width (GdaReportDocument *document,
				     gfloat borderwidth)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->borderwidth = borderwidth;
}

/**
 * gda_report_document_get_border_style
 */
GdaReportLineStyle
gda_report_document_get_border_style (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->borderstyle;
}

/**
 * gda_report_document_set_border_style
 */
void
gda_report_document_set_border_style (GdaReportDocument *document,
				     GdaReportLineStyle borderstyle)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->borderstyle = borderstyle;
}

/**
 * gda_report_document_get_font_family
 */
gchar *
gda_report_document_get_font_family (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->fontfamily;
}

/**
 * gda_report_document_set_font_family
 */
void
gda_report_document_set_font_family (GdaReportDocument *document,
				    gchar *fontfamily)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->fontfamily = fontfamily;
}

/**
 * gda_report_document_get_font_size
 */
gint
gda_report_document_get_font_size (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->fontsize;
}

/**
 * gda_report_document_set_font_size
 */
void
gda_report_document_set_font_size (GdaReportDocument *document,
				  gint fontsize)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->fontsize = fontsize;
}

/**
 * gda_report_document_get_font_weight
 */
GdaReportFontWeight
gda_report_document_get_font_weight (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->fontweight;
}

/**
 * gda_report_document_set_font_weight
 */
void
gda_report_document_set_font_weight (GdaReportDocument *document,
				    GdaReportFontWeight fontweight)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->fontweight = fontweight;
}

/**
 * gda_report_document_get_font_italic
 */
gboolean
gda_report_document_get_font_italic (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->fontitalic;
}

/**
 * gda_report_document_set_font_italic
 */
void
gda_report_document_set_font_italic (GdaReportDocument *document,
				    gboolean fontitalic)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->fontitalic = fontitalic;
}

/**
 * gda_report_document_get_halignment
 */
GdaReportHAlignment
gda_report_document_get_halignment (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->halignment;
}

/**
 * gda_report_document_set_halignment
 */
void
gda_report_document_set_halignment (GdaReportDocument *document,
				    GdaReportHAlignment alignment)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->halignment = alignment;
}

/**
 * gda_report_document_get_valignment
 */
GdaReportVAlignment
gda_report_document_get_valignment (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->valignment;
}

/**
 * gda_report_document_set_valignment
 */
void
gda_report_document_set_valignment (GdaReportDocument *document,
				    GdaReportVAlignment alignment)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->valignment = alignment;
}

/**
 * gda_report_document_get_word_wrap
 */
gboolean
gda_report_document_get_word_wrap (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->wordwrap;
}

/**
 * gda_report_document_set_word_wrap
 */
void
gda_report_document_set_word_wrap (GdaReportDocument *document,
				  gboolean wordwrap)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->wordwrap = wordwrap;
}

/**
 * gda_report_document_get_neg_value_color
 */
GdaReportColor *
gda_report_document_get_neg_value_color (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->negvaluecolor;
}

/**
 * gda_report_document_set_neg_value_color
 */
void
gda_report_document_set_neg_value_color (GdaReportDocument *document,
				       GdaReportColor *negvaluecolor)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->negvaluecolor = negvaluecolor;
}

/**
 * gda_report_document_get_date_format
 */
gchar *
gda_report_document_get_date_format (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->dateformat;
}

/**
 * gda_report_document_set_date_format
 */
void
gda_report_document_set_date_format (GdaReportDocument *document,
				     gchar *dateformat)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->dateformat = dateformat;
}

/**
 * gda_report_document_get_precision
 */
gint8
gda_report_document_get_precision (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->precision;
}

/**
 * gda_report_document_set_precision
 */
void
gda_report_document_set_precision (GdaReportDocument *document,
				   gint8 precision)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->precision = precision;
}

/**
 * gda_report_document_get_currency
 */
gchar *
gda_report_document_get_currency (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->currency;
}

/**
 * gda_report_document_set_currency
 */
void
gda_report_document_set_currency (GdaReportDocument *document,
				  gchar *currency)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->currency = currency;
}

/**
 * gda_report_document_get_comma_separator
 */
gboolean
gda_report_document_get_comma_separator (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->commaseparator;
}

/**
 * gda_report_document_set_comma_separator
 */
void
gda_report_document_set_comma_separator (GdaReportDocument *document,
					 gboolean commaseparator)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->commaseparator = commaseparator;
}

/**
 * gda_report_document_get_line_width
 */
gfloat
gda_report_document_get_line_width (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->linewidth;
}

/**
 * gda_report_document_set_line_width
 */
void
gda_report_document_set_line_width (GdaReportDocument *document,
				   gfloat linewidth)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->linewidth = linewidth;
}

/**
 * gda_report_document_get_line_color
 */
GdaReportColor *
gda_report_document_get_line_color (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), NULL);

	return document->priv->linecolor;
}

/**
 * gda_report_document_set_line_color
 */
void
gda_report_document_set_line_color (GdaReportDocument *document,
				    GdaReportColor *linecolor)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->linecolor = linecolor;
}

/**
 * gda_report_document_get_line_style
 */
GdaReportLineStyle
gda_report_document_get_line_style (GdaReportDocument *document)
{
	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (document), -1);

	return document->priv->linestyle;
}

/**
 * gda_report_document_set_line_style
 */
void
gda_report_document_set_line_style (GdaReportDocument *document,
				   GdaReportLineStyle linestyle)
{
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT (document));

	document->priv->linestyle = linestyle;
}
