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

#ifndef __gda_report_element_h__
#define __gda_report_element_h__

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

typedef struct _GdaReportElement       GdaReportElement;
typedef struct _GdaReportElementClass  GdaReportElementClass;

struct _GdaReportElement {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_ReportElement	corba_report_element;
	gchar*		name;
	GList*		errors_head;
};

struct _GdaReportElementClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReportElement* object, GList* errors);
	void (* error)   (GdaReportElement* object, GList* errors);
};

#define GDA_TYPE_REPORT_ELEMENT          (gda_report_element_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_ELEMENT(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_ELEMENT, GdaReportElement)
#define GDA_REPORT_ELEMENT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_ELEMENT, GdaReportElementClass)
#define GDA_IS_REPORT_ELEMENT(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_ELEMENT)
#define GDA_IS_REPORT_ELEMENT_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_ELEMENT)
#else
#define GDA_REPORT_ELEMENT(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_ELEMENT, GdaReportElement)
#define GDA_REPORT_ELEMENT_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_ELEMENT, GdaReportElementClass)
#define GDA_IS_REPORT_ELEMENT(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_ELEMENT)
#define GDA_IS_REPORT_ELEMENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_ELEMENT))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_element_get_type	(void);
#else
GtkType			gda_report_element_get_type	(void);
#endif

GdaReportElement*	gda_report_element_new		(gchar* name);
void			gda_report_element_free		(GdaReportElement* object);

gchar*			gda_report_element_get_name	(GdaReportElement* object);
gint			gda_report_element_set_name	(GdaReportElement* object, gchar* name);

gint			gda_report_element_add_attribute	(gchar* name, gchar* value);
gint			gda_report_element_remove_attribute	(gchar* name);
GdaReportAttribute*	gda_report_element_get_attribute	(gchar* name);
gint			gda_report_element_set_attribute	(gchar* name, gchar* value);
GdaReportElement*	gda_report_element_add_child		(gchar* name);
gint			gda_report_element_remove_child		(GdaReportElement* child);
GList*			gda_report_element_get_children		(void);

#if defined(__cplusplus)
}
#endif

#endif
