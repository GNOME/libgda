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

#ifndef __gda_report_text_object_h__
#define __gda_report_text_object_h__

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

typedef struct _GdaReportTextObject       GdaReportTextObject;
typedef struct _GdaReportTextObjectClass  GdaReportTextObjectClass;

struct _GdaReportTextObject {
	GdaReportObject		parent;
	GDA_ReportTextObject	corba_reporttextobject;
	gchar*			name;
	GList*			errors_head;
};

struct _GdaReportTextObjectClass {
	GdaReportObjectClass	parent_class;

	void (* warning) (GdaReportTextObject* object, GList* errors);
	void (* error)   (GdaReportTextObject* object, GList* errors);
};

#define GDA_TYPE_REPORT_TEXT_OBJECT          (gda_report_text_object_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_TEXT_OBJECT(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_TEXT_OBJECT, GdaReportTextObject)
#define GDA_REPORT_TEXT_OBJECT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_TEXT_OBJECT, GdaReportTextObjectClass)
#define GDA_IS_REPORT_TEXT_OBJECT(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_TEXT_OBJECT)
#define GDA_IS_REPORT_TEXT_OBJECT_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_TEXT_OBJECT)
#else
#define GDA_REPORT_TEXT_OBJECT(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_TEXT_OBJECT, GdaReportTextObject)
#define GDA_REPORT_TEXT_OBJECT_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_TEXT_OBJECT, GdaReportTextObjectClass)
#define GDA_IS_REPORT_TEXT_OBJECT(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_TEXT_OBJECT)
#define GDA_IS_REPORT_TEXT_OBJECT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_TEXT_OBJECT))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_text_object_get_type	(void);
#else
GtkType			gda_report_text_object_get_type	(void);
#endif

GdaReportTextObject*	gda_report_text_object_new		(gchar* name);
void			gda_report_text_object_free		(GdaReportTextObject* object);

gchar*			gda_report_text_object_get_fontfamily	(GdaReportTextObject* object);
void			gda_report_text_object_set_fontfamily	(GdaReportTextObject* object,
								 gchar* fontfamily);
gshort			gda_report_text_object_get_fontsize	(GdaReportTextObject* object);
void			gda_report_text_object_set_fontsize	(GdaReportTextObject* object,
								 gshort fontsize);
ReportFontWeight	gda_report_text_object_get_fontweight	(GdaReportTextObject* object);
void			gda_report_text_object_set_fontweight	(GdaReportTextObject* object,
								 ReportFontWeight fontweight);
gboolean		gda_report_text_object_get_fontitalic	(GdaReportTextObject* object);
void			gda_report_text_object_set_fontitalic	(GdaReportTextObject* object,
								 gboolean fontitalic);
ReportHAlignment	gda_report_text_object_get_halignment	(GdaReportTextObject* object);
void			gda_report_text_object_set_halignment	(GdaReportTextObject* object,
								 ReportHAlignment halignment);
ReportVAlignment	gda_report_text_object_get_valignment	(GdaReportTextObject* object);
void			gda_report_text_object_set_valignment	(GdaReportTextObject* object,
								 ReportVAlignment valignment);
gboolean		gda_report_text_object_get_wordwrap	(GdaReportTextObject* object);
void			gda_report_text_object_set_wordwrap	(GdaReportTextObject* object,
								 gboolean wordwrap);

#if defined(__cplusplus)
}
#endif

#endif
