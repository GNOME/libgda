/* GDA report libary
 * Copyright (C) 2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Carlos Perelló Marín <carlos@gnome-db.org>
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

#if !defined(__gda_report_types_h__)
#  define __gda_report_types_h__

#define ITEM_REPORT_NAME  		"report"
#define ITEM_QUERYLIST_NAME  		"querylist"
#define ITEM_QUERY_NAME  		"query"
#define ITEM_SQLQUERY_NAME  		"sqlquery"
#define ITEM_REPORTHEADER_NAME		"reportheader"
#define ITEM_REPORTFOOTER_NAME  	"reportfooter"
#define ITEM_PAGEHEADERLIST_NAME	"pageheaderlist"
#define ITEM_PAGEHEADER_NAME		"pageheader"
#define ITEM_PAGEFOOTERLIST_NAME	"pagefooterlist"
#define ITEM_PAGEFOOTER_NAME		"pagefooter"
#define ITEM_DATAAHEADER_NAME		"dataheader"
#define ITEM_DATAFOOTER_NAME		"datafooter"
#define ITEM_DATALIST_NAME		"datalist"
#define ITEM_DETAIL_NAME		"detail"
#define ITEM_GROUPHEADER_NAME		"groupheader"
#define ITEM_GROUPFOOTER_NAME		"groupfooter"
#define ITEM_PICTURE_NAME		"picture"
#define ITEM_LINE_NAME			"line"
#define ITEM_LABEL_NAME			"label"
#define ITEM_REPFIELD_NAME		"repfield"
#define ITEM_SPECIAL_NAME		"special"


typedef struct _GdaReportColor GdaReportColor;
typedef struct _GdaReportColorPrivate GdaReportColorPrivate;
typedef struct _GdaReportNumber GdaReportNumber;
typedef struct _GdaReportNumberPrivate GdaReportNumberPrivate;

struct _GdaReportColor {
	GdaReportColorPrivate *priv;
};

struct _GdaReportNumber {
	GdaReportNumberPrivate *priv;
};


GdaReportColor *gda_report_types_color_new (guint8 red, guint8 blue, guint8 yellow);

gchar *gda_report_types_color_to_value (GdaReportColor *color);

GdaReportColor *gda_report_types_value_to_color (gchar *value);

GdaReportNumber *gda_report_types_number_new (gdouble value);

gdouble gda_report_types_number_to_double (GdaReportNumber *number);

gchar *gda_report_types_number_to_value (GdaReportNumber *number);

GdaReportNumber *gda_report_types_value_to_number (gchar *value);

#endif
