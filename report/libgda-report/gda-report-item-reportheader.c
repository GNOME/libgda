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

#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-valid.h>
#include <libgda-report/gda-report-item-reportheader.h>

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
	GdaReportItemReportHeader *item;
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORTHEADER, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = valid;
	GDA_REPORT_ITEM(item)->priv->node = xmlNewNode(NULL, ITEM_REPORTHEADER_NAME);	
	
	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_reportheader_new_from_dom
 */
GdaReportItem *
gda_report_item_reportheader_new_from_dom (xmlNodePtr node)
{
	GdaReportItemReportHeader *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORTHEADER, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return GDA_REPORT_ITEM(item);	
}


/*
 * gda_report_item_reportheader_to_dom
 */
xmlNodePtr 
gda_report_item_reportheader_to_dom (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	return gda_report_item_to_dom (item);
}


/*
 * gda_report_item_reportheader_set_active
 */
gboolean 
gda_report_item_reportheader_set_active (GdaReportItem *item,
					 const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "active", value); 
}


/*
 * gda_report_item_reportheader_get_active
 */
gchar * 
gda_report_item_reportheader_get_active (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	return gda_report_item_get_attribute (item, "active");
}


/*
 * gda_report_item_reportheader_set_visible
 */
gboolean 
gda_report_item_reportheader_set_visible (GdaReportItem *item,
					  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "visible", value); 
}


/*
 * gda_report_item_reportheader_get_visible
 */
gchar * 
gda_report_item_reportheader_get_visible (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	return gda_report_item_get_attribute (item, "visible");
}


/*
 * gda_report_item_reportheader_set_height
 */
gboolean 
gda_report_item_reportheader_set_height (GdaReportItem *item,
				         GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "height", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_reportheader_get_height
 */
GdaReportNumber *
gda_report_item_reportheader_get_height (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (item, "height"));
}


/*
 * gda_report_item_reportheader_set_newpage
 */
gboolean 
gda_report_item_reportheader_set_newpage (GdaReportItem *item,
					  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "newpage", value); 
}


/*
 * gda_report_item_reportheader_get_newpage
 */
gchar * 
gda_report_item_reportheader_get_newpage (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	return gda_report_item_get_attribute (item, "newpage");
}


/*
 * gda_report_item_reportheader_set_bgcolor
 */
gboolean 
gda_report_item_reportheader_set_bgcolor (GdaReportItem *item,
				          GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bgcolor", value);
}


/*
 * gda_report_item_reportheader_get_bgcolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_bgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "bgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_reportheader_set_fgcolor
 */
gboolean 
gda_report_item_reportheader_set_fgcolor (GdaReportItem *item,
				          GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "fgcolor", value);
}


/*
 * gda_report_item_reportheader_get_fgcolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_fgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "fgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_reportheader_set_bordercolor
 */
gboolean 
gda_report_item_reportheader_set_bordercolor (GdaReportItem *item,
				              GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bordercolor", value);
}


/*
 * gda_report_item_reportheader_get_bordercolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_bordercolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "bordercolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bordercolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_reportheader_set_borderwidth
 */
gboolean 
gda_report_item_reportheader_set_borderwidth (GdaReportItem *item,
					      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "borderwidth", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_reportheader_get_borderwidth
 */
GdaReportNumber *
gda_report_item_reportheader_get_borderwidth (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "borderwidth");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderwidth");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_reportheader_set_borderstyle
 */
gboolean 
gda_report_item_reportheader_set_borderstyle (GdaReportItem *item,
					      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "borderstyle", value); 
}


/*
 * gda_report_item_reportheader_get_borderstyle
 */
gchar * 
gda_report_item_reportheader_get_borderstyle (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "borderstyle");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderstyle");

	return value;
}


/*
 * gda_report_item_reportheader_set_fontfamily
 */
gboolean 
gda_report_item_reportheader_set_fontfamily (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "fontfamily", value); 
}


/*
 * gda_report_item_reportheader_get_fontfamily
 */
gchar * 
gda_report_item_reportheader_get_fontfamily (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "fontfamily");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontfamily");

	return value;
}


/*
 * gda_report_item_reportheader_set_fontsize
 */
gboolean 
gda_report_item_reportheader_set_fontsize (GdaReportItem *item,
					   GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "fontsize", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_reportheader_get_fontsize
 */
GdaReportNumber *
gda_report_item_reportheader_get_fontsize (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "fontsize");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontsize");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_reportheader_set_fontweight
 */
gboolean 
gda_report_item_reportheader_set_fontweight (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "fontweight", value); 
}


/*
 * gda_report_item_reportheader_get_fontweight
 */
gchar * 
gda_report_item_reportheader_get_fontweight (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "fontweight");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontweight");

	return value;
}


/*
 * gda_report_item_reportheader_set_fontitalic
 */
gboolean 
gda_report_item_reportheader_set_fontitalic (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "fontitalic", value); 
}


/*
 * gda_report_item_reportheader_get_fontitalic
 */
gchar * 
gda_report_item_reportheader_get_fontitalic (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "fontitalic");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontitalic");

	return value;
}


/*
 * gda_report_item_reportheader_set_halignment
 */
gboolean 
gda_report_item_reportheader_set_halignment (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "halignment", value); 
}


/*
 * gda_report_item_reportheader_get_halignment
 */
gchar * 
gda_report_item_reportheader_get_halignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "halignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "halignment");

	return value;
}


/*
 * gda_report_item_reportheader_set_valignment
 */
gboolean 
gda_report_item_reportheader_set_valignment (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "valignment", value); 
}


/*
 * gda_report_item_reportheader_get_valignment
 */
gchar * 
gda_report_item_reportheader_get_valignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "valignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "valignment");

	return value;
}


/*
 * gda_report_item_reportheader_set_wordwrap
 */
gboolean 
gda_report_item_reportheader_set_wordwrap (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "wordwrap", value); 
}


/*
 * gda_report_item_reportheader_get_wordwrap
 */
gchar * 
gda_report_item_reportheader_get_wordwrap (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "wordwrap");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "wordwrap");

	return value;
}


/*
 * gda_report_item_reportheader_set_negvaluecolor
 */
gboolean 
gda_report_item_reportheader_set_negvaluecolor (GdaReportItem *item,
				                 GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "negvaluecolor", value);
}


/*
 * gda_report_item_reportheader_get_negvaluecolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_negvaluecolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "negvaluecolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "negvaluecolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_reportheader_set_dateformat
 */
gboolean 
gda_report_item_reportheader_set_dateformat (GdaReportItem *item,
					     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "dateformat", value); 
}


/*
 * gda_report_item_reportheader_get_dateformat
 */
gchar * 
gda_report_item_reportheader_get_dateformat (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "dateformat");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "dateformat");

	return value;
}


/*
 * gda_report_item_reportheader_set_precision
 */
gboolean 
gda_report_item_reportheader_set_precision (GdaReportItem *item,
					    GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "precision", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_reportheader_get_precision
 */
GdaReportNumber *
gda_report_item_reportheader_get_precision (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "precision");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "precision");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_reportheader_set_currency
 */
gboolean 
gda_report_item_reportheader_set_currency (GdaReportItem *item,
					   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "currency", value); 
}


/*
 * gda_report_item_reportheader_get_currency
 */
gchar * 
gda_report_item_reportheader_get_currency (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "currency");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "currency");

	return value;
}


/*
 * gda_report_item_reportheader_set_commaseparator
 */
gboolean 
gda_report_item_reportheader_set_commaseparator (GdaReportItem *item,
					         const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "commaseparator", value); 
}


/*
 * gda_report_item_reportheader_get_commaseparator
 */
gchar * 
gda_report_item_reportheader_get_commaseparator (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "commaseparator");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "commaseparator");

	return value;
}


/*
 * gda_report_item_reportheader_set_linewidth
 */
gboolean 
gda_report_item_reportheader_set_linewidth (GdaReportItem *item,
					    GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "linewidth", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_reportheader_get_linewidth
 */
GdaReportNumber *
gda_report_item_reportheader_get_linewidth (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "linewidth");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linewidth");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_reportheader_set_linecolor
 */
gboolean 
gda_report_item_reportheader_set_linecolor (GdaReportItem *item,
				            GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "linecolor", value);
}


/*
 * gda_report_item_reportheader_get_linecolor
 */
GdaReportColor * 
gda_report_item_reportheader_get_linecolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "linecolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linecolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_reportheader_set_linestyle
 */
gboolean 
gda_report_item_reportheader_set_linestyle (GdaReportItem *item,
					    const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), FALSE);
	return gda_report_item_set_attribute (item, "linestyle", value); 
}


/*
 * gda_report_item_reportheader_get_linestyle
 */
gchar * 
gda_report_item_reportheader_get_linestyle (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER(item), NULL);
	value = gda_report_item_get_attribute (item, "linestyle");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linestyle");

	return value;
}



