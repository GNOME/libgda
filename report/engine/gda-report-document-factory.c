/* GDA Report Engine
 * Copyright (C) 2000 CodeFactory AB
 * Copyright (C) 2000 Richard Hult <rhult@codefactory.se>
 * Copyright (C) 2000-2001, The Free Software Foundation
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
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
 * 	Carlos Perelló Marín <carlos@gnome-db.org>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <bonobo.h>
#include <liboaf/liboaf.h>
#include "gda-report-document-factory.h"

#define REPORT_DOCUMENTFACTORY_OAFIID "OAFIID:GNOME_GDA_Report_DocumentFactory"

static void gda_report_document_factory_class_init (GdaReportDocumentFactoryClass *klass);
static void gda_report_document_factory_init       (GdaReportDocumentFactory      *factory);
static void gda_report_document_factory_destroy    (GtkObject          *object);

static BonoboGenericFactoryClass *parent_class = NULL;
GtkType
gda_report_document_factory_get_type (void)
{
	static GtkType object_type = 0;
	
	if (object_type == 0) {
		GtkType type_of_parent;
		static const GtkTypeInfo object_info = {
			"GdaReportDocumentFactory",
			sizeof (GdaReportDocumentFactory),
			sizeof (GdaReportDocumentFactoryClass),
			(GtkClassInitFunc) gda_report_document_factory_class_init,
			(GtkObjectInitFunc) gda_report_document_factory_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};
		type_of_parent = BonoboGenericFactory_get_type ();
		object_type = gtk_type_unique (type_of_parent, &object_info);
		parent_class = gtk_type_class (type_of_parent);
	}
	return object_type;
}

static POA_GDA_Report_DocumentFactory__vepv document_factory_vepv;

struct _GdaReportDocumentFactoryPriv {
	gint     ref_count;
	GList   *documents; /* List of all reports available. */
};

static void
document_destroyed (GtkObject *object, GdaReportDocumentFactory *df)
{
	GdaReportDocument *document;

	document = GDA_REPORT_DOCUMENT (object);

	df->priv->documents = g_list_remove (df->priv->documents, document);

	/* FIXME: We also need to remove the file for the @object report */

}

static BonoboObject *
document_fn (BonoboGenericFactory factory, void *closure)
{
	GdaReportDocument        *document;
	GdaReportDocumentFactory *document_factory;

	document_factory = GDA_REPORT_DOCUMENT_FACTORY (factory);

	/* FIXME: We must create here the Report Document */
	document = NULL;

	document_factory->priv->documents = g_list_prepend (
		document_factory->priv->documents, document);

	gtk_signal_connect (GTK_OBJECT (document),
			    "destroy",
			    document_destroyed,
			    document_factory);

	return BONOBO_OBJECT (document);
}

POA_GDA_Report_DocumentFactory__epv *
gda_report_document_factory_get_epv (void)
{
	POA_GDA_Report_DocumentFactory__epv *epv;

	epv = g_new0 (POA_GDA_Report_DocumentFactory__epv, 1);

	return epv;
}

static void
ref (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaReportDocumentFactory * factory = GDA_REPORT_DOCUMENT_FACTORY (bonobo_object_from_servant (servant));

	factory->priv->ref_count++;
}

static void
unref (PortableServer_Servant  servant, CORBA_Environment      *ev)
{
	GdaReportDocumentFactory *factory = GDA_REPORT_DOCUMENT_FACTORY (bonobo_object_from_servant (servant));

	factory->priv->ref_count--;
	if (factory->priv->ref_count <= 0) {
		/* Exit. */
		gda_log_message (_("Shutting down."));
		gtk_main_quit ();
	}
}

static GDA_Report_DocumentList *
get_documents (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaReportDocumentFactory *df;
	GdaReportDocument        *document;
	GList                    *list;
	GDA_Report_DocumentList  *documents;
	GDA_Report_Document      *document_co;
	guint                     len, i;

	df = GDA_REPORT_DOCUMENT_FACTORY (bonobo_object_from_servant (servant));

	list = df->priv->documents;
	len = g_list_length (list);

	documents = GDA_Report_DocumentList__alloc ();
	documents->_buffer = CORBA_sequence_GDA_Report_Document_allocbuf (len);
	documents->_length = len;
	documents->_maximum = len;
	CORBA_sequence_set_release (documents, FALSE);

	for (i = 0; i < len; i++, list = list->next) {
		document = list->data;

		bonobo_object_ref (BONOBO_OBJECT (document));
		document_co = CORBA_Object_duplicate (BONOBO_OBJREF (document), NULL);
		documents->_buffer[i] = document_co;
	}

	return documents;
}

static void
init_corba_class (void)
{
	gda_report_document_factory_vepv.GNOME_ObjectFactory_epv = bonobo_generic_factory_get_epv ();
	gda_report_document_factory_vepv.GDA_Report_DocumentFactory_epv = gda_report_document_factory_get_epv ();

	gda_report_document_factory_vepv.GNOME_ObjectFactory_epv->ref = ref;
	gda_report_document_factory_vepv.GNOME_ObjectFactory_epv->unref = unref;

	gda_report_document_factory_vepv.GDA_Report_DocumentFactory_epv->getDocuments = get_documents;
}

static void
gda_report_document_factory_class_init (GdaReportDocumentFactoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	object_class->destroy = gda_report_document_factory_destroy;

	init_corba_class ();
}

static void
gda_report_document_factory_init (GdaReportDocumentFactory *factory)
{
	factory->priv = g_new0 (GdaReportDocumentFactoryPriv, 1);
}

void
gda_report_document_factory_construct (GdaReportDocumentFactory   *factory,
				       GDA_Report_DocumentFactory  corba_factory)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (GDA_IS_REPORT_DOCUMENT_FACTORY (factory));
	g_return_if_fail (corba_factory != CORBA_OBJECT_NIL);

	/* Call parent constructor. */
	bonobo_generic_factory_construct (GDA_REPORT_DOCUMENT_FACTORY_OAFIID,
					  BONOBO_GENERIC_FACTORY (factory),
					  corba_factory,
					  document_fn,
					  NULL,
					  NULL);
}

static GDA_Report_DocumentFactory
create_document_factory (BonoboObject *factory)
{
	POA_GDA_Report_DocumentFactory *servant;
	CORBA_Environment                   ev;

	servant = (POA_GDA_Report_DocumentFactory *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &gda_report_document_factory_vepv;

	CORBA_exception_init (&ev);
	POA_GDA_Report_DocumentFactory__init ((PortableServer_Servant) servant, &ev);

	if (ev._major != CORBA_NO_EXCEPTION){
		gda_log_message (_("document factory: Could not init the servant."));
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	
	/*
	 * Activate the CORBA object & bind to the servant.
	 */
	return (GDA_Report_DocumentFactory) bonobo_object_activate_servant (factory, servant);
}

GdaReportDocumentFactory *
gda_report_document_factory_new (void)
{
	GdaReportDocumentFactory   *factory;
	GDA_Report_DocumentFactory  corba_factory;
	CORBA_Environment           ev;
	gboolean                    is_nil;

	factory = gtk_type_new (gda_report_document_factory_get_type ());

	corba_factory = create_document_factory (BONOBO_OBJECT (factory));

	CORBA_exception_init (&ev);
	is_nil = CORBA_Object_is_nil (corba_factory, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION || is_nil) {
		gda_log_message (_("gda_report_document_factory_new(): could not create the CORBA factory"));
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	gda_report_document_factory_construct (factory, corba_factory);     

	return factory;
}

static void
gda_report_document_factory_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}
