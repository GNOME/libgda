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

#include <gda-report.h>

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#define GDA_REPORTSTREAM_NEXT	-1

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_ReportStream       Gda_ReportStream;
typedef struct _Gda_ReportStreamClass  Gda_ReportStreamClass;

struct _Gda_ReportStream {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_ReportStream  corba_reportstream;
	Gda_ReportEngine* engine;
	GList*		errors_head;
	gint32		seek;
};

struct _Gda_ReportStreamClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (Gda_ReportStream* object, GList* errors);
	void (* error)   (Gda_ReportStream* object, GList* errors);
};

#define GDA_TYPE_REPORTSTREAM          (gda_reportstream_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORTSTREAM(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORTSTREAM, Gda_ReportStream)
#define GDA_REPORTSTREAM_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORTSTREAM, Gda_ReportStreamClass)
#define GDA_IS_REPORTSTREAM(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORTSTREAM)
#define GDA_IS_REPORTSTREAM_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORTSTREAM)
#else
#define GDA_REPORTSTREAM(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORTSTREAM, Gda_ReportStream)
#define GDA_REPORTSTREAM_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORTSTREAM, GdaReportStreamClass)
#define GDA_IS_REPORTSTREAM(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORTSTREAM)
#define GDA_IS_REPORTSTREAM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORTSTREAM))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_stream_get_type	(void);
#else
GtkType			gda_report_stream_get_type	(void);
#endif

Gda_ReportStream*	gda_report_stream_new		(Gda_ReportEngine* engine);
void			gda_report_stream_free		(Gda_ReportStream* object);

gint32			gda_report_stream_readChunk	(Gda_ReportStream* object, guchar** data,
							 gint32 start, gint32 size);
gint32			gda_report_stream_writeChunk	(Gda_ReportStream* object, guchar* data,
							 gint32 size);
gint32			gda_report_stream_length	(Gda_ReportStream* object);

#if defined(__cplusplus)
}
#endif

#endif
