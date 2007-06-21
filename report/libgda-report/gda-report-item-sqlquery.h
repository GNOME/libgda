/* GDA report libary
 * Copyright (C) 1998-2003 The GNOME Foundation.
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

#if !defined(__gda_report_item_sqlquery_h__)
#  define __gda_report_item_sqlquery_h__

#include <glib-object.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-types.h>


G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM_SQLQUERY		 (gda_report_item_sqlquery_get_type())
#define GDA_REPORT_ITEM_SQLQUERY(obj)		 (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM_SQLQUERY, GdaReportItemSqlQuery))
#define GDA_REPORT_ITEM_SQLQUERY_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM_SQLQUERY, GdaReportItemSqlQueryClass))
#define GDA_REPORT_IS_ITEM_SQLQUERY(obj)	 (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM_SQLQUERY))
#define GDA_REPORT_IS_ITEM_SQLQUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM_SQLQUERY))

typedef struct _GdaReportItemSqlQuery GdaReportItemSqlQuery;
typedef struct _GdaReportItemSqlQueryClass GdaReportItemSqlQueryClass;

struct _GdaReportItemSqlQuery {
	GdaReportItem object;
};

struct _GdaReportItemSqlQueryClass {
	GdaReportItemClass parent_class;
};


GType gda_report_item_sqlquery_get_type (void) G_GNUC_CONST;

GdaReportItem *gda_report_item_sqlquery_new (GdaReportValid *valid,
					     const gchar *id,
					     const gchar *sql);

GdaReportItem *gda_report_item_sqlquery_new_from_dom (xmlNodePtr node);

xmlNodePtr gda_report_item_sqlquery_to_dom (GdaReportItem *item);

gboolean gda_report_item_sqlquery_remove (GdaReportItem *item);

gboolean gda_report_item_sqlquery_set_id (GdaReportItem *item,
				          const gchar *value);

gchar *gda_report_item_sqlquery_get_id (GdaReportItem *item);

gboolean gda_report_item_sqlquery_set_sql (GdaReportItem *item,
				           const gchar *sql);

gchar *gda_report_item_sqlquery_get_sql (GdaReportItem *item);


G_END_DECLS


#endif
