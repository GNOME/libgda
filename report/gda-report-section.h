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

#ifndef __gda_report_section_h__
#define __gda_report_section_h__

#ifdef HAVE_GOBJECT
#include <glib-object.h>
#else
#include <gtk/gtk.h>
#endif

#include <gda-report-defs.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaReportSection      GdaReportSection
typedef struct _GdaReportSectionClass  GdaReportSectionClass;

struct _GdaReportSection {
#ifdef HAVE_GOBJECT
	GObject		object;
#else
	GtkObject	object;
#endif
	GDA_ReportSection  corba_reportsection;
	GList*		errors_head;
};

struct _GdaReportSectionClass {
#ifdef HAVE_GOBJECT
	GObjectClass	parent_class;
	GObjectClass	*parent;
#else
	GtkObjectClass	parent_class;
#endif
	void (* warning) (GdaReportSection* object, GList* errors);
	void (* error)   (GdaReportSection* object, GList* errors);
};

#define GDA_TYPE_REPORT_SECTION          (gda_report_section_get_type())
#ifdef HAVE_GOBJECT
#define GDA_REPORT_SECTION(obj) \
		G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_SECTION, GdaReportSection)
#define GDA_REPORT_SECTION_CLASS(klass) \
		G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_SECTION, GdaReportSectionClass)
#define GDA_IS_REPORT_SECTION(obj) \
		G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_SECTION)
#define GDA_IS_REPORT_SECTION_CLASS(klass) \
		G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_SECTION)
#else
#define GDA_REPORT_SECTION(obj) \
		GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_SECTION, GdaReportSection)
#define GDA_REPORT_SECTION_CLASS(klass) \
		GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_SECTION, GdaReportSectionClass)
#define GDA_IS_REPORT_SECTION(obj) \
		GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_SECTION)
#define GDA_IS_REPORT_SECTION_CLASS(klass) \
		GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_SECTION))
#endif


#ifdef HAVE_GOBJECT
GType			gda_report_section_get_type	(void);
#else
GtkType			gda_report_section_get_type	(void);
#endif

GdaReportSection*	gda_report_section_new		();
void			gda_report_section_free		(GdaReportSection* object);

gboolean	gda_report_section_get_status		(GdaReportSection* object);
void		gda_report_section_set_status		(GdaReportSection* object,
							 gboolean status);
gboolean	gda_report_section_get_visible		(GdaReportSection* object);
void		gda_report_section_set_status		(GdaReportSection* object,
							 gboolean visible);
gfloat		gda_report_section_get_height		(GdaReportSection* object);
void		gda_report_section_set_height		(GdaReportSection* object,
							 gfloat height);
GdkColor	gda_report_section_get_bgcolor		(GdaReportSection* object);
void		gda_report_section_set_bgcolor		(GdaReportSection* object,
							 GdkColor bgcolor);
GdkColor	gda_report_section_get_fgcolor		(GdaReportSection* object);
void		gda_report_section_set_fgcolor		(GdaReportSection* object,
							 GdkColor fgcolor);
GdkColor	gda_report_section_get_bordercolor	(GdaReportSection* object);
void		gda_report_section_set_bordercolor	(GdaReportSection* object,
							 GdkColor bordercolor);
gfloat		gda_report_section_get_borderwidth	(GdaReportSection* object);
void		gda_report_section_set_borderwidth	(GdaReportSection* object,
							 gfloat borderwidth);
ReportLineStyle	gda_report_section_get_borderstyle	(GdaReportSection* object);
void		gda_report_section_set_borderstyle	(GdaReportSection* object,
							 ReportLineStyle borderstyle);
gchar*		gda_report_section_get_fontfamily	(GdaReportSection* object);
void		gda_report_section_set_fontfamily	(GdaReportSection* object,
							 gchar* fontfamily);
gushort		gda_report_section_get_fontsize		(GdaReportSection* object);
void		gda_report_section_set_fontsize		(GdaReportSection* object,
							 gshort fontsize);
ReportFontWeight	gda_report_section_get_fontweight	(GdaReportSection* object);
void		gda_report_section_set_fontweight	(GdaReportSection* object,
							 ReportFontWeight fontweight);
gboolean	gda_report_section_get_fontitalic	(GdaReportSection* object);
void		gda_report_section_set_fontitalic	(GdaReportSection* object,
							 gboolean fontitalic);
ReportHAlignment	gda_report_section_get_halignment	(GdaReportSection* object);
void		gda_report_section_set_halignment	(GdaReportSection* object,
							 ReportHAlignment halignment);
ReportVAlignment	gda_report_section_get_valignment	(GdaReportSection* object);
void		gda_report_section_set_valignment	(GdaReportSection* object,
							 ReportVAlignment valignment);
gboolean	gda_report_section_get_wordwrap		(GdaReportSection* object),
void		gda_report_section_set_wordwrap		(GdaReportSection* object,
							 gboolean wordwrap);
GdkColor	gda_report_section_get_negvaluecolor	(GdaReportSection* object);
void		gda_report_section_set_negvaluecolor	(GdaReportSection* object,
							 GdkColor negvaluecolor);
gchar*		gda_report_section_get_dateformat	(GdaReportSection* object);
void		gda_report_section_set_dateformat	(GdaReportSection* object,
							 gchar* dateformat);
gushort		gda_report_section_get_precision	(GdaReportSection* object);
void		gda_report_section_set_precision	(GdaReportSection* object,
							 gushort precision);
gchar*		gda_report_section_get_currency		(GdaReportSection* object);
void		gda_report_section_set_currency		(GdaReportSection* object,
							 gchar* currency);
gboolean	gda_report_section_get_commaseparator	(GdaReportSection* object);
void		gda_report_section_set_commaseparator	(GdaReportSection* object,
							 gboolean commaseparator);
gfloat		gda_report_section_get_linewidth	(GdaReportSection* object);
void		gda_report_section_set_linewidth	(GdaReportSection* object,
							 gfloat linewidth);
GdkColor	gda_report_section_get_linecolor	(GdaReportSection* object);
void		gda_report_section_set_linecolor	(GdaReportSection* object,
							 GdkColor linecolor);
ReportLineStyle	gda_report_section_get_linestyle	(GdaReportSection* object);
void		gda_report_section_set_linestyle	(GdaReportSection* object,
							 ReportLineStyle linestyle);

void		gda_report_section_add_child		(GdaReportSection* object,
							 GdaReportElement child);
gushort		gda_report_section_remove_child		(GdaReportSection* object,
							 GdaReportElement child);
GList		gda_report_section_get_children		(GdaReportSection* object);

#if defined(__cplusplus)
}
#endif

#endif
