/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_REPORT_RML_DOCUMENT_H__
#define __GDA_REPORT_RML_DOCUMENT_H__

#include <libgda-report/gda-report-document.h>

G_BEGIN_DECLS


/* error reporting */
extern GQuark gda_report_rml_document_error_quark (void);
#define GDA_REPORT_RML_DOCUMENT_ERROR gda_report_rml_document_error_quark ()

typedef enum {
	GDA_REPORT_RML_DOCUMENT_GENERAL_ERROR
} GdaReportRmlDocumentError;


#define GDA_TYPE_REPORT_RML_DOCUMENT            (gda_report_rml_document_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaReportRmlDocument, gda_report_rml_document, GDA, REPORT_RML_DOCUMENT, GdaReportDocument)

struct _GdaReportRmlDocumentClass {
	GdaReportDocumentClass       parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-report-rml-document
 * @short_description: Report document based on the RML dialect
 * @title: GdaReportRmlDocument
 * @stability: Stable
 * @see_also: #GdaReportDocument
 *
 * The #GdaReportRmlDocument makes it easy to create reports based on a RML
 * template file. 
 *
 * RML (Report Markup Language) is an XML dialect which allows one to describe in a very precise way
 * layouts which can then be converted to PDF. For more information, see the
 * <ulink url="http://www.reportlab.org/">ReportLab</ulink> or
 * <ulink url="http://en.openreport.org/index.py/static/page/trml2pdf">OpenReport</ulink> web pages.
 */

GType              gda_report_rml_document_get_type        (void) G_GNUC_CONST;

GdaReportDocument *gda_report_rml_document_new             (GdaReportEngine *engine);

G_END_DECLS

#endif
