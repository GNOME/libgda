/* libgda library
 *
 * Copyright (C) 2000 Carlos Perelló Marín <carlos@gnome-db.org>
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

#include <glib.h>
#include <orb/orbit.h>
#include <gda-error.h>
#include <GDA_Report.h>

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* Data Structures and Prototypes for the LibGDA Report Client
 * Library
 */
typedef struct _Gda_Report_Stream       Gda_Report_Stream;
typedef struct _Gda_Report_StreamClass  Gda_Report_StreamClass;

#define GDA_TYPE_REPORTSTREAM          (gda_reportstream_get_type())

#ifdef HAVE_GOBJECT
#define GDA_REPORTSTREAM         (obj)   G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORTSTREAM, \
								     Gda_Report_Stream)
#define GDA_REPORTSTREAM_CLASS   (klass) G_TYPE_CHECK_CLASS_CAST    (klass, GDA_TYPE_REPORTSTREAM, \
								     Gda_Report_StreamClass)
#define GDA_IS_REPORTSTREAM      (obj)   G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORTSTREAM)
#define GDA_IS_REPORTSTREAM_CLASS(klass) GTK_CHECK_CLASS_TYPE       ((klass), GDA_TYPE_REPORTSTREAM)
#else
#define GDA_REPORTSTREAM         (obj)   GTK_CHECK_CAST             (obj, GDA_TYPE_REPORTSTREAM, \
								     Gda_Report_Stream)
#define GDA_REPORTSTREAM_CLASS   (klass) GTK_CHECK_CLASS_CAST       (klass, GDA_TYPE_REPORTSTREAM, \
								     Gda_Report_StreamClass)
#define GDA_IS_REPORTSTREAM      (obj)   GTK_CHECK_TYPE             (obj, GDA_TYPE_REPORTSTREAM)
#define GDA_IS_REPORTSTREAM_CLASS(klass) GTK_CHECK_CLASS_TYPE       ((klass), GDA_TYPE_REPORTSTREAM)
#endif

struct _Gda_Report_Stream {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	CORBA_Object	reportstream;
	CORBA_ORB	orb;
	GList*		errors_head;
};

struct _Gda_Report_StreamClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (Gda_Report_Stream* object, GList* errors);
	void (* error)   (Gda_Report_Stream* object, GList* errors);
};

#ifdef HAVE_GOBJECT
GType        gda_reportstream_get_type      (void);
#else
GtkType      gda_reportstream_get_type      (void);
#endif

Gda_Report_Stream*  gda_reportstream_new        (CORBA_ORB orb);
void               gda_reportstream_free       (Gda_Report_Stream* object);

GDA_Report_StreamChunk*  gda_reportstream_readChunk  (gint start, gint size);
gint               gda_reportstream_writeChunk (GDA_Report_StreamChunk* data, gint size);
gint               gda_reportstream_length     (Gda_Report_Stream* object);

#if defined(__cplusplus)
}
#endif

#endif
