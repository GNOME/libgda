/*
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <santi@gnome-db.org>
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
void test_6 (void);
void test_7 (void);
void test_8 (void);
void test_9 (void);


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
		g_print ("	6 -> Setting new reportheader and reportfooter\n");
		g_print ("	7 -> Working with pageheaderlist\n");
		g_print ("	8 -> Creating a new report\n");
		g_print ("	9 -> Executing a simple report\n");
		return 0;
	}
	
	gda_init ("test", "0.1", argc, argv);
	if (g_ascii_strncasecmp(argv[1], "1", 1) == 0) test_1();
	if (g_ascii_strncasecmp(argv[1], "2", 1) == 0) test_2();
	if (g_ascii_strncasecmp(argv[1], "3", 1) == 0) test_3();
	if (g_ascii_strncasecmp(argv[1], "4", 1) == 0) test_4();
	if (g_ascii_strncasecmp(argv[1], "5", 1) == 0) test_5();
	if (g_ascii_strncasecmp(argv[1], "6", 1) == 0) test_6();
	if (g_ascii_strncasecmp(argv[1], "7", 1) == 0) test_7();
	if (g_ascii_strncasecmp(argv[1], "8", 1) == 0) test_8();
	if (g_ascii_strncasecmp(argv[1], "9", 1) == 0) test_9();

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

	g_print ("TEST: Current reportstyle is: %s\n", gda_report_item_report_get_reportstyle (item));

	g_print ("TEST: Setting reportstyle to non-valid-value\n"); 
	if (!gda_report_item_report_set_reportstyle (item, "non-valid-value")) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Setting reportstyle to 'form' \n");
	if (!gda_report_item_report_set_reportstyle (item, "form")) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Current reportstyle is: %s\n", gda_report_item_report_get_reportstyle (item));

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
	
	g_print ("TEST: Current topmargin is: %s\n", gda_report_types_number_to_value(gda_report_item_report_get_topmargin (item)));

	g_print ("TEST: Setting topmargin to 4.75\n"); 
	if (!gda_report_item_report_set_topmargin (item, gda_report_types_number_new(4.75))) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Current topmargin is: %s\n", gda_report_types_number_to_value(gda_report_item_report_get_topmargin (item)));

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
	
	color = gda_report_item_report_get_bgcolor (item);
	g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	g_print ("TEST: Setting bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_report_set_bgcolor (item, color)) 
		g_print ("TEST: Error during set\n");
	
	color = gda_report_item_report_get_bgcolor (item);
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
	GdaReportNumber *width;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	report = gda_report_document_get_root_item (document);
	if (report == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	reportheader = gda_report_item_report_get_reportheader (report);
	if (reportheader == NULL)
	{
		g_print ("TEST: Can't get reportheader \n");
		return;
	}
	
	color = gda_report_item_reportheader_get_bgcolor (reportheader);
	if (color == NULL)
		g_print ("TEST: Current bgcolor is NULL \n");
	else
		g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	width = gda_report_item_reportheader_get_linewidth (reportheader);
	if (width == NULL)
		g_print ("TEST: Current linewidth is NULL \n");
	else
		g_print ("TEST: Current linewidth is: %s\n", gda_report_types_number_to_value(width));

	g_print ("TEST: Setting bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_reportheader_set_bgcolor (reportheader, color)) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Setting linewidth to 1.75\n"); 
	width = gda_report_types_number_new (1.75);
	if (!gda_report_item_reportheader_set_linewidth (reportheader, width)) 
		g_print ("TEST: Error during set\n");
	
	color = gda_report_item_reportheader_get_bgcolor (reportheader);
	g_print ("TEST: Current bgcolor is: %s\n", gda_report_types_color_to_value(color));

	width = gda_report_item_reportheader_get_linewidth (reportheader);
	g_print ("TEST: Current linewidth is: %s\n", gda_report_types_number_to_value(width));

	g_print ("TEST: Saving results to test-5-result.xml\n");
	gda_report_document_save_file ("test-5-result.xml", document);
}



void 
test_6 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *report;
	GdaReportItem *reportheader;
	GdaReportItem *reportfooter;
	GdaReportColor *color;
	GdaReportNumber *width;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);

	report = gda_report_document_get_root_item (document);
	if (report == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	reportheader = gda_report_item_reportheader_new (validator);
	if (reportheader == NULL)
	{
		g_print ("TEST: Can't create a  reportheader \n");
		return;
	}
	
	reportfooter = gda_report_item_reportfooter_new (validator);
	if (reportheader == NULL)
	{
		g_print ("TEST: Can't create a reportfooter \n");
		return;
	}
	
	g_print ("TEST: Setting reportheader bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_reportheader_set_bgcolor (reportheader, color)) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Setting reportfooter linewidth to 1.75\n"); 
	width = gda_report_types_number_new (1.75);
	if (!gda_report_item_reportfooter_set_linewidth (reportfooter, width)) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: Setting new reportheader to report\n"); 
	if (!gda_report_item_report_set_reportheader (report, reportheader))
		g_print ("TEST: Error during set\n");

	g_print ("TEST: Setting new reportfooter to report\n"); 
	if (!gda_report_item_report_set_reportfooter (report, reportfooter)) 
		g_print ("TEST: Error during set\n");

	g_print ("TEST: Saving results to test-6-result.xml\n");
	gda_report_document_save_file ("test-6-result.xml", document);
}



void 
test_7 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *report;
	GdaReportItem *pageheader;
	GdaReportItem *pagefooter;
	GdaReportItem *label;
	GdaReportItem *repfield;
	GdaReportItem *sqlquery1;
	GdaReportItem *sqlquery2;
	GdaReportColor *color;
	GdaReportNumber *width;
	GdaReportNumber *height;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new_from_uri ("valid-example.xml", validator);
	sqlquery1 = gda_report_item_sqlquery_new (validator, "sqlquery1", "select * from some_table");
	sqlquery2 = gda_report_item_sqlquery_new (validator, "sqlquery2", "select * from another_table");

	report = gda_report_document_get_root_item (document);
	if (report == NULL) 
	{
		g_print ("TEST: Can't get root node \n");
		return;
	}
	
	pageheader = gda_report_item_pageheader_new (validator);
	if (pageheader == NULL)
	{
		g_print ("TEST: Can't create a pageheader \n");
		return;
	}
	
	g_print ("TEST: Setting pageheader bgcolor to 127 127 127\n"); 
	color = gda_report_types_color_new (127, 127, 127);
	if (!gda_report_item_pageheader_set_bgcolor (pageheader, color)) 
		g_print ("TEST: Error during set\n");
	
	g_print ("TEST: We have %i pageheaders\n", 
		gda_report_item_report_get_pageheaderlist_length(report));
	
	g_print ("TEST: Setting new pageheader to report\n"); 
	if (!gda_report_item_report_add_nth_pageheader (report, pageheader, 0))
		g_print ("TEST: Error during set\n");

	g_print ("TEST: We have %i pageheaders\n", 
		gda_report_item_report_get_pageheaderlist_length(report));
	
	g_print ("TEST: Setting pageheader linewidth to 1.75\n"); 
	pageheader = gda_report_item_report_get_nth_pageheader (report, 1);
	width = gda_report_types_number_new (1.75);
	if (!gda_report_item_pageheader_set_linewidth (pageheader, width)) 
		g_print ("TEST: Error during set\n");	

	g_print ("TEST: We have %i pagefooters\n", 
		gda_report_item_report_get_pagefooterlist_length(report));
	
	g_print ("TEST: Setting pagefooter height to 16\n"); 
	pagefooter = gda_report_item_report_get_nth_pagefooter (report, 0);
	height = gda_report_types_number_new (16);
	if (!gda_report_item_pagefooter_set_height (pagefooter, height)) 
		g_print ("TEST: Error during set\n");	

	g_print ("TEST: Removing l_publishing field\n"); 
	repfield = gda_report_item_report_get_repfield_by_id (report, "l_publishing");
	if (repfield != NULL)
		gda_report_item_repfield_remove (repfield);
	else g_print("TEST: repfield not found\n");

	g_print ("TEST: Removing l_publishing label\n"); 
	label = gda_report_item_report_get_label_by_id (report, "l_publishing");
	if (label != NULL)
		gda_report_item_label_remove (label);
	else g_print("TEST: label not found\n");

	g_print ("TEST: Removing f_isbn field\n"); 
	repfield = gda_report_item_report_get_repfield_by_id (report, "f_isbn");
	if (repfield != NULL)
		gda_report_item_repfield_remove (repfield);
	else g_print("TEST: repfield not found\n");

	g_print ("TEST: Adding a new sqlquery\n");
	if (!gda_report_item_report_add_sqlquery (report, sqlquery1))
		g_print ("TEST: Error during set\n");	

	g_print ("TEST: Adding another sqlquery\n");
	if (!gda_report_item_report_add_sqlquery (report, sqlquery2))
		g_print ("TEST: Error during set\n");	
	
	sqlquery1 = gda_report_item_report_get_sqlquery (report, "sqlquery1");
	gda_report_item_sqlquery_set_sql (sqlquery1, "select * from dual");
	
	g_print ("TEST: Saving results to test-7-result.xml\n");
	gda_report_document_save_file ("test-7-result.xml", document);
}


void 
test_8 (void) 
{
	GdaReportValid *validator;
	GdaReportDocument  *document;
	GdaReportItem *report;
	GdaReportItem *reportheader;
	GdaReportItem *label1;
	GdaReportItem *label2;
 
	validator = gda_report_valid_load ();
	document = gda_report_document_new (validator);
	report = gda_report_item_report_new (validator);
	reportheader = gda_report_item_reportheader_new (validator);
	label1 = gda_report_item_label_new (validator, "label1");
	label2 = gda_report_item_label_new (validator, "label1");

	g_print ("TEST: Setting reportheader to report.  \nIt should fail becouse report not belongs to a document\n"); 
	if (!gda_report_item_report_set_reportheader (report, reportheader)) 
		g_print ("TEST: Error during set\n");	

	g_print ("TEST: Assigning the report to a document\n"); 
	if (!gda_report_document_set_root_item (document, report)) 
		g_print ("TEST: Error during set\n");	
	
	g_print ("TEST: Setting reportheader to report.  Now it should works\n"); 
	if (!gda_report_item_report_set_reportheader (report, reportheader)) 
		g_print ("TEST: Error during set\n");	
	
	g_print ("TEST: Adding a label to reportheader\n"); 
	if (!gda_report_item_reportheader_add_element (reportheader, label1)) 
		g_print ("TEST: Error during set\n");	
	
	g_print ("TEST: Adding a label with the same ID to reportheader\n"); 
	if (!gda_report_item_reportheader_add_element (reportheader, label2)) 
		g_print ("TEST: Error during set\n");	
	
}


void
test_9 (void) 
{
	GdaReportDocument  *document;
	GdaReportResult    *res_mem;
	GdaReportResult    *res_file;
 
	document = gda_report_document_new_from_uri ("valid-example.xml", NULL);
	res_mem = gda_report_result_new_to_memory (document);
	res_file = gda_report_result_new_to_uri ("test-9-result.xml", document);
}


