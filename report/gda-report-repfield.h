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

#ifndef __gda_report_repfield_h__
#define __gda_report_repfield_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <GDA_Report.h>
#include <gdk/gdktypes.h>

typedef struct _GdaReportRepfield       GdaReportRepfield;
typedef struct _GdaReportRepfieldClass  GdaReportRepfieldClass;

struct _GdaReportRepfield {
	GdaReportTextObject	parent;
	GDA_ReportRepfield	corba_report_repfield;
	gchar*			name;
	GList*			errors_head;
};

struct _GdaReportRepfieldClass {
	GdaReportTextObjectClass	parent_class;

	void (* warning) (GdaReportRepfield* object, GList* errors);
	void (* error)   (GdaReportRepfield* object, GList* errors);
};

#define GDA_TYPE_REPORT_REPFIELD          (gda_report_repfield_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_REPFIELD(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_REPFIELD, GdaReportRepfield)
#define GDA_REPORT_REPFIELD_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_REPFIELD, GdaReportRepfieldClass)
#define GDA_IS_REPORT_REPFIELD(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_REPFIELD)
#define GDA_IS_REPORT_REPFIELD_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_REPFIELD)
#else
#define GDA_REPORT_REPFIELD(obj)          GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_REPFIELD, GdaReportRepfield)
#define GDA_REPORT_REPFIELD_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_REPFIELD, GdaReportRepfieldClass)
#define GDA_IS_REPORT_REPFIELD(obj)       GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_REPFIELD)
#define GDA_IS_REPORT_REPFIELD_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_REPFIELD))
#endif

#ifdef HAVE_GOBJECT
GType			gda_report_repfield_get_type	(void);
#else
GtkType			gda_report_repfield_get_type	(void);
#endif

GdaReportRepfield*	gda_report_repfield_new		(gchar* name);
void			gda_report_repfield_free	(GdaReportRepfield* object);

gchar*			gda_report_repfield_get_query	(GdaReportRepfield* object);
void			gda_report_repfield_set_query	(GdaReportRepfield* object,
							 gchar* query);
gchar*			gda_report_repfield_get_value	(GdaReportRepfield* object);
void			gda_report_repfield_set_value	(GdaReportRepfield* object,
							 gchar* value);
gchar*			gda_report_repfield_get_datatype	(GdaReportRepfield* object);
void			gda_report_repfield_set_datatype	(GdaReportRepfield* object,
								 gchar* datatype);
gchar*			gda_report_repfield_get_dateformat	(GdaReportRepfield* object);
void			gda_report_repfield_set_dateformat	(GdaReportRepfield* object,
								 gchar* dateformat);
ushort			gda_report_repfield_get_precision	(GdaReportRepfield* object);
void			gda_report_repfield_set_precision	(GdaReportRepfield* object,
								 ushort precision);
gchar*			gda_report_repfield_get_currency	(GdaReportRepfield* object);
void			gda_report_repfield_set_currency	(GdaReportRepfield* object,
								 gchar* currency);
GdkColor*		gda_report_repfield_get_negvaluecolor	(GdaReportRepfield* object);
void			gda_report_repfield_set_negvaluecolor	(GdaReportRepfield* object,
								 GdkColor* negvaluecolor);
gboolean		gda_report_repfield_get_commaseparator	(GdaReportRepfield* object);
void			gda_report_repfield_set_commaseparator	(GdaReportRepfield* object,
								 gboolean commaseparator);
								 
#if defined(__cplusplus)
}
#endif

#endif
