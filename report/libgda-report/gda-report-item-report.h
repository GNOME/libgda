/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <scamps@users.sourceforge.net>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_report_item_report_h__)
#  define __gda_report_item_report_h__

#include <glib-object.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-types.h>


G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM_REPORT		(gda_report_item_report_get_type())
#define GDA_REPORT_ITEM_REPORT(obj)		(G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM_REPORT, GdaReportItemReport))
#define GDA_REPORT_ITEM_REPORT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM_REPORT, GdaReportItemReportClass))
#define GDA_REPORT_IS_ITEM_REPORT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM_REPORT))
#define GDA_REPORT_IS_ITEM_REPORT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM_REPORT))

typedef struct _GdaReportItemReport GdaReportItemReport;
typedef struct _GdaReportItemReportClass GdaReportItemReportClass;

struct _GdaReportItemReport {
	GdaReportItem object;
};

struct _GdaReportItemReportClass {
	GdaReportItemClass parent_class;
};


GType gda_report_item_report_get_type (void);

GdaReportItem *gda_report_item_report_new (GdaReportValid *valid);

GdaReportItem *gda_report_item_report_new_from_dom (xmlNodePtr node);

gboolean gda_report_item_report_set_reportstyle (GdaReportItemReport *item, 
						const gchar* value);

GdaReportItem *gda_report_item_report_get_reportheader (GdaReportItemReport *item);

gchar *gda_report_item_report_get_reportstyle (GdaReportItemReport *item);

gboolean gda_report_item_report_set_pagesize (GdaReportItemReport *item,
					      const gchar *value);

gchar *gda_report_item_report_get_pagesize (GdaReportItemReport *item);

gboolean gda_report_item_report_set_orientation (GdaReportItemReport *item,
				                 const gchar *value);
						 
gchar *gda_report_item_report_get_orientation (GdaReportItemReport *item);
						 
gboolean gda_report_item_report_set_units (GdaReportItemReport *item,
				           const gchar *value);
					   
gchar *gda_report_item_report_get_units (GdaReportItemReport *item);

gboolean gda_report_item_report_set_topmargin (GdaReportItemReport *item,
				               GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_topmargin (GdaReportItemReport *item);

gboolean gda_report_item_report_set_bottommargin (GdaReportItemReport *item,
				                  GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_bottommargin (GdaReportItemReport *item);

gboolean gda_report_item_report_set_leftmargin (GdaReportItemReport *item,
				                GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_leftmargin (GdaReportItemReport *item);

gboolean gda_report_item_report_set_rightmargin (GdaReportItemReport *item,
				                 GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_rightmargin (GdaReportItemReport *item);

gboolean gda_report_item_report_set_bgcolor (GdaReportItemReport *item,
	 			             GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_bgcolor (GdaReportItemReport *item);

gboolean gda_report_item_report_set_fgcolor (GdaReportItemReport *item,
	 			             GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_fgcolor (GdaReportItemReport *item);

gboolean gda_report_item_report_set_bordercolor (GdaReportItemReport *item,
	 			                 GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_bordercolor (GdaReportItemReport *item);

gboolean gda_report_item_report_set_borderwidth (GdaReportItemReport *item,
						 GdaReportNumber *number);
						 
GdaReportNumber *gda_report_item_report_get_borderwidth (GdaReportItemReport *item);

gboolean gda_report_item_report_set_borderstyle (GdaReportItemReport *item,
						 const gchar *value);

gchar * gda_report_item_report_get_borderstyle (GdaReportItemReport *item);

gboolean gda_report_item_report_set_fontfamily (GdaReportItemReport *item,
					        const gchar *value);

gchar *gda_report_item_report_get_fontfamily (GdaReportItemReport *item);

gboolean gda_report_item_report_set_fontsize (GdaReportItemReport *item,
					      GdaReportNumber *number);
					      
GdaReportNumber *gda_report_item_report_get_fontsize (GdaReportItemReport *item);
					      
gboolean gda_report_item_report_set_fontweight (GdaReportItemReport *item,
					        const gchar *value);

gchar *gda_report_item_report_get_fontweight (GdaReportItemReport *item);

gboolean gda_report_item_report_set_fontitalic (GdaReportItemReport *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_fontitalic (GdaReportItemReport *item);
						
gboolean gda_report_item_report_set_halignment (GdaReportItemReport *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_halignment (GdaReportItemReport *item);

gboolean gda_report_item_report_set_valignment (GdaReportItemReport *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_valignment (GdaReportItemReport *item);

gboolean gda_report_item_report_set_wordwrap (GdaReportItemReport *item,
					      const gchar *value);

gchar *gda_report_item_report_get_wordwrap (GdaReportItemReport *item);

gboolean gda_report_item_report_set_negvaluecolor (GdaReportItemReport *item,
						   GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_negvaluecolor (GdaReportItemReport *item);

gboolean gda_report_item_report_set_dateformat (GdaReportItemReport *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_dateformat (GdaReportItemReport *item);

gboolean gda_report_item_report_set_precision (GdaReportItemReport *item,
					       GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_precision (GdaReportItemReport *item);

gboolean gda_report_item_report_set_currency (GdaReportItemReport *item,
					      const gchar *value);

gchar *gda_report_item_report_get_currency (GdaReportItemReport *item);

gboolean gda_report_item_report_set_commaseparator (GdaReportItemReport *item,
						    const gchar *value);

gchar *gda_report_item_report_get_commaseparator (GdaReportItemReport *item);

gboolean gda_report_item_report_set_linewidth (GdaReportItemReport *item,
					       GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_linewidth (GdaReportItemReport *item);

gboolean gda_report_item_report_set_linecolor (GdaReportItemReport *item,
					       GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_linecolor (GdaReportItemReport *item);

gboolean gda_report_item_report_set_linestyle (GdaReportItemReport *item,
					       const gchar *value);
					       gchar * 

gda_report_item_report_get_linestyle (GdaReportItemReport *item);


G_END_DECLS


#endif
