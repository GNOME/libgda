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

#if !defined(__gda_report_item_reportheader_h__)
#  define __gda_report_item_reportheader_h__

#include <glib-object.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-types.h>


G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM_REPORTHEADER		(gda_report_item_reportheader_get_type())
#define GDA_REPORT_ITEM_REPORTHEADER(obj)		(G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM_REPORTHEADER, GdaReportItemReportHeader))
#define GDA_REPORT_ITEM_REPORTHEADER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM_REPORTHEADER, GdaReportItemReportHeaderClass))
#define GDA_REPORT_IS_ITEM_REPORTHEADER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM_REPORTHEADER))
#define GDA_REPORT_IS_ITEM_REPORTHEADER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM_REPORTHEADER))

typedef struct _GdaReportItemReportHeader GdaReportItemReportHeader;
typedef struct _GdaReportItemReportHeaderClass GdaReportItemReportHeaderClass;

struct _GdaReportItemReportHeader {
	GdaReportItem object;
};

struct _GdaReportItemReportHeaderClass {
	GdaReportItemClass parent_class;
};


GType gda_report_item_reportheader_get_type (void);

GdaReportItem *gda_report_item_reportheader_new (GdaReportValid *valid);

GdaReportItem *gda_report_item_reportheader_new_from_parent (GdaReportItem *parent);

GdaReportItem *gda_report_item_reportheader_new_from_dom (xmlNodePtr node);

gboolean gda_report_item_reportheader_set_bgcolor (GdaReportItemReportHeader *item,
	 			                   GdaReportColor *value);

GdaReportColor *gda_report_item_reportheader_get_bgcolor (GdaReportItemReportHeader *item);


G_END_DECLS


#endif
