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

#ifndef __gda_report_format_h__
#define __gda_report_format_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>
#include <gda-report-element.h>
#include <gda-report-stream.h>

typedef struct _Gda_ReportFormat       Gda_ReportFormat;
typedef struct _Gda_ReportFormatClass  Gda_ReportFormatClass;

struct _Gda_ReportFormat {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_ReportFormat	corba_report_format;
	GList*		errors_head;
};

struct _Gda_ReportFormatClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (Gda_ReportFormat* object, GList* errors);
	void (* error)   (Gda_ReportFormat* object, GList* errors);
};

#define GDA_TYPE_REPORT_FORMAT          (gda_report_format_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_FORMAT(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_FORMAT, Gda_ReportFormat)
#define GDA_REPORT_FORMAT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_FORMAT, Gda_ReportFormatClass)
#define GDA_IS_REPORT_FORMAT(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_FORMAT)
#define GDA_IS_REPORT_FORMAT_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_FORMAT)
#else
#define GDA_REPORT_FORMAT(obj)    GTK_CHECK_CAST(obj, GDA_TYPE_REPORTFORMAT, Gda_ReportFormat)
#define GDA_REPORT_FORMAT_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_FORMAT, GdaReportFormatClass)
#define GDA_IS_REPORT_FORMAT(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_FORMAT)
#define GDA_IS_REPORT_FORMAT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_FORMAT))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_format_get_type	(void);
#else
GtkType			gda_report_format_get_type	(void);
#endif

void			gda_report_format_free		(Gda_ReportFormat* object);

Gda_ReportElement*	gda_report_format_get_root_element	(void);
Gda_ReportStream*	gda_report_format_get_stream		(void);

#if defined(__cplusplus)
}
#endif

#endif
