/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <santi@gnome-db.org>
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

gboolean gda_report_item_report_set_reportheader (GdaReportItem *report,
						  GdaReportItem *header);

GdaReportItem *gda_report_item_report_get_reportheader (GdaReportItem *item);

gboolean gda_report_item_report_set_reportfooter (GdaReportItem *report,
						  GdaReportItem *footer);

GdaReportItem *gda_report_item_report_get_reportfooter (GdaReportItem *item);

gint gda_report_item_report_get_pageheaderlist_length (GdaReportItem *report);

gboolean gda_report_item_report_add_nth_pageheader (GdaReportItem *report,
						    GdaReportItem *pageheader,
						    gint position);

GdaReportItem *gda_report_item_report_get_nth_pageheader (GdaReportItem *report,
							  gint position);

gint gda_report_item_report_get_pagefooterlist_length (GdaReportItem *report);

gboolean gda_report_item_report_add_nth_pagefooter (GdaReportItem *report,
						    GdaReportItem *pagefooter,
						    gint position);

GdaReportItem *gda_report_item_report_get_nth_pagefooter (GdaReportItem *report,
							  gint position);

gboolean gda_report_item_report_set_reportstyle (GdaReportItem *item, 
						const gchar *value);

GdaReportItem *gda_report_item_report_get_label_by_id (GdaReportItem *report,
						       const gchar *id);

gchar *gda_report_item_report_get_reportstyle (GdaReportItem *item);

gboolean gda_report_item_report_set_pagesize (GdaReportItem *item,
					      const gchar *value);

gchar *gda_report_item_report_get_pagesize (GdaReportItem *item);

gboolean gda_report_item_report_set_orientation (GdaReportItem *item,
				                 const gchar *value);
						 
gchar *gda_report_item_report_get_orientation (GdaReportItem *item);
						 
gboolean gda_report_item_report_set_units (GdaReportItem *item,
				           const gchar *value);
					   
gchar *gda_report_item_report_get_units (GdaReportItem *item);

gboolean gda_report_item_report_set_topmargin (GdaReportItem *item,
				               GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_topmargin (GdaReportItem *item);

gboolean gda_report_item_report_set_bottommargin (GdaReportItem *item,
				                  GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_bottommargin (GdaReportItem *item);

gboolean gda_report_item_report_set_leftmargin (GdaReportItem *item,
				                GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_leftmargin (GdaReportItem *item);

gboolean gda_report_item_report_set_rightmargin (GdaReportItem *item,
				                 GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_rightmargin (GdaReportItem *item);

gboolean gda_report_item_report_set_bgcolor (GdaReportItem *item,
	 			             GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_bgcolor (GdaReportItem *item);

gboolean gda_report_item_report_set_fgcolor (GdaReportItem *item,
	 			             GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_fgcolor (GdaReportItem *item);

gboolean gda_report_item_report_set_bordercolor (GdaReportItem *item,
	 			                 GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_bordercolor (GdaReportItem *item);

gboolean gda_report_item_report_set_borderwidth (GdaReportItem *item,
						 GdaReportNumber *number);
						 
GdaReportNumber *gda_report_item_report_get_borderwidth (GdaReportItem *item);

gboolean gda_report_item_report_set_borderstyle (GdaReportItem *item,
						 const gchar *value);

gchar * gda_report_item_report_get_borderstyle (GdaReportItem *item);

gboolean gda_report_item_report_set_fontfamily (GdaReportItem *item,
					        const gchar *value);

gchar *gda_report_item_report_get_fontfamily (GdaReportItem *item);

gboolean gda_report_item_report_set_fontsize (GdaReportItem *item,
					      GdaReportNumber *number);
					      
GdaReportNumber *gda_report_item_report_get_fontsize (GdaReportItem *item);
					      
gboolean gda_report_item_report_set_fontweight (GdaReportItem *item,
					        const gchar *value);

gchar *gda_report_item_report_get_fontweight (GdaReportItem *item);

gboolean gda_report_item_report_set_fontitalic (GdaReportItem *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_fontitalic (GdaReportItem *item);
						
gboolean gda_report_item_report_set_halignment (GdaReportItem *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_halignment (GdaReportItem *item);

gboolean gda_report_item_report_set_valignment (GdaReportItem *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_valignment (GdaReportItem *item);

gboolean gda_report_item_report_set_wordwrap (GdaReportItem *item,
					      const gchar *value);

gchar *gda_report_item_report_get_wordwrap (GdaReportItem*item);

gboolean gda_report_item_report_set_negvaluecolor (GdaReportItem *item,
						   GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_negvaluecolor (GdaReportItem *item);

gboolean gda_report_item_report_set_dateformat (GdaReportItem *item,
					        const gchar *value);
						
gchar *gda_report_item_report_get_dateformat (GdaReportItem *item);

gboolean gda_report_item_report_set_precision (GdaReportItem *item,
					       GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_precision (GdaReportItem *item);

gboolean gda_report_item_report_set_currency (GdaReportItem *item,
					      const gchar *value);

gchar *gda_report_item_report_get_currency (GdaReportItem *item);

gboolean gda_report_item_report_set_commaseparator (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_report_get_commaseparator (GdaReportItem *item);

gboolean gda_report_item_report_set_linewidth (GdaReportItem *item,
					       GdaReportNumber *number);

GdaReportNumber *gda_report_item_report_get_linewidth (GdaReportItem *item);

gboolean gda_report_item_report_set_linecolor (GdaReportItem *item,
					       GdaReportColor *color);

GdaReportColor *gda_report_item_report_get_linecolor (GdaReportItem *item);

gboolean gda_report_item_report_set_linestyle (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_report_get_linestyle (GdaReportItem *item);


G_END_DECLS


#endif
