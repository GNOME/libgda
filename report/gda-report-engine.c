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

#include "config.h"
#include <gda-report.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

struct Gda_ReportEngine
{
  GDA_ReportEngine corba_engine;
};

/**
* gda_report_engine_load
 *
 * Connect a client application to the report engine, as defined in the
 * system
 */
Gda_ReportEngine *
gda_report_engine_load (void)
{
  Gda_ReportEngine* engine;
  CORBA_Environment ev;
  
  engine = g_new0(Gda_ReportEngine, 1);
  
  /* activate CORBA object */
  CORBA_exception_init(&ev);
  engine->corba_engine = oaf_activate_from_id(GDA_REPORT_OAFIID, 0, NULL, &ev);
  if (!CORBA_Object_is_nil(engine->corba_engine, &ev))
    {
      /* FIXME: add authentication? */
      CORBA_exception_free(&ev);
      return engine;
    }
  
  /* free memory on error */
  g_free((gpointer) engine);
  return NULL;
}

/**
 * gda_report_engine_unload
 */
void
gda_report_engine_unload (Gda_ReportEngine *engine)
{
  CORBA_Environment* ev;
  
  g_return_if_fail(engine != NULL);
  g_return_if_fail(engine->corba_engine != CORBA_OBJECT_NIL);
  
  /* deactivates CORBA server */
  CORBA_exception_init(&ev);
  CORBA_Object_release(engine, &ev);
  if (!gda_corba_handle_exception(&ev))
    {
      gda_log_error(_("CORBA exception unloading report engine: %s"), CORBA_exception_id(&ev));
    }
  
  /* free all memory */
  g_free((gpointer) engine);
}

/**
 * gda_report_engine_query_reports
 */
GList *
gda_report_engine_query_reports (Gda_ReportEngine *engine, const gchar *condition, Gda_ReportFlags flags)
{
  GList*            list = NULL;
  CORBA_Environment ev;

  g_return_val_if_fail(engine != NULL, NULL);
}
