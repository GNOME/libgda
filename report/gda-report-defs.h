/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_report_defs_h__)
#  define __gda_report_defs_h__

#include <gda-common.h>

#define GDA_REPORT_OAFIID "OAFIID:GNOME_GDA_ReportEngine"

enum ReportPositionFreq {
	POSITION_FREQ_FIRST,
	POSITION_FREQ_INSIDE,
	POSITION_FREQ_LAST,
	POSITION_FREQ_ALLBUTFIRST,
	POSITION_FREQ_ALLBUTLAST,
	POSITION_FREQ_FIRSTANDLAST,
	POSITION_FREQ_ALL
};

enum ReportPageFreq {
	PAGE_FREQ_EVEN,
	PAGE_FREQ_ODD,
	PAGE_FREQ_ALL
};

enum ReportLineStyle {
	LINE_STYLE_NONE,
	LINE_STYLE_SOLID,
	LINE_STYLE_DASH,
	LINE_STYLE_DOT,
	LINE_STYLE_DASHDOT,
	LINE_STYLE_DASHDOTDOT
};

enum ReportFontWeight {
	FONT_WEIGHT_LIGHT,
	FONT_WEIGHT_NORMAL,
	FONT_WEIGHT_SEMIBOLD,
	FONT_WEIGHT_BOLD,
	FONT_WEIGHT_BLACK
};

enum ReportHAlignment {
	HALIGNMENT_STANDARD,
	HALIGNMENT_LEFT,
	HALIGNMENT_CENTER,
	HALIGNMENT_RIGHT
};

enum ReportVAlignment {
	VALIGNMENT_TOP,
	VALIGNMENT_CENTER,
	VALIGNMENT_BOTTOM
};

enum ReportSize {
	SIZE_CUT,
	SIZE_SCALE
};

enum ReportRatio {
	RATIO_FIXED,
	RATIO_FLOAT
};

enum ReportSource {
	SOURCE_INTERN,
	SOURCE_EXTERN
};

enum ReportPageSize {
	PAGE_SIZE_A3,
	PAGE_SIZE_A4,
	PAGE_SIZE_A5,
	PAGE_SIZE_A6,
	PAGE_SIZE_B3,
	PAGE_SIZE_B4,
	PAGE_SIZE_B5,
	PAGE_SIZE_B6,
	PAGE_SIZE_LETTER,
	PAGE_SIZE_LEGAL,
	PAGE_SIZE_EXECUTIVE
};

enum ReportOrientation {
	ORIENTATION_PORTRAIT,
	ORIENTATION_LANDSCAPE
};

enum ReportUnits {
	UNITS_INCH,
	UNITS_CM,
	UNITS_PT
};
	
typedef struct _GdaReportAttribute GdaReportAttribute;

struct _GdaReportAttribute {
	gchar* name;
	gchar* value;
};


#endif
