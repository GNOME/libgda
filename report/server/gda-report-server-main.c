/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
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
 */

#include <gda-report-server.h>
#include <signal.h>

PortableServer_POA glb_the_poa;
CORBA_Object glb_engine = CORBA_OBJECT_NIL;

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
main (int argc, char *argv[]) {
	struct                    sigaction act;
	sigset_t                  empty_mask;
	CORBA_Environment         ev;
	OAF_RegistrationResult    result;
	CORBA_char*               objref;
	PortableServer_POAManager pm;
	
	/* start report engine in daemon mode */
	chdir("/");
	umask(0);
	
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
	
	/* initialize CORBA server */
	CORBA_exception_init(&ev);
	if (!oaf_init(argc, argv)) {
		gda_log_error(_("Failed to initialize OAF: please mail bug report to OAF maintainers"));
		exit(1);
	}
	glb_the_poa = (PortableServer_POA) CORBA_ORB_resolve_initial_references(gda_corba_get_orb(),
	                                                                        "RootPOA",
	                                                                        &ev);

	glb_engine = impl_GDA_ReportEngine__create(glb_the_poa, &ev);
	if (CORBA_Object_is_nil(glb_engine, &ev)) {
		gda_log_error(_("Could not activate report engine"));
		exit(1);
	}
	objref = CORBA_ORB_object_to_string(gda_corba_get_orb(),
	                                    glb_engine,
	                                    &ev);

	/* register with OAF */
	result = oaf_active_server_register(GDA_REPORT_OAFIID, glb_engine);
	if (result != OAF_REG_SUCCESS) {
		switch (result) {
			case OAF_REG_NOT_LISTED :
				gda_log_error(_("OAF doesn't know about our IID; indicates broken installation. Exiting..."));
				break;
			case OAF_REG_ALREADY_ACTIVE :
				gda_log_error(_("Another instance of the report engine is already registered with OAF. Exiting..."));
				break;
			default :
				gda_log_error(_("Unknown error registering report engine with OAF. Exiting"));
		}
		exit(-1);
	}
	gda_log_message(_("Registered report engine with ID = %s"), objref);
	
	pm = PortableServer_POA__get_the_POAManager(glb_the_poa, &ev);
	PortableServer_POAManager_activate(pm, &ev);
	
	/* initialize everything */
	report_server_cache_init();
	report_server_db_init();
	
	/* run the application */
	CORBA_ORB_run(gda_corba_get_orb(), &ev);
	
	return 0;
}
