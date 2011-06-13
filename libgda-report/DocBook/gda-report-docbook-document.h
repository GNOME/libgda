/*
 * Copyright (C) 2007 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_REPORT_DOCBOOK_DOCUMENT_H__
#define __GDA_REPORT_DOCBOOK_DOCUMENT_H__

#include <libgda-report/gda-report-document.h>

#define GDA_TYPE_REPORT_DOCBOOK_DOCUMENT            (gda_report_docbook_document_get_type())
#define GDA_REPORT_DOCBOOK_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DOCBOOK_DOCUMENT, GdaReportDocbookDocument))
#define GDA_REPORT_DOCBOOK_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DOCBOOK_DOCUMENT, GdaReportDocbookDocumentClass))
#define GDA_IS_REPORT_DOCBOOK_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_DOCBOOK_DOCUMENT))
#define GDA_IS_REPORT_DOCBOOK_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_DOCBOOK_DOCUMENT))

G_BEGIN_DECLS

typedef struct _GdaReportDocbookDocument      GdaReportDocbookDocument;
typedef struct _GdaReportDocbookDocumentClass GdaReportDocbookDocumentClass;
typedef struct _GdaReportDocbookDocumentPrivate GdaReportDocbookDocumentPrivate;

struct _GdaReportDocbookDocument {
	GdaReportDocument            base;
	GdaReportDocbookDocumentPrivate *priv;
};

struct _GdaReportDocbookDocumentClass {
	GdaReportDocumentClass       parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType              gda_report_docbook_document_get_type        (void) G_GNUC_CONST;

GdaReportDocument *gda_report_docbook_document_new             (GdaReportEngine *engine);

G_END_DECLS

#endif
