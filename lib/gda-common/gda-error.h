/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_error_h__)
#  define __gda_error_h__

#include <gda-common-defs.h>

G_BEGIN_DECLS

#define GDA_TYPE_ERROR            (gda_error_get_type())
#define GDA_ERROR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ERROR, GdaError))
#define GDA_ERROR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ERROR, GdaErrorClass))
#define GDA_IS_ERROR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_ERROR))
#define GDA_IS_ERROR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_ERROR))

typedef struct _GdaError        GdaError;
typedef struct _GdaErrorClass   GdaErrorClass;
typedef struct _GdaErrorPrivate GdaErrorPrivate;

struct _GdaError {
	GObject object;
	GdaErrorPrivate *priv;
};

struct _GdaErrorClass {
	GObjectClass parent_class;
};

GType         gda_error_get_type (void);

GdaError     *gda_error_new (void);
GList        *gda_error_list_from_exception (CORBA_Environment * ev);
void          gda_error_to_exception (GdaError * error,
				      CORBA_Environment * ev);
void          gda_error_list_to_exception (GList * error_list,
					   CORBA_Environment * ev);
GNOME_Database_ErrorSeq *
              gda_error_list_to_corba_seq (GList * error_list);
void          gda_error_free (GdaError * error);
void          gda_error_list_free (GList * errors);

const gchar  *gda_error_get_description (GdaError *error);
void          gda_error_set_description (GdaError *error, const gchar *description);
const glong   gda_error_get_number (GdaError *error);
void          gda_error_set_number (GdaError *error, glong number);
const gchar  *gda_error_get_source (GdaError *error);
void          gda_error_set_source (GdaError *error, const gchar *source);
const gchar  *gda_error_get_sqlstate (GdaError *error);
void          gda_error_set_sqlstate (GdaError *error, const gchar *sqlstate);

G_END_DECLS

#endif
