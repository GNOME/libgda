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

#ifndef __gda_report_group_header_h__
#define __gda_report_group_header_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#include <gda-report-defs.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaReportGroupHeader      GdaReportGroupHeader;
typedef struct _GdaReportGroupHeaderClass  GdaReportGroupHeaderClass;

struct _GdaReportGroupHeader {
	GdaReportSection	parent;
	
	GDA_ReportGroupHeader	corba_reportgroupheader;
	GList*			errors_head;
};

struct _GdaReportGroupHeaderClass {
	GdaReportSectionClass	parent_class;
	
	void (* warning) (GdaReportGroupHeader* object, GList* errors);
	void (* error)   (GdaReportGroupHeader* object, GList* errors);
};

#define GDA_TYPE_REPORT_GROUP_HEADER          (gda_report_group_header_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_GROUP_HEADER(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_GROUP_HEADER, GdaReportGroupHeader)
#define GDA_REPORT_GROUP_HEADER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_GROUP_HEADER, GdaReportGroupHeaderClass)
#define GDA_IS_REPORT_GROUP_HEADER(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_GROUP_HEADER)
#define GDA_IS_REPORT_GROUP_HEADER_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_GROUP_HEADER)
#else
#define GDA_REPORT_GROUP_HEADER(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_GROUP_HEADER, GdaReportGroupHeader)
#define GDA_REPORT_GROUP_HEADER_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_GROUP_HEADER, GdaReportGroupHeaderClass)
#define GDA_IS_REPORT_GROUP_HEADER(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_GROUP_HEADER)
#define GDA_IS_REPORT_GROUP_HEADER_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_GROUP_HEADER))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_group_header_get_type	(void);
#else
GtkType			gda_report_group_header_get_type	(void);
#endif

GdaReportGroupHeader*	gda_report_group_header_new		();
void			gda_report_group_header_free		(GdaReportGroupHeader* object);


gboolean		gda_report_group_header_get_newpage	(GdaReportGroupHeader* object);
void			gda_report_group_header_set_newpage	(GdaReportGroupHeader* object,
								 gboolean newpage);
gchar*			gda_report_group_header_get_groupvar	(GdaReportGroupHeader* object);
void			gda_report_group_header_set_groupvar	(GdaReportGroupHeader* object,
								 gchar* groupvar);
								 
#if defined(__cplusplus)
}
#endif

#endif
