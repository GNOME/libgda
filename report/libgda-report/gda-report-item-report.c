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
#include <libgda-report/gda-report-item-report.h>

#define ITEM_REPORT_NAME  "report"

static void gda_report_item_report_class_init (GdaReportItemReportClass *
					       klass);
static void gda_report_item_report_init (GdaReportItemReport * valid,
					 GdaReportItemReportClass * klass);
static void gda_report_item_report_finalize (GObject * object);

static GdaReportItemClass *parent_class = NULL;

/*
 * GdaReportItemReport class implementation
 */
static void
gda_report_item_report_class_init (GdaReportItemReportClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_item_report_finalize;
}


static void
gda_report_item_report_init (GdaReportItemReport * item,
			     GdaReportItemReportClass * klass)
{

}


static void
gda_report_item_report_finalize (GObject * object)
{
	g_return_if_fail (GDA_REPORT_IS_ITEM_REPORT (object));

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}


GType
gda_report_item_report_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
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
		type = g_type_register_static (GDA_REPORT_TYPE_ITEM,
					       "GdaReportItemReport", &info,
					       0);
	}
	return type;
}


/*
 * gda_report_item_report_new
 */
GdaReportItem *
gda_report_item_report_new (GdaReportValid * valid)
{
	GdaReportItemReport *item;

	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORT, NULL);
	GDA_REPORT_ITEM (item)->priv->valid = valid;
	GDA_REPORT_ITEM (item)->priv->node = xmlNewNode (NULL, ITEM_REPORT_NAME);
	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_report_new_from_dom
 */
GdaReportItem *
gda_report_item_report_new_from_dom (xmlNodePtr node)
{
	GdaReportItemReport *item;

	g_return_val_if_fail (node != NULL, NULL);
	item = g_object_new (GDA_REPORT_TYPE_ITEM_REPORT, NULL);
	GDA_REPORT_ITEM (item)->priv->valid =
		gda_report_valid_new_from_dom (xmlGetIntSubset (node->doc));
	GDA_REPORT_ITEM (item)->priv->node = node;

	return GDA_REPORT_ITEM(item);
}


/*
 * gda_report_item_report_set_reportheader
 */
gboolean
gda_report_item_report_set_reportheader (GdaReportItem *report,
					 GdaReportItem *header)
{
	xmlNodePtr cur;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (report), FALSE);
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTHEADER (header),
			      FALSE);

	cur = report->priv->node->children;
	if (cur == NULL)
		/* report has no children, this will be the first one */
		return gda_report_item_add_child (report, header);

	while (cur != NULL)
	{
		if (!xmlNodeIsText (cur))
		{
			/* reportheader already exists, we will replace it */
			if (g_ascii_strcasecmp (cur->name, "reportheader") == 0)
				return gda_report_item_replace
					(gda_report_item_new_from_dom (cur), header);

			/* We want reportheader to be the next element to query, if it exists */
			/* there is no reportheader, and query is previus to current node */
			if ((g_ascii_strcasecmp (cur->name, "reportheader") != 0)
			    && (g_ascii_strcasecmp (cur->name, "query") != 0))
				return gda_report_item_add_previous
					(gda_report_item_new_from_dom (cur), header);
		}
		cur = cur->next;
	}
	return FALSE;
}


/*
 * gda_report_item_report_get_reportheader
 */
GdaReportItem *
gda_report_item_report_get_reportheader (GdaReportItem * item)
{
	xmlNodePtr node;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	node = item->priv->node->children;
	while ((node != NULL) && (g_ascii_strcasecmp (node->name, "reportheader") != 0))
		node = node->next;

	if (node == NULL)
	{
		gda_log_error ("There is no report header in this report \n");
		return NULL;
	}
	else
		return gda_report_item_reportheader_new_from_dom (node);
}


/*
 * gda_report_item_report_set_reporthfooter
 */
gboolean
gda_report_item_report_set_reportfooter (GdaReportItem *report,
					 GdaReportItem *footer)
{
	xmlNodePtr cur;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (report), FALSE);
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORTFOOTER (footer), FALSE);

	cur = report->priv->node->last;
	if (cur != NULL)
	{
		while (xmlNodeIsText (cur)) cur = cur->prev;

		if (g_ascii_strcasecmp (cur->name, "reportfooter") == 0)
			/* reportfooter already exists, we will replace it */
			return gda_report_item_replace (gda_report_item_new_from_dom (cur), footer);
		else
			/* We want reportfooter to be the last element */
			/* the last is not a reportfooter, so we will add it as next */
			return gda_report_item_add_next	(gda_report_item_new_from_dom (cur), footer);
	}
	else
		/* report has no children, this will be the first one */
		return gda_report_item_add_child (report, footer);

	return FALSE;
}


/*
 * gda_report_item_report_get_reportfooter
 */
GdaReportItem *
gda_report_item_report_get_reportfooter (GdaReportItem *item)
{
	xmlNodePtr node;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	
	node = item->priv->node->last;
	while ((node != NULL) && (g_ascii_strcasecmp (node->name, "reportfooter") != 0))
		node = node->prev;

	if (node == NULL)
	{
		gda_log_error ("There is no report footer in this report \n");
		return NULL;
	}
	else
		return gda_report_item_reportfooter_new_from_dom (node);
}


/*
 * gda_report_item_report_set_reportstyle
 */
gboolean
gda_report_item_report_set_reportstyle (GdaReportItem *item,
					const gchar * value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "reportstyle", value);
}


/*
 * gda_report_item_report_get_reportstyle 
 */
gchar *
gda_report_item_report_get_reportstyle (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "reportstyle");
}


/*
 * gda_report_item_report_set_pagesize
 */
gboolean
gda_report_item_report_set_pagesize (GdaReportItem *item,
				     const gchar * value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "pagesize", value);
}


/*
 * gda_report_item_report_get_pagesize
 */
gchar *
gda_report_item_report_get_pagesize (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "pagesize");
}


/*
 * gda_report_item_report_set_orientation
 */
gboolean
gda_report_item_report_set_orientation (GdaReportItem *item,
					const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "orientation", value);
}


/*
 * gda_report_item_report_get_orientation
 */
gchar *
gda_report_item_report_get_orientation (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "orientation");
}


/*
 * gda_report_item_report_set_units
 */
gboolean
gda_report_item_report_set_units (GdaReportItem *item,
				  const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "units", value);
}


/*
 * gda_report_item_report_get_units
 */
gchar *
gda_report_item_report_get_units (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "units");
}


/*
 * gda_report_item_report_set_topmargin
 */
gboolean
gda_report_item_report_set_topmargin (GdaReportItem *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "topmargin",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_topmargin
 */
GdaReportNumber *
gda_report_item_report_get_topmargin (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute (item, "topmargin"));
}


/*
 * gda_report_item_report_set_bottommargin
 */
gboolean
gda_report_item_report_set_bottommargin (GdaReportItem *item,
					 GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "bottommargin",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_bottommargin
 */
GdaReportNumber *
gda_report_item_report_get_bottommargin (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute(item, "bottommargin"));
}


/*
 * gda_report_item_report_set_leftmargin
 */
gboolean
gda_report_item_report_set_leftmargin (GdaReportItem *item,
				       GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "leftmargin",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_leftmargin
 */
GdaReportNumber *
gda_report_item_report_get_leftmargin (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (gda_report_item_get_attribute(item, "leftmargin"));
}


/*
 * gda_report_item_report_set_rightmargin
 */
gboolean
gda_report_item_report_set_rightmargin (GdaReportItem *item,
					GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "rightmargin",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_rightmargin
 */
GdaReportNumber *
gda_report_item_report_get_rightmargin (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute (item, "rigthmargin"));
}


/*
 * gda_report_item_report_set_bgcolor
 */
gboolean
gda_report_item_report_set_bgcolor (GdaReportItem *item,
				    GdaReportColor *color)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bgcolor", value);
}


/*
 * gda_report_item_report_get_bgcolor
 */
GdaReportColor *
gda_report_item_report_get_bgcolor (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_color (
			gda_report_item_get_attribute(item, "bgcolor"));
}


/*
 * gda_report_item_report_set_fgcolor
 */
gboolean
gda_report_item_report_set_fgcolor (GdaReportItem *item,
				    GdaReportColor *color)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "fgcolor", value);
}


/*
 * gda_report_item_report_get_fgcolor
 */
GdaReportColor *
gda_report_item_report_get_fgcolor (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_color (
			gda_report_item_get_attribute(item, "fgcolor"));
}


/*
 * gda_report_item_report_set_bordercolor
 */
gboolean
gda_report_item_report_set_bordercolor (GdaReportItem *item,
					GdaReportColor *color)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "bordercolor", value);
}


/*
 * gda_report_item_report_get_bordercolor
 */
GdaReportColor *
gda_report_item_report_get_bordercolor (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_color (
			gda_report_item_get_attribute(item, "bordercolor"));
}


/*
 * gda_report_item_report_set_borderwidth
 */
gboolean
gda_report_item_report_set_borderwidth (GdaReportItem * item,
					GdaReportNumber * number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "borderwidth",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_borderwidth
 */
GdaReportNumber *
gda_report_item_report_get_borderwidth (GdaReportItem * item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute (item, "rigthmargin"));
}


/*
 * gda_report_item_report_set_borderstyle
 */
gboolean
gda_report_item_report_set_borderstyle (GdaReportItem *item,
					const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "borderstyle", value);
}


/*
 * gda_report_item_report_get_borderstyle
 */
gchar *
gda_report_item_report_get_borderstyle (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "borderstyle");
}


/*
 * gda_report_item_report_set_fontfamily
 */
gboolean
gda_report_item_report_set_fontfamily (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "fontfamily", value);
}


/*
 * gda_report_item_report_get_fontfamily
 */
gchar *
gda_report_item_report_get_fontfamily (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "fontfamily");
}


/*
 * gda_report_item_report_set_fontsize
 */
gboolean
gda_report_item_report_set_fontsize (GdaReportItem *item,
				     GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "fontsize",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_fontsize
 */
GdaReportNumber *
gda_report_item_report_get_fontsize (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute(item, "rigthmargin"));
}


/*
 * gda_report_item_report_set_fontweight
 */
gboolean
gda_report_item_report_set_fontweight (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "fontweight", value);
}


/*
 * gda_report_item_report_get_fontweight
 */
gchar *
gda_report_item_report_get_fontweight (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "fontweight");
}


/*
 * gda_report_item_report_set_fontitalic
 */
gboolean
gda_report_item_report_set_fontitalic (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "fontitalic", value);
}


/*
 * gda_report_item_report_get_fontitalic
 */
gchar *
gda_report_item_report_get_fontitalic (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "fontitalic");
}


/*
 * gda_report_item_report_set_halignment
 */
gboolean
gda_report_item_report_set_halignment (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "halignment", value);
}


/*
 * gda_report_item_report_get_halignment
 */
gchar *
gda_report_item_report_get_halignment (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "halignment");
}


/*
 * gda_report_item_report_set_valignment
 */
gboolean
gda_report_item_report_set_valignment (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "valignment", value);
}


/*
 * gda_report_item_report_get_valignment
 */
gchar *
gda_report_item_report_get_valignment (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "valignment");
}


/*
 * gda_report_item_report_set_wordwrap
 */
gboolean
gda_report_item_report_set_wordwrap (GdaReportItem *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "wordwrap", value);
}


/*
 * gda_report_item_report_get_wordwrap
 */
gchar *
gda_report_item_report_get_wordwrap (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "wordwrap");
}


/*
 * gda_report_item_report_set_negvaluecolor
 */
gboolean
gda_report_item_report_set_negvaluecolor (GdaReportItem * item,
					  GdaReportColor * color)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "negvaluecolor", value);
}


/*
 * gda_report_item_report_get_negvaluecolor
 */
GdaReportColor *
gda_report_item_report_get_negvaluecolor (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_color (
			gda_report_item_get_attribute(item, "negvaluecolor"));
}


/*
 * gda_report_item_report_set_dateformat
 */
gboolean
gda_report_item_report_set_dateformat (GdaReportItem *item,
				       const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "dateformat", value);
}


/*
 * gda_report_item_report_get_dateformat
 */
gchar *
gda_report_item_report_get_dateformat (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "dateformat");
}


/*
 * gda_report_item_report_set_precision
 */
gboolean
gda_report_item_report_set_precision (GdaReportItem *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "precision",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_precision
 */
GdaReportNumber *
gda_report_item_report_get_precision (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute(item, "rigthmargin"));
}


/*
 * gda_report_item_report_set_currency
 */
gboolean
gda_report_item_report_set_currency (GdaReportItem *item,
				     const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "currency", value);
}


/*
 * gda_report_item_report_get_currency
 */
gchar *
gda_report_item_report_get_currency (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "currency");
}


/*
 * gda_report_item_report_set_commaseparator
 */
gboolean
gda_report_item_report_set_commaseparator (GdaReportItem *item,
					   const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "commaseparator", value);
}


/*
 * gda_report_item_report_get_commaseparator
 */
gchar *
gda_report_item_report_get_commaseparator (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "commaseparator");
}


/*
 * gda_report_item_report_set_linewidth
 */
gboolean
gda_report_item_report_set_linewidth (GdaReportItem *item,
				      GdaReportNumber *number)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "linewidth",
					      gda_report_types_number_to_value(number));
}


/*
 * gda_report_item_report_get_linewidth
 */
GdaReportNumber *
gda_report_item_report_get_linewidth (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_number (
			gda_report_item_get_attribute(item, "rigthmargin"));
}


/*
 * gda_report_item_report_set_linecolor
 */
gboolean
gda_report_item_report_set_linecolor (GdaReportItem *item,
				      GdaReportColor *color)
{
	gchar *value;

	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	value = gda_report_types_color_to_value (color);
	return gda_report_item_set_attribute (item, "linecolor", value);
}


/*
 * gda_report_item_report_get_linecolor
 */
GdaReportColor *
gda_report_item_report_get_linecolor (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_types_value_to_color (
			gda_report_item_get_attribute(item, "linecolor"));
}



/*
 * gda_report_item_report_set_linestyle
 */
gboolean
gda_report_item_report_set_linestyle (GdaReportItem *item,
				      const gchar *value)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), FALSE);
	return gda_report_item_set_attribute (item, "linestyle", value);
}


/*
 * gda_report_item_report_get_linestyle
 */
gchar *
gda_report_item_report_get_linestyle (GdaReportItem *item)
{
	g_return_val_if_fail (GDA_REPORT_IS_ITEM_REPORT (item), NULL);
	return gda_report_item_get_attribute (item, "linestyle");
}
