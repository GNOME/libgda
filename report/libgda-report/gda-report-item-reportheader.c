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

#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-valid.h>
#include <libgda-report/gda-report-item-reportheader.h>

#define ITEM_REPORTHEADER_NAME	"reportheader"

static void gda_report_item_reportheader_class_init (GdaReportItemReportHeaderClass *klass);
static void gda_report_item_reportheader_init       (GdaReportItemReportHeader *valid,
					             GdaReportItemReportHeaderClass *klass);
static void gda_report_item_reportheader_finalize   (GObject *object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemReportHeader class implementation
 */
static void
gda_report_item_reportheader_class_init (GdaReportItemReportHeaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_reportheader_finalize;
}


static void
gda_report_item_reportheader_init (GdaReportItemReportHeader *item, 
		      	           GdaReportItemReportHeaderClass *klass)
{
	
}


static void
gda_report_item_reportheader_finalize (GObject *object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER (object));

	if(G_OBJECT_CLASS(parent_class)->finalize) \
                (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


GType
gda_report_item_reportheader_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemReportHeaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_reportheader_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItemReportHeader),
			0,
			(GInstanceInitFunc) gda_report_item_reportheader_init
		};
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM, "GdaReportItemReportHeader", &info, 0);
	}
	return type;
}


/*
 * gda_report_item_reportheader_new
 */
GdaReportItem *
gda_report_item_reportheader_new (GdaReportValid *valid)
{
	GdaReportItem *item;
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORTHEADER, NULL);
	item = gda_report_item_new (valid, ITEM_REPORTHEADER_NAME);
	return item;
}


/*
 * gda_report_item_reportheader_new_from_parent
 */
GdaReportItem *
gda_report_item_reportheader_new_from_parent (GdaReportItem *parent)
{
	GdaReportItem *item;
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORTHEADER, NULL);
	item = gda_report_item_new_child (parent, ITEM_REPORTHEADER_NAME);
	return item;	
}


/*
 * gda_report_item_reportheader_new_from_dom
 */
GdaReportItem *
gda_report_item_reportheader_new_from_dom (xmlNodePtr node)
{
	GdaReportItem *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORTHEADER, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return item;	
}


/*
 * gda_report_item_reportheader_set_bgcolor
 */
gboolean 
gda_report_item_reportheader_set_bgcolor (GdaReportItemReportHeader *item,
				          GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "bgcolor", value);
}


/*
 * gda_report_item_reportheader_get_bgcolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_bgcolor (GdaReportItemReportHeader *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "bgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (GDA_REPORT_ITEM(item), "bgcolor");
	
	return gda_report_types_value_to_color (value);
}


