
#include "gdaIncludes.h"


int main(int argc, char *argv[]) {
	CORBA_Environment ev;
	CORBA_ORB orb;
	GdaRecordset *rst = NULL;
	GdaConnection *cnc = NULL;
	gulong i = 0;

	/* initialize CORBA/bonobo */
	CORBA_exception_init(&ev);
	orb = gnome_CORBA_init("gda-example-app", "1.01", &argc, argv, 0, &ev);
	CORBA_exception_free(&ev);

	cnc = new GdaConnection(orb);
	cnc->open("testdsn","username","password");

	rst = new GdaRecordset();
	rst->setConnection(cnc);
	rst->open("SELECT * FROM testtable;");

	for (i=0;i<rst->affectedRows();i++) printf("%s\n",rst->field("testfield")->convertToNewString());

	rst->close();
	cnc->close();
	delete rst;
	delete cnc;

	return 0;
}
