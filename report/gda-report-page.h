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

#ifndef __gda_report_page_h__
#define __gda_report_page_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>
#include <gda-report-page-header.h>
#include <gda-report-data-header.h>
#include <gda-report-data-footer.h>
#include <gda-report-page-footer.h>

typedef struct _GdaReportPage		GdaReportPage;
typedef struct _GdaReportPageClass	GdaReportPageClass;

struct _GdaReportPage {
#ifdef HAVE_GOBJECT
	GObject		parent;
#else
	GtkObject	parent;
#endif
	GDA_ReportPage  corba_reportpage;
	GList*		errors_head;
};

struct _GdaReportPageClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReportPage* object, GList* errors);
	void (* error)   (GdaReportPage* object, GList* errors);
};

#define GDA_TYPE_REPORT_PAGE          (gda_report_page_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_PAGE(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_PAGE, GdaReportPage)
#define GDA_REPORT_PAGE_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_PAGE, GdaReportPageClass)
#define GDA_IS_REPORT_PAGE(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_PAGE)
#define GDA_IS_REPORT_PAGE_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_PAGE)
#else
#define GDA_REPORT_PAGE(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_PAGE, GdaReportPage)
#define GDA_REPORT_PAGE_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_PAGE, GdaReportPageClass)
#define GDA_IS_REPORT_PAGE(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_PAGE)
#define GDA_IS_REPORT_PAGE_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_PAGE))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_page_get_type	(void);
#else
GtkType			gda_report_page_get_type	(void);
#endif

GdaReportPage*		gda_report_page_new		();
void			gda_report_page_free		(GdaReportPage* object);

GdaReportPageHeader*	gda_report_page_get_pheader	(GdaReportPage* object);
void			gda_report_page_set_pheader	(GdaReportPage* object,
							 GdaReportPageHeader* pheader);
GdaReportDataHeader*	gda_report_page_get_dheader	(GdaReportPage* object);
void			gda_report_page_set_dheader	(GdaReportPage* object,
							 GdaReportDataHeader* dheader);
GList*			gda_report_page_get_datalist	(GdaReportPage* object);
void			gda_report_page_set_datalist	(GdaReportPage* object,
							 GList* datalist);
GdaReportDataFooter*	gda_report_page_get_dfooter	(GdaReportPage* object);
void			gda_report_page_set_dfooter	(GdaReportPage* object,
							 GdaReportDataFooter* dfooter);
GdaReportPageFooter*	gda_report_page_get_pfooter	(GdaReportPage* object);
void			gda_report_page_set_pfooter	(GdaReportPage* object,
							 GdaReportPageFooter* pfooter);
							 
#if defined(__cplusplus)
}
#endif

#endif
