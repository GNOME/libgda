/*
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <scamps@users.sourceforge.net>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <libgda/libgda.h>
#include <libgda-report/gda-report.h>

void test_1 (void);
void test_2 (void);
void test_3 (void);
void test_4 (void);
void test_5 (void);


int
main (int argc, char **argv)
{
	if (argc < 2) 
	{
		g_print ("Use gda-report-test number, where number is one of the following: \n");
		g_print ("	1 -> Loading a valid report, changing reportstyle, and saving results\n");
		g_print ("	2 -> Trying to load a non-valid report\n");
		g_print ("	3 -> Working with numerical attributes\n");
		g_print ("	4 -> Working with color attributes\n");
		g_print ("	5 -> Setting a reportheader attribute\n");
		return 0;
	}
	
	gda_init ("test", "0.1", argc, argv);
	if (g_ascii_strncasecmp(argv[1], "1", 1) == 0) test_1();
	if (g_ascii_strncasecmp(argv[1], "2", 1) == 0) test_2();
	if (g_ascii_strncasecmp(argv[1], "3", 1) == 0) test_3();
	if (g_ascii_strncasecmp(argv[1], "4", 1) == 0) test_4();
	if (g_ascii_strncasecmp(argv[1], "5", 1) == 0) test_5();

	return 0;
}



void 
test_1 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *item;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	item = gda_report_document_get_root_item (document);
	if (item == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	g_print ("TEST: Setting non-valid-attribute to some-value\n"); 
	if (!gda_report_item_set_attribute (item, "non-valid-attribute", "some-value")) 
		g_print ("TEST: Error during set\n");

	g_print ("TEST: Current reportstyle is: %s\n", gda_report_item_report_get_reportstyle (GDA_REPORT_ITEM_REPORT(item)));

	g_print ("TEST: Setting reportstyle to non-valid-value\n"); 
	if (!gda_report_item_report_set_reportstyle (GDA_REPORT_ITEM_REPORT(item), "non-valid-value")) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Setting reportstyle to 'form' \n");
	if (!gda_report_item_report_set_reportstyle (GDA_REPORT_ITEM_REPORT(item), "form")) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Current reportstyle is: %s\n", gda_report_item_report_get_reportstyle (GDA_REPORT_ITEM_REPORT(item)));

	g_print ("TEST: Saving results to test-1-result.xml\n");
	gda_report_document_save_file ("test-1-result.xml", document);
}


void test_2 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("non-valid-example.xml", validator);
}


void 
test_3 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *item;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	item = gda_report_document_get_root_item (document);
	if (item == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	g_print ("TEST: Current topmargin is: %s\n", gda_report_types_number_to_value(gda_report_item_report_get_topmargin (GDA_REPORT_ITEM_REPORT(item))));

	g_print ("TEST: Setting topmargin to 4.75\n"); 
	if (!gda_report_item_report_set_topmargin (GDA_REPORT_ITEM_REPORT(item), gda_report_types_number_new(4.75))) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Current topmargin is: %s\n", gda_report_types_number_to_value(gda_report_item_report_get_topmargin (GDA_REPORT_ITEM_REPORT(item))));

	g_print ("TEST: Saving results to test-3-result.xml\n");
	gda_report_document_save_file ("test-3-result.xml", document);
}


void 
test_4 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *item;
	GdaReportColor *color;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	item = gda_report_document_get_root_item (document);
	if (item == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	color = gda_report_item_report_get_bgcolor (GDA_REPORT_ITEM_REPORT(item));
	g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	g_print ("TEST: Setting bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_report_set_bgcolor (GDA_REPORT_ITEM_REPORT(item), color)) 
		g_print ("TEST: Error during set\n");
	
	color = gda_report_item_report_get_bgcolor (GDA_REPORT_ITEM_REPORT(item));
	g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	g_print ("TEST: Saving results to test-4-result.xml\n");
	gda_report_document_save_file ("test-4-result.xml", document);
}


void 
test_5 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *report;
	GdaReportItem *reportheader;
	GdaReportColor *color;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	report = gda_report_document_get_root_item (document);
	if (report == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	reportheader = gda_report_item_report_get_reportheader (GDA_REPORT_ITEM_REPORT(report));
	if (reportheader == NULL)
	{
		g_print ("TEST: Can't get reportheader \n");
		return;
	}
	
	color = gda_report_item_reportheader_get_bgcolor (GDA_REPORT_ITEM_REPORTHEADER(reportheader));
	if (color == NULL)
		g_print ("TEST: Current bgcolor is NULL \n");
	else
		g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	g_print ("TEST: Setting bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_reportheader_set_bgcolor (GDA_REPORT_ITEM_REPORTHEADER(reportheader), color)) 
		g_print ("TEST: Error during set\n");
	
	color = gda_report_item_reportheader_get_bgcolor (GDA_REPORT_ITEM_REPORTHEADER(reportheader));
	g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	g_print ("TEST: Saving results to test-5-result.xml\n");
	gda_report_document_save_file ("test-5-result.xml", document);
}

