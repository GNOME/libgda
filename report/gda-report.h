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
#include <gda-report-defs.h>
#include <gda-report-format.h>
#include <gda-report-output.h>
#include <gda-report-header.h>
#include <gda-report-footer.h>

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
ReportPageSize		gda_report_get_pagesize		(GdaReport* object);
void			gda_report_set_pagesize		(GdaReport* object,
							 ReportPageSize pagesize);
ReportOrientation	gda_report_get_orientation	(GdaReport* object);
void			gda_report_set_orientation	(GdaReport* object,
							 ReportOrientation orientation);
ReportUnits		gda_report_get_units		(GdaReport* object);
void			gda_report_set_units		(GdaReport* object,
							 ReportUnits units);
gfloat			gda_report_get_topmargin	(GdaReport* object);
void			gda_report_set_topmargin	(GdaReport* object,
							 gfloat topmargin);
gfloat			gda_report_get_bottommargin	(GdaReport* object);
void			gda_report_set_bottommargin	(GdaReport* object,
							 gfloat bottommargin);
gfloat			gda_report_get_leftmargin	(GdaReport* object);
void			gda_report_set_leftmargin	(GdaReport* object,
							 gfloat leftmargin);
gfloat			gda_report_get_rightmargin	(GdaReport* object);
void			gda_report_set_rightmargin	(GdaReport* object,
							 gfloat rightmargin);
GdkColor*		gda_report_get_bgcolor		(GdaReport* object);
void			gda_report_set_bgcolor		(GdaReport* object,
							 GdkColor* bgcolor);
GdkColor*		gda_report_get_fgcolor		(GdaReport* object);
void			gda_report_set_fgcolor		(GdaReport* object,
							 GdkColor* fgcolor);
GdkColor*		gda_report_get_bordercolor	(GdaReport* object);
void			gda_report_set_bordercolor	(GdaReport* object,
							 GdkColor* bordercolor);
gfloat			gda_report_get_borderwidth	(GdaReport* object);
void			gda_report_set_borderwidth	(GdaReport* object,
							 gfloat borderwidth);
ReportLineStyle		gda_report_get_borderstyle	(GdaReport* object);
void			gda_report_set_borderstyle	(GdaReport* object,
							 ReportLineStyle borderstyle);
gchar*			gda_report_get_fontfamily	(GdaReport* object);
void			gda_report_set_fontfamily	(GdaReport* object,
							 gchar* fontfamily);
gushort			gda_report_get_fontsize		(GdaReport* object);
void			gda_report_set_fontsize		(GdaReport* object,
							 gshort fontsize);
ReportFontWeight	gda_report_get_fontweight	(GdaReport* object);
void			gda_report_set_fontweight	(GdaReport* object,
							 ReportFontWeight fontweight);
gboolean		gda_report_get_fontitalic	(GdaReport* object);
void			gda_report_set_fontitalic	(GdaReport* object,
							 gboolean fontitalic);
ReportHAlignment	gda_report_get_halignment	(GdaReport* object);
void			gda_report_set_halignment	(GdaReport* object,
							 ReportHAlignment halignment);
ReportVAlignment	gda_report_get_valignment	(GdaReport* object);
void			gda_report_set_valignment	(GdaReport* object,
							 ReportVAlignment valignment);
gboolean		gda_report_get_wordwrap		(GdaReport* object),
void			gda_report_set_wordwrap		(GdaReport* object,
							 gboolean wordwrap);
GdkColor*		gda_report_get_negvaluecolor	(GdaReport* object);
void			gda_report_set_negvaluecolor	(GdaReport* object,
							 GdkColor* negvaluecolor);
gchar*			gda_report_get_dateformat	(GdaReport* object);
void			gda_report_set_dateformat	(GdaReport* object,
							 gchar* dateformat);
gushort			gda_report_get_precision	(GdaReport* object);
void			gda_report_set_precision	(GdaReport* object,
							 gushort precision);
gchar*			gda_report_get_currency		(GdaReport* object);
void			gda_report_set_currency		(GdaReport* object,
							 gchar* currency);
gboolean		gda_report_get_commaseparator	(GdaReport* object);
void			gda_report_set_commaseparator	(GdaReport* object,
							 gboolean commaseparator);
gfloat			gda_report_get_linewidth	(GdaReport* object);
void			gda_report_set_linewidth	(GdaReport* object,
							 gfloat linewidth);
GdkColor*		gda_report_get_linecolor	(GdaReport* object);
void			gda_report_set_linecolor	(GdaReport* object,
							 GdkColor* linecolor);
ReportLineStyle		gda_report_get_linestyle	(GdaReport* object);
void			gda_report_set_linestyle	(GdaReport* object,
							 ReportLineStyle linestyle);
GdaReportHeader*	gda_report_get_rheader		(GdaReport* object);
void			gda_report_set_rheader		(GdaReport* object,
							 GdaReportHeader* rheader);
GList*			gda_report_get_plist		(GdaReport* object);
void			gda_report_set_plist		(GdaReport* object,
							 GList* plist);
GdaReportFooter*	gda_report_get_rfooter		(GdaReport* object);
void			gda_report_set_rfooter		(GdaReport* object,
							 GdaReportFooter* rfooter);

GdaReportFormat*	gda_report_get_format		(GdaReport* object);

GdaReportOutput*	gda_report_run			(GdaReport* object, GList* param,
							 gint32 flags);
							 
gboolean		gda_report_isLocked		(GdaReport* object);
void			gda_report_lock			(GdaReport* object);
void			gda_report_unlock		(GdaReport* object);


#if defined(__cplusplus)
}
#endif

#endif
