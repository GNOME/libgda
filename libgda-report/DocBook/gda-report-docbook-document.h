/* GDA 
 * Copyright (C) 2007 The GNOME Foundation.
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
};

GType              gda_report_docbook_document_get_type        (void) G_GNUC_CONST;

GdaReportDocument *gda_report_docbook_document_new             (GdaReportEngine *engine);

G_END_DECLS

#endif
