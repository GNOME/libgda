/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <santi@gnome-db.org>
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

#if !defined(__gda_report_result_h__)
#  define __gda_report_result_h__

#include <libxml/tree.h>
#include <glib-object.h>
#include <libgda-report/gda-report-document.h>

G_BEGIN_DECLS

#define GDA_TYPE_REPORT_RESULT            (gda_report_result_get_type())
#define GDA_REPORT_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_RESULT, GdaReportResult))
#define GDA_REPORT_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_RESULT, GdaReportResultClass))
#define GDA_REPORT_IS_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_RESULT))
#define GDA_REPORT_IS_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_RESULT))

typedef struct _GdaReportResult GdaReportResult;
typedef struct _GdaReportResultClass GdaReportResultClass;
typedef struct _GdaReportResultPrivate GdaReportResultPrivate;

struct _GdaReportResult {
	GObject object;
	GdaReportResultPrivate *priv;
};

struct _GdaReportResultClass {
	GObjectClass parent_class;
};

GType gda_report_result_get_type (void);

GdaReportResult *gda_report_result_new_to_memory (GdaReportDocument *document);

GdaReportResult *gda_report_result_new_to_uri (const gchar *uri,
					       GdaReportDocument *document);


G_END_DECLS

#endif
