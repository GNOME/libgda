/* libgda library
 *
 * Copyright (C) 2000,2001 Carlos Perelló Marín <carlos@gnome-db.org>
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

#ifndef __gda_report_stream_h__
#define __gda_report_stream_h__

#include <gda-report-engine.h>

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#define GDA_REPORT_STREAM_NEXT	-1

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>

typedef struct _GdaReportStream       GdaReportStream;
typedef struct _GdaReportStreamClass  GdaReportStreamClass;

struct _GdaReportStream {
#ifdef HAVE_GOBJECT
	GObject			parent;
#else
	GtkObject		parent;
#endif
	GDA_ReportStream	corba_reportstream;
	GdaReportEngine*	engine;
	GList*			errors_head;
	gint32			seek;
};

struct _GdaReportStreamClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass*	parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReportStream* object, GList* errors);
	void (* error)   (GdaReportStream* object, GList* errors);
};

#define GDA_TYPE_REPORT_STREAM          (gda_reportstream_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_STREAM(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_STREAM, GdaReportStream)
#define GDA_REPORT_STREAM_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_STREAM, GdaReportStreamClass)
#define GDA_IS_REPORT_STREAM(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_STREAM)
#define GDA_IS_REPORT_STREAM_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_STREAM)
#else
#define GDA_REPORT_STREAM(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_STREAM, GdaReportStream)
#define GDA_REPORT_STREAM_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_STREAM, GdaReportStreamClass)
#define GDA_IS_REPORT_STREAM(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_STREAM)
#define GDA_IS_REPORT_STREAM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_STREAM))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_stream_get_type	(void);
#else
GtkType			gda_report_stream_get_type	(void);
#endif

GdaReportStream*	gda_report_stream_new		(GdaReportEngine* engine);
void			gda_report_stream_free		(GdaReportStream* object);

gint32			gda_report_stream_readChunk	(GdaReportStream* object, guchar** data,
							 gint32 start, gint32 size);
gint32			gda_report_stream_writeChunk	(GdaReportStream* object, guchar* data,
							 gint32 size);
gint32			gda_report_stream_length	(GdaReportStream* object);

#if defined(__cplusplus)
}
#endif

#endif
