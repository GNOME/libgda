/* libgda library
 *
 * Copyright (C) 2000 Carlos Perelló Marín <carlos@gnome-db.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gda-common.h>
#include <gda-report-stream.h>

enum {
  REPORTSTREAM_ERROR,
  REPORTSTREAM_WARNING,
  LAST_SIGNAL
};

static gint
gda_reportstream_signals[LAST_SIGNAL] = {0, };

#ifdef HAVE_GOBJECT
static void    gda_reportstream_class_init  (Gda_Report_StreamClass *klass,
                                             gpointer data);
static void    gda_reportstream_init        (Gda_Report_Stream *object,
                                             Gda_Report_StreamClass *klass);
#else
static void    gda_reportstream_class_init    (Gda_Report_StreamClass* klass);
static void    gda_reportstream_init          (Gda_Report_Stream* object);
#endif

static void    gda_reportstream_real_error    (Gda_Report_Stream* object, GList* errors);
static void    gda_reportstream_real_warning  (Gda_Report_Stream* object, GList* errors);

static void
gda_reportstream_real_error (Gda_Report_Stream* object, GList* errors)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	object->errors_head = g_list_concat(object->errors_head, errors);
}

static void
gda_reportstream_real_warning (Gda_Report_Stream* object, GList* warnings)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_reportstream_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (Gda_Report_StreamClass),			/* class_size */
			NULL,						/* base_init */
			NULL,						/* base_finalize */
			(GClassInitFunc) gda_reportstream_class_init,	/* class_init */
			NULL,						/* class_finalize */
			NULL,						/* class_data */
			sizeof (Gda_Report_Stream),			/* instance_size */
			0,						/* n_preallocs */
			(GInstanceInitFunc) gda_reportstream_init,	/* instance_init */
			NULL,						/* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "Gda_Report_Stream", &info);
	}
	return type;
}

#else
GtkType
gda_reportstream_get_type (void)
{
	static guint gda_reportstream_type = 0;

	if (!gda_reportstream_type) {
		GtkTypeInfo gda_reportstream_info = {
			"Gda_Report_Stream",
			sizeof (Gda_Report_Stream),
			sizeof (Gda_Report_StreamClass),
			(GtkClassInitFunc) gda_reportstream_class_init,
			(GtkObjectInitFunc) gda_reportstream_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL,
		};
		gda_reportstream_type = gtk_type_unique(gtk_object_get_type(),
							&gda_reportstream_info);
	}
	return gda_reportstream_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_reportstream_class_init (Gda_Report_StreamClass *klass, gpointer data)
{
	/* FIXME: No GObject signals yet */
	klass->error = gda_reportstream_real_error;
	klass->warning = gda_reportstream_real_warning;

}
#else
static void
gda_reportstream_class_init (Gda_Report_StreamClass* klass)
{
	GtkObjectClass* object_class = GTK_OBJECT_CLASS(klass);

	gda_reportstream_signals[REPORTSTREAM_ERROR] = \
			gtk_signal_new("error",
					GTK_RUN_FIRST,
					object_class->type,
					GTK_SIGNAL_OFFSET(Gda_Report_StreamClass, error),
					gtk_marshal_NONE__POINTER,
					GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_reportstream_signals[REPORTSTREAM_WARNING] = \
			gtk_signal_new("warning",
					GTK_RUN_LAST,
					object_class->type,
					GTK_SIGNAL_OFFSET(Gda_Report_StreamClass, warning),
					gtk_marshal_NONE__POINTER,
					GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class, gda_reportstream_signals, LAST_SIGNAL);
	klass->error   = gda_reportstream_real_error;
	klass->warning = gda_reportstream_real_warning;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_reportstream_init (Gda_Report_Stream* object, Gda_Report_StreamClass *klass)
#else
gda_reportstream_init (Gda_Report_Stream *object)
#endif
{
	g_return_if_fail(GDA_REPORTSTREAM_IS_OBJECT(object));
}

static gint
gda_reportstream_corba_exception (Gda_Report_Stream* object, CORBA_Environment* ev)
{
	Gda_Error* error;

	g_return_val_if_fail(ev != 0, -1);

	switch (ev->_major) {
		case CORBA_NO_EXCEPTION:
			return 0;
		case CORBA_SYSTEM_EXCEPTION: {
			CORBA_SystemException* sysexc;
			
			sysexc = CORBA_exception_value(ev);
			error = gda_error_new();
			error->source = g_strdup("[CORBA System Exception]");
			switch(sysexc->minor) {
				case ex_CORBA_COMM_FAILURE:
					error->description = \
						g_strdup_printf(_("%s: The server didn't respond."),
								CORBA_exception_id(ev));
					break;
	 			default:
					error->description = \
						g_strdup_printf(_("%s: An Error occured in the CORBA system."),
								CORBA_exception_id(ev));
					break;
			}
			gda_reportstream_add_single_error(object, error);
			return -1;
		}
		case CORBA_USER_EXCEPTION:
			error = gda_error_new();
			error->source = g_strdup("[CORBA User Exception]");
			gda_reportstream_add_single_error(object, error);
			return -1;
		default:
			g_error(_("Unknown CORBA exception for reportstream"));
	}
	return 0;
}

/**
 * gda_reportstream_new:
 * @orb: the ORB
 *
 * Allocates space for a client reportstream object
 *
 * Returns: the pointer to the allocated object
 */
Gda_Report_Stream*
gda_reportstream_new (CORBA_ORB orb)
{
	Gda_Report_Stream* object;

	g_return_val_if_fail(orb != NULL, 0);

#ifdef HAVE_GOBJECT
	object = GDA_REPORTSTREAM (g_object_new (GDA_TYPE_REPORTSTREAM, NULL));
#else
	object = gtk_type_new(gda_reportstream_get_type());
#endif
	object->orb = orb;
	return object;
}

/**
 * gda_reportstream_free:
 * @object: the reportstream
 *
 * If exists a reportstream object, the object is freed.
 *
 */
void
gda_reportstream_free (Gda_Report_Stream* object)
{
	g_return_if_fail(IS_GDA_REPORTSTREAM(object));
  
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (object));
#else
	gtk_object_destroy (GTK_OBJECT (object));
#endif
}

/**
 * gda_reportstream_readchunk:
 * @start: The position where we must start to read.
 * @size: The number of bytes we must read.
 *
 * Returns: The size (in bytes) of the report stream, -1 if exists an error.
 *
 */
GDA_Report_StreamChunk*
gda_reportstream_readChunk (gint start, gint size)
{
	
}

/**
 * gda_reportstream_readchunk:
 * @start: The position where we must start to read.
 * @size: The number of bytes we must read.
 *
 * Returns: The size (in bytes) of the report stream, -1 if exists an error.
 *
 */
gint
gda_reportstream_writeChunk (GDA_Report_StreamChunk* data, gint size)
{
	
}

/**
 * gda_reportstream_length:
 *
 * Gets the report stream's size in bytes.
 *
 * Returns: The size (in bytes) of the report stream, -1 if exists an error.
 *
 */
gint
gda_reportstream_length (Gda_Report_Stream* object)
{
	CORBA_Environment ev;
	CORBA_long        size;
  
	g_return_val_if_fail(IS_GDA_REPORTSTREAM(object), 0);
  
	CORBA_exception_init(&ev);
	size = GDA_Report_Stream_getLength(object, &ev);
	if (gda_reportstream_corba_exception(object, &ev))
		return -1;
	else
		return size;

}
