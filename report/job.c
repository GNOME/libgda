/* GDA report engine
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include <libgda-report/gda-report-document.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>
#include "job.h"

GdaReportOutput *
engine_job_process_report (GdaReportEngine *engine,
			   const gchar *xml,
			   GdaParameterList *params,
			   CORBA_Environment *ev)
{
	GdaReportDocument *document;

	bonobo_return_val_if_fail (GDA_IS_REPORT_ENGINE (engine), NULL, ev);
	bonobo_return_val_if_fail (xml != NULL, NULL, ev);

	/* parse the document */
	document = gda_report_document_new_from_string (xml);
	if (GDA_IS_REPORT_DOCUMENT (document)) {
		bonobo_exception_general_error_set (
			ev, NULL, _("Could not parse XML report"));
		return CORBA_OBJECT_NIL;
	}
}
