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
#include <libgda-report/gda-report-item-report.h>

#define ITEM_REPORT_NAME	"report"

static void gda_report_item_report_class_init (GdaReportItemReportClass *klass);
static void gda_report_item_report_init       (GdaReportItemReport *valid,
					       GdaReportItemReportClass *klass);
static void gda_report_item_report_finalize   (GObject *object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemReport class implementation
 */
static void
gda_report_item_report_class_init (GdaReportItemReportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_report_finalize;
}


static void
gda_report_item_report_init (GdaReportItemReport *item, 
		      	     GdaReportItemReportClass *klass)
{
	
}


static void
gda_report_item_report_finalize (GObject *object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_REPORT (object));

	if(G_OBJECT_CLASS(parent_class)->finalize) \
                (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


GType
gda_report_item_report_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemReportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_report_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItemReport),
			0,
			(GInstanceInitFunc) gda_report_item_report_init
		};
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM, "GdaReportItemReport", &info, 0);
	}
	return type;
}


/*
 * gda_report_item_report_new
 */
GdaReportItem *
gda_report_item_report_new (GdaReportValid *valid)
{
	GdaReportItem *item;
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORT, NULL);
	item = gda_report_item_new (valid, ITEM_REPORT_NAME);
	return item;
}


/*
 * gda_report_item_report_new_from_dom
 */
GdaReportItem *
gda_report_item_report_new_from_dom (xmlNodePtr node)
{
	GdaReportItem *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORT, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return item;	
}


/*
 * gda_report_item_report_get_reportheader
 */
GdaReportItem *
gda_report_item_report_get_reportheader (GdaReportItemReport *item)
{
	xmlNodePtr node;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	node = xmlGetLastChild (GDA_REPORT_ITEM(item)->priv->node);
	while ((node != NULL) && (g_ascii_strcasecmp(node->name, "reportheader") != 0)) 
		node = node->prev;

	if (node == NULL)
	{
		gda_log_error ("There is no report header in this report \n");
		return NULL;
	}
	else
		return gda_report_item_reportheader_new_from_dom(node);
}


/*
 * gda_report_item_report_set_reportstyle
 */
gboolean 
gda_report_item_report_set_reportstyle (GdaReportItemReport *item,
					const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "reportstyle", value); 
}


/*
 * gda_report_item_report_get_reportstyle 
 */
gchar * 
gda_report_item_report_get_reportstyle (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "reportstyle");
}


/*
 * gda_report_item_report_set_pagesize
 */
gboolean 
gda_report_item_report_set_pagesize (GdaReportItemReport *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "pagesize", value); 
}


/*
 * gda_report_item_report_get_pagesize
 */
gchar * 
gda_report_item_report_get_pagesize (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "pagesize");
}


/*
 * gda_report_item_report_set_orientation
 */
gboolean 
gda_report_item_report_set_orientation (GdaReportItemReport *item,
				        const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "orientation", value); 
}


/*
 * gda_report_item_report_get_orientation
 */
gchar * 
gda_report_item_report_get_orientation (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "orientation");
}


/*
 * gda_report_item_report_set_units
 */
gboolean 
gda_report_item_report_set_units (GdaReportItemReport *item,
				  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "units", value); 
}


/*
 * gda_report_item_report_get_units
 */
gchar * 
gda_report_item_report_get_units (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "units");
}


/*
 * gda_report_item_report_set_topmargin
 */
gboolean 
gda_report_item_report_set_topmargin (GdaReportItemReport *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "topmargin", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_topmargin
 */
GdaReportNumber *
gda_report_item_report_get_topmargin (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "topmargin"));
}


/*
 * gda_report_item_report_set_bottommargin
 */
gboolean 
gda_report_item_report_set_bottommargin (GdaReportItemReport *item,
				         GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "bottommargin", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_bottommargin
 */
GdaReportNumber *
gda_report_item_report_get_bottommargin (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "bottommargin"));
}


/*
 * gda_report_item_report_set_leftmargin
 */
gboolean 
gda_report_item_report_set_leftmargin (GdaReportItemReport *item,
				       GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "leftmargin", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_leftmargin
 */
GdaReportNumber *
gda_report_item_report_get_leftmargin (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number(gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "leftmargin"));
}


/*
 * gda_report_item_report_set_rightmargin
 */
gboolean 
gda_report_item_report_set_rightmargin (GdaReportItemReport *item,
				        GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "rightmargin", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_rightmargin
 */
GdaReportNumber *
gda_report_item_report_get_rightmargin (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "rigthmargin"));
}


/*
 * gda_report_item_report_set_bgcolor
 */
gboolean 
gda_report_item_report_set_bgcolor (GdaReportItemReport *item,
				    GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "bgcolor", value);
}


/*
 * gda_report_item_report_get_bgcolor
 */
GdaReportColor * 
gda_report_item_report_get_bgcolor (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_types_value_to_color (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "bgcolor"));
}


/*
 * gda_report_item_report_set_fgcolor
 */
gboolean 
gda_report_item_report_set_fgcolor (GdaReportItemReport *item,
				    GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "fgcolor", value);
}


/*
 * gda_report_item_report_get_fgcolor
 */
GdaReportColor * 
gda_report_item_report_get_fgcolor (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_types_value_to_color (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "fgcolor"));
}


/*
 * gda_report_item_report_set_bordercolor
 */
gboolean 
gda_report_item_report_set_bordercolor (GdaReportItemReport *item,
  				        GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "bordercolor", value);
}


/*
 * gda_report_item_report_get_bordercolor
 */
GdaReportColor * 
gda_report_item_report_get_bordercolor (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_types_value_to_color (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "bordercolor"));
}


/*
 * gda_report_item_report_set_borderwidth
 */
gboolean 
gda_report_item_report_set_borderwidth (GdaReportItemReport *item,
				        GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "borderwidth", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_borderwidth
 */
GdaReportNumber *
gda_report_item_report_get_borderwidth (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "rigthmargin"));
}


/*
 * gda_report_item_report_set_borderstyle
 */
gboolean 
gda_report_item_report_set_borderstyle (GdaReportItemReport *item,
					const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "borderstyle", value); 
}


/*
 * gda_report_item_report_get_borderstyle
 */
gchar * 
gda_report_item_report_get_borderstyle (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "borderstyle");
}


/*
 * gda_report_item_report_set_fontfamily
 */
gboolean 
gda_report_item_report_set_fontfamily (GdaReportItemReport *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "fontfamily", value); 
}


/*
 * gda_report_item_report_get_fontfamily
 */
gchar * 
gda_report_item_report_get_fontfamily (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "fontfamily");
}


/*
 * gda_report_item_report_set_fontsize
 */
gboolean 
gda_report_item_report_set_fontsize (GdaReportItemReport *item,
				     GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "fontsize", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_fontsize
 */
GdaReportNumber *
gda_report_item_report_get_fontsize (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "rigthmargin"));
}


/*
 * gda_report_item_report_set_fontweight
 */
gboolean 
gda_report_item_report_set_fontweight (GdaReportItemReport *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "fontweight", value); 
}


/*
 * gda_report_item_report_get_fontweight
 */
gchar * 
gda_report_item_report_get_fontweight (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "fontweight");
}


/*
 * gda_report_item_report_set_fontitalic
 */
gboolean 
gda_report_item_report_set_fontitalic (GdaReportItemReport *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "fontitalic", value); 
}


/*
 * gda_report_item_report_get_fontitalic
 */
gchar * 
gda_report_item_report_get_fontitalic (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "fontitalic");
}


/*
 * gda_report_item_report_set_halignment
 */
gboolean 
gda_report_item_report_set_halignment (GdaReportItemReport *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "halignment", value); 
}


/*
 * gda_report_item_report_get_halignment
 */
gchar * 
gda_report_item_report_get_halignment (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "halignment");
}


/*
 * gda_report_item_report_set_valignment
 */
gboolean 
gda_report_item_report_set_valignment (GdaReportItemReport *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "valignment", value); 
}


/*
 * gda_report_item_report_get_valignment
 */
gchar * 
gda_report_item_report_get_valignment (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "valignment");
}


/*
 * gda_report_item_report_set_wordwrap
 */
gboolean 
gda_report_item_report_set_wordwrap (GdaReportItemReport *item,
    			             const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "wordwrap", value); 
}


/*
 * gda_report_item_report_get_wordwrap
 */
gchar * 
gda_report_item_report_get_wordwrap (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "wordwrap");
}


/*
 * gda_report_item_report_set_negvaluecolor
 */
gboolean 
gda_report_item_report_set_negvaluecolor (GdaReportItemReport *item,
   				          GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "negvaluecolor", value);
}


/*
 * gda_report_item_report_get_negvaluecolor
 */
GdaReportColor * 
gda_report_item_report_get_negvaluecolor (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_types_value_to_color (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "negvaluecolor"));
}


/*
 * gda_report_item_report_set_dateformat
 */
gboolean 
gda_report_item_report_set_dateformat (GdaReportItemReport *item,
     			               const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "dateformat", value); 
}


/*
 * gda_report_item_report_get_dateformat
 */
gchar * 
gda_report_item_report_get_dateformat (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "dateformat");
}


/*
 * gda_report_item_report_set_precision
 */
gboolean 
gda_report_item_report_set_precision (GdaReportItemReport *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "precision", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_precision
 */
GdaReportNumber *
gda_report_item_report_get_precision (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "rigthmargin"));
}


/*
 * gda_report_item_report_set_currency
 */
gboolean 
gda_report_item_report_set_currency (GdaReportItemReport *item,
  			             const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "currency", value); 
}


/*
 * gda_report_item_report_get_currency
 */
gchar * 
gda_report_item_report_get_currency (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "currency");
}


/*
 * gda_report_item_report_set_commaseparator
 */
gboolean 
gda_report_item_report_set_commaseparator (GdaReportItemReport *item,
					   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "commaseparator", value); 
}


/*
 * gda_report_item_report_get_commaseparator
 */
gchar * 
gda_report_item_report_get_commaseparator (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "commaseparator");
}


/*
 * gda_report_item_report_set_linewidth
 */
gboolean 
gda_report_item_report_set_linewidth (GdaReportItemReport *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "linewidth", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_report_get_linewidth
 */
GdaReportNumber *
gda_report_item_report_get_linewidth (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), 0);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "rigthmargin"));
}


/*
 * gda_report_item_report_set_linecolor
 */
gboolean 
gda_report_item_report_set_linecolor (GdaReportItemReport *item,
				      GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "linecolor", value);
}


/*
 * gda_report_item_report_get_linecolor
 */
GdaReportColor * 
gda_report_item_report_get_linecolor (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_types_value_to_color (gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "linecolor"));
}



/*
 * gda_report_item_report_set_linestyle
 */
gboolean 
gda_report_item_report_set_linestyle (GdaReportItemReport *item,
		 		      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), FALSE);
	return gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "linestyle", value); 
}


/*
 * gda_report_item_report_get_linestyle
 */
gchar * 
gda_report_item_report_get_linestyle (GdaReportItemReport *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT(item), NULL);
	return gda_report_item_get_attribute (GDA_REPORT_ITEM(item), "linestyle");
}
