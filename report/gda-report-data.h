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

#ifndef __gda_report_data_h__
#define __gda_report_data_h__

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
#include <gda-report-group-header.h>
#include <gda-report-detail.h>
#include <gda-report-group-footer.h>

typedef struct _GdaReportData      GdaReportData;
typedef struct _GdaReportDataClass  GdaReportDataClass;

struct _GdaReportData {
#ifdef HAVE_GOBJECT
	GObject		parent;
#else
	GtkObject	parent;
#endif
	GDA_ReportData  corba_reportdata;
	GList*		errors_head;
};

struct _GdaReportDataClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReportData* object, GList* errors);
	void (* error)   (GdaReportData* object, GList* errors);
};

#define GDA_TYPE_REPORT_DATA          (gda_report_data_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_DATA(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DATA, GdaReportData)
#define GDA_REPORT_DATA_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DATA, GdaReportDataClass)
#define GDA_IS_REPORT_DATA(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_DATA)
#define GDA_IS_REPORT_DATA_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_DATA)
#else
#define GDA_REPORT_DATA(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_DATA, GdaReportData)
#define GDA_REPORT_DATA_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_DATA, GdaReportDataClass)
#define GDA_IS_REPORT_DATA(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_DATA)
#define GDA_IS_REPORT_DATA_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_DATA))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_data_get_type	(void);
#else
GtkType			gda_report_data_get_type	(void);
#endif

GdaReportData*		gda_report_data_new		();
void			gda_report_data_free		(GdaReportData* object);

GdaReportGroupHeader*	gda_report_data_get_gheader	(GdaReportData* object);
void			gda_report_data_set_gheader	(GdaReportData* object,
							 GdaReportGroupHeader* gheader);
GdaReportDetail*	gda_report_data_get_detail	(GdaReportData* object);
void			gda_report_data_set_detail	(GdaReportData* object,
							 GdaReportDetail* detail);
GdaReportGroupFooter*	gda_report_data_get_gfooter	(GdaReportData* object);
void			gda_report_data_set_gfooter	(GdaReportData* object,
							 GdaReportGroupFooter* gfooter;

#if defined(__cplusplus)
}
#endif

#endif
