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

#ifndef __gda_report_object_h__
#define __gda_report_object_h__

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

typedef struct _GdaReportObject       GdaReportObject;
typedef struct _GdaReportObjectClass  GdaReportObjectClass;

struct _GdaReportObject {
	GdaReportElement	parent;
	GDA_ReportObject	corba_report_object;
	gchar*			name;
	GList*			errors_head;
};

struct _GdaReportObjectClass {
	GdaReportElementClass	parent_class;

	void (* warning) (GdaReportObject* object, GList* errors);
	void (* error)   (GdaReportObject* object, GList* errors);
};

#define GDA_TYPE_REPORT_OBJECT          (gda_report_object_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_OBJECT(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_OBJECT, GdaReportObject)
#define GDA_REPORT_OBJECT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_OBJECT, GdaReportObjectClass)
#define GDA_IS_REPORT_OBJECT(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_OBJECT)
#define GDA_IS_REPORT_OBJECT_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_OBJECT)
#else
#define GDA_REPORT_OBJECT(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_OBJECT, GdaReportObject)
#define GDA_REPORT_OBJECT_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_OBJECT, GdaReportObjectClass)
#define GDA_IS_REPORT_OBJECT(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_OBJECT)
#define GDA_IS_REPORT_OBJECT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_OBJECT))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_object_get_type	(void);
#else
GtkType			gda_report_object_get_type	(void);
#endif

GdaReportObject*	gda_report_object_new		(gchar* name);
void			gda_report_object_free		(GdaReportObject* object);

gfloat			gda_report_object_get_x		(GdaReportObject* object);
void			gda_report_object_set_x		(GdaReportObject* object,
							 gfloat x);
gfloat			gda_report_object_get_y		(GdaReportObject* object);
void			gda_report_object_set_y		(GdaReportObject* object,
							 gfloat y);
gfloat			gda_report_object_get_width	(GdaReportObject* object);
void			gda_report_object_set_width	(GdaReportObject* object,
							 gfloat width);
gfloat			gda_report_object_get_height	(GdaReportObject* object);
void			gda_report_object_set_height	(GdaReportObject* object,
							 gfloat height);
GdkColor		gda_report_object_get_bgcolor	(GdaReportObject* object);
void			gda_report_object_set_bgcolor	(GdaRerportObject* object,
							 GdkColor bgcolor);
GdkColor		gda_report_object_get_fgcolor	(GdaReportObject* object);
void			gda_report_object_set_fgcolor	(GdaReportObject* object,
							 GdkColor fgcolor);
GdkColor		gda_report_object_get_bordercolor	(GdaReportObject* object);
void			gda_report_object_set_bordercolor	(GdaReportObject* object,
								 GdkColor bordercolor);
gfloat			gda_report_object_get_borderwidth	(GdaReportObject* object);
void			gda_report_object_set_borderwidth	(GdaReportObject* object,
								 gfloat borderwidth);
ReportLineStyle		gda_report_object_get_borderstyle	(GdaReportObject* object);
void			gda_report_object_set_borderstyle	(GdaReportObject* object,
								 ReportLineStyle borderstyle);

void			gda_report_object_set_geometry	(gfloat x, gfloat y, 
							 gfloat width, gfloat height);
void			gda_report_object_move		(gfloat x, gfloat y);

#if defined(__cplusplus)
}
#endif

#endif
