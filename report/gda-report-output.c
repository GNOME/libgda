/* GDA report engine
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <bonobo/bonobo-exception.h>
#include "gda-report-output.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaReportOutputPrivate {
	GdaReportDocument *input_xml;
};

static void gda_report_output_class_init (GdaReportOutputClass *klass);
static void gda_report_output_init       (GdaReportOutput *output,
					  GdaReportOutputClass *klass);
static void gda_report_output_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * CORBA methods implementation
 */

static Bonobo_Stream
impl_ReportOutput_convert (PortableServer_Servant servant,
			   const CORBA_char *mime_type,
			   CORBA_Environment *ev)
{
	GdaReportOutput *output = bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_REPORT_OUTPUT (output), CORBA_OBJECT_NIL, ev);
}

/*
 * GdaReportOutput class implementation
 */

static void
gda_report_output_class_init (GdaReportOutputClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Report_Output__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_report_output_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->convert = impl_ReportOutput_convert;
}

static void
gda_report_output_init (GdaReportOutput *output, GdaReportOutputClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_OUTPUT (output));

	/* allocate private structure */
	output->priv = g_new0 (GdaReportOutputPrivate, 1);
	output->priv->input_xml = NULL;
}

static void
gda_report_output_finalize (GObject *object)
{
	GdaReportOutput *output = (GdaReportOutput *) object;

	g_return_if_fail (GDA_IS_REPORT_OUTPUT (output));

	/* free memory */
	if (BONOBO_IS_OBJECT (output->priv->input_xml)) {
		bonobo_object_unref (BONOBO_OBJECT (output->priv->input_xml));
		output->priv->input_xml = NULL;
	}

	g_free (output->priv);
	output->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaReportOutput,
			 GNOME_Database_Report_Output,
			 PARENT_TYPE,
			 gda_report_output)

GdaReportOutput *
gda_report_output_new (GdaReportDocument *input)
{
	GdaReportOutput *output;

	g_return_val_if_fail (GDA_IS_REPORT_DOCUMENT (input), NULL);

	output = g_object_new (GDA_TYPE_REPORT_OUTPUT, NULL);

	bonobo_object_ref (BONOBO_OBJECT (input));
	output->priv->input_xml = input;

	return output;
}
