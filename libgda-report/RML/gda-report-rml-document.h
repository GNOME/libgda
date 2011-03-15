/*
 * Copyright (C) 2007 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_REPORT_RML_DOCUMENT_H__
#define __GDA_REPORT_RML_DOCUMENT_H__

#include <libgda-report/gda-report-document.h>

#define GDA_TYPE_REPORT_RML_DOCUMENT            (gda_report_rml_document_get_type())
#define GDA_REPORT_RML_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_RML_DOCUMENT, GdaReportRmlDocument))
#define GDA_REPORT_RML_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_RML_DOCUMENT, GdaReportRmlDocumentClass))
#define GDA_IS_REPORT_RML_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_RML_DOCUMENT))
#define GDA_IS_REPORT_RML_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_RML_DOCUMENT))

G_BEGIN_DECLS

typedef struct _GdaReportRmlDocument      GdaReportRmlDocument;
typedef struct _GdaReportRmlDocumentClass GdaReportRmlDocumentClass;
typedef struct _GdaReportRmlDocumentPrivate GdaReportRmlDocumentPrivate;

struct _GdaReportRmlDocument {
	GdaReportDocument            base;
	GdaReportRmlDocumentPrivate *priv;
};

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
