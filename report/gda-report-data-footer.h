/* libgda library
 *
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __gda_report_data_footer_h__
#define __gda_report_data_footer_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>
#include <gda-report-defs.h>

typedef struct _GdaReportDataFooter		GdaReportDataFooter;
typedef struct _GdaReportDataFooterClass	GdaReportDataFooterClass;

struct _GdaReportDataFooter {
	GdaReportSection	parent;
	
	GDA_ReportDataFooter	corba_reportdatafooter;
	GList*			errors_head;
};

struct _GdaReportDataFooterClass {
	GdaReportSectionClass	parent_class;
	
	void (* warning) (GdaReportDataFooter* object, GList* errors);
	void (* error)   (GdaReportDataFooter* object, GList* errors);
};

#define GDA_TYPE_REPORT_DATA_FOOTER          (gda_report_data_footer_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_DATA_FOOTER(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DATA_FOOTER, GdaReportDataFooter)
#define GDA_REPORT_DATA_FOOTER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DATA_FOOTER, GdaReportDataFooterClass)
#define GDA_IS_REPORT_DATA_FOOTER(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_DATA_FOOTER)
#define GDA_IS_REPORT_DATA_FOOTER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_DATA_FOOTER)
#else
#define GDA_REPORT_DATA_FOOTER(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_DATA_FOOTER, GdaReportDataFooter)
#define GDA_REPORT_DATA_FOOTER_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_DATA_FOOTER, GdaReportDataFooterClass)
#define GDA_IS_REPORT_DATA_FOOTER(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_DATA_FOOTER)
#define GDA_IS_REPORT_DATA_FOOTER_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_DATA_FOOTER))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_data_footer_get_type	(void);
#else
GtkType			gda_report_data_footer_get_type	(void);
#endif

GdaReportDataFooter*	gda_report_data_footer_new	();
void			gda_report_data_footer_free	(GdaReportDataFooter* object);

#if defined(__cplusplus)
}
#endif

#endif
