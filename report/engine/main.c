/* GDA Report Engine
 * Copyright (C) 2000-2001 The Free Software Foundation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
 * 	Carlos Perelló Marín <carlos@gnome-db.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include "gda-report-document-factory.h"

static GdaReportDocumentFactory *document_factory;

/*
 * Static functions
 */
static void
signal_handler (int signo) {
	static gint in_fatal = 0;
	
	/* avoid loops */
	if (in_fatal > 0) return;
	++in_fatal;
	
	switch (signo) {
		/* Fast cleanup only */
		case SIGSEGV:
		case SIGBUS:
		case SIGILL:
			gda_log_error(_("Received signal %d, shutting down."), signo);
			abort();
			break;
		/* maybe it's more feasible to clean up more mess */
		case SIGFPE:
		case SIGPIPE:
		case SIGTERM:
			gda_log_error(_("Received signal %d, shutting down."), signo);
			exit (1);
			break;
		case SIGHUP:
			gda_log_error(_("Received signal %d, shutting down cleanly"), signo);
			--in_fatal;
			break;
		default:
			break;
	}
}

int
main (int argc, char *argv[])
{
	CORBA_ORB orb;
	struct sigaction act;
	sigset_t empty_mask;

	bindtextdomain (PACKAGE, GDA_LOCALEDIR);
	textdomain (PACKAGE);
	
	gnome_init_with_popt_table ("gda-report-engine", VERSION, argc, argv,
				    oaf_popt_options, 0, NULL);
	
	/* session setup */
	sigemptyset(&empty_mask);
	act.sa_handler = signal_handler;
	act.sa_mask = empty_mask;
	act.sa_flags = 0;
	sigaction(SIGTERM,  &act, 0);
	sigaction(SIGILL,  &act, 0);
	sigaction(SIGBUS,  &act, 0);
	sigaction(SIGFPE,  &act, 0);
	sigaction(SIGHUP,  &act, 0);
	sigaction(SIGSEGV, &act, 0);
	sigaction(SIGABRT, &act, 0);

	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, 0);

	orb = oaf_init(argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL,
			 CORBA_OBJECT_NIL) == FALSE) {
		gda_log_error(_("The report engine could not initialize Bonobo.");
	}
	
	document_factory = gda_report_document_factory_new ();
	
	/* initialize everything */
/*	report_server_cache_init();
	report_server_db_init();*/
	
	/* run the application */
	bonobo_main ();
	
	bonobo_object_unref (BONOBO_OBJECT (document_factory));
	document_factory = NULL;

	return 0;
}
