/* GNOME DB libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
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

#ifndef __gda_error_h__
#define __gda_error_h__ 1

#include <glib.h>
#include <orb/orbit.h>
#include <gda.h>

/* The error object. Holds error messages resulting from CORBA exceptions
 * or server errors. 
 */

typedef struct _Gda_Error       Gda_Error;
typedef struct _Gda_ErrorClass  Gda_ErrorClass;

#define GDA_TYPE_ERROR            (gda_error_get_type())
#define GDA_ERROR(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_ERROR, Gda_Error)
#define GDA_ERROR_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_ERROR, GdaErrorClass)
#define IS_GDA_ERROR(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_ERROR)
#define IS_GDA_ERROR_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_ERROR))

struct _Gda_Error
{
  GtkObject       object;
  gchar*          description;
  glong           number;
  gchar*          source;
  gchar*          helpurl;
  gchar*          sqlstate;
  gchar*          nativeMsg;
  gchar*          realcommand;
};

struct _Gda_ErrorClass
{
  GtkObjectClass parent_class;
};

guint        gda_error_get_type        (void);
Gda_Error*   gda_error_new             (void);
GList*       gda_errors_from_exception (CORBA_Environment* ev);
void         gda_error_free            (Gda_Error* error);
void         gda_error_list_free       (GList* errors);

const gchar* gda_error_description     (Gda_Error* error);
const glong  gda_error_number          (Gda_Error* error);
const gchar* gda_error_source          (Gda_Error* error);
const gchar* gda_error_helpurl         (Gda_Error* error);
const gchar* gda_error_sqlstate        (Gda_Error* error);
const gchar* gda_error_nativeMsg       (Gda_Error* error);
const gchar* gda_error_realcommand     (Gda_Error* error);

#endif
							     
  
  
