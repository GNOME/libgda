/* GDA report engine
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

#if !defined(__gda_report_output_h__)
#  define __gda_report_output_h__

#include <bonobo/bonobo-xobject.h>
#include <libgda-report/GNOME_Database_Report.h>
#include <libgda-report/gda-report-document.h>

#define GDA_TYPE_REPORT_OUTPUT            (gda_report_output_get_type())
#define GDA_REPORT_OUTPUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_OUTPUT, GdaReportOutput))
#define GDA_REPORT_OUTPUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_OUTPUT, GdaReportOutputClass))
#define GDA_IS_REPORT_OUTPUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_OUTPUT))
#define GDA_IS_REPORT_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_OUTPUT))

typedef struct _GdaReportOutput        GdaReportOutput;
typedef struct _GdaReportOutputClass   GdaReportOutputClass;
typedef struct _GdaReportOutputPrivate GdaReportOutputPrivate;

struct _GdaReportOutput {
	BonoboXObject object;
	GdaReportOutputPrivate *priv;
};

struct _GdaReportOutputClass {
	BonoboXObjectClass parent_class;
	POA_GNOME_Database_Report_Output__epv epv;
};

GType            gda_report_output_get_type (void);
GdaReportOutput *gda_report_output_new (GdaReportDocument *input);

G_END_DECLS

#endif
