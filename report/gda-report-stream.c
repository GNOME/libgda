/* libgda library
 *
 * Copyright (C) 2000,2001 Carlos Perelló Marín <carlos@gnome-db.org>
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

#include <gda-report-stream.h>


enum {
	REPORT_STREAM_ERROR,
	REPORT_STREAM_WARNING,
	LAST_SIGNAL
};

static gint
gda_report_stream_signals[LAST_SIGNAL] = {0, };


#ifdef HAVE_GOBJECT
static void    gda_report_stream_class_init    (GdaReportStreamClass *klass,
						gpointer data);
static void    gda_report_stream_init          (GdaReportStream *object,
						GdaReportStreamClass *klass);
#else
static void    gda_report_stream_class_init    (GdaReportStreamClass* klass);
static void    gda_report_stream_init          (GdaReportStream* object);
#endif

static void    gda_report_stream_real_error    (GdaReportStream* object, GList* errors);
static void    gda_report_stream_real_warning  (GdaReportStream* object, GList* errors);

static void
gda_report_stream_real_error (GdaReportStream* object, GList* errors)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	object->errors_head = g_list_concat(object->errors_head, errors);
}

static void
gda_report_stream_real_warning (GdaReportStream* object, GList* warnings)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_report_stream_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaReportStreamClass),			/* class_size */
			NULL,						/* base_init */
			NULL,						/* base_finalize */
			(GClassInitFunc) gda_report_stream_class_init,	/* class_init */
			NULL,						/* class_finalize */
			NULL,						/* class_data */
			sizeof (GdaReportStream),			/* instance_size */
			0,						/* n_preallocs */
			(GInstanceInitFunc) gda_report_stream_init,	/* instance_init */
			NULL,						/* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaReportStream", &info);
	}
	return type;
}
#else
GtkType
gda_report_stream_get_type (void)
{
	static guint gda_report_stream_type = 0;

	if (!gda_report_stream_type) {
		GtkTypeInfo gda_report_stream_info = {
			"GdaReportStream",
			sizeof (GdaReportStream),
			sizeof (GdaReportStreamClass),
			(GtkClassInitFunc) gda_report_stream_class_init,
			(GtkObjectInitFunc) gda_report_stream_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL,
		};
		gda_report_stream_type = gtk_type_unique(gtk_object_get_type(),
							 &gda_report_stream_info);
	}
	return gda_report_stream_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_report_stream_class_init (GdaReportStreamClass *klass, gpointer data)
{
	/* FIXME: No GObject signals yet */
	klass->error = gda_report_stream_real_error;
	klass->warning = gda_report_stream_real_warning;
}
#else
static void
gda_report_stream_class_init (GdaReportStreamClass* klass)
{
	GtkObjectClass*   object_class = GTK_OBJECT_CLASS(klass);
  
	gda_report_stream_signals[REPORT_STREAM_ERROR] = \
				gtk_signal_new("error",
						GTK_RUN_FIRST,
						object_class->type,
						GTK_SIGNAL_OFFSET(GdaReportStreamClass, error),
						gtk_marshal_NONE__POINTER,
						GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_report_stream_signals[REPORT_STREAM_WARNING] = \
				gtk_signal_new("warning",
						GTK_RUN_LAST,
						object_class->type,
						GTK_SIGNAL_OFFSET(GdaReportStreamClass, warning),
						gtk_marshal_NONE__POINTER,
						GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class, gda_report_stream_signals, LAST_SIGNAL);
	klass->error   = gda_report_stream_real_error;
	klass->warning = gda_report_stream_real_warning;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_report_stream_init (GdaReportStream* object, GdaReportStreamClass *klass)
#else
gda_report_stream_init (GdaReportStream *object)
#endif
{
	g_return_if_fail(GDA_REPORT_STREAM_IS_OBJECT(object));
	object->corba_reportstream = CORBA_OBJECT_NIL;
	object->engine = NULL;
	object->seek = 0;
	object->errors_head = NULL;
}

/**
 * gda_report_stream_new:
 * @engine: the Report Engine to connect
 *
 * Allocates space for a client reportstream object
 *
 * Returns: the pointer to the allocated object
 */
GdaReportStream*
gda_report_stream_new (GdaReportEngine* engine)
{
	CORBA_Environment ev;
	GdaReportStream* object;
	
	g_return_val_if_fail(IS_GDA_REPORT_ENGINE(engine), NULL);

#ifdef HAVE_GOBJECT
	object = GDA_REPORT_STREAM (g_object_new (GDA_TYPE_REPORT_STREAM, NULL));
#else
	object = gtk_type_new(gda_report_stream_get_type());
#endif
	object->engine = engine;
	CORBA_exception_init(&ev);
	object->corba_reportstream = GDA_ReportEngine_createStream(engine->corba_engine, &ev);
	/*  if (gda_reportstream_corba_exception(object, &ev))
	return -1;
	else*/
	CORBA_exception_free(&ev);
	return object;

}

/**
 * gda_report_stream_free:
 * @object: the reportstream
 *
 * If exists a reportstream object, the object is freed.
 *
 */
void
gda_report_stream_free (GdaReportStream* object)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_REPORT_STREAM(object));
	g_return_if_fail(object->corba_reportstream != CORBA_OBJECT_NIL);

	CORBA_exception_init(&ev);
	CORBA_Object_release(object->corba_reportstream, &ev);
	if (!gda_corba_handle_exception(&ev)) {
		gda_log_error(_("CORBA exception unloading report engine: %s"), CORBA_exception_id(&ev));
	}

	CORBA_exception_free(&ev);

#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (object));
#else
	gtk_object_destroy (GTK_OBJECT (object));
#endif
}

/**
 * gda_report_stream_readchunk:
 * @object: The ReportStream object.
 * @data:   The pointer to a pointer where we will put the reference to the buffer
 *	    we have read, this buffer MUST be freed after its use.
 * @start: The position where we must start to read, if this param has the
 *	   GDA_REPORTSTREAM_NEXT value, it starts from the previus possition.
 * @size: The number of bytes we must read.
 *
 * Returns: The size (in bytes) of the data buffer, -1 if exists an error.
 *
 */
gint32
gda_report_stream_readChunk (GdaReportStream* object, guchar** data, gint32 start, gint32 size)
{
	CORBA_Environment ev;
	GDA_ReportStreamChunk *chunk;
	gint32 real_size;
	
	g_return_val_if_fail(IS_GDA_REPORT_STREAM(object), -1);

	if (start == GDA_REPORT_STREAM_NEXT) {
		chunk = GDA_ReportStream_readChunk(object->corba_reportstream, object->seek, size, &ev);
		
	} else {
		chunk = GDA_ReportStream_readChunk(object->corba_reportstream, start, size, &ev);
	}
	/*  if (gda_reportstream_corba_exception(object, &ev))
	return -1;
	else*/
	CORBA_exception_free(&ev);

	real_size = chunk->_length;
	*data = (guchar *) g_memdup(chunk->_buffer, real_size);
	if (start == GDA_REPORT_STREAM_NEXT) {
		object->seek = real_size;
	} else {
		object->seek += real_size;
	}
	CORBA_sequence_set_release (chunk, CORBA_TRUE);
	CORBA_free (chunk);
	return real_size;
}

/**
 * gda_report_stream_writechunk:
 * @object: The ReportStream object.
 * @data:   The pointer to the data which will be writted.
 * @size:   The number of bytes we must write (the data buffer MUST be <= size.
 *
 * Returns: The number of bytes we have wroten to the ReportStream, -1 if exists
 *	    an error.
 *
 */
gint32
gda_report_stream_writeChunk (GdaReportStream* object, guchar* data, gint32 size)
{
	CORBA_Environment ev;
	GDA_ReportStreamChunk chunk;
	gint32 real_size;
	
	g_return_val_if_fail(IS_GDA_REPORT_STREAM(object), -1);

	chunk._maximum = 0;
	chunk._length = size;
	chunk._buffer = (CORBA_octet *) g_memdup(data, size);
	chunk._release = CORBA_FALSE;
 
	real_size = GDA_ReportStream_writeChunk(object->corba_reportstream, &chunk, size, &ev);
	/*  if (gda_reportstream_corba_exception(object, &ev))
	return -1;
	else*/
	CORBA_exception_free(&ev);
	CORBA_sequence_set_release (&chunk, CORBA_TRUE);
	CORBA_free (chunk._buffer);
	return real_size;
}

/**
 * gda_report_stream_length:
 *
 * Gets the report stream's size in bytes.
 *
 * Returns: The size (in bytes) of the report stream, -1 if exists an error.
 *
 */
gint32
gda_report_stream_length (GdaReportStream* object)
{
	CORBA_Environment ev;
	gint32            size;

	g_return_val_if_fail(IS_GDA_REPORT_STREAM(object), -1);
	
	CORBA_exception_init(&ev);
	size = GDA_ReportStream_getLength(object->corba_reportstream, &ev);
	/*  if (gda_reportstream_corba_exception(object, &ev))
		return -1;
	else*/
	CORBA_exception_free(&ev);
	return size;

}
