/* GDA report libary
 * Copyright (C) 2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Carlos Perelló Marín <carlos@gnome-db.org>
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

typedef enum {
        GDA_REPORT_STYLE_FORM,
	GDA_REPORT_STYLE_LIST
} GdaReportStyle;

/* FIXME: This one should come from the available papers not a fixed value */
typedef enum {
	GDA_REPORT_PAGE_SIZE_A3,
	GDA_REPORT_PAGE_SIZE_A4,
	GDA_REPORT_PAGE_SIZE_A5,
	GDA_REPORT_PAGE_SIZE_A6,
	GDA_REPORT_PAGE_SIZE_B3,
	GDA_REPORT_PAGE_SIZE_B4,
	GDA_REPORT_PAGE_SIZE_B5,
	GDA_REPORT_PAGE_SIZE_B6,
	GDA_REPORT_PAGE_SIZE_LETTER,
	GDA_REPORT_PAGE_SIZE_LEGAL,
	GDA_REPORT_PAGE_SIZE_EXECUTIVE
} GdaReportPageSize;

typedef enum {
	GDA_REPORT_ORIENTATION_PORTRAIT,
	GDA_REPORT_ORIENTATION_LANDSCAPE
} GdaReportOrientation;

typedef enum {
	GDA_REPORT_UNITS_INCH,
	GDA_REPORT_UNITS_CM,
	GDA_REPORT_UNITS_PT
} GdaReportUnits;

typedef enum {
	GDA_REPORT_LINE_STYLE_NONE,
	GDA_REPORT_LINE_STYLE_SOLID,
	GDA_REPORT_LINE_STYLE_DASH,
	GDA_REPORT_LINE_STYLE_DOT,
	GDA_REPORT_LINE_STYLE_DASH_DOT,
	GDA_REPORT_LINE_STYLE_DASH_DOT_DOT
} GdaReportLineStyle;

typedef enum {
	GDA_REPORT_FONT_WEIGHT_LIGHT,
	GDA_REPORT_FONT_WEIGHT_NORMAL,
	GDA_REPORT_FONT_WEIGHT_SEMIBOLD,
	GDA_REPORT_FONT_WEIGHT_BOLD,
	GDA_REPORT_FONT_WEIGHT_BLACK
} GdaReportFontWeight;

typedef enum {
	GDA_REPORT_H_ALIGNMENT_STANDAR,
	GDA_REPORT_H_ALIGNMENT_LEFT,
	GDA_REPORT_H_ALIGNMENT_CENTER,
	GDA_REPORT_H_ALIGNMENT_RIGHT
} GdaReportHAlignment;

typedef enum {
	GDA_REPORT_V_ALIGNMENT_TOP,
	GDA_REPORT_V_ALIGNMENT_CENTER,
	GDA_REPORT_V_ALIGNMENT_BOTTOM
} GdaReportVAlignment;

typedef struct {
	guint8 red;
	guint8 blue;
	guint8 yellow;
} GdaReportColor;

#endif
