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
#include <libgda-report/gda-report-item-label.h>

static void gda_report_item_label_class_init (GdaReportItemLabelClass *klass);
static void gda_report_item_label_init       (GdaReportItemLabel *valid,
					      GdaReportItemLabelClass *klass);
static void gda_report_item_label_finalize   (GObject *object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemLabel class implementation
 */
static void
gda_report_item_label_class_init (GdaReportItemLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_label_finalize;
}


static void
gda_report_item_label_init (GdaReportItemLabel *item, 
	      	            GdaReportItemLabelClass *klass)
{
	
}


static void
gda_report_item_label_finalize (GObject *object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_LABEL (object));

	if(G_OBJECT_CLASS(parent_class)->finalize) \
                (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


GType
gda_report_item_label_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemLabelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_label_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItemLabel),
			0,
			(GInstanceInitFunc) gda_report_item_label_init
		};
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM, "GdaReportItemLabel", &info, 0);
	}
	return type;
}


/*
 * gda_report_item_label_new
 */
GdaReportItem *
gda_report_item_label_new (GdaReportValid *valid,
			   const gchar *id)
{
	GdaReportItemLabel *item;
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_LABEL, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = valid;
	GDA_REPORT_ITEM(item)->priv->node = xmlNewNode(NULL, ITEM_LABEL_NAME);	
	gda_report_item_set_attribute (GDA_REPORT_ITEM(item), "id", id);
	
	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_label_new_from_dom
 */
GdaReportItem *
gda_report_item_label_new_from_dom (xmlNodePtr node)
{
	GdaReportItemLabel *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_LABEL, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return GDA_REPORT_ITEM(item);	
}


/*
 * gda_report_item_label_to_dom
 */
xmlNodePtr 
gda_report_item_label_to_dom (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	return gda_report_item_to_dom (item);
}


/*
 * gda_report_item_label_remove
 */
gboolean 
gda_report_item_label_remove (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_remove (item);
}


/*
 * gda_report_item_label_set_id
 */
gboolean 
gda_report_item_label_set_id (GdaReportItem *item,
			      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "id", value); 	
}


/*
 * gda_report_item_label_get_id 
 */
gchar *
gda_report_item_label_get_id (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	return gda_report_item_get_attribute (item, "active");
}


/*
 * gda_report_item_label_set_active
 */
gboolean 
gda_report_item_label_set_active (GdaReportItem *item,
				  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "active", value); 
}


/*
 * gda_report_item_label_get_active
 */
gchar * 
gda_report_item_label_get_active (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	return gda_report_item_get_attribute (item, "active");
}


/*
 * gda_report_item_label_set_visible
 */
gboolean 
gda_report_item_label_set_visible (GdaReportItem *item,
				   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "visible", value); 
}


/*
 * gda_report_item_label_get_visible
 */
gchar * 
gda_report_item_label_get_visible (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	return gda_report_item_get_attribute (item, "visible");
}


/*
 * gda_report_item_label_set_text
 */
gboolean 
gda_report_item_label_set_text (GdaReportItem *item,
				   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "text", value); 
}


/*
 * gda_report_item_label_get_text
 */
gchar * 
gda_report_item_label_get_text (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	return gda_report_item_get_attribute (item, "text");
}


/*
 * gda_report_item_label_set_x
 */
gboolean 
gda_report_item_label_set_x (GdaReportItem *item,
			     GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "x", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_label_get_x
 */
GdaReportNumber *
gda_report_item_label_get_x (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "x");
	if (value == NULL)
		return NULL;
	else
		return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_y
 */
gboolean 
gda_report_item_label_set_y (GdaReportItem *item,
			     GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "y", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_label_get_y
 */
GdaReportNumber *
gda_report_item_label_get_y (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "y");
	if (value == NULL)
		return NULL;
	else
		return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_width
 */
gboolean 
gda_report_item_label_set_width (GdaReportItem *item,
			         GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "width", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_label_get_width
 */
GdaReportNumber *
gda_report_item_label_get_width (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "width");
	if (value == NULL)
		return NULL;
	else
		return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_height
 */
gboolean 
gda_report_item_label_set_height (GdaReportItem *item,
			          GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "height", gda_report_types_number_to_value(number));
}
 

/*
 * gda_report_item_label_get_height
 */
GdaReportNumber *
gda_report_item_label_get_height (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "height");
	if (value == NULL)
		return NULL;
	else
		return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_bgcolor
 */
gboolean 
gda_report_item_label_set_bgcolor (GdaReportItem *item,
			           GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bgcolor", value);
}


/*
 * gda_report_item_label_get_bgcolor
 */
GdaReportColor * 
gda_report_item_label_get_bgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "bgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_label_set_fgcolor
 */
gboolean 
gda_report_item_label_set_fgcolor (GdaReportItem *item,
				   GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "fgcolor", value);
}


/*
 * gda_report_item_label_get_fgcolor
 */
GdaReportColor * 
gda_report_item_label_get_fgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "fgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_label_set_bordercolor
 */
gboolean 
gda_report_item_label_set_bordercolor (GdaReportItem *item,
				       GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bordercolor", value);
}


/*
 * gda_report_item_label_get_bordercolor
 */
GdaReportColor * 
gda_report_item_label_get_bordercolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "bordercolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bordercolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_label_set_borderwidth
 */
gboolean 
gda_report_item_label_set_borderwidth (GdaReportItem *item,
				       GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "borderwidth", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_label_get_borderwidth
 */
GdaReportNumber *
gda_report_item_label_get_borderwidth (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "borderwidth");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderwidth");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_borderstyle
 */
gboolean 
gda_report_item_label_set_borderstyle (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "borderstyle", value); 
}


/*
 * gda_report_item_label_get_borderstyle
 */
gchar * 
gda_report_item_label_get_borderstyle (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "borderstyle");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderstyle");

	return value;
}


/*
 * gda_report_item_label_set_fontfamily
 */
gboolean 
gda_report_item_label_set_fontfamily (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontfamily", value); 
}


/*
 * gda_report_item_label_get_fontfamily
 */
gchar * 
gda_report_item_label_get_fontfamily (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontfamily");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontfamily");

	return value;
}


/*
 * gda_report_item_label_set_fontsize
 */
gboolean 
gda_report_item_label_set_fontsize (GdaReportItem *item,
				    GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontsize", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_label_get_fontsize
 */
GdaReportNumber *
gda_report_item_label_get_fontsize (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontsize");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontsize");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_label_set_fontweight
 */
gboolean 
gda_report_item_label_set_fontweight (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontweight", value); 
}


/*
 * gda_report_item_label_get_fontweight
 */
gchar * 
gda_report_item_label_get_fontweight (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontweight");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontweight");

	return value;
}


/*
 * gda_report_item_label_set_fontitalic
 */
gboolean 
gda_report_item_label_set_fontitalic (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontitalic", value); 
}


/*
 * gda_report_item_label_get_fontitalic
 */
gchar * 
gda_report_item_label_get_fontitalic (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontitalic");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontitalic");

	return value;
}


/*
 * gda_report_item_label_set_halignment
 */
gboolean 
gda_report_item_label_set_halignment (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "halignment", value); 
}


/*
 * gda_report_item_label_get_halignment
 */
gchar * 
gda_report_item_label_get_halignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "halignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "halignment");

	return value;
}


/*
 * gda_report_item_label_set_valignment
 */
gboolean 
gda_report_item_label_set_valignment (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "valignment", value); 
}


/*
 * gda_report_item_label_get_valignment
 */
gchar * 
gda_report_item_label_get_valignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "valignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "valignment");

	return value;
}


/*
 * gda_report_item_label_set_wordwrap
 */
gboolean 
gda_report_item_label_set_wordwrap (GdaReportItem *item,
				    const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), FALSE);
	return gda_report_item_set_attribute (item, "wordwrap", value); 
}


/*
 * gda_report_item_label_get_wordwrap
 */
gchar * 
gda_report_item_label_get_wordwrap (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(item), NULL);
	value = gda_report_item_get_attribute (item, "wordwrap");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "wordwrap");

	return value;
}


