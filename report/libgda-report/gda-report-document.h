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

#if !defined(__gda_report_document_h__)
#  define __gda_report_document_h__

#include <libgda/gda-xml-document.h>
#include <libgda-report/gda-report-types.h>

G_BEGIN_DECLS

#define GDA_TYPE_REPORT_DOCUMENT            (gda_report_document_get_type())
#define GDA_REPORT_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DOCUMENT, GdaReportDocument))
#define GDA_REPORT_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DOCUMENT, GdaReportDocumentClass))
#define GDA_IS_REPORT_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_DOCUMENT))
#define GDA_IS_REPORT_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_DOCUMENT))

typedef struct _GdaReportDocument GdaReportDocument;
typedef struct _GdaReportDocumentClass GdaReportDocumentClass;
typedef struct _GdaReportDocumentPrivate GdaReportDocumentPrivate;

struct _GdaReportDocument {
	GdaXmlDocument document;
	GdaReportDocumentPrivate *priv;
};

struct _GdaReportDocumentClass {
	GdaXmlDocumentClass parent_class;
};

GType gda_report_document_get_type (void);
GdaReportDocument *gda_report_document_new (void);
GdaReportDocument *gda_report_document_new_from_string (const gchar * xml);
GdaReportDocument *gda_report_document_new_from_uri (const gchar * uri);

GdaReportStyle gda_report_document_get_reportstyle (GdaReportDocument *
						    document);
void gda_report_document_set_reportstyle (GdaReportDocument * document,
					  GdaReportStyle reportstyle);
GdaReportPageSize gda_report_document_get_pagesize (GdaReportDocument *
						    document);
void gda_report_document_set_pagesize (GdaReportDocument * document,
				       GdaReportPageSize pagesize);
GdaReportOrientation gda_report_document_get_orientation (GdaReportDocument
							  * document);
void gda_report_document_set_orientation (GdaReportDocument * document,
					  GdaReportOrientation
					  orientation);
GdaReportUnits gda_report_document_get_units (GdaReportDocument *
					      document);
void gda_report_document_set_units (GdaReportDocument * document,
				    GdaReportUnits units);
gfloat gda_report_document_get_topmargin (GdaReportDocument * document);
void gda_report_document_set_topmargin (GdaReportDocument * document,
					gfloat topmargin);
gfloat gda_report_document_get_bottommargin (GdaReportDocument * document);
void gda_report_document_set_bottommargin (GdaReportDocument * document,
					   gfloat bottommargin);
gfloat gda_report_document_get_leftmargin (GdaReportDocument * document);
void gda_report_document_set_leftmargin (GdaReportDocument * document,
					 gfloat leftmargin);
gfloat gda_report_document_get_rightmargin (GdaReportDocument * document);
void gda_report_document_set_rightmargin (GdaReportDocument * document,
					  gfloat rightmargin);
GdaReportColor *gda_report_document_get_bgcolor (GdaReportDocument *
						 document);
void gda_report_document_set_bgcolor (GdaReportDocument * document,
				      GdaReportColor * bgcolor);
GdaReportColor *gda_report_document_get_fgcolor (GdaReportDocument *
						 document);
void gda_report_document_set_fgcolor (GdaReportDocument * document,
				      GdaReportColor * fgcolor);
GdaReportColor *gda_report_document_get_bordercolor (GdaReportDocument *
						     document);
void gda_report_document_set_bordercolor (GdaReportDocument * document,
					  GdaReportColor * bordercolor);
gfloat gda_report_document_get_borderwidth (GdaReportDocument * document);
void gda_report_document_set_borderwidth (GdaReportDocument * document,
					  gfloat borderwidth);
GdaReportLineStyle gda_report_document_get_borderstyle (GdaReportDocument
							  * document);
void gda_report_document_set_borderstyle (GdaReportDocument * document,
					  GdaReportLineStyle
					  borderstyle);
gchar *gda_report_document_get_fontfamily (GdaReportDocument * document);
void gda_report_document_set_fontfamily (GdaReportDocument * document,
					 gchar * fontfamily);
gint gda_report_document_get_fontsize (GdaReportDocument * document);
void gda_report_document_set_fontsize (GdaReportDocument * document,
				       gint fontsize);
GdaReportFontWeight gda_report_document_get_fontweight (GdaReportDocument *
							document);
void gda_report_document_set_fontweight (GdaReportDocument * document,
					 GdaReportFontWeight fontweight);
gboolean gda_report_document_get_fontitalic (GdaReportDocument * document);
void gda_report_document_set_fontitalic (GdaReportDocument * document,
					 gboolean fontitalic);
GdaReportHAlignment gda_report_document_get_halignment (GdaReportDocument *
						       document);
void gda_report_document_set_halignment (GdaReportDocument * document,
					 GdaReportHAlignment alignment);
GdaReportVAlignment gda_report_document_get_valignment (GdaReportDocument *
							document);
void gda_report_document_set_valignment (GdaReportDocument * document,
					 GdaReportVAlignment alignment);
gboolean gda_report_document_get_wordwrap (GdaReportDocument * document);
void gda_report_document_set_wordwrap (GdaReportDocument * document,
				       gboolean wordwrap);
GdaReportColor *gda_report_document_get_negvaluecolor (GdaReportDocument *
						       document);
void gda_report_document_set_negvaluecolor (GdaReportDocument * document,
					    GdaReportColor * negvaluecolor);
gchar *gda_report_document_get_dateformat (GdaReportDocument * document);
void gda_report_document_set_dateformat (GdaReportDocument * document,
					 gchar * dateformat);
gint8 gda_report_document_get_precision (GdaReportDocument * document);
void gda_report_document_set_precision (GdaReportDocument * document,
					gint8 precision);
gchar *gda_report_document_get_currency (GdaReportDocument * document);
void gda_report_document_set_currency (GdaReportDocument * document,
				       gchar * currency);
gboolean gda_report_document_get_commaseparator (GdaReportDocument *
						 document);
void gda_report_document_set_commaseparator (GdaReportDocument * document,
					     gboolean commaseparator);
gfloat gda_report_document_get_linewidth (GdaReportDocument * document);
void gda_report_document_set_linewidth (GdaReportDocument * document,
					gfloat linewidth);
GdaReportColor *gda_report_document_get_linecolor (GdaReportDocument *
						   document);
void gda_report_document_set_linecolor (GdaReportDocument * document,
					GdaReportColor * linecolor);
GdaReportLineStyle gda_report_document_get_linestyle (GdaReportDocument *
						      document);
void gda_report_document_set_linestyle (GdaReportDocument * document,
					GdaReportLineStyle linestyle);

G_END_DECLS

#endif
