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

#ifndef __gda_report_page_footer_h__
#define __gda_report_page_footer_h__

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

typedef struct _GdaReportPageFooter		GdaReportPageFooter;
typedef struct _GdaReportPageFooterClass	GdaReportPageFooterClass;

struct _GdaReportPageFooter {
	GdaReportSection	parent;
	
	GDA_ReportPageFooter	corba_reportpagefooter;
	GList*			errors_head;
};

struct _GdaReportPageFooterClass {
	GdaReportSectionClass	parent_class;
	
	void (* warning) (GdaReportPageFooter* object, GList* errors);
	void (* error)   (GdaReportPageFooter* object, GList* errors);
};

#define GDA_TYPE_REPORT_PAGE_FOOTER          (gda_report_page_footer_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_PAGE_FOOTER(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_PAGE_FOOTER, GdaReportPageFooter)
#define GDA_REPORT_PAGE_FOOTER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_PAGE_FOOTER, GdaReportPageFooterClass)
#define GDA_IS_REPORT_PAGE_FOOTER(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_PAGE_FOOTER)
#define GDA_IS_REPORT_PAGE_FOOTER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_PAGE_FOOTER)
#else
#define GDA_REPORT_PAGE_FOOTER(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_PAGE_FOOTER, GdaReportPageFooter)
#define GDA_REPORT_PAGE_FOOTER_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_PAGE_FOOTER, GdaReportPageFooterClass)
#define GDA_IS_REPORT_PAGE_FOOTER(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_PAGE_FOOTER)
#define GDA_IS_REPORT_PAGE_FOOTER_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_PAGE_FOOTER))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_page_footer_get_type	(void);
#else
GtkType			gda_report_page_footer_get_type	(void);
#endif

GdaReportPageFooter*	gda_report_page_footer_new		();
void			gda_report_page_footer_free		(GdaReportPageFooter* object);

ReportPositionFreq	gda_report_page_footer_get_positionfreq	(GdaReportPageFooter* object);
void			gda_report_page_footer_set_positionfreq	(GdaReportPageFooter* object,
								 ReportPositionFreq positionfreq);
ReportPageFreq		gda_report_page_footer_get_pagefreq	(GdaReportPageFooter* object);
void			gda_report_page_footer_set_pagefreq	(GdaReportPageFooter* object,
								 ReportPageFreq pagefreq);

#if defined(__cplusplus)
}
#endif

#endif
