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

#if !defined(__gda_report_engine_h__)
#  define __gda_report_engine_h__

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_Report_Engine Gda_Report_Engine;

Gda_Report_Engine* gda_report_engine_load          (void);
void              gda_report_engine_unload        (Gda_Report_Engine *engine);

GList*            gda_report_engine_query_reports (Gda_Report_Engine *engine,
                                                   const gchar *condition,
                                                   Gda_Report_Flags flags);

#if defined(__cplusplus)
}
#endif

#endif
