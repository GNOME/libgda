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

#include <glib.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-types.h>

#define GDA_REPORT_COLOR_VALUE_LENGTH	12
#define GDA_REPORT_NUMBER_VALUE_LENGTH	 7


struct _GdaReportColorPrivate {
        guint8 red;
        guint8 blue;
        guint8 yellow;
};

struct _GdaReportNumberPrivate {
	gdouble value;
};


/*
 * gda_report_types_color_new
 */
GdaReportColor *
gda_report_types_color_new (guint8 red, guint8 blue, guint8 yellow)
{
	GdaReportColor *color;

	color = g_new0 (GdaReportColor, 1);
	color->priv = g_new0 (GdaReportColorPrivate, 1);
	color->priv->red = red;
	color->priv->blue = blue;
	color->priv->yellow = yellow;
	
	return color;
}


/*
 * gda_report_types_color_to_value
 */
gchar *
gda_report_types_color_to_value (GdaReportColor *color)
{
	GString *string;

	g_return_val_if_fail (color != NULL, NULL);
	
	string = g_string_new_len ("", GDA_REPORT_COLOR_VALUE_LENGTH);	
	g_snprintf (string->str, GDA_REPORT_COLOR_VALUE_LENGTH, "%i %i %i", 
		    color->priv->red, color->priv->blue, color->priv->yellow);

	return string->str;
}


/*
 * gda_report_types_value_to_color
 */
GdaReportColor *
gda_report_types_value_to_color (gchar *value)
{
	GdaReportColor *color;

	g_return_val_if_fail (value != NULL, NULL);

	color = g_new0 (GdaReportColor, 1);
	color->priv = g_new0 (GdaReportColorPrivate, 1);	
	sscanf (value, "%i %i %i", (int *) &color->priv->red, (int *) &color->priv->blue, (int *) &color->priv->yellow);

	return color;
}


/*
 * gda_report_types_number_new
 */
GdaReportNumber *
gda_report_types_number_new (gdouble value)
{
	GdaReportNumber *number;

	number = g_new0 (GdaReportNumber, 1);
	number->priv = g_new0 (GdaReportNumberPrivate, 1);
	number->priv->value = value;
	return number;
}


/*
 * gda_report_types_number_to_value
 */
gchar *
gda_report_types_number_to_value (GdaReportNumber *number)
{
	GString *string;

	g_return_val_if_fail (number != NULL, NULL);
	
	string = g_string_new_len ("", GDA_REPORT_NUMBER_VALUE_LENGTH);	
	g_ascii_dtostr (string->str, GDA_REPORT_NUMBER_VALUE_LENGTH, number->priv->value);
	return string->str;
}


/*
 * gda_report_types_value_to_number
 */
GdaReportNumber *
gda_report_types_value_to_number (gchar *value)
{
	return gda_report_types_number_new (g_ascii_strtod (value, NULL));
}


