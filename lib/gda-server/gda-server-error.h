/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_server_error_h__)
#  define __gda_server_error_h__

#include <glib.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	gchar* description;
	glong  number;
	gchar* source;
	gchar* helpfile;
	gchar* helpctxt;
	gchar* sqlstate;
	gchar* native;
} GdaServerError;

GdaServerError* gda_server_error_new              (void);
gchar*          gda_server_error_get_description  (GdaServerError *error);
void            gda_server_error_set_description  (GdaServerError *error, const gchar *description);
glong           gda_server_error_get_number       (GdaServerError *error);
void            gda_server_error_set_number       (GdaServerError *error, glong number);
void            gda_server_error_set_source       (GdaServerError *error, const gchar *source);
void            gda_server_error_set_help_file    (GdaServerError *error, const gchar *helpfile);
void            gda_server_error_set_help_context (GdaServerError *error, const gchar *helpctxt);
void            gda_server_error_set_sqlstate     (GdaServerError *error, const gchar *sqlstate);
void            gda_server_error_set_native       (GdaServerError *error, const gchar *native);
void            gda_server_error_free             (GdaServerError *error);

void            gda_server_error_make             (GdaServerError *error,
						   GdaServerRecordset *recset,
						   GdaServerConnection *cnc,
						   gchar *where);

#if defined(__cplusplus)
}
#endif

#endif
