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

#ifndef __gda_report_picture_h__
#define __gda_report_picture_h__

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

typedef struct _GdaReportPicture       GdaReportPicture;
typedef struct _GdaReportPictureClass  GdaReportPictureClass;

struct _GdaReportPicture {
	GdaReportElement	parent;
	GDA_ReportPicture	corba_report_picture;
	gchar*			name;
	GList*			errors_head;
};

struct _GdaReportPictureClass {
	GdaReportElementClass	parent_class;

	void (* warning) (GdaReportPicture* object, GList* errors);
	void (* error)   (GdaReportPicture* object, GList* errors);
};

#define GDA_TYPE_REPORT_PICTURE          (gda_report_picture_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_PICTURE(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_PICTURE, GdaReportPicture)
#define GDA_REPORT_PICTURE_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_PICTURE, GdaReportPictureClass)
#define GDA_IS_REPORT_PICTURE(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_PICTURE)
#define GDA_IS_REPORT_PICTURE_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_PICTURE)
#else
#define GDA_REPORT_PICTURE(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_PICTURE, GdaReportPicture)
#define GDA_REPORT_PICTURE_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_PICTURE, GdaReportPictureClass)
#define GDA_IS_REPORT_PICTURE(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_PICTURE)
#define GDA_IS_REPORT_PICTURE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_PICTURE))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_picture_get_type	(void);
#else
GtkType			gda_report_picture_get_type	(void);
#endif

GdaReportPicture*	gda_report_picture_new		(gchar* name);
void			gda_report_picture_free		(GdaReportPicture* object);

gfloat			gda_report_picture_get_x	(GdaReportPicture* object);
void			gda_report_picture_set_x	(GdaReportPicture* object,
							 gfloat x);
gfloat			gda_report_picture_get_y	(GdaReportPicture* object);
void			gda_report_picture_set_y	(GdaReportPicture* object,
							 gfloat y);
gfloat			gda_report_picture_get_width	(GdaReportPicture* object);
void			gda_report_picture_set_width	(GdaReportPicture* object,
							 gfloat width);
gfloat			gda_report_picture_get_height	(GdaReportPicture* object);
void			gda_report_picture_set_height	(GdaReportPicture* object,
							 gfloat height);
ReportSize		gda_report_picture_get_size	(GdaReportPicture* object);
void			gda_report_picture_set_size	(GdaRerportPicture* object,
							 ReportSize size);
ReportRatio		gda_report_picture_get_aspectratio	(GdaReportPicture* object);
void			gda_report_picture_set_aspectratio	(GdaReportPicture* object,
								 ReportRatio aspectratio);
gchar*			gda_report_picture_get_format	(GdaReportPicture* object);
void			gda_report_picture_set_format	(GdaReportPicture* object,
							 gchar* format);
ReportSource		gda_report_picture_get_source	(GdaReportPicture* object);
void			gda_report_picture_set_source	(GdaReportPicture* object,
							 ReportSource source);
gchar*			gda_report_picture_get_data	(GdaReportPicture* object);
void			gda_report_picture_set_data	(GdaReportPicture* object,
							 gchar* data);
		
#if defined(__cplusplus)
}
#endif

#endif
