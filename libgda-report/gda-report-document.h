/*
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2003 Santi Camps <santi@gnome-db.org>
 * Copyright (C) 2003 Santi Camps Taltavull <santi@src.gnome.org>
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_REPORT_DOCUMENT_H__
#define __GDA_REPORT_DOCUMENT_H__

#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gda_report_document_error_quark (void);
#define GDA_REPORT_DOCUMENT_ERROR gda_report_document_error_quark ()

typedef enum {
	GDA_REPORT_DOCUMENT_GENERAL_ERROR
} GdaReportDocumentError;



#define GDA_TYPE_REPORT_DOCUMENT            (gda_report_document_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaReportDocument, gda_report_document, GDA, REPORT_DOCUMENT, GObject)

struct _GdaReportDocumentClass {
	GObjectClass              parent_class;

	/* virtual methods */
	gboolean                (*run_as_html) (GdaReportDocument *doc, const gchar *filename, GError **error);
	gboolean                (*run_as_pdf) (GdaReportDocument *doc, const gchar *filename, GError **error);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-report-document
 * @short_description: Report document
 * @title: GdaReportDocument
 * @stability: Stable
 * @see_also:
 *
 * The #GdaReportDocument wraps the usage of a #GdaReportEngine for specific HTML or PDF targets. This class is
 * abstract (no instance be created directly), and one of its subclasses has to be used.
 */

void                  gda_report_document_set_template    (GdaReportDocument *doc, const gchar *file);
gboolean              gda_report_document_run_as_html     (GdaReportDocument *doc, const gchar *filename, GError **error);
gboolean              gda_report_document_run_as_pdf      (GdaReportDocument *doc, const gchar *filename, GError **error);

G_END_DECLS

#endif
