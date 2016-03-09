/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>
#include <libgda-xslt.h>
#include <libxslt/xsltutils.h>

gchar *inputfile = NULL;
gchar *outputfile = NULL;
gchar *xslfile = NULL;
gchar *dsn = NULL;

static GOptionEntry entries[] = {
        { "in", 'i', 0, G_OPTION_ARG_STRING, &inputfile, "Input file", NULL},
        { "out", 'o', 0, G_OPTION_ARG_STRING, &outputfile, "Output file", NULL},
        { "xsl", 'x', 0, G_OPTION_ARG_STRING, &xslfile, "XSL file", NULL},
	{ "dsn", 'd', 0, G_OPTION_ARG_STRING, &dsn, "Data source name", NULL},
        { NULL }
};

static int sqlxslt_process_xslt_file_ext (GdaXsltExCont *sql_ctx, 
					  const char *inputFile, 
					  const char *xslFileName, 
					  const char *outputFileName);


int
main (int argc, char *argv[])
{
        GdaConnection *cnc;
	GError *error = NULL;
	GOptionContext *context;

	context = g_option_context_new ("LibgdaXsltProc");
	g_option_context_add_main_entries (context, entries, NULL);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_print ("Can't parse arguments: %s", error->message);
                exit (EXIT_FAILURE);
        }
        g_option_context_free (context);

	if (!inputfile) {
		g_print ("Missing input file (use --help option)\n");
		exit (EXIT_FAILURE);
	}
	if (!outputfile) {
		g_print ("Missing output file (use --help option)\n");
		exit (EXIT_FAILURE);
	}
	if (!xslfile) {
		g_print ("Missing XSL file (use --help option)\n");
		exit (EXIT_FAILURE);
	}
	if (!dsn) {
		g_print ("Missing Data source name, using the default \"SalesTest\"\n");
		dsn = "SalesTest";
	}

        gda_init ();

	/* open connection */
	cnc = gda_connection_open_from_dsn (dsn, NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
        if (!cnc) {
                g_print ("Could not open connection to DSN '%s': %s\n", dsn,
                         error && error->message ? error->message : "No detail");
                exit (EXIT_FAILURE);
        }

	/* run XSL transform */
	GdaXsltExCont *sql_ctx;
	int ret;

	sql_ctx = gda_xslt_create_context_simple (cnc, &error);
	if (!sql_ctx) {
		g_print ("gda_xslt_create_context_simple error: %s\n", 
			 error && error->message ? error->message : "No detail");
		exit (EXIT_FAILURE);
	}

	ret = sqlxslt_process_xslt_file_ext (sql_ctx, inputfile, xslfile, outputfile);
	g_print ("Checking the sql execution context\n");
	if (sql_ctx->error) 
		g_print ("exec error: %s\n",
			 sql_ctx->error->message ? sql_ctx->error->message : "No detail");
	else 
		g_print("No error on the sql extension\n");

	ret = gda_xslt_finalize_context (sql_ctx);

	/* close connection */
        g_object_unref (G_OBJECT (cnc));

        return EXIT_SUCCESS;
}

/*
 * Process the input file 
 */
static int
sqlxslt_process_xslt_file_ext (GdaXsltExCont *sql_ctx, const char *inputFile, 
			       const char *xslFileName, const char *outputFileName) {
	int ret = 0;
	xmlDocPtr doc,res;
	xsltStylesheetPtr xsltdoc;
	xsltTransformContextPtr ctxt;

	static int init = 0;

	if( !init ) {
		g_print ("SQL extension init.\n");
		init = 1;
		gda_xslt_register();
	}

	doc = xmlReadFile(inputFile,NULL,0);
	if (!doc) {
		g_print ("read file failed\n");
		ret = -1;
		goto endf;
	}

	xsltdoc = xsltParseStylesheetFile((xmlChar*) xslFileName);
	if (!xsltdoc) {
		g_print ("xsltParseStylesheetFile failed\n");
		goto cleanupdoc;
	}

	ctxt = xsltNewTransformContext (xsltdoc, doc);
	gda_xslt_set_execution_context (ctxt, sql_ctx);

	res = xsltApplyStylesheetUser (xsltdoc, doc, NULL, NULL, stderr, ctxt);
	if ((ctxt->state == XSLT_STATE_ERROR) || (ctxt->state == XSLT_STATE_STOPPED)) 
		g_print ("xslt process failed (error or stop)\n");
	if (!res) {
		g_print ("xslt process failed, return NULL\n");
		goto cleanupxslt;
	}

	xsltFreeTransformContext (ctxt);

	ret = xsltSaveResultToFilename (outputFileName,res,xsltdoc,0);
	if (ret < 0) {
		g_print ("xsltSaveResultToFilename failed\n");
		goto cleanupresult;
	}

	if (!g_file_test (outputFileName, G_FILE_TEST_EXISTS)) {
		g_print ("No or empty XSLT output file, check your XSL!\n");
		goto cleanupresult;
	}

 cleanupresult:
	xmlFreeDoc (res);
 cleanupxslt:
	xsltFreeStylesheet (xsltdoc);
 cleanupdoc:
	xmlFreeDoc (doc);
 endf:
	return ret;
}

