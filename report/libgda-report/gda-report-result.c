/* GDA report libary
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <santi@gnome-db.org>
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

#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-item-report.h>
#include <libgda-report/gda-report-item-pageheader.h>
#include <libgda-report/gda-report-item-pagefooter.h>
#include <libgda-report/gda-report-item-detail.h>
#include <libgda-report/gda-report-result.h>

#define FIRST_PAGE	1

#define RESULT_REPORT_NAME 	"report"
#define RESULT_PAGE_NAME   	"page"
#define RESULT_FONT_NAME   	"font"
#define RESULT_UNITS_NAME  	"units"
#define RESULT_PAGESIZE_NAME   	"pagesize"
#define RESULT_ORIENTATION_NAME "orientation"
#define RESULT_BGCOLOR_NAME     "bgcolor"
#define RESULT_FONTFAMILY_NAME  "fontfamily"
#define RESULT_FONTSIZE_NAME    "fontsize"
#define RESULT_FONTWEIGHT_NAME  "fontweight"
#define RESULT_FONTITALIC_NAME  "fontitalic"


struct _GdaReportResultPrivate {
	// pool of executed queries

	xmlOutputBufferPtr out;      // output buffer 
	xmlNodePtr 	page;        // current page node
	xmlNodePtr 	font;        // current font node
	gint       	page_number; // current page number (first = 1)
	gdouble    	height;	     // current page size
	gdouble         width;
	gdouble    	y;	     // current position
};


static void gda_report_result_class_init (GdaReportResultClass *klass);
static void gda_report_result_init       (GdaReportResult *result,
					  GdaReportResultClass *klass);
static void gda_report_result_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

void gda_report_result_set_font (GdaReportResult *result,
				 gchar *fontfamily,
				 GdaReportNumber *fontsize,
				 gchar *fontweight,
				 gchar *fontitalic);

void gda_report_result_report_start (GdaReportItem *report,
				     GdaReportResult *result);

void gda_report_result_page_start (GdaReportItem *report,
			           GdaReportResult *result);

void gda_report_result_reportheader (GdaReportItem *report,
				     GdaReportResult *result);

void gda_report_result_pageheader (GdaReportItem *report,
				   GdaReportResult *result);

void gda_report_result_datalist (GdaReportItem *report,
				 GdaReportResult *result);

void gda_report_result_pagefooter (GdaReportItem *report,
				   GdaReportResult *result);

void gda_report_result_reportfooter (GdaReportItem *report,
				     GdaReportResult *result);

void gda_report_result_page_end (GdaReportItem *report,
			         GdaReportResult *result);

void gda_report_result_report_end (GdaReportItem *report,
				   GdaReportResult *result);

gboolean gda_report_result_construct (GdaReportDocument *doc,
				      GdaReportResult *result);


/*
 * GdaReportResult class implementation
 */
static void
gda_report_result_class_init (GdaReportResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_report_result_finalize;
}

static void
gda_report_result_init (GdaReportResult *result, GdaReportResultClass *klass)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT (result));

	result->priv = g_new0 (GdaReportResultPrivate, 1);
	result->priv->out = NULL;
}

static void
gda_report_result_finalize (GObject *object)
{
	GdaReportResult *result = (GdaReportResult *) object;

	g_return_if_fail (GDA_REPORT_IS_RESULT (result));

	if (result->priv->out != NULL)
		xmlOutputBufferClose (result->priv->out);
	g_free (result->priv);
	result->priv = NULL;
	
	parent_class->finalize (object);
}


GType
gda_report_result_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportResultClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_result_class_init,
			NULL,
			NULL,
			sizeof (GdaReportResult),
			0,
			(GInstanceInitFunc) gda_report_result_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaReportResult", &info, 0);
	}
	return type;
}



/*
 * gda_report_result_set_font
 */
void
gda_report_result_set_font (GdaReportResult *result,
			    gchar *fontfamily,
			    GdaReportNumber *fontsize,
			    gchar *fontweight,
			    gchar *fontitalic)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	
	g_return_if_fail (fontfamily != NULL);	
	g_return_if_fail (fontsize   != NULL);	
	g_return_if_fail (fontweight != NULL);	
	g_return_if_fail (fontitalic != NULL);
	
	if (result->priv->font == NULL) 
	{
		/* There is no current font, we'll create it */ 
		result->priv->font = xmlNewNode(NULL, RESULT_FONT_NAME);
		xmlSetProp(result->priv->font, RESULT_FONTFAMILY_NAME, fontfamily);
		xmlSetProp(result->priv->font, RESULT_FONTSIZE_NAME, 
			gda_report_types_number_to_value(fontsize));
		xmlSetProp(result->priv->font, RESULT_FONTWEIGHT_NAME, fontweight);
		xmlSetProp(result->priv->font, RESULT_FONTITALIC_NAME, fontitalic);		
		xmlAddChild(result->priv->page, result->priv->font);
	}
	else
	{
		/* There is a current font.  It's the same as new font ? */
		if ((g_strcasecmp(xmlGetProp(result->priv->font, RESULT_FONTFAMILY_NAME), 
			fontfamily) != 0) ||
		    (g_strcasecmp(xmlGetProp(result->priv->font, RESULT_FONTSIZE_NAME), 
			gda_report_types_number_to_value(fontsize)) != 0) ||
		    (g_strcasecmp(xmlGetProp(result->priv->font, RESULT_FONTWEIGHT_NAME), 
			fontweight) != 0) ||
		    (g_strcasecmp(xmlGetProp(result->priv->font, RESULT_FONTITALIC_NAME), 
			fontitalic) != 0))
		{
			/* font has changed.  we should close previous font tag 
			   and start a new one */
			result->priv->font = xmlNewNode(NULL, RESULT_FONT_NAME);
			xmlSetProp(result->priv->font, RESULT_FONTFAMILY_NAME, fontfamily);
			xmlSetProp(result->priv->font, RESULT_FONTSIZE_NAME, 
				gda_report_types_number_to_value(fontsize));
			xmlSetProp(result->priv->font, RESULT_FONTWEIGHT_NAME, fontweight);
			xmlSetProp(result->priv->font, RESULT_FONTITALIC_NAME, fontitalic);		
			xmlAddChild(result->priv->page, result->priv->font);
		}
	}
}


/*
 * gda_report_result_report_start
 */
void
gda_report_result_report_start (GdaReportItem *report,
			        GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	xmlOutputBufferWriteString (result->priv->out, "<");
	xmlOutputBufferWriteString (result->priv->out, RESULT_REPORT_NAME);	
	xmlOutputBufferWriteString (result->priv->out, " ");
	xmlOutputBufferWriteString (result->priv->out, RESULT_UNITS_NAME); 
	xmlOutputBufferWriteString (result->priv->out, "=\"");
	xmlOutputBufferWriteString (result->priv->out, gda_report_item_report_get_units(report));
	xmlOutputBufferWriteString (result->priv->out, "\"");		    	
	xmlOutputBufferWriteString (result->priv->out, ">\n");	
	
	result->priv->page_number = FIRST_PAGE - 1;
}
	

/*
 * gda_report_result_page_start
 */
void
gda_report_result_page_start (GdaReportItem *report,
			      GdaReportResult *result)
{
	GdaReportItem *reportheader;
	GdaReportItem *pageheader;
	GdaReportItem *pagefooter;
	gint length;
	gint i;
	
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	/* New page is started */
	result->priv->page_number++;	
	result->priv->y = 0;
	result->priv->font = NULL;
	
	result->priv->page  = xmlNewNode (NULL, RESULT_PAGE_NAME);
	xmlSetProp (result->priv->page, RESULT_PAGESIZE_NAME, 
		gda_report_item_report_get_pagesize(report));
	xmlSetProp (result->priv->page, RESULT_ORIENTATION_NAME,
		gda_report_item_report_get_orientation(report));
	xmlSetProp (result->priv->page, RESULT_BGCOLOR_NAME,
		gda_report_types_color_to_value(gda_report_item_report_get_bgcolor(report)));
	
	/* FIXME: This shoud be paper size in current units */
	result->priv->height = 27;
	result->priv->width  = 22;
	
	/* If this page is the first one, we should print reportheader */
	if (result->priv->page_number == FIRST_PAGE) {
		reportheader = gda_report_item_report_get_reportheader(report);
		gda_report_result_reportheader(reportheader, result);
	}

	/* We now print all the page headers */
	length = gda_report_item_report_get_pageheaderlist_length(report);
	for (i=0; i<length; i++) 
	{
		pageheader = gda_report_item_report_get_nth_pageheader(report, i);
		gda_report_result_pageheader(pageheader, result);
	}

	/* We should calculate now the height of page footer list */
	length = gda_report_item_report_get_pagefooterlist_length(report);
	for (i=0; i<length; i++) 
	{
		pagefooter = gda_report_item_report_get_nth_pagefooter(report, i);
		result->priv->height -=	gda_report_types_number_to_double(
					gda_report_item_pagefooter_get_height(pagefooter));
	}	
}
	

/*
 * gda_report_result_reportheader
 */
void
gda_report_result_reportheader (GdaReportItem *report,
			        GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	
}


/*
 * gda_report_result_pageheader
 */
void
gda_report_result_pageheader (GdaReportItem *report,
			      GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	/* evaluate if it should be printed (positionfreq, pagefreq) */
}


/*
 * gda_report_result_datalist
 */
void
gda_report_result_datalist (GdaReportItem *report,
			    GdaReportResult *result)
{
	GdaReportItem *detail;
	
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	/* FIXME: support for groupheader / groupfooter should be added here, 
	   but now we will start just with detail section */
	
	detail = gda_report_item_report_get_detail (report);	
	gda_report_result_set_font (result,
		gda_report_item_detail_get_fontfamily(detail),
		gda_report_item_detail_get_fontsize(detail),
		gda_report_item_detail_get_fontweight(detail),
		gda_report_item_detail_get_fontitalic(detail));
	
}


/*
 * gda_report_result_pagefooter
 */
void
gda_report_result_pagefooter (GdaReportItem *report,
			      GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	
	/* evaluate if it should be printed (positionfreq, pagefreq) */
}


/*
 * gda_report_result_reportfooter
 * @report:
 * @result:
 *
 * Returns:
 */
void
gda_report_result_reportfooter (GdaReportItem *report,
			        GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	
}


/*
 * gda_report_result_page_end
 * @report:
 * @result:
 *
 * Returns:
 * 
 */
void 
gda_report_result_page_end (GdaReportItem *report,
 		            GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	/* For each page, node is dumped to the buffer and freed */	
	xmlOutputBufferWriteString (result->priv->out, "\n");
	xmlNodeDumpOutput (result->priv->out, NULL, result->priv->page, 1, TRUE, NULL);
	xmlFreeNode (result->priv->page);
}


/*
 * gda_report_result_report_end
 * @report:
 * @result:
 *
 * Returns:
 * 
 */
void
gda_report_result_report_end (GdaReportItem *report,
			      GdaReportResult *result)
{
	g_return_if_fail (GDA_REPORT_IS_RESULT(result));	

	xmlOutputBufferWriteString (result->priv->out, "</");
	xmlOutputBufferWriteString (result->priv->out, RESULT_REPORT_NAME);
	xmlOutputBufferWriteString (result->priv->out, ">\n");		
	xmlOutputBufferClose (result->priv->out);
}
	

/*
 * gda_report_result_construct
 * @document:
 * @result:
 *
 * Executes the #GdaReportDocument and put the results in 
 * the #GdaReportResult
 *
 * Returns: true if all ok, false otherwise
 */
gboolean
gda_report_result_construct (GdaReportDocument *document,
			     GdaReportResult *result)
{
	GdaReportItem *report;	

	g_return_val_if_fail (GDA_REPORT_IS_RESULT(result), FALSE);	
	report = gda_report_document_get_root_item (document);

	gda_report_result_report_start (report, result);
	gda_report_result_page_start (report, result);
	gda_report_result_datalist (report, result);
	gda_report_result_reportfooter (report, result);
	gda_report_result_report_end (report, result);

	return TRUE;
}



/**
 * gda_report_result_new_to_memory
 * @document:
 *
 * Create in memory a new #GdaReportResult object, 
 * which is the result to process a #GdaReportDocument object
 *
 * Returns: the newly created object.
 */
GdaReportResult *
gda_report_result_new_to_memory (GdaReportDocument *document)
{
	GdaReportResult *result;

	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT(document), NULL);
	result = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);
	result->priv->out = xmlAllocOutputBuffer (NULL);
	
	if (gda_report_result_construct (document, result))
		return result;
	else
		return NULL;
}



/**
 * gda_report_result_new_to_uri
 * @uri:
 * @document:
 *
 * Create in a uri a new #GdaReportResult object, 
 * which is the result to process a #GdaReportDocument object
 *
 * Returns: the newly created object.
 */
GdaReportResult *
gda_report_result_new_to_uri (const gchar *uri,
			      GdaReportDocument *document)
{
	GdaReportResult *result;

	g_return_val_if_fail (GDA_REPORT_IS_DOCUMENT(document), NULL);
	result = g_object_new (GDA_TYPE_REPORT_DOCUMENT, NULL);
	result->priv->out = xmlOutputBufferCreateFilename (uri, NULL, 0);

	if (gda_report_result_construct (document, result))
		return result;
	else
		return NULL;
}


