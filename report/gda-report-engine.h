/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_report_engine_h__)
#define __gda_report_engine_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#include <GDA_Report.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_ReportEngine Gda_ReportEngine;
typedef struct _Gda_ReportEngineClass Gda_ReportEngineClass;

typedef enum {
	GDA_REPORT_FLAGS_NONE,
} Gda_ReportFlags;

struct _Gda_ReportEngine {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_ReportEngine corba_engine;
	GList*		errors_head;
};

struct _Gda_ReportEngineClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (Gda_ReportEngine* object, GList* errors);
	void (* error)   (Gda_ReportEngine* object, GList* errors);
};


#define GDA_TYPE_REPORTENGINE          (gda_report_engine_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORTENGINE(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORTENGINE, Gda_ReportEngine)
#define GDA_REPORTENGINE_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORTENGINE, Gda_ReportEngineClass)
#define GDA_IS_REPORTENGINE(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORTENGINE)
#define GDA_IS_REPORTENGINE_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORTENGINE)
#else
#define GDA_REPORTENGINE(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORTENGINE, Gda_ReportEngine)
#define GDA_REPORTENGINE_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORTENGINE, GdaReportEngineClass)
#define GDA_IS_REPORTENGINE(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORTENGINE)
#define GDA_IS_REPORTENGINE_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORTENGINE))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_engine_get_type	(void);
#else
GtkType			gda_report_engine_get_type	(void);
#endif

Gda_ReportEngine*	gda_report_engine_new		(void);
void			gda_report_engine_free		(Gda_ReportEngine *engine);

GList*			gda_report_engine_query_reports	(Gda_ReportEngine *engine,
							 const gchar *condition,
							 Gda_ReportFlags flags);

#if defined(__cplusplus)
}
#endif

#endif
