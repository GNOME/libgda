/* GDA Report Engine
 * Copyright (C) 2000 CodeFactory AB
 * Copyright (C) 2000 Richard Hult <rhult@codefactory.se>
 * Copyright (C) 2001 The Free Software Foundation
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
 * 	Richard Hult
 * 	Carlos Perelló Marín <carlos@gnome-db.org>
 */

#ifndef __GDA_REPORT_DOCUMENT_FACTORY_H__
#define __GDA_REPORT_DOCUMENT_FACTORY_H__

#include "GDA_Report.h"

#define GDA_REPORT_DOCUMENT_FACTORY_TYPE        (gda_report_document_factory_get_type ())
#define GDA_REPORT_DOCUMENT_FACTORY(o)          (GTK_CHECK_CAST ((o), GDA_REPORT_DOCUMENT_FACTORY_TYPE, GdaReportDocumentFactory))
#define GDA_REPORT_DOCUMENT_FACTORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GDA_REPORT_DOCUMENT_FACTORY_TYPE, GdaReportDocumentFactoryClass))
#define GDA_REPORT_IS_DOCUMENT_FACTORY(o)       (GTK_CHECK_TYPE ((o), GDA_REPORT_DOCUMENT_FACTORY_TYPE))
#define GDA_REPORT_IS_DOCUMENT_FACTORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GDA_REPORT_DOCUMENT_FACTORY_TYPE))

typedef struct _GdaReportDocumentFactoryPriv GdaReportDocumentFactoryPriv;

typedef struct {
	BonoboGenericFactory     parent;
	GdaReportDocumentFactoryPriv      *priv;
} GdaReportDocumentFactory;

typedef struct {
	BonoboGenericFactoryClass parent_class;
} GdaReportDocumentFactoryClass;

GtkType		gda_report_document_factory_get_type  (void);
void		gda_report_document_factory_construct (GdaReportDocumentFactory *factory,
						       GDA_Report_DocumentFactory  corba_factory);
GdaReportDocumentFactory  *gda_report_document_factory_new       (void);

POA_GDA_Report_DocumentFactory__epv *
gda_report_document_factory_get_epv                   (void);

#endif /* __GDA_REPORT_DOCUMENT_FACTORY_H__ */
