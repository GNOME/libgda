/* GDA report libary
 * Copyright (C) 1998-2001 The Free Software Foundation
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

G_BEGIN_DECLS

#define GDA_TYPE_REPORT_DOCUMENT            (gda_report_document_get_type())
#define GDA_REPORT_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DOCUMENT, GdaReportDocument))
#define GDA_REPORT_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DOCUMENT, GdaReportDocumentClass))
#define GDA_IS_REPORT_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_DOCUMENT))
#define GDA_IS_REPORT_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_DOCUMENT))

typedef struct _GdaReportDocument        GdaReportDocument;
typedef struct _GdaReportDocumentClass   GdaReportDocumentClass;
typedef struct _GdaReportDocumentPrivate GdaReportDocumentPrivate;

struct _GdaReportDocument {
	GdaXmlDocument document;
	GdaReportDocumentPrivate *priv;
};

struct _GdaReportDocumentClass {
	GdaXmlDocumentClass parent_class;
};

GType              gda_report_document_get_type (void);
GdaReportDocument *gda_report_document_new (void);

G_END_DECLS

#endif
