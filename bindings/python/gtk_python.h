/* GNOME DB python binding
 * Copyright (C) 2000 Alvaro Lopez Ortega
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <Python.h>

/**
 * The follow functions (with some changes):
 * Copyright (C) 1997-1999 James Henstridge <james@daa.com.au>
 */

typedef struct
{
	PyObject_HEAD GtkObject * obj;
}
Py_GtkObject;

#define gtk_get(i) (((Py_GtkObject *)(i))->obj)


static void
Py_Gtk_dealloc (Py_GtkObject * self)
{
	gtk_object_unref (self->obj);
	PyMem_DEL (self);
}

static int
Py_Gtk_compare (Py_GtkObject * one, Py_GtkObject * two)
{
	if (one->obj == two->obj)
		return 0;
	if (one->obj < two->obj)
		return 1;
	return -1;
}

static long
Py_Gtk_hash (Py_GtkObject * self)
{
	return (long) self->obj;
}

static PyObject *
Py_Gtk_repr (Py_GtkObject * self)
{
	char buffer[0x080];

	sprintf (buffer, "<GtkObject of type %s at %lx>",
		 gtk_type_name (gtk_get (self)->klass->type),
		 (long) gtk_get (self));
	return PyString_FromString (buffer);
}


static gchar _GtkObject_Type__doc__[] = "Type of Gda-clnt";

static PyTypeObject GtkObject_Type = {
	PyObject_HEAD_INIT (&PyType_Type)
		0,		/* ob_size */
	"GtkObject",		/* tp_name */
	sizeof (Py_GtkObject),	/* tp_basicsize */
	0,			/* tp_itemsize */
	(destructor) Py_Gtk_dealloc,	/* tp_dealloc */
	(printfunc) 0,		/* tp_print */
	(getattrfunc) 0,	/* tp_getattr */
	(setattrfunc) 0,	/* tp_setattr */
	(cmpfunc) Py_Gtk_compare,	/* tp_compare */
	(reprfunc) Py_Gtk_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	(hashfunc) Py_Gtk_hash,	/* tp_hash */
	(ternaryfunc) 0,	/* tp_call */
	(reprfunc) 0,		/* tp_str */
	0L, 0L, 0L, 0L,
	_GtkObject_Type__doc__
};



/* PyGtk_New needs PyObject_Type 
 */

static PyObject *
PyGtk_New (GtkObject * go)
{
	Py_GtkObject *self;
	self = (Py_GtkObject *) PyObject_NEW (Py_GtkObject, &GtkObject_Type);

	if (self == NULL)
		return NULL;

	self->obj = go;
	gtk_object_ref (self->obj);
	return (PyObject *) self;
}
