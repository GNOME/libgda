/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
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

#ifndef __gda_report_client_h__
#define __gda_report_client_h__

#include <gda-common.h>
#include <GDA_Report.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#define textdomain(String)
#define gettext(String) (String)
#define dgettext(Domain,Message) (Message)
#define dcgettext(Domain,Message,Type) (Message)
#define bindtextdomain(Domain,Directory)
#define _(String) (String)
#define N_(String) (String)
#endif

#include <gda-report-defs.h>
#include <gda-report-format.h>
#include <gda-report-stream.h>
#include <gda-report-output.h>
#include <gda-report-engine.h>
#include <gda-report.h>
#include <gda-report-section.h>
#include <gda-report-header.h>
#include <gda-report-page.h>
#include <gda-report-page-header.h>
#include <gda-report-data-header.h>
#include <gda-report-group-header.h>
#include <gda-report-detail.h>
#include <gda-report-data.h>
#include <gda-report-element.h>
#include <gda-report-picture.h>
#include <gda-report-line.h>
#include <gda-report-object.h>
#include <gda-report-text-object.h>
#include <gda-report-label.h>
#include <gda-report-special.h>
#include <gda-report-repfield.h>
#include <gda-report-group-footer.h>
#include <gda-report-data-footer.h>
#include <gda-report-page-footer.h>
#include <gda-report-footer.h>

void gda_report_init ();

#endif
