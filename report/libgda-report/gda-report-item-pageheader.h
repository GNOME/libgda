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

#if !defined(__gda_report_item_pageheader_h__)
#  define __gda_report_item_pageheader_h__

#include <glib-object.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-types.h>


G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM_PAGEHEADER			(gda_report_item_pageheader_get_type())
#define GDA_REPORT_ITEM_PAGEHEADER(obj)			(G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM_PAGEHEADER, GdaReportItemPageHeader))
#define GDA_REPORT_ITEM_PAGEHEADER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM_PAGEHEADER, GdaReportItemPageHeaderClass))
#define GDA_REPORT_IS_ITEM_PAGEHEADER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM_PAGEHEADER))
#define GDA_REPORT_IS_ITEM_PAGEHEADER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM_PAGEHEADER))

typedef struct _GdaReportItemPageHeader GdaReportItemPageHeader;
typedef struct _GdaReportItemPageHeaderClass GdaReportItemPageHeaderClass;

struct _GdaReportItemPageHeader {
	GdaReportItem object;
};

struct _GdaReportItemPageHeaderClass {
	GdaReportItemClass parent_class;
};


GType gda_report_item_pageheader_get_type (void);

GdaReportItem *gda_report_item_pageheader_new (GdaReportValid *valid);

GdaReportItem *gda_report_item_pageheader_new_from_dom (xmlNodePtr node);

xmlNodePtr gda_report_item_pageheader_to_dom (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_active (GdaReportItem *item,
						  const gchar *value);

gchar *gda_report_item_pageheader_get_active (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_visible (GdaReportItem *item,
						   const gchar *value);

gchar *gda_report_item_pageheader_get_visible (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_height (GdaReportItem *item,
						  GdaReportNumber *number);

GdaReportNumber *gda_report_item_pageheader_get_height (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_positionfreq (GdaReportItem *item,
						      const gchar *value);

gchar *gda_report_item_pageheader_get_positionfreq (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_pagefreq (GdaReportItem *item,
						  const gchar *value);

gchar *gda_report_item_pageheader_get_pagefreq (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_bgcolor (GdaReportItem *item,
	 			                 GdaReportColor *value);

GdaReportColor *gda_report_item_pageheader_get_bgcolor (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_fgcolor (GdaReportItem *item,
	 			                 GdaReportColor *value);

GdaReportColor *gda_report_item_pageheader_get_fgcolor (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_bordercolor (GdaReportItem *item,
						     GdaReportColor *value);

GdaReportColor *gda_report_item_pageheader_get_bordercolor (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_borderwidth (GdaReportItem *item,
						     GdaReportNumber *number);

GdaReportNumber *gda_report_item_pageheader_get_borderwidth (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_borderstyle (GdaReportItem *item,
						     const gchar *value);

gchar *gda_report_item_pageheader_get_borderstyle (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_fontfamily (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_fontfamily (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_fontsize (GdaReportItem *item,
						  GdaReportNumber *number);

GdaReportNumber *gda_report_item_pageheader_get_fontsize (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_fontweight (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_fontweight (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_fontitalic (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_fontitalic (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_halignment (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_halignment (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_valignment (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_valignment (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_wordwrap (GdaReportItem *item,
						  const gchar *value);

gchar *gda_report_item_pageheader_get_wordwrap (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_negvaluecolor (GdaReportItem *item,
						       GdaReportColor *value);

GdaReportColor *gda_report_item_pageheader_get_negvaluecolor (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_dateformat (GdaReportItem *item,
						    const gchar *value);

gchar *gda_report_item_pageheader_get_dateformat (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_precision (GdaReportItem *item,
						   GdaReportNumber *number);

GdaReportNumber *gda_report_item_pageheader_get_precision (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_currency (GdaReportItem *item,
						  const gchar *value);

gchar *gda_report_item_pageheader_get_currency (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_commaseparator (GdaReportItem *item,
						        const gchar *value);

gchar *gda_report_item_pageheader_get_commaseparator (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_linewidth (GdaReportItem *item,
						   GdaReportNumber *number);

GdaReportNumber *gda_report_item_pageheader_get_linewidth (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_linecolor (GdaReportItem *item,
						   GdaReportColor *value);

GdaReportColor *gda_report_item_pageheader_get_linecolor (GdaReportItem *item);

gboolean gda_report_item_pageheader_set_linestyle (GdaReportItem *item,
						   const gchar *value);

gchar *gda_report_item_pageheader_get_linestyle (GdaReportItem *item);



G_END_DECLS


#endif
