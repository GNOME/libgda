/* GNOME DB Server Library
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
#include "gda-server.h"
#include <signal.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#endif

#include <popt.h>
#include <liboaf/liboaf.h>

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

/*
 * Private functions
 */
static void
signal_handler (int signo)
{
  static gint in_fatal = 0;

  /* avoid loops */
  if (in_fatal > 0) return;
  ++in_fatal;

  switch (signo)
    {
    case SIGSEGV : /* fast cleanup only */
    case SIGBUS :
    case SIGILL :
      gda_log_error(_("Received signal %d, shutting down"), signo);
      abort();
      break;
    case SIGFPE : /* clean up more */
    case SIGPIPE :
    case SIGTERM :
      gda_log_error(_("Received signal %d, shutting down"), signo);
      exit(1);
      break;
    case SIGHUP :
      gda_log_message(_("Received signal %d. Ignoring"), signo);
      --in_fatal;
      break;
    default :
      break;
    }
}

static void
initialize_signals (void)
{
  struct sigaction act;
  sigset_t         empty_mask;

  /* session setup */
  sigemptyset(&empty_mask);
  act.sa_handler = signal_handler;
  act.sa_mask = empty_mask;
  act.sa_flags = 0;
  sigaction(SIGTERM, &act, 0);
  sigaction(SIGILL, &act, 0);
  sigaction(SIGBUS, &act, 0);
  sigaction(SIGFPE, &act, 0);
  sigaction(SIGHUP, &act, 0);
  sigaction(SIGSEGV, &act, 0);
  sigaction(SIGABRT, &act, 0);

  act.sa_handler = SIG_IGN;
  sigaction(SIGINT, &act, 0);
}

/**
 * gda_server_init
 * @app_id: application name
 * @version: application version
 * @nargs: number of arguments
 * @args: arguments
 *
 * Initialize all needed stuff for a GDA provider to run. This includes initializing
 * the object type system and the CORBA stuff
 */
void
gda_server_init (const gchar *app_id, const gchar *version, gint nargs, gchar *args[])
{
  extern struct poptOption oaf_popt_options[];
  static gboolean          initialized = FALSE;
  poptContext              pctx;

  if (initialized)
    {
      gda_log_error(_("Attempt to initialize an already initialized provider"));
      return;
    }

  initialize_signals();

  oaf_init(nargs, args);
  
#ifdef HAVE_GOBJECT
  g_set_prgname (app_id);
  g_type_init ();
#else
  /* process commands */
  pctx = poptGetContext(app_id, nargs, args, oaf_popt_options, 0);
  while (poptGetNextOpt(pctx) >= 0) ;
  poptFreeContext(pctx);
  
  gtk_type_init();
#endif
}

