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

#ifndef __gda_report_line_h__
#define __gda_report_line_h__

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

typedef struct _GdaReportLine       GdaReportLine;
typedef struct _GdaReportLineClass  GdaReportLineClass;

struct _GdaReportLine {
	GdaReportElement	parent;
	GDA_ReportLine		corba_report_line;
	gchar*			name;
	GList*			errors_head;
};

struct _GdaReportLineClass {
	GdaReportElementClass	parent_class;
	
	void (* warning) (GdaReportLine* object, GList* errors);
	void (* error)   (GdaReportLine* object, GList* errors);
};

#define GDA_TYPE_REPORT_LINE          (gda_report_line_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_LINE(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_LINE, GdaReportLine)
#define GDA_REPORT_LINE_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_LINE, GdaReportLineClass)
#define GDA_IS_REPORT_LINE(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_LINE)
#define GDA_IS_REPORT_LINE_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_LINE)
#else
#define GDA_REPORT_LINE(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_LINE, GdaReportLine)
#define GDA_REPORT_LINE_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_LINE, GdaReportLineClass)
#define GDA_IS_REPORT_LINE(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_LINE)
#define GDA_IS_REPORT_LINE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_LINE))
#endif

#ifdef HAVE_GOBJECT
GType		gda_report_line_get_type	(void);
#else
GtkType		gda_report_line_get_type	(void);
#endif

GdaReportLine*	gda_report_line_new		(gchar* name);
void		gda_report_line_free		(GdaReportLine* object);

gfloat		gda_report_line_get_x1		(GdaReportLine* object);
void		gda_report_line_set_x1		(GdaReportLine* object,
						 gfloat x1);
gfloat		gda_report_line_get_y1		(GdaReportLine* object);
void		gda_report_line_set_y1		(GdaReportLine* object,
						 gfloat y1);
gfloat		gda_report_line_get_x2		(GdaReportLine* object);
void		gda_report_line_set_x2		(GdaReportLine* object,
						 gfloat x2);
gfloat		gda_report_line_get_y2		(GdaReportLine* object);
void		gda_report_line_set_y2		(GdaReportLine* object,
						 gfloat y2);
gfloat		gda_report_line_get_width	(GdaReportLine* object);
void		gda_report_line_set_width	(GdaReportLine* object,
						 gfloat width);
GdkColor	gda_report_line_get_color	(GdaReportLine* object);
void		gda_report_line_set_color	(GdaReportLine* object,
						 GdkColor color);
ReportLineStyle	gda_report_line_get_style	(GdaReportLine* object);
void		gda_report_line_set_style	(GdaReportLine* object,
						 ReportLineStyle style);

#if defined(__cplusplus)
}
#endif

#endif
