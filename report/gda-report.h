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

#ifndef __gda_report_h__
#define __gda_report_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>
#include <gda-report-engine.h>
#include <gda-report-element.h>
#include <gda-report-format.h>
#include <gda-report-output.h>

typedef struct _GdaReport       GdaReport;
typedef struct _GdaReportClass  GdaReportClass;

struct _GdaReport {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_Report	corba_report;
	GdaReportEngine* engine;
	gchar*		rep_name;
	gchar*		description;
	GList*		errors_head;
};

struct _GdaReportClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReport* object, GList* errors);
	void (* error)   (GdaReport* object, GList* errors);
};

#define GDA_TYPE_REPORT          (gda_report_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT, GdaReport)
#define GDA_REPORT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT, GdaReportClass)
#define GDA_IS_REPORT(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT)
#define GDA_IS_REPORT_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT)
#else
#define GDA_REPORT(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT, GdaReport)
#define GDA_REPORT_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT, GdaReportClass)
#define GDA_IS_REPORT(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT)
#define GDA_IS_REPORT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_get_type		(void);
#else
GtkType			gda_report_get_type		(void);
#endif

GdaReport*		gda_report_new			(gchar* rep_name, gchar* description);
void			gda_report_free			(GdaReport* object);

gchar*			gda_report_get_name		(GdaReport* object);
gint			gda_report_set_name		(GdaReport* object, gchar* name);

gchar*			gda_report_get_description	(GdaReport* object);
gint			gda_report_set_description	(GdaReport* object, gchar* description);

GdaReportElement*	gda_report_get_elements		(GdaReport* object);
gint			gda_report_set_elements		(GdaReport* object, GdaReportElement* element);

GdaReportFormat*	gda_report_get_format		(GdaReport* object);

GdaReportOutput*	gda_report_run			(GdaReport* object, GList param, gint32 flags);
							 
gboolean		gda_report_isLocked		(GdaReport* object);
void			gda_report_lock			(GdaReport* object);
void			gda_report_unlock		(GdaReport* object);


#if defined(__cplusplus)
}
#endif

#endif
