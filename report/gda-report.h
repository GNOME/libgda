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

#if !defined(__gda_report_h__)
#  define __gda_report_h__

#include <gda-common.h>
#include <GDA_Report.h>

typedef enum
{
  GDA_REPORT_FLAGS_NONE,
} Gda_ReportFlags;

#include <gda-report-defs.h>
#include <gda-report-engine.h>

void gda_report_init ();

#endif
