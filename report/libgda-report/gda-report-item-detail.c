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

#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-valid.h>
#include <libgda-report/gda-report-item-label.h>
#include <libgda-report/gda-report-item-repfield.h>
#include <libgda-report/gda-report-item-detail.h>

static void gda_report_item_detail_class_init (GdaReportItemDetailClass *klass);
static void gda_report_item_detail_init       (GdaReportItemDetail *valid,
					       GdaReportItemDetailClass *klass);
static void gda_report_item_detail_finalize   (GObject *object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemDetail class implementation
 */
static void
gda_report_item_detail_class_init (GdaReportItemDetailClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_detail_finalize;
}


static void
gda_report_item_detail_init (GdaReportItemDetail *item, 
		      	     GdaReportItemDetailClass *klass)
{
	
}


static void
gda_report_item_detail_finalize (GObject *object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_DETAIL (object));

	if(G_OBJECT_CLASS(parent_class)->finalize) \
                (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


GType
gda_report_item_detail_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportItemDetailClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_item_detail_class_init,
			NULL,
			NULL,
			sizeof (GdaReportItemDetail),
			0,
			(GInstanceInitFunc) gda_report_item_detail_init
		};
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM, "GdaReportItemDetail", &info, 0);
	}
	return type;
}


/*
 * gda_report_item_detail_new
 */
GdaReportItem *
gda_report_item_detail_new (GdaReportValid *valid)
{
	GdaReportItemDetail *item;
	
	g_return_val_if_fail (GDA_IS_REPORT_VALID(valid), NULL);
	
	item = g_object_new (GDA_REPORT_TYPE_ITEM_DETAIL, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = valid;
	GDA_REPORT_ITEM(item)->priv->node = xmlNewNode(NULL, ITEM_DETAIL_NAME);	
	
	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_detail_new_from_dom
 */
GdaReportItem *
gda_report_item_detail_new_from_dom (xmlNodePtr node)
{
	GdaReportItemDetail *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_DETAIL, NULL);
	GDA_REPORT_ITEM(item)->priv->valid = gda_report_valid_new_from_dom(xmlGetIntSubset(node->doc));
	GDA_REPORT_ITEM(item)->priv->node = node;	
	
	return GDA_REPORT_ITEM(item);	
}


/*
 * gda_report_item_detail_to_dom
 */
xmlNodePtr 
gda_report_item_detail_to_dom (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_item_to_dom (item);
}


/*
 * gda_report_item_detail_add_element
 */
gboolean
gda_report_item_detail_add_element (GdaReportItem *detail,
				    GdaReportItem *element)
{
	gchar *id;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(detail), FALSE);
	
	/* News kinds of elements should be added here */
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_LABEL(element) || 
			      GDA_REPORT_IS_ITEM_REPFIELD(element), FALSE);
	
	/* Child elements are only allowed when parent element belongs to a report document */
	g_return_val_if_fail (gda_report_item_belongs_to_report_document(detail), FALSE);

	id = gda_report_item_get_attribute (element, "id");
	if (gda_report_item_get_child_by_id (
		gda_report_item_get_report (detail), id) != NULL)
	{
		gda_log_error (_("An element with ID %s already exists in the report"), id);
		return FALSE;
	}	
	return gda_report_item_add_child (detail, element);
}



/*
 * gda_report_item_detail_get_label_by_id
 */
GdaReportItem *
gda_report_item_detail_get_label_by_id (GdaReportItem *detail,
				        const gchar *id)
{
	GdaReportItem *label;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL (detail), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	
	label = gda_report_item_get_child_by_id (detail, id);
	if (label != NULL) 
	{
		if (g_ascii_strcasecmp(gda_report_item_get_item_type(label), ITEM_LABEL_NAME) != 0)
		{
			gda_log_error (_("Item with ID %s is not a label"), id);
			return NULL;
		}
		else
			return gda_report_item_label_new_from_dom (label->priv->node);
	}
	else
		return NULL;
}



/*
 * gda_report_item_detail_get_repfield_by_id
 */
GdaReportItem *
gda_report_item_detail_get_repfield_by_id (GdaReportItem *detail,
				           const gchar *id)
{
	GdaReportItem *repfield;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL (detail), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	
	repfield = gda_report_item_get_child_by_id (detail, id);
	if (repfield != NULL)
	{
		if (g_ascii_strcasecmp(gda_report_item_get_item_type(repfield), ITEM_REPFIELD_NAME) != 0)
		{
			gda_log_error (_("Item with ID %s is not a repfield"), id);
			return NULL;
		}
		else		
			return gda_report_item_repfield_new_from_dom (repfield->priv->node);
	}
	else
		return NULL;
}


/*
 * gda_report_item_detail_get_first_item 
 */
GdaReportItem *
gda_report_item_detail_get_first_item (GdaReportItem *detail)
{
	GdaReportItem *result;
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL (detail), NULL);
	
	result = gda_report_item_get_first_child (detail);
	if (result != NULL)
	{
		if (g_ascii_strcasecmp(gda_report_item_get_item_type(result), ITEM_REPFIELD_NAME) == 0)
			return gda_report_item_repfield_new_from_dom (result->priv->node);

		if (g_ascii_strcasecmp(gda_report_item_get_item_type(result), ITEM_LABEL_NAME) == 0)
			return gda_report_item_label_new_from_dom (result->priv->node);
	}
	return NULL;
}


/*
 * gda_report_item_detail_get_next_item 
 */
GdaReportItem *
gda_report_item_detail_get_next_item (GdaReportItem *detail,
				      GdaReportItem *item)
{
	GdaReportItem *result;
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL (detail), NULL);
	
	result = gda_report_item_get_next_child (detail, item);
	if (result != NULL)
	{
		if (g_ascii_strcasecmp(gda_report_item_get_item_type(result), ITEM_REPFIELD_NAME) == 0)
			return gda_report_item_repfield_new_from_dom (result->priv->node);

		if (g_ascii_strcasecmp(gda_report_item_get_item_type(result), ITEM_LABEL_NAME) == 0)
			return gda_report_item_label_new_from_dom (result->priv->node);
	}
	return NULL;
}


/*
 * gda_report_item_detail_remove
 */
gboolean 
gda_report_item_detail_remove (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_remove (item);
}


/*
 * gda_report_item_detail_set_query
 */
gboolean 
gda_report_item_detail_set_query (GdaReportItem *item,
				  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "query", value); 
}


/*
 * gda_report_item_detail_get_query
 */
gchar * 
gda_report_item_detail_get_query (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_item_get_attribute (item, "query");
}


/*
 * gda_report_item_detail_set_active
 */
gboolean 
gda_report_item_detail_set_active (GdaReportItem *item,
				   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "active", value); 
}


/*
 * gda_report_item_detail_get_active
 */
gchar * 
gda_report_item_detail_get_active (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_item_get_attribute (item, "active");
}


/*
 * gda_report_item_detail_set_visible
 */
gboolean 
gda_report_item_detail_set_visible (GdaReportItem *item,
				    const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "visible", value); 
}


/*
 * gda_report_item_detail_get_visible
 */
gchar * 
gda_report_item_detail_get_visible (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_item_get_attribute (item, "visible");
}


/*
 * gda_report_item_detail_set_height
 */
gboolean 
gda_report_item_detail_set_height (GdaReportItem *item,
				   GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "height", gda_report_types_number_to_value(number)); 
}


/*
 * gda_report_item_detail_get_height
 */
GdaReportNumber *
gda_report_item_detail_get_height (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_types_value_to_number (gda_report_item_get_attribute (item, "height"));
}


/*
 * gda_report_item_detail_set_pagefreq
 */
gboolean 
gda_report_item_detail_set_pagefreq (GdaReportItem *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "pagefreq", value); 
}


/*
 * gda_report_item_detail_get_pagefreq
 */
gchar * 
gda_report_item_detail_get_pagefreq (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	return gda_report_item_get_attribute (item, "pagefreq");
}


/*
 * gda_report_item_detail_set_bgcolor
 */
gboolean 
gda_report_item_detail_set_bgcolor (GdaReportItem *item,
				    GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bgcolor", value);
}


/*
 * gda_report_item_detail_get_bgcolor
 */
GdaReportColor * 
gda_report_item_detail_get_bgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "bgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_detail_set_fgcolor
 */
gboolean 
gda_report_item_detail_set_fgcolor (GdaReportItem *item,
				    GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "fgcolor", value);
}


/*
 * gda_report_item_detail_get_fgcolor
 */
GdaReportColor * 
gda_report_item_detail_get_fgcolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "fgcolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fgcolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_detail_set_bordercolor
 */
gboolean 
gda_report_item_detail_set_bordercolor (GdaReportItem *item,
					GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bordercolor", value);
}


/*
 * gda_report_item_detail_get_bordercolor
 */
GdaReportColor * 
gda_report_item_detail_get_bordercolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "bordercolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "bordercolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_detail_set_borderwidth
 */
gboolean 
gda_report_item_detail_set_borderwidth (GdaReportItem *item,
					GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "borderwidth", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_detail_get_borderwidth
 */
GdaReportNumber *
gda_report_item_detail_get_borderwidth (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "borderwidth");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderwidth");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_detail_set_borderstyle
 */
gboolean 
gda_report_item_detail_set_borderstyle (GdaReportItem *item,
					const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "borderstyle", value); 
}


/*
 * gda_report_item_detail_get_borderstyle
 */
gchar * 
gda_report_item_detail_get_borderstyle (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "borderstyle");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "borderstyle");

	return value;
}


/*
 * gda_report_item_detail_set_fontfamily
 */
gboolean 
gda_report_item_detail_set_fontfamily (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontfamily", value); 
}


/*
 * gda_report_item_detail_get_fontfamily
 */
gchar * 
gda_report_item_detail_get_fontfamily (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontfamily");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontfamily");

	return value;
}


/*
 * gda_report_item_detail_set_fontsize
 */
gboolean 
gda_report_item_detail_set_fontsize (GdaReportItem *item,
				     GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontsize", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_detail_get_fontsize
 */
GdaReportNumber *
gda_report_item_detail_get_fontsize (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontsize");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontsize");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_detail_set_fontweight
 */
gboolean 
gda_report_item_detail_set_fontweight (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontweight", value); 
}


/*
 * gda_report_item_detail_get_fontweight
 */
gchar * 
gda_report_item_detail_get_fontweight (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontweight");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontweight");

	return value;
}


/*
 * gda_report_item_detail_set_fontitalic
 */
gboolean 
gda_report_item_detail_set_fontitalic (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "fontitalic", value); 
}


/*
 * gda_report_item_detail_get_fontitalic
 */
gchar * 
gda_report_item_detail_get_fontitalic (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "fontitalic");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "fontitalic");

	return value;
}


/*
 * gda_report_item_detail_set_halignment
 */
gboolean 
gda_report_item_detail_set_halignment (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "halignment", value); 
}


/*
 * gda_report_item_detail_get_halignment
 */
gchar * 
gda_report_item_detail_get_halignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "halignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "halignment");

	return value;
}


/*
 * gda_report_item_detail_set_valignment
 */
gboolean 
gda_report_item_detail_set_valignment (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "valignment", value); 
}


/*
 * gda_report_item_detail_get_valignment
 */
gchar * 
gda_report_item_detail_get_valignment (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "valignment");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "valignment");

	return value;
}


/*
 * gda_report_item_detail_set_wordwrap
 */
gboolean 
gda_report_item_detail_set_wordwrap (GdaReportItem *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "wordwrap", value); 
}


/*
 * gda_report_item_detail_get_wordwrap
 */
gchar * 
gda_report_item_detail_get_wordwrap (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "wordwrap");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "wordwrap");

	return value;
}


/*
 * gda_report_item_detail_set_negvaluecolor
 */
gboolean 
gda_report_item_detail_set_negvaluecolor (GdaReportItem *item,
				          GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "negvaluecolor", value);
}


/*
 * gda_report_item_detail_get_negvaluecolor
 */
GdaReportColor * 
gda_report_item_detail_get_negvaluecolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "negvaluecolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "negvaluecolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_detail_set_dateformat
 */
gboolean 
gda_report_item_detail_set_dateformat (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "dateformat", value); 
}


/*
 * gda_report_item_detail_get_dateformat
 */
gchar * 
gda_report_item_detail_get_dateformat (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "dateformat");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "dateformat");

	return value;
}


/*
 * gda_report_item_detail_set_precision
 */
gboolean 
gda_report_item_detail_set_precision (GdaReportItem *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "precision", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_detail_get_precision
 */
GdaReportNumber *
gda_report_item_detail_get_precision (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "precision");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "precision");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_detail_set_currency
 */
gboolean 
gda_report_item_detail_set_currency (GdaReportItem *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "currency", value); 
}


/*
 * gda_report_item_detail_get_currency
 */
gchar * 
gda_report_item_detail_get_currency (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "currency");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "currency");

	return value;
}


/*
 * gda_report_item_detail_set_commaseparator
 */
gboolean 
gda_report_item_detail_set_commaseparator (GdaReportItem *item,
					   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "commaseparator", value); 
}


/*
 * gda_report_item_detail_get_commaseparator
 */
gchar * 
gda_report_item_detail_get_commaseparator (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "commaseparator");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "commaseparator");

	return value;
}


/*
 * gda_report_item_detail_set_linewidth
 */
gboolean 
gda_report_item_detail_set_linewidth (GdaReportItem *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "linewidth", gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_detail_get_linewidth
 */
GdaReportNumber *
gda_report_item_detail_get_linewidth (GdaReportItem *item)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "linewidth");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linewidth");

	return gda_report_types_value_to_number(value);
}


/*
 * gda_report_item_detail_set_linecolor
 */
gboolean 
gda_report_item_detail_set_linecolor (GdaReportItem *item,
				      GdaReportColor *color)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "linecolor", value);
}


/*
 * gda_report_item_detail_get_linecolor
 */
GdaReportColor * 
gda_report_item_detail_get_linecolor (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "linecolor");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linecolor");
	
	return gda_report_types_value_to_color (value);
}


/*
 * gda_report_item_detail_set_linestyle
 */
gboolean 
gda_report_item_detail_set_linestyle (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), FALSE);
	return gda_report_item_set_attribute (item, "linestyle", value); 
}


/*
 * gda_report_item_detail_get_linestyle
 */
gchar * 
gda_report_item_detail_get_linestyle (GdaReportItem *item)
{
	gchar *value;
	
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_DETAIL(item), NULL);
	value = gda_report_item_get_attribute (item, "linestyle");
	if (value == NULL) 
		value = gda_report_item_get_inherit_attribute (item, "linestyle");

	return value;
}



