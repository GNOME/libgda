/* GDA report libary
 * Copyright (C) 2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Carlos Perelló Marín <carlos@gnome-db.org>
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

#if !defined(__gda_report_types_h__)
#  define __gda_report_types_h__

typedef struct _GdaReportColor GdaReportColor;
typedef struct _GdaReportColorPrivate GdaReportColorPrivate;
typedef struct _GdaReportNumber GdaReportNumber;
typedef struct _GdaReportNumberPrivate GdaReportNumberPrivate;

struct _GdaReportColor {
	GdaReportColorPrivate *priv;
};

struct _GdaReportNumber {
	GdaReportNumberPrivate *priv;
};


GdaReportColor *gda_report_types_color_new (guint8 red, guint8 blue, guint8 yellow);

gchar *gda_report_types_color_to_value (GdaReportColor *color);

GdaReportColor *gda_report_types_value_to_color (gchar *value);

GdaReportNumber *gda_report_types_number_new (gdouble value);

gchar *gda_report_types_number_to_value (GdaReportNumber *number);

GdaReportNumber *gda_report_types_value_to_number (gchar *value);

#endif
