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

#ifndef __gda_report_header_h__
#define __gda_report_header_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>

typedef struct _GdaReportHeader      GdaReportHeader
typedef struct _GdaReportHeaderClass  GdaReportHeaderClass;

struct _GdaReportHeader {
	GdaReportSection	parent;
	
	GDA_ReportHeader	corba_reportheader;
	GList*			errors_head;
};

struct _GdaReportHeaderClass {
	GdaReportSectionClass	parent_class;
	
	void (* warning) (GdaReportHeader* object, GList* errors);
	void (* error)   (GdaReportHeader* object, GList* errors);
};

#define GDA_TYPE_REPORT_HEADER          (gda_report_header_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_HEADER(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_HEADER, GdaReportHeader)
#define GDA_REPORT_HEADER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_HEADER, GdaReportHeaderClass)
#define GDA_IS_REPORT_HEADER(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_HEADER)
#define GDA_IS_REPORT_HEADER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_HEADER)
#else
#define GDA_REPORT_HEADER(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_HEADER, GdaReportHeader)
#define GDA_REPORT_HEADER_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_HEADER, GdaReportHeaderClass)
#define GDA_IS_REPORT_HEADER(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_HEADER)
#define GDA_IS_REPORT_HEADER_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_HEADER))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_header_get_type	(void);
#else
GtkType			gda_report_header_get_type	(void);
#endif

GdaReportHeader*	gda_report_header_new		();
void			gda_report_header_free		(GdaReportHeader* object);

gboolean		gda_report_header_get_newpage	(GdaReportHeader* object);
void			gda_report_header_set_newpage	(GdaReportHeader* object,
							 gboolean newpage);

#if defined(__cplusplus)
}
#endif

#endif
