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

#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-valid.h>
#include <libgda-report/gda-report-item-sqlquery.h>

static void gda_report_item_sqlquery_class_init (GdaReportItemSqlQueryClass *klass);
static void gda_report_item_sqlquery_init       (GdaReportItemSqlQuery *valid,
					      GdaReportItemSqlQueryClass *klass);
static void gda_report_item_sqlquery_finalize   (GObject *object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemSqlQuery class implementation
 */
static void
gda_report_item_sqlquery_class_init (GdaReportItemSqlQueryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_sqlquery_finalize;
}


static void
gda_report_item_sqlquery_init (GdaReportItemSqlQuery *item, 
	      	            GdaReportItemSqlQueryClass *klass)
{
	
}


static void
gda_report_item_sqlquery_finalize (GObject *object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY (object));

	if(G_OBJECT_CLASS(parent_class)->finalize) \
                (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


GType
gda_report_item_sqlquery_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemSqlQueryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_sqlquery_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItemSqlQuery),
			0,
			(GInstanceInitFunc) gda_report_item_sqlquery_init
		};
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM, "GdaReportItemSqlQuery", &info, 0);
	}
	return type;
}


/*
 * gda_report_item_sqlquery_new
 */
GdaReportItem *
gda_report_item_sqlquery_new (GdaReportValid *valid,
			      const gchar *id,
			      const gchar *sql)
{
	GdaReportItemSqlQuery *item;
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_SQLQUERY, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = valid;
	GDA_REPORT_ITEM(item)->priv->node = xmlNewNode(NULL, ITEM_SQLQUERY_NAME);	
	gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "id", id);
	gda_report_item_set_content (GDA_REPORT_ITEM(item), sql);
	
	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_sqlquery_new_from_dom
 */
GdaReportItem *
gda_report_item_sqlquery_new_from_dom (xmlNodePtr node)
{
	GdaReportItemSqlQuery *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_SQLQUERY, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return GDA_REPORT_ITEM(item);	
}


/*
 * gda_report_item_sqlquery_to_dom
 */
xmlNodePtr 
gda_report_item_sqlquery_to_dom (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), NULL);
	return gda_report_item_to_dom (item);
}


/*
 * gda_report_item_sqlquery_remove
 */
gboolean 
gda_report_item_sqlquery_remove (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), FALSE);
	return gda_report_item_remove (item);
}


/*
 * gda_report_item_sqlquery_set_id
 */
gboolean 
gda_report_item_sqlquery_set_id (GdaReportItem *item,
			         const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), FALSE);
	return gda_report_item_set_attribute (item, "id", value); 	
}


/*
 * gda_report_item_sqlquery_get_id 
 */
gchar *
gda_report_item_sqlquery_get_id (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), NULL);
	return gda_report_item_get_attribute (item, "id");
}


/*
 * gda_report_item_sqlquery_set_sql 
 */
gboolean 
gda_report_item_sqlquery_set_sql (GdaReportItem *item,
			          const gchar *sql)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), FALSE);
	return gda_report_item_set_content (item, sql);
}


/*
 * gda_report_item_sqlquery_get_sql 
 */
gchar *
gda_report_item_sqlquery_get_sql (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_SQLQUERY(item), NULL);
	return gda_report_item_get_content (item);
}
